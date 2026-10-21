#include "serialization/SceneLua.h"

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "core/Diagnostics.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"
#include "game/GameAssetsIO.h"

namespace loblox::scenelua {
namespace {

using json = nlohmann::json;
constexpr std::size_t kMaxSceneBytes = 16u * 1024u * 1024u;
constexpr int kMaxNesting = 64;
constexpr std::size_t kMaxValues = 250000;

class LiteralParser {
public:
    explicit LiteralParser(const std::string& input) : input_(input) {}

    bool parse(json& output, std::string& error) {
        skip();
        if (!word("return")) return fail(error, "esperado `return` no inicio");
        if (!value(output, 0, error)) return false;
        skip();
        if (position_ != input_.size()) return fail(error, "conteudo executavel ou dados extras rejeitados");
        return true;
    }

private:
    const std::string& input_;
    std::size_t position_ = 0;
    std::size_t values_ = 0;

    bool fail(std::string& error, const std::string& message) const {
        error = "Lua declarativo invalido no byte " + std::to_string(position_) + ": " + message;
        return false;
    }

    void skip() {
        for (;;) {
            while (position_ < input_.size() &&
                   std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_;
            if (position_ + 1 < input_.size() && input_[position_] == '-' && input_[position_ + 1] == '-') {
                position_ += 2;
                while (position_ < input_.size() && input_[position_] != '\n') ++position_;
                continue;
            }
            break;
        }
    }

    bool word(const char* expected) {
        skip();
        std::size_t cursor = position_;
        while (cursor < input_.size() &&
               (std::isalnum(static_cast<unsigned char>(input_[cursor])) || input_[cursor] == '_')) ++cursor;
        const std::size_t length = std::char_traits<char>::length(expected);
        if (input_.compare(position_, cursor - position_, expected) != 0 || cursor - position_ != length) return false;
        position_ = cursor;
        return true;
    }

    bool identifier(std::string& result) {
        skip();
        if (position_ >= input_.size() ||
            !(std::isalpha(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) return false;
        const std::size_t start = position_++;
        while (position_ < input_.size() &&
               (std::isalnum(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) ++position_;
        result = input_.substr(start, position_ - start);
        return true;
    }

    bool stringValue(json& output, std::string& error) {
        ++position_;
        std::string result;
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"') { output = std::move(result); return true; }
            if (static_cast<unsigned char>(c) < 0x20) return fail(error, "controle invalido em string");
            if (c != '\\') { result.push_back(c); continue; }
            if (position_ >= input_.size()) return fail(error, "escape incompleto");
            const char escaped = input_[position_++];
            switch (escaped) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                default: return fail(error, "escape nao suportado");
            }
        }
        return fail(error, "string sem fechamento");
    }

    bool table(json& output, int depth, std::string& error) {
        if (depth >= kMaxNesting) return fail(error, "tabela excede limite de profundidade");
        ++position_;
        skip();
        if (position_ < input_.size() && input_[position_] == '}') {
            ++position_; output = json::array(); return true;
        }
        const std::size_t before = position_;
        std::string first;
        const bool identifierStart = identifier(first);
        skip();
        const bool isObject = identifierStart && position_ < input_.size() && input_[position_] == '=';
        position_ = before;
        if (isObject) {
            output = json::object();
            for (;;) {
                std::string key;
                if (!identifier(key)) return fail(error, "chave deve ser um identificador");
                skip();
                if (position_ >= input_.size() || input_[position_++] != '=') return fail(error, "esperado `=` apos chave");
                json item;
                if (!value(item, depth + 1, error)) return false;
                if (output.contains(key)) return fail(error, "chave duplicada `" + key + "`");
                output[key] = std::move(item);
                skip();
                if (position_ >= input_.size()) return fail(error, "tabela sem fechamento");
                if (input_[position_] == '}') { ++position_; return true; }
                if (input_[position_++] != ',') return fail(error, "esperada virgula entre campos");
                skip();
                if (position_ < input_.size() && input_[position_] == '}') { ++position_; return true; }
            }
        }
        output = json::array();
        for (;;) {
            json item;
            if (!value(item, depth + 1, error)) return false;
            output.push_back(std::move(item));
            skip();
            if (position_ >= input_.size()) return fail(error, "tabela sem fechamento");
            if (input_[position_] == '}') { ++position_; return true; }
            if (input_[position_++] != ',') return fail(error, "esperada virgula entre itens");
            skip();
            if (position_ < input_.size() && input_[position_] == '}') { ++position_; return true; }
        }
    }

    bool value(json& output, int depth, std::string& error) {
        skip();
        if (++values_ > kMaxValues) return fail(error, "cena excede limite de valores");
        if (position_ >= input_.size()) return fail(error, "valor ausente");
        if (input_[position_] == '{') return table(output, depth, error);
        if (input_[position_] == '"') return stringValue(output, error);
        if (word("true")) { output = true; return true; }
        if (word("false")) { output = false; return true; }
        char* end = nullptr;
        const char* start = input_.c_str() + position_;
        const double number = std::strtod(start, &end);
        if (end != start) {
            const std::size_t length = static_cast<std::size_t>(end - start);
            bool integral = true;
            for (std::size_t i = 0; i < length; ++i) {
                if (start[i] == '.' || start[i] == 'e' || start[i] == 'E') integral = false;
            }
            position_ += static_cast<std::size_t>(end - start);
            if (!std::isfinite(number)) return fail(error, "numero deve ser finito");
            if (integral) {
                errno = 0;
                char* integerEnd = nullptr;
                const long long integer = std::strtoll(start, &integerEnd, 10);
                if (errno == ERANGE || integerEnd != end) return fail(error, "inteiro fora da faixa suportada");
                output = integer;
            } else {
                output = number;
            }
            return true;
        }
        return fail(error, "somente tabelas, strings, numeros e booleanos sao aceitos");
    }
};

bool HasOnlyKeys(const json& object, std::initializer_list<const char*> allowed,
                 std::string& error, const std::string& where) {
    if (!object.is_object()) { error = where + " deve ser tabela nomeada"; return false; }
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool found = false;
        for (const char* key : allowed) if (it.key() == key) { found = true; break; }
        if (!found) { error = where + " contem propriedade desconhecida `" + it.key() + "`"; return false; }
    }
    return true;
}

bool ValidateSchema(const json& root, std::string& error) {
    if (!HasOnlyKeys(root, {"format", "version", "nextId", "objects"}, error, "raiz")) return false;
    if (!root.contains("format") || !root["format"].is_string() ||
        root["format"].get<std::string>() != "loblox-scene") {
        error = "formato declarativo desconhecido (esperado loblox-scene)"; return false;
    }
    if (!root.contains("version") || !root["version"].is_number_integer() ||
        root["version"].get<long long>() != 1) {
        error = "versao do formato Lua nao suportada (esperado 1)"; return false;
    }
    if (!root.contains("objects") || !root["objects"].is_array()) {
        error = "objects deve ser uma lista de objetos"; return false;
    }
    if (root.contains("nextId") && !root["nextId"].is_number_integer()) {
        error = "nextId deve ser inteiro"; return false;
    }
    for (std::size_t i = 0; i < root["objects"].size(); ++i) {
        const json& object = root["objects"][i];
        const std::string prefix = "objects[" + std::to_string(i + 1) + "]";
        if (!object.is_object() || !object.contains("class") || !object["class"].is_string()) {
            error = prefix + " precisa declarar class"; return false;
        }
        const std::string type = object["class"].get<std::string>();
        if (type == "Workspace") {
            if (!HasOnlyKeys(object, {"id", "class", "name", "parent"}, error, prefix)) return false;
        } else if (type == "Lighting") {
            if (!HasOnlyKeys(object, {"id", "class", "name", "parent", "skyColor", "ambientColor",
                                      "sunDirection", "sunIntensity", "brightness",
                                      "fogColor", "fogDensity", "skyboxEnabled", "skyEnabled",
                                      "skyZenithColor", "skyHorizonColor", "skyGroundColor",
                                      "sunColor", "sunSize", "atmosphereStrength", "cloudDensity",
                                      "cloudScale", "cloudSpeed", "cloudSoftness", "skyRotation"}, error, prefix)) return false;
        } else if (type == "Part" || type == "Baseplate" || type == "SpawnLocation") {
            if (!HasOnlyKeys(object, {"id", "class", "name", "parent", "shape", "position", "rotation",
                                      "size", "color", "material", "anchored", "canCollide"}, error, prefix)) return false;
        } else {
            error = prefix + " declara classe desconhecida `" + type + "`"; return false;
        }
    }
    return true;
}

void Indent(std::ostringstream& out, int depth) {
    for (int i = 0; i < depth * 4; ++i) out.put(' ');
}

void Emit(const json& value, std::ostringstream& out, int depth) {
    if (value.is_object()) {
        out << "{\n";
        for (auto it = value.begin(); it != value.end(); ++it) {
            Indent(out, depth + 1); out << it.key() << " = "; Emit(it.value(), out, depth + 1); out << ",\n";
        }
        Indent(out, depth); out << '}';
    } else if (value.is_array()) {
        if (value.empty()) { out << "{}"; return; }
        out << "{\n";
        for (const auto& item : value) {
            Indent(out, depth + 1); Emit(item, out, depth + 1); out << ",\n";
        }
        Indent(out, depth); out << '}';
    } else if (value.is_string()) {
        out << value.dump(-1, ' ', false, json::error_handler_t::strict);
    } else {
        out << value.dump();
    }
}

}  // namespace

std::string toLua(const Scene& scene) {
    if (game::EncodeAssets(scene.gameAssets()) != game::EncodeAssets(game::GameAssets{})) {
        throw std::runtime_error("Lua v1 nao suporta rigs/animacoes; salve o projeto JSON v2 para preservar os assets");
    }
    json document = json::parse(projectio::toJson(scene));
    if (document["version"].get<int>() > 1)
        throw std::runtime_error("Lua declarativo v1 nao suporta hierarquia; salve JSON v3");
    document["format"] = "loblox-scene";
    std::ostringstream out;
    out << "-- Loblox declarative scene v1. Literal data only; never executed.\nreturn ";
    Emit(document, out, 0);
    out << '\n';
    return out.str();
}

bool fromLua(Scene& scene, const std::string& text, std::string& error) {
    error.clear();
    if (text.size() > kMaxSceneBytes) { error = "arquivo Lua declarativo excede 16 MiB"; return false; }
    json document;
    LiteralParser parser(text);
    if (!parser.parse(document, error)) return false;
    if (!ValidateSchema(document, error)) return false;
    document["format"] = projectio::kFormatName;
    return projectio::fromJson(scene, document.dump(), error);
}

bool save(const Scene& scene, const std::string& path, std::string& error) {
    const auto started = std::chrono::steady_clock::now();
    const std::string correlation = diag::newCorrelationId();
    diag::Event begin;
    begin.subsystem = "persistence";
    begin.name = "lua_export_begin";
    begin.correlationId = correlation;
    begin.file = path;
    begin.context = {{"format", "lua"}};
    diag::log(begin);
    const auto finish = [&](bool ok) {
        diag::Event end;
        end.level = ok ? diag::Level::Info : diag::Level::Error;
        end.subsystem = "persistence";
        end.name = "lua_export_end";
        end.correlationId = correlation;
        end.file = path;
        end.durationMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started).count();
        end.result = ok ? "ok" : "error";
        end.error = ok ? std::string() : error;
        end.context = {{"format", "lua"}, {"parts", scene.parts().size()}};
        diag::log(end);
        return ok;
    };

    error.clear();
    if (path.empty()) {
        error = "caminho de exportacao vazio";
        return finish(false);
    }
    if (!scene.validate(error)) {
        error = "cena nao pode ser exportada: " + error;
        return finish(false);
    }
    std::string text;
    try {
        text = toLua(scene);
    } catch (const std::exception& exception) {
        error = "falha ao serializar cena Lua: " + std::string(exception.what());
        return finish(false);
    }
    return finish(projectio::writeFileAtomically(path, text, error));
}

bool load(Scene& scene, const std::string& path, std::string& error) {
    const auto started = std::chrono::steady_clock::now();
    const std::string correlation = diag::newCorrelationId();
    diag::Event begin;
    begin.subsystem = "persistence";
    begin.name = "lua_import_begin";
    begin.correlationId = correlation;
    begin.file = path;
    begin.context = {{"format", "lua"}};
    diag::log(begin);
    const auto finish = [&](bool ok) {
        diag::Event end;
        end.level = ok ? diag::Level::Info : diag::Level::Error;
        end.subsystem = "persistence";
        end.name = "lua_import_end";
        end.correlationId = correlation;
        end.file = path;
        end.durationMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started).count();
        end.result = ok ? "ok" : "error";
        end.error = ok ? std::string() : error;
        end.context = {{"format", "lua"}, {"parts", scene.parts().size()}};
        diag::log(end);
        return ok;
    };

    error.clear();
    std::error_code sizeError;
    const std::uintmax_t size = std::filesystem::file_size(path, sizeError);
    if (!sizeError && size > kMaxSceneBytes) {
        error = "arquivo Lua declarativo excede 16 MiB";
        return finish(false);
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "nao foi possivel abrir `" + path + "`";
        return finish(false);
    }
    std::ostringstream data;
    data << in.rdbuf();
    return finish(fromLua(scene, data.str(), error));
}

}  // namespace loblox::scenelua

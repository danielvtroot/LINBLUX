#include "serialization/ProjectIO.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "objects/Baseplate.h"
#include "objects/Lighting.h"
#include "objects/Part.h"
#include "objects/SpawnLocation.h"
#include "scene/Scene.h"

namespace loblox {
namespace projectio {

namespace {

using json = nlohmann::json;

struct ParsedObject {
    InstanceId id = kInvalidInstanceId;
    InstanceId parent = kInvalidInstanceId;
    std::string name;
    InstanceClass instanceClass = InstanceClass::Part;
    PartShape shape = PartShape::Block;
    Transform transform;
    Color color{255, 255, 255, 255};
    PartMaterial material = PartMaterial::SmoothPlastic;
    bool anchored = false;
    bool canCollide = true;
};

struct ParsedLighting {
    Color skyColor{120, 180, 240, 255};
    Color ambientColor{150, 150, 160, 255};
    Vector3 sunDirection{-0.45f, -1.0f, -0.35f};
    float sunIntensity = 1.0f;
    float brightness = 1.0f;
};

bool ReadFloat(const json& value, const char* field, float& out, std::string& error) {
    if (!value.is_number()) {
        error = std::string("campo '") + field + "' deve ser numerico";
        return false;
    }
    const double number = value.get<double>();
    if (!std::isfinite(number)) {
        error = std::string("campo '") + field + "' nao e finito";
        return false;
    }
    out = static_cast<float>(number);
    return true;
}

bool ReadVector3(const json& value, const char* field, Vector3& out, std::string& error) {
    if (!value.is_array() || value.size() != 3) {
        error = std::string("campo '") + field + "' deve ser [x, y, z]";
        return false;
    }
    return ReadFloat(value[0], field, out.x, error) &&
           ReadFloat(value[1], field, out.y, error) &&
           ReadFloat(value[2], field, out.z, error);
}

bool ReadColor(const json& value, const char* field, Color& out, std::string& error) {
    if (!value.is_array() || (value.size() != 3 && value.size() != 4)) {
        error = std::string("campo '") + field + "' deve ser [r, g, b] ou [r, g, b, a]";
        return false;
    }
    const int count = static_cast<int>(value.size());
    unsigned char channels[4] = {out.r, out.g, out.b, out.a};
    for (int i = 0; i < count; ++i) {
        if (!value[i].is_number_integer()) {
            error = std::string("campo '") + field + "' deve conter inteiros 0..255";
            return false;
        }
        const int channel = value[i].get<int>();
        if (channel < 0 || channel > 255) {
            error = std::string("campo '") + field + "' fora do intervalo 0..255";
            return false;
        }
        channels[i] = static_cast<unsigned char>(channel);
    }
    out = Color{channels[0], channels[1], channels[2], channels[3]};
    return true;
}

const json* FindField(const json& object, const char* key, std::string& error) {
    const auto it = object.find(key);
    if (it == object.end()) {
        error = std::string("campo obrigatorio ausente: '") + key + "'";
        return nullptr;
    }
    return &(*it);
}

bool ParseObject(const json& value, std::size_t index, ParsedObject& out,
                 ParsedLighting& lighting, bool& hasLighting, std::string& error) {
    const std::string prefix = "objeto[" + std::to_string(index) + "]: ";
    if (!value.is_object()) {
        error = prefix + "deve ser um objeto JSON";
        return false;
    }

    const json* idField = FindField(value, "id", error);
    if (idField == nullptr) {
        error = prefix + error;
        return false;
    }
    if (!idField->is_number_unsigned() || idField->get<InstanceId>() == kInvalidInstanceId) {
        error = prefix + "'id' deve ser inteiro positivo";
        return false;
    }
    out.id = idField->get<InstanceId>();

    const json* parentField = FindField(value, "parent", error);
    if (parentField == nullptr) {
        error = prefix + error;
        return false;
    }
    if (!parentField->is_number_unsigned() && !parentField->is_number_integer()) {
        error = prefix + "'parent' deve ser inteiro (0 = raiz)";
        return false;
    }
    const long long parentValue = parentField->get<long long>();
    if (parentValue < 0) {
        error = prefix + "'parent' invalido";
        return false;
    }
    out.parent = static_cast<InstanceId>(parentValue);

    const json* classField = FindField(value, "class", error);
    if (classField == nullptr || !classField->is_string()) {
        error = prefix + "campo 'class' deve ser string";
        return false;
    }
    if (!InstanceClassFromName(classField->get<std::string>(), out.instanceClass)) {
        error = prefix + "classe desconhecida '" + classField->get<std::string>() + "'";
        return false;
    }

    const json* nameField = FindField(value, "name", error);
    if (nameField == nullptr || !nameField->is_string()) {
        error = prefix + "campo 'name' deve ser string";
        return false;
    }
    out.name = nameField->get<std::string>();
    if (out.name.empty() || out.name.size() > 128) {
        error = prefix + "nome vazio ou longo demais (max 128)";
        return false;
    }

    if (out.instanceClass == InstanceClass::Workspace) {
        return true;
    }

    if (out.instanceClass == InstanceClass::Lighting) {
        if (hasLighting) {
            error = prefix + "mais de um Lighting no projeto";
            return false;
        }
        const json* sky = FindField(value, "skyColor", error);
        const json* ambient = FindField(value, "ambientColor", error);
        const json* sunDirection = FindField(value, "sunDirection", error);
        const json* intensity = FindField(value, "sunIntensity", error);
        const json* brightness = FindField(value, "brightness", error);
        if (sky == nullptr || ambient == nullptr || sunDirection == nullptr ||
            intensity == nullptr || brightness == nullptr) {
            error = prefix + error;
            return false;
        }
        if (!ReadColor(*sky, "skyColor", lighting.skyColor, error) ||
            !ReadColor(*ambient, "ambientColor", lighting.ambientColor, error) ||
            !ReadVector3(*sunDirection, "sunDirection", lighting.sunDirection, error) ||
            !ReadFloat(*intensity, "sunIntensity", lighting.sunIntensity, error) ||
            !ReadFloat(*brightness, "brightness", lighting.brightness, error)) {
            error = prefix + error;
            return false;
        }
        hasLighting = true;
        return true;
    }

    // Parts (Part, Baseplate, SpawnLocation).
    const json* shapeField = FindField(value, "shape", error);
    if (shapeField == nullptr || !shapeField->is_string() ||
        !PartShapeFromName(shapeField->get<std::string>(), out.shape)) {
        error = prefix + "'shape' invalido (Block ou Sphere)";
        return false;
    }
    const json* position = FindField(value, "position", error);
    const json* rotation = FindField(value, "rotation", error);
    const json* size = FindField(value, "size", error);
    if (position == nullptr || rotation == nullptr || size == nullptr) {
        error = prefix + error;
        return false;
    }
    Vector3 eulerDegrees{0.0f, 0.0f, 0.0f};
    if (!ReadVector3(*position, "position", out.transform.position, error) ||
        !ReadVector3(*rotation, "rotation", eulerDegrees, error) ||
        !ReadVector3(*size, "size", out.transform.size, error)) {
        error = prefix + error;
        return false;
    }
    if (std::fabs(eulerDegrees.x) > 100000.0f || std::fabs(eulerDegrees.y) > 100000.0f ||
        std::fabs(eulerDegrees.z) > 100000.0f) {
        error = prefix + "rotacao fora de faixa";
        return false;
    }
    out.transform.rotation = QuatFromEulerDegrees(eulerDegrees);

    if (out.transform.size.x < kMinPartSize || out.transform.size.y < kMinPartSize ||
        out.transform.size.z < kMinPartSize) {
        error = prefix + "tamanho abaixo do minimo (0.2)";
        return false;
    }

    const json* color = FindField(value, "color", error);
    const json* material = FindField(value, "material", error);
    const json* anchored = FindField(value, "anchored", error);
    const json* canCollide = FindField(value, "canCollide", error);
    if (color == nullptr || material == nullptr || anchored == nullptr || canCollide == nullptr) {
        error = prefix + error;
        return false;
    }
    if (!ReadColor(*color, "color", out.color, error)) {
        error = prefix + error;
        return false;
    }
    if (!material->is_string() ||
        !PartMaterialFromName(material->get<std::string>(), out.material)) {
        error = prefix + "'material' desconhecido";
        return false;
    }
    if (!anchored->is_boolean() || !canCollide->is_boolean()) {
        error = prefix + "'anchored' e 'canCollide' devem ser booleanos";
        return false;
    }
    out.anchored = anchored->get<bool>();
    out.canCollide = canCollide->get<bool>();
    return true;
}

}  // namespace

std::string toJson(const Scene& scene) {
    json root;
    root["format"] = kFormatName;
    root["version"] = kFormatVersion;
    root["nextId"] = scene.nextId();

    json objects = json::array();

    if (scene.workspace() != nullptr) {
        objects.push_back({{"id", scene.workspace()->id()},
                           {"class", "Workspace"},
                           {"name", scene.workspace()->name()},
                           {"parent", 0}});
    }
    if (scene.lighting() != nullptr) {
        const Lighting& lighting = *scene.lighting();
        const Vector3 sun = lighting.sunDirection();
        objects.push_back({{"id", lighting.id()},
                           {"class", "Lighting"},
                           {"name", lighting.name()},
                           {"parent", 0},
                           {"skyColor", {lighting.skyColor().r, lighting.skyColor().g,
                                         lighting.skyColor().b, lighting.skyColor().a}},
                           {"ambientColor",
                            {lighting.ambientColor().r, lighting.ambientColor().g,
                             lighting.ambientColor().b, lighting.ambientColor().a}},
                           {"sunDirection", {sun.x, sun.y, sun.z}},
                           {"sunIntensity", lighting.sunIntensity()},
                           {"brightness", lighting.brightness()}});
    }

    for (Part* part : scene.parts()) {
        const Vector3 position = part->position();
        const Vector3 euler = part->rotationEulerDegrees();
        const Vector3 size = part->size();
        const Color color = part->color();
        objects.push_back({{"id", part->id()},
                           {"class", part->className()},
                           {"name", part->name()},
                           {"parent", scene.workspace() != nullptr ? scene.workspace()->id() : 0},
                           {"shape", PartShapeName(part->shape())},
                           {"position", {position.x, position.y, position.z}},
                           {"rotation", {euler.x, euler.y, euler.z}},
                           {"size", {size.x, size.y, size.z}},
                           {"color", {color.r, color.g, color.b, color.a}},
                           {"material", PartMaterialName(part->material())},
                           {"anchored", part->anchored()},
                           {"canCollide", part->canCollide()}});
    }

    root["objects"] = std::move(objects);
    return root.dump(2) + "\n";
}

bool fromJson(Scene& scene, const std::string& text, std::string& error) {
    error.clear();
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error& exception) {
        error = std::string("JSON invalido: ") + exception.what();
        return false;
    }

    if (!root.is_object()) {
        error = "raiz do projeto deve ser um objeto JSON";
        return false;
    }
    if (root.value("format", std::string()) != kFormatName) {
        error = "formato desconhecido (esperado '" + std::string(kFormatName) + "')";
        return false;
    }
    if (!root.contains("version") || !root["version"].is_number_integer()) {
        error = "campo 'version' ausente ou invalido";
        return false;
    }
    const int version = root["version"].get<int>();
    if (version < 1 || version > kFormatVersion) {
        error = "versao nao suportada: " + std::to_string(version) +
                " (suportada ate " + std::to_string(kFormatVersion) + ")";
        return false;
    }
    if (!root.contains("objects") || !root["objects"].is_array()) {
        error = "campo 'objects' ausente ou invalido";
        return false;
    }

    std::vector<ParsedObject> parsed;
    ParsedLighting lighting;
    bool hasLighting = false;

    std::set<InstanceId> ids;
    InstanceId workspaceId = kInvalidInstanceId;
    int workspaceCount = 0;

    for (std::size_t i = 0; i < root["objects"].size(); ++i) {
        ParsedObject object;
        if (!ParseObject(root["objects"][i], i, object, lighting, hasLighting, error)) {
            return false;
        }
        if (!ids.insert(object.id).second) {
            error = "id duplicado no projeto: " + std::to_string(object.id);
            return false;
        }
        if (object.instanceClass == InstanceClass::Workspace) {
            ++workspaceCount;
            workspaceId = object.id;
            if (object.parent != 0) {
                error = "Workspace deve estar na raiz (parent = 0)";
                return false;
            }
        }
        parsed.push_back(object);
    }

    if (workspaceCount != 1) {
        error = "projeto deve conter exatamente um Workspace";
        return false;
    }
    if (!hasLighting) {
        error = "projeto deve conter um Lighting";
        return false;
    }

    for (const ParsedObject& object : parsed) {
        if (object.instanceClass == InstanceClass::Workspace) {
            continue;
        }
        if (object.instanceClass == InstanceClass::Lighting) {
            if (object.parent != 0) {
                error = "Lighting deve estar na raiz (parent = 0)";
                return false;
            }
            continue;
        }
        if (object.parent != workspaceId) {
            error = "objeto '" + object.name + "' deve pertencer ao Workspace";
            return false;
        }
        if (object.id == workspaceId) {
            error = "id reservado do Workspace reutilizado por '" + object.name + "'";
            return false;
        }
    }

    // --- Validacao concluida: agora reconstroi a cena. ---
    scene.clearObjects();
    scene.workspace()->setName(
        [&parsed]() {
            for (const ParsedObject& object : parsed) {
                if (object.instanceClass == InstanceClass::Workspace) {
                    return object.name;
                }
            }
            return std::string("Workspace");
        }());
    scene.lighting()->setName([&parsed]() {
        for (const ParsedObject& object : parsed) {
            if (object.instanceClass == InstanceClass::Lighting) {
                return object.name;
            }
        }
        return std::string("Lighting");
    }());
    scene.lighting()->setSkyColor(lighting.skyColor);
    scene.lighting()->setAmbientColor(lighting.ambientColor);
    scene.lighting()->setSunDirection(lighting.sunDirection);
    scene.lighting()->setSunIntensity(lighting.sunIntensity);
    scene.lighting()->setBrightness(lighting.brightness);

    for (const ParsedObject& object : parsed) {
        if (object.instanceClass == InstanceClass::Workspace ||
            object.instanceClass == InstanceClass::Lighting) {
            continue;
        }
        if (scene.find(object.id) != nullptr) {
            error = "id conflita com um servico da cena: " + std::to_string(object.id);
            return false;
        }

        Part* part = nullptr;
        if (object.instanceClass == InstanceClass::Baseplate) {
            part = scene.createBaseplate(object.transform.position, object.transform.size,
                                         object.name, object.id);
        } else if (object.instanceClass == InstanceClass::SpawnLocation) {
            part = scene.createSpawnLocation(object.transform.position, object.transform.size,
                                             object.name, object.id);
        } else {
            part = scene.createPart(object.shape, object.transform.position,
                                    object.transform.size, object.name, object.id);
        }
        if (part == nullptr) {
            error = "falha ao recriar objeto '" + object.name + "' (id " +
                    std::to_string(object.id) + ")";
            return false;
        }
        part->setRotation(object.transform.rotation);
        part->setColor(object.color);
        part->setMaterial(object.material);
        // A Baseplate e estrutural: permanece ancorada mesmo se o arquivo disser o contrario.
        part->setAnchored(object.instanceClass == InstanceClass::Baseplate ? true
                                                                          : object.anchored);
        part->setCanCollide(object.canCollide);
        part->clearDirty();
    }

    if (root.contains("nextId")) {
        if (!root["nextId"].is_number_unsigned() && !root["nextId"].is_number_integer()) {
            error = "campo 'nextId' invalido";
            return false;
        }
        const long long next = root["nextId"].get<long long>();
        if (next > 0) {
            scene.setNextId(static_cast<InstanceId>(next));
        }
    }
    scene.select(nullptr);
    return true;
}

bool save(const Scene& scene, const std::string& path, std::string& error) {
    error.clear();
    const std::filesystem::path target(path);
    if (target.empty()) {
        error = "caminho de salvamento vazio";
        return false;
    }

    std::error_code directoryError;
    const std::filesystem::path parent = target.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, directoryError);
        if (directoryError) {
            error = "nao foi possivel criar '" + parent.string() + "': " +
                    directoryError.message();
            return false;
        }
    }

    const std::string text = toJson(scene);
    const std::filesystem::path temporary = target.string() + ".tmp";

    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "nao foi possivel abrir '" + temporary.string() + "' para escrita";
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) {
            error = "falha ao escrever '" + temporary.string() + "'";
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporary, target, renameError);
    if (renameError) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "falha ao substituir '" + target.string() + "': " + renameError.message();
        return false;
    }
    return true;
}

bool load(Scene& scene, const std::string& path, std::string& error) {
    error.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "nao foi possivel abrir '" + path + "'";
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) {
        error = "falha ao ler '" + path + "'";
        return false;
    }
    return fromJson(scene, buffer.str(), error);
}

}  // namespace projectio
}  // namespace loblox

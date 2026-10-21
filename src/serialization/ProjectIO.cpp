#include "serialization/ProjectIO.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Diagnostics.h"
#include "objects/Baseplate.h"
#include "objects/Lighting.h"
#include "objects/Part.h"
#include "objects/SpawnLocation.h"
#include "scene/Scene.h"
#include "game/GameAssetsIO.h"

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
    std::string source;
    bool enabled = true;
};

struct ParsedLighting {
    Color skyColor{120, 180, 240, 255};
    Color ambientColor{150, 150, 160, 255};
    Vector3 sunDirection{-0.45f, -1.0f, -0.35f};
    float sunIntensity = 1.0f;
    float brightness = 1.0f;
    Color fogColor{120, 180, 240, 255};
    float fogDensity = 0.0f;
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
    if (!std::isfinite(out)) {
        error = std::string("campo '") + field + "' fora da faixa float";
        return false;
    }
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
        unsigned int channel = 0;
        if (value[i].is_number_unsigned()) {
            const auto raw = value[i].get<unsigned long long>();
            if (raw > 255) {
                error = std::string("campo '") + field + "' fora do intervalo 0..255";
                return false;
            }
            channel = static_cast<unsigned int>(raw);
        } else {
            const long long raw = value[i].get<long long>();
            if (raw < 0 || raw > 255) {
                error = std::string("campo '") + field + "' fora do intervalo 0..255";
                return false;
            }
            channel = static_cast<unsigned int>(raw);
        }
        if (channel > 255) {
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

    if (IsScriptClass(out.instanceClass)) {
        if (!value.contains("source") || !value["source"].is_string() ||
            !value.contains("enabled") || !value["enabled"].is_boolean()) {
            error=prefix+"Script requires source string and enabled boolean"; return false;
        }
        out.source=value["source"].get<std::string>(); out.enabled=value["enabled"].get<bool>();
        if (out.source.size()>262144) { error=prefix+"Script source exceeds 256 KiB"; return false; }
        return true;
    }
    if (out.instanceClass == InstanceClass::Workspace ||
        (IsRootServiceClass(out.instanceClass) && out.instanceClass!=InstanceClass::Lighting) || out.instanceClass == InstanceClass::Folder ||
        out.instanceClass == InstanceClass::Model) {
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
        // Campos novos são opcionais: projetos v1/v2 carregam sem fog.
        std::string ignored;
        if (const json* fog = FindField(value, "fogColor", ignored); fog != nullptr)
            if (!ReadColor(*fog, "fogColor", lighting.fogColor, error)) {
                error = prefix + error;
                return false;
            }
        if (const json* fogDensity = FindField(value, "fogDensity", ignored); fogDensity != nullptr)
            if (!ReadFloat(*fogDensity, "fogDensity", lighting.fogDensity, error)) {
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
    // A plain legacy scene stays v1; gameplay assets opt into the v2 schema.
    const auto gameData = game::EncodeAssets(scene.gameAssets());
    const bool hasGame = gameData != game::EncodeAssets(game::GameAssets{});
    const bool hierarchy = scene.workspace()->fallenPartsDestroyHeight()!=-500 || std::any_of(scene.objects().begin(),scene.objects().end(),[](const auto& o) {
        return o->instanceClass() == InstanceClass::Folder || o->instanceClass() == InstanceClass::Model ||
            !o->attributes().empty() ||
            IsScriptClass(o->instanceClass()) || (IsRootServiceClass(o->instanceClass()) &&
                o->instanceClass()!=InstanceClass::Workspace && o->instanceClass()!=InstanceClass::Lighting);
    });
    root["version"] = hierarchy ? 3 : hasGame ? 2 : 1;
    if (hasGame) root["game"] = gameData;
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
                           {"brightness", lighting.brightness()},
                           {"fogColor", {lighting.fogColor().r, lighting.fogColor().g,
                                         lighting.fogColor().b, lighting.fogColor().a}},
                           {"fogDensity", lighting.fogDensity()}});
    }

    std::function<void(const Instance*)> append = [&](const Instance* node) {
        if (!node->isPart()) {
            objects.push_back({{"id",node->id()},{"class",node->className()},
                {"name",node->name()},{"parent",node->parent()?node->parent()->id():0}});
            if (IsScriptClass(node->instanceClass())) {
                const auto& script=static_cast<const Script&>(*node);
                objects.back()["source"]=script.source; objects.back()["enabled"]=script.enabled;
            }
            for (const auto* child : node->children()) append(child);
            return;
        }
        const Part* part = static_cast<const Part*>(node);
        const Vector3 position = part->position();
        const Vector3 euler = part->rotationEulerDegrees();
        const Vector3 size = part->size();
        const Color color = part->color();
        objects.push_back({{"id", part->id()},
                           {"class", part->className()},
                           {"name", part->name()},
                           {"parent", part->parent()->id()},
                           {"shape", PartShapeName(part->shape())},
                           {"position", {position.x, position.y, position.z}},
                           {"rotation", {euler.x, euler.y, euler.z}},
                           {"size", {size.x, size.y, size.z}},
                           {"color", {color.r, color.g, color.b, color.a}},
                           {"material", PartMaterialName(part->material())},
                           {"anchored", part->anchored()},
                           {"canCollide", part->canCollide()}});
        for (const auto* child : node->children()) append(child);
    };
    for (const auto* child : scene.workspace()->children()) append(child);
    for (const auto& object:scene.objects())
        if (!object->parent() && object.get()!=scene.workspace() && object.get()!=scene.lighting()) append(object.get());

    for (auto& entry:objects) {
        const auto* node=scene.find(entry["id"].get<InstanceId>());
        if (node && !node->attributes().empty()) {
            auto& attrs=entry["attributes"]; attrs=json::object();
            for (const auto& pair:node->attributes()) std::visit([&](const auto& value){attrs[pair.first]=value;},pair.second);
        }
        if (node==scene.workspace() && scene.workspace()->fallenPartsDestroyHeight()!=-500)
            entry["fallenPartsDestroyHeight"]=scene.workspace()->fallenPartsDestroyHeight();
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

    try {
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
    const double versionNumber = root["version"].get<double>();
    if (versionNumber < 1 || versionNumber > kFormatVersion) {
        error = "versao nao suportada: " + root["version"].dump() +
                " (suportada ate " + std::to_string(kFormatVersion) + ")";
        return false;
    }
    const int version = root["version"].get<int>();
    if (!root.contains("objects") || !root["objects"].is_array()) {
        error = "campo 'objects' ausente ou invalido";
        return false;
    }

    std::vector<ParsedObject> parsed;
    ParsedLighting lighting;
    bool hasLighting = false;

    std::set<InstanceId> ids;
    InstanceId workspaceId = kInvalidInstanceId;
    InstanceId lightingId = kInvalidInstanceId;
    int workspaceCount = 0;

    for (std::size_t i = 0; i < root["objects"].size(); ++i) {
        ParsedObject object;
        if (!ParseObject(root["objects"][i], i, object, lighting, hasLighting, error)) {
            return false;
        }
        if (version < 3 && (object.instanceClass == InstanceClass::Folder || object.instanceClass == InstanceClass::Model ||
            IsScriptClass(object.instanceClass) || (IsRootServiceClass(object.instanceClass) &&
                object.instanceClass!=InstanceClass::Workspace && object.instanceClass!=InstanceClass::Lighting))) {
            error = "hierarquia requer projeto version 3"; return false;
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
        if(object.instanceClass==InstanceClass::Lighting)lightingId=object.id;
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
        if (IsRootServiceClass(object.instanceClass)) {
            if (object.parent != 0) {
                error = "servico deve estar na raiz (parent = 0)";
                return false;
            }
            continue;
        }
        if ((version < 3 && object.parent != workspaceId) || object.parent == 0 || !ids.count(object.parent)) {
            error = "objeto '" + object.name + "' deve pertencer ao Workspace";
            return false;
        }
        if (object.id == workspaceId) {
            error = "id reservado do Workspace reutilizado por '" + object.name + "'";
            return false;
        }
    }

    // Ids de servico da cena atual sao reservados: rejeitar antes de limpar
    // qualquer objeto, para nao deixar a cena parcialmente reconstruida.
    const InstanceId serviceIds[2] = {scene.workspace() != nullptr ? scene.workspace()->id() : 0,
                                      scene.lighting() != nullptr ? scene.lighting()->id() : 0};
    for (const ParsedObject& object : parsed) {
        if(version>=3)break; // incoming root IDs are preserved in v3
        if (object.instanceClass == InstanceClass::Workspace ||
            object.instanceClass == InstanceClass::Lighting) {
            continue;
        }
        if (object.id == serviceIds[0] || object.id == serviceIds[1]) {
            error = "id " + std::to_string(object.id) +
                    " reservado por um servico da cena atual (objeto '" + object.name + "')";
            return false;
        }
    }

    InstanceId requestedNextId = 0;
    if (root.contains("nextId")) {
        const json& nextField = root["nextId"];
        if (!nextField.is_number_unsigned() && !nextField.is_number_integer()) {
            error = "campo 'nextId' invalido";
            return false;
        }
        if (nextField.is_number_unsigned()) {
            const auto value = nextField.get<unsigned long long>();
            if (value == 0 || value >= static_cast<unsigned long long>(kInvalidInstanceId - 1)) {
                error = "campo 'nextId' fora da faixa suportada";
                return false;
            }
            requestedNextId = static_cast<InstanceId>(value);
        } else {
            const long long value = nextField.get<long long>();
            if (value <= 0 || static_cast<unsigned long long>(value) >=
                                  static_cast<unsigned long long>(kInvalidInstanceId - 1)) {
                error = "campo 'nextId' fora da faixa suportada";
                return false;
            }
            requestedNextId = static_cast<InstanceId>(value);
        }
    }

    // --- Validacao concluida: constroi uma cena candidata isolada. ---
    // A cena atual permanece intocada ate que todos os objetos tenham sido
    // criados com sucesso. A troca final e O(1) e nao pode falhar.
    Scene staged(version>=3?workspaceId:1,version>=3?lightingId:2);
    if (version >= 2 && root.contains("game")) {
        if (!root.contains("game") || !game::DecodeAssets(root["game"], staged.gameAssets(), error)) {
            if (error.empty()) error = "version 2 requires game assets";
            return false;
        }
    } else if (version == 2 || root.contains("game")) {
        error = "game assets require project version 2";
        return false;
    }
    staged.workspace()->setName(
        [&parsed]() {
            for (const ParsedObject& object : parsed) {
                if (object.instanceClass == InstanceClass::Workspace) {
                    return object.name;
                }
            }
            return std::string("Workspace");
        }());
    staged.lighting()->setName([&parsed]() {
        for (const ParsedObject& object : parsed) {
            if (object.instanceClass == InstanceClass::Lighting) {
                return object.name;
            }
        }
        return std::string("Lighting");
    }());
    staged.lighting()->setSkyColor(lighting.skyColor);
    staged.lighting()->setAmbientColor(lighting.ambientColor);
    staged.lighting()->setSunDirection(lighting.sunDirection);
    staged.lighting()->setSunIntensity(lighting.sunIntensity);
    staged.lighting()->setBrightness(lighting.brightness);
    for (const auto& value:root["objects"]) if (value["class"]=="Workspace" && value.contains("fallenPartsDestroyHeight")) {
        float height;
        if (version<3 || !ReadFloat(value["fallenPartsDestroyHeight"],"fallenPartsDestroyHeight",height,error) ||
            !staged.workspace()->setFallenPartsDestroyHeight(height)) {error="invalid v3 FallenPartsDestroyHeight";return false;}
    }

    for (const auto& object:parsed) {
        if (!IsRootServiceClass(object.instanceClass) || object.instanceClass==InstanceClass::Workspace || object.instanceClass==InstanceClass::Lighting) continue;
        if (staged.service(object.instanceClass) || !staged.ensureService(object.instanceClass,object.id)) {
            error="servico duplicado ou id invalido: "+object.name; return false;
        }
        staged.service(object.instanceClass)->setName(object.name);
    }

    for (const ParsedObject& object : parsed) {
        if (IsRootServiceClass(object.instanceClass)) {
            continue;
        }
        if (IsScriptClass(object.instanceClass)) {
            auto* script=staged.createScript(object.instanceClass,staged.workspace(),object.name,object.id);
            if (!script) { error="falha ao criar Script "+object.name; return false; }
            script->source=object.source; script->enabled=object.enabled; continue;
        }
        if (object.instanceClass == InstanceClass::Folder || object.instanceClass == InstanceClass::Model) {
            if (!staged.createContainer(object.instanceClass,nullptr,object.name,object.id)) {
                error = "falha ao criar container " + object.name; return false;
            }
            continue;
        }
        Part* part = nullptr;
        if (object.instanceClass == InstanceClass::Baseplate) {
            part = staged.createBaseplate(object.transform.position, object.transform.size,
                                          object.name, object.id);
        } else if (object.instanceClass == InstanceClass::SpawnLocation) {
            part = staged.createSpawnLocation(object.transform.position, object.transform.size,
                                              object.name, object.id);
        } else {
            part = staged.createPart(object.shape, object.transform.position,
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

    for (const auto& object : parsed) {
        if (IsRootServiceClass(object.instanceClass)) continue;
        auto* parent = object.parent == workspaceId ? staged.workspace() : staged.find(object.parent);
        if (!staged.reparent(staged.find(object.id),parent,error)) {
            error = "objeto '"+object.name+"': "+error; return false;
        }
    }
    if (requestedNextId != 0) staged.setNextId(requestedNextId);
    for (const auto& value:root["objects"]) if (value.contains("attributes")) {
        const auto& attrs=value["attributes"];
        if (version<3 || !attrs.is_object() || attrs.size()>64) { error="attributes requires v3 object, max 64"; return false; }
        const auto cls=value["class"].get<std::string>();
        auto* node=cls=="Workspace"?static_cast<Instance*>(staged.workspace()):cls=="Lighting"?static_cast<Instance*>(staged.lighting()):staged.find(value["id"].get<InstanceId>());
        for (auto it=attrs.begin();it!=attrs.end();++it) {
            bool ok=false;
            if (it.value().is_boolean()) ok=node->setAttribute(it.key(),it.value().get<bool>());
            else if (it.value().is_number()) ok=node->setAttribute(it.key(),it.value().get<double>());
            else if (it.value().is_string()) ok=node->setAttribute(it.key(),it.value().get<std::string>());
            if (!ok) { error="invalid attribute: "+it.key(); return false; }
        }
    }
    staged.select(nullptr);
    if (!staged.validate(error)) {
        error = "cena candidata viola invariantes: " + error;
        return false;
    }
    scene.replaceContentsFrom(staged);
    return true;
    } catch (const std::exception& exception) {
        error = std::string("projeto invalido: ") + exception.what();
        return false;
    }
}

bool writeFileAtomically(const std::string& path, const std::string& content,
                         std::string& error) {
    error.clear();
    const std::filesystem::path target(path);
    if (target.empty() || target.filename().empty()) {
        error = "caminho de salvamento vazio";
        return false;
    }

    const std::filesystem::path parentDirectory = target.parent_path().empty()
                                                     ? std::filesystem::path(".")
                                                     : target.parent_path();
    std::error_code directoryError;
    std::filesystem::create_directories(parentDirectory, directoryError);
    if (directoryError) {
        error = "nao foi possivel criar '" + parentDirectory.string() + "': " +
                directoryError.message();
        return false;
    }

    std::string temporaryTemplate =
        (parentDirectory / (target.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> temporaryName(temporaryTemplate.begin(), temporaryTemplate.end());
    temporaryName.push_back('\0');
    const int descriptor = ::mkstemp(temporaryName.data());
    if (descriptor < 0) {
        error = "nao foi possivel criar arquivo temporario ao lado de '" + target.string() +
                "': " + std::strerror(errno);
        return false;
    }
    const std::filesystem::path temporary(temporaryName.data());

    struct stat previousMetadata {};
    if (::stat(target.c_str(), &previousMetadata) == 0 && S_ISREG(previousMetadata.st_mode)) {
        (void)::fchmod(descriptor, previousMetadata.st_mode & 0777);
    }

    std::size_t written = 0;
    while (written < content.size()) {
        const ssize_t result =
            ::write(descriptor, content.data() + written, content.size() - written);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) {
            const int savedErrno = errno;
            ::close(descriptor);
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            error = "falha ao escrever arquivo temporario para '" + target.string() + "': " +
                    std::strerror(savedErrno);
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    if (::fsync(descriptor) != 0) {
        const int savedErrno = errno;
        ::close(descriptor);
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "falha ao sincronizar arquivo temporario para '" + target.string() + "': " +
                std::strerror(savedErrno);
        return false;
    }
    if (::close(descriptor) != 0) {
        const int savedErrno = errno;
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "falha ao fechar arquivo temporario para '" + target.string() + "': " +
                std::strerror(savedErrno);
        return false;
    }

    if (::rename(temporary.c_str(), target.c_str()) != 0) {
        const int savedErrno = errno;
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "falha ao substituir '" + target.string() + "': " + std::strerror(savedErrno);
        return false;
    }

    // fsync do diretorio garante que o rename tambem sobreviva a queda. Alguns
    // filesystems nao implementam fsync de diretorio; nesse caso EINVAL e
    // ENOTSUP nao indicam perda de dados.
    const int directoryFd =
        ::open(parentDirectory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directoryFd < 0) {
        error = "arquivo gravado, mas nao foi possivel abrir o diretorio '" +
                parentDirectory.string() + "': " + std::strerror(errno);
        return false;
    }
    const int syncResult = ::fsync(directoryFd);
    const int syncErrno = errno;
    ::close(directoryFd);
    if (syncResult != 0 && syncErrno != EINVAL && syncErrno != ENOTSUP) {
        error = "arquivo gravado, mas falhou a sincronizacao do diretorio '" +
                parentDirectory.string() + "': " + std::strerror(syncErrno);
        return false;
    }
    return true;
}

namespace {

bool SaveImpl(const Scene& scene, const std::string& path, std::string& error) {
    error.clear();
    if (path.empty()) {
        error = "caminho de salvamento vazio";
        return false;
    }
    if (!scene.validate(error)) {
        error = "cena nao pode ser salva: " + error;
        return false;
    }

    std::string text;
    try {
        text = toJson(scene);
        // Validate the produced document before creating/replacing any file.
        Scene validation;
        if (!fromJson(validation, text, error)) {
            error = "cena nao pode ser salva: " + error;
            return false;
        }
    } catch (const std::exception& exception) {
        error = "falha ao serializar a cena: " + std::string(exception.what());
        return false;
    }

    return writeFileAtomically(path, text, error);
}

double ElapsedMs(std::chrono::steady_clock::time_point started) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - started).count();
}

bool LoadImpl(Scene& scene, const std::string& path, std::string& error) {
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

}  // namespace

bool save(const Scene& scene, const std::string& path, std::string& error) {
    const auto started = std::chrono::steady_clock::now();
    const std::string correlation = diag::newCorrelationId();
    diag::Event begin;
    begin.subsystem = "persistence";
    begin.name = "save_begin";
    begin.correlationId = correlation;
    begin.file = path;
    begin.context = {{"format", "json"}};
    diag::log(begin);

    const bool ok = SaveImpl(scene, path, error);
    diag::Event end;
    end.level = ok ? diag::Level::Info : diag::Level::Error;
    end.subsystem = "persistence";
    end.name = "save_end";
    end.correlationId = correlation;
    end.file = path;
    end.durationMs = ElapsedMs(started);
    end.result = ok ? "ok" : "error";
    end.error = ok ? std::string() : error;
    end.context = {{"format", "json"}, {"parts", scene.parts().size()}};
    diag::log(end);
    return ok;
}

bool load(Scene& scene, const std::string& path, std::string& error) {
    const auto started = std::chrono::steady_clock::now();
    const std::string correlation = diag::newCorrelationId();
    diag::Event begin;
    begin.subsystem = "persistence";
    begin.name = "load_begin";
    begin.correlationId = correlation;
    begin.file = path;
    begin.context = {{"format", "json"}};
    diag::log(begin);

    const bool ok = LoadImpl(scene, path, error);
    diag::Event end;
    end.level = ok ? diag::Level::Info : diag::Level::Error;
    end.subsystem = "persistence";
    end.name = "load_end";
    end.correlationId = correlation;
    end.file = path;
    end.durationMs = ElapsedMs(started);
    end.result = ok ? "ok" : "error";
    end.error = ok ? std::string() : error;
    end.context = {{"format", "json"},
                   {"parts", scene.parts().size()},
                   {"objects", scene.objects().size()}};
    diag::log(end);
    return ok;
}

}  // namespace projectio
}  // namespace loblox

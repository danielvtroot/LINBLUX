#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <map>
#include <variant>
#include <vector>
#include <cmath>
#include "raylib.h"

namespace loblox {

using InstanceId = std::uint64_t;
constexpr InstanceId kInvalidInstanceId = 0;

enum class InstanceClass {
    Workspace,
    Lighting,
    Part,
    Baseplate,
    SpawnLocation,
    Folder,
    Model,
    Script,
    LocalScript,
    ModuleScript,
    ServerScriptService,
    ServerStorage,
    ReplicatedStorage,
    Decal,
    Texture,
    Attachment,
};


const char* InstanceClassName(InstanceClass instanceClass);
bool InstanceClassFromName(const std::string& name, InstanceClass& out);
bool IsScriptClass(InstanceClass type);
bool IsRootServiceClass(InstanceClass type);
inline bool IsSurfaceClass(InstanceClass type) { return type==InstanceClass::Decal || type==InstanceClass::Texture; }

// New Part labels may be compacted at creation; manual renames are validated.
bool ValidPartName(const std::string& name);
std::string NewPartName(const std::string& name);
class Scene;

// No da hierarquia da cena. Nao possui os filhos (a Scene possui todos os
// objetos); guarda apenas os vinculos usados pela arvore do Explorer.
class Instance {
public:
    using Attribute = std::variant<bool,double,std::string>;
    virtual ~Instance() = default;

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    InstanceId id() const { return id_; }

    const std::string& name() const { return name_; }
    void setName(const std::string& name);
    bool trySetName(const std::string& name,std::string& error);
    bool setAttribute(const std::string& name, Attribute value);
    const std::map<std::string,Attribute>& attributes() const { return attributes_; }
    void removeAttribute(const std::string& name) { attributes_.erase(name); }

    virtual const char* className() const = 0;
    virtual InstanceClass instanceClass() const = 0;
    virtual bool isPart() const { return false; }

    Instance* parent() const { return parent_; }
    const std::vector<Instance*>& children() const { return children_; }
    bool isAncestorOf(const Instance* other) const;
    bool hasChildren() const { return !children_.empty(); }

    // Servicos (Workspace, Lighting) e a Baseplate padrao nao podem ser apagados.
    bool deletable() const { return deletable_; }
    void setDeletable(bool deletable) { deletable_ = deletable; }

protected:
    Instance(InstanceId id, std::string name);

    friend class Scene;
    void attachChild(Instance* child);
    void detachChild(Instance* child);

    InstanceId id_;
    std::string name_;
    Instance* parent_ = nullptr;
    std::vector<Instance*> children_;
    bool deletable_ = true;
    std::map<std::string,Attribute> attributes_;
};

struct ModelFrame {
    Vector3 position{};
    Quaternion rotation{0,0,0,1};
    float scale=1;
    bool valid() const;
};
class Attachment final : public Instance {
public:
    Attachment(InstanceId id,std::string name):Instance(id,std::move(name)){}
    const char* className() const override { return "Attachment"; }
    InstanceClass instanceClass() const override { return InstanceClass::Attachment; }
    const ModelFrame& localFrame() const { return local_; }
    InstanceId jointTarget() const { return jointTarget_; }
    bool setLocalFrame(const ModelFrame& frame) {
        if(!frame.valid()||frame.scale!=1)return false;
        local_=frame;
        return true;
    }
private:
    friend class Scene;
    ModelFrame local_;
    InstanceId jointTarget_=0;
};
// Groups have no physics body. Scene owns propagation of a Model's frame.
class Container final : public Instance {
public:
    Container(InstanceId id, std::string name, InstanceClass type)
        : Instance(id, std::move(name)), type_(type) {}
    const char* className() const override { return InstanceClassName(type_); }
    InstanceClass instanceClass() const override { return type_; }
    bool hasModelFrame() const { return hasModelFrame_; }
    const ModelFrame& modelFrame() const { return modelFrame_; }
private:
    friend class Scene;
    InstanceClass type_;
    bool hasModelFrame_=false;
    ModelFrame modelFrame_;
};

class Script final : public Instance {
public:
    Script(InstanceId id, std::string name, InstanceClass type)
        : Instance(id,std::move(name)), type_(type) {}
    const char* className() const override { return InstanceClassName(type_); }
    InstanceClass instanceClass() const override { return type_; }
    std::string source = "print(\"Olá do LINBLUX\")\n";
    bool enabled = true;
private:
    InstanceClass type_;
};

// A surface is an independently selectable/deletable child of a block Part.
// Front=-Z, Back=+Z, Left=-X, Right=+X, Top=+Y, Bottom=-Y.
struct SurfaceProperties {
    std::string image;
    int face=0;
    float transparency=0, rotation=0;
    Color tint{255,255,255,255};
    float studsPerTileU=4, studsPerTileV=4, offsetU=0, offsetV=0;
    bool valid() const {
        return image.size()<=512 && face>=0 && face<6 &&
            std::isfinite(transparency) && transparency>=0 && transparency<=1 &&
            std::isfinite(rotation) && std::fabs(rotation)<=36000 &&
            std::isfinite(studsPerTileU) && studsPerTileU>=.05f && studsPerTileU<=10000 &&
            std::isfinite(studsPerTileV) && studsPerTileV>=.05f && studsPerTileV<=10000 &&
            std::isfinite(offsetU) && std::fabs(offsetU)<=10000 && std::isfinite(offsetV) && std::fabs(offsetV)<=10000;
    }
};
class Surface final : public Instance {
public:
    Surface(InstanceId id,std::string name,InstanceClass type):Instance(id,std::move(name)),type_(type){}
    const char* className() const override { return InstanceClassName(type_); }
    InstanceClass instanceClass() const override { return type_; }
    const SurfaceProperties& properties() const { return properties_; }
    bool setProperties(const SurfaceProperties& value) { if(!value.valid())return false;properties_=value;return true; }
private:
    InstanceClass type_;
    SurfaceProperties properties_;
};

}  // namespace loblox

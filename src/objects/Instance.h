#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <map>
#include <variant>
#include <vector>

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
};


const char* InstanceClassName(InstanceClass instanceClass);
bool InstanceClassFromName(const std::string& name, InstanceClass& out);
bool IsScriptClass(InstanceClass type);
bool IsRootServiceClass(InstanceClass type);

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

// Organizational groups: no independent transform or physics body.
class Container final : public Instance {
public:
    Container(InstanceId id, std::string name, InstanceClass type)
        : Instance(id, std::move(name)), type_(type) {}
    const char* className() const override { return InstanceClassName(type_); }
    InstanceClass instanceClass() const override { return type_; }
private:
    InstanceClass type_;
};

class Script final : public Instance {
public:
    Script(InstanceId id, std::string name, InstanceClass type)
        : Instance(id,std::move(name)), type_(type) {}
    const char* className() const override { return InstanceClassName(type_); }
    InstanceClass instanceClass() const override { return type_; }
    std::string source = "print(\"Hello from Loblox\")\n";
    bool enabled = true;
private:
    InstanceClass type_;
};

}  // namespace loblox

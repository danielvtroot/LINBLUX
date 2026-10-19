#pragma once

#include <cstdint>
#include <string>
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
};

const char* InstanceClassName(InstanceClass instanceClass);
bool InstanceClassFromName(const std::string& name, InstanceClass& out);

class Scene;

// No da hierarquia da cena. Nao possui os filhos (a Scene possui todos os
// objetos); guarda apenas os vinculos usados pela arvore do Explorer.
class Instance {
public:
    virtual ~Instance() = default;

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    InstanceId id() const { return id_; }

    const std::string& name() const { return name_; }
    void setName(const std::string& name);

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
};

}  // namespace loblox

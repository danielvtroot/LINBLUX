#include "objects/Instance.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace loblox {

const char* InstanceClassName(InstanceClass instanceClass) {
    switch (instanceClass) {
        case InstanceClass::Workspace:
            return "Workspace";
        case InstanceClass::Lighting:
            return "Lighting";
        case InstanceClass::Part:
            return "Part";
        case InstanceClass::Baseplate:
            return "Baseplate";
        case InstanceClass::SpawnLocation:
            return "SpawnLocation";
        case InstanceClass::Folder: return "Folder";
        case InstanceClass::Model: return "Model";
        case InstanceClass::Script: return "Script";
        case InstanceClass::LocalScript: return "LocalScript";
        case InstanceClass::ModuleScript: return "ModuleScript";
        case InstanceClass::ServerScriptService: return "ServerScriptService";
        case InstanceClass::ServerStorage: return "ServerStorage";
        case InstanceClass::ReplicatedStorage: return "ReplicatedStorage";
    }
    return "Instance";
}

bool InstanceClassFromName(const std::string& name, InstanceClass& out) {
    for (auto type : {InstanceClass::Script,InstanceClass::LocalScript,InstanceClass::ModuleScript,
        InstanceClass::ServerScriptService,InstanceClass::ServerStorage,InstanceClass::ReplicatedStorage})
        if (name == InstanceClassName(type)) { out=type; return true; }
    if (name == "Folder") { out = InstanceClass::Folder; return true; }
    if (name == "Model") { out = InstanceClass::Model; return true; }
    if (name == "Workspace") {
        out = InstanceClass::Workspace;
        return true;
    }
    if (name == "Lighting") {
        out = InstanceClass::Lighting;
        return true;
    }
    if (name == "Part") {
        out = InstanceClass::Part;
        return true;
    }
    if (name == "Baseplate") {
        out = InstanceClass::Baseplate;
        return true;
    }
    if (name == "SpawnLocation") {
        out = InstanceClass::SpawnLocation;
        return true;
    }
    return false;
}

Instance::Instance(InstanceId id, std::string name)
    : id_(id), name_(std::move(name)) {
    if (name_.empty()) {
        name_ = "Instance";
    }
}

bool IsScriptClass(InstanceClass type) {
    return type==InstanceClass::Script || type==InstanceClass::LocalScript || type==InstanceClass::ModuleScript;
}
bool IsRootServiceClass(InstanceClass type) {
    return type==InstanceClass::Workspace || type==InstanceClass::Lighting ||
        type==InstanceClass::ServerScriptService || type==InstanceClass::ServerStorage || type==InstanceClass::ReplicatedStorage;
}

void Instance::setName(const std::string& name) {
    if (!name.empty() && name.size()<=128) {
        name_ = name;
    }
}
bool Instance::setAttribute(const std::string& name, Attribute value) {
    if (name.empty() || name.size()>64 || (!attributes_.count(name) && attributes_.size()>=64)) return false;
    if (const auto* n=std::get_if<double>(&value); n && !std::isfinite(*n)) return false;
    if (const auto* s=std::get_if<std::string>(&value); s && s->size()>4096) return false;
    attributes_[name]=std::move(value); return true;
}

bool Instance::isAncestorOf(const Instance* other) const {
    for (const Instance* node = other; node != nullptr; node = node->parent_) {
        if (node == this) {
            return true;
        }
    }
    return false;
}

void Instance::attachChild(Instance* child) {
    if (child == nullptr || child->parent_ == this) {
        return;
    }
    if (child->parent_ != nullptr) {
        child->parent_->detachChild(child);
    }
    child->parent_ = this;
    children_.push_back(child);
}

void Instance::detachChild(Instance* child) {
    const auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        children_.erase(it);
    }
    if (child != nullptr && child->parent_ == this) {
        child->parent_ = nullptr;
    }
}

}  // namespace loblox

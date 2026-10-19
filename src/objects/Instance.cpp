#include "objects/Instance.h"

#include <algorithm>
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
    }
    return "Instance";
}

bool InstanceClassFromName(const std::string& name, InstanceClass& out) {
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

void Instance::setName(const std::string& name) {
    if (!name.empty()) {
        name_ = name;
    }
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

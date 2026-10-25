#pragma once

#include <string>
#include <cmath>

#include "objects/Instance.h"

namespace loblox {

// Container principal da cena; todas as Parts construidas pelo usuario ficam
// aqui. A hierarquia real vive na Instance, nao existe arvore paralela de UI.
class Workspace : public Instance {
public:
    explicit Workspace(InstanceId id, std::string name = "Workspace");

    const char* className() const override { return "Workspace"; }
    InstanceClass instanceClass() const override { return InstanceClass::Workspace; }
    float fallenPartsDestroyHeight() const { return fallenHeight_; }
    bool setFallenPartsDestroyHeight(float value) {
        if (!std::isfinite(value) || value < -100000 || value > 100000) return false;
        fallenHeight_=value;return true;
    }
private:
    float fallenHeight_=-500;
};

}  // namespace loblox

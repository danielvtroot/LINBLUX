#pragma once

#include <string>

#include "objects/Part.h"

namespace loblox {

// Piso inicial do projeto: uma Part especial, ancorada e protegida contra
// exclusao acidental. Participa da fisica como qualquer outro corpo estatico.
class Baseplate : public Part {
public:
    explicit Baseplate(InstanceId id, std::string name = "Baseplate");

    const char* className() const override { return "Baseplate"; }
    InstanceClass instanceClass() const override { return InstanceClass::Baseplate; }
};

}  // namespace loblox

#pragma once

#include <string>

#include "objects/Part.h"

namespace loblox {

// Local de nascimento futuro. Nesta etapa e apenas uma peca editavel com
// aparencia propria; nenhum personagem e criado.
class SpawnLocation : public Part {
public:
    explicit SpawnLocation(InstanceId id, std::string name = "SpawnLocation");

    const char* className() const override { return "SpawnLocation"; }
    InstanceClass instanceClass() const override { return InstanceClass::SpawnLocation; }
};

}  // namespace loblox

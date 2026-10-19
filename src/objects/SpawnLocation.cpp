#include "objects/SpawnLocation.h"

#include <utility>

namespace loblox {

SpawnLocation::SpawnLocation(InstanceId id, std::string name)
    : Part(id, std::move(name), PartShape::Block) {
    setSize(Vector3{12.0f, 1.0f, 12.0f});
    setPosition(Vector3{0.0f, 0.5f, 0.0f});
    setColor(Color{0, 162, 255, 255});
    setMaterial(PartMaterial::SmoothPlastic);
    setAnchored(true);
    setCanCollide(true);
    clearDirty();
}

}  // namespace loblox

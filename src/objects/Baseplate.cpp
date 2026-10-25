#include "objects/Baseplate.h"

#include <utility>

namespace loblox {

Baseplate::Baseplate(InstanceId id, std::string name)
    : Part(id, std::move(name), PartShape::Block) {
    setSize(Vector3{160.0f, 1.0f, 160.0f});
    setPosition(Vector3{0.0f, -0.5f, 0.0f});
    setColor(Color{163, 162, 165, 255});
    setMaterial(PartMaterial::SmoothPlastic);
    setAnchored(true);
    setCanCollide(true);
    clearDirty();
}

}  // namespace loblox

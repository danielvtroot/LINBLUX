#include "objects/Lighting.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace loblox {

Lighting::Lighting(InstanceId id, std::string name)
    : Instance(id, std::move(name)) {}

void Lighting::setSunDirection(Vector3 direction) {
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                   direction.z * direction.z);
    if (length < 1e-5f) {
        return;
    }
    sunDirection_ = Vector3Scale(direction, 1.0f / length);
}

void Lighting::setSunIntensity(float intensity) {
    sunIntensity_ = std::clamp(intensity, 0.0f, 3.0f);
}

void Lighting::setBrightness(float brightness) {
    brightness_ = std::clamp(brightness, 0.2f, 2.0f);
}

}  // namespace loblox

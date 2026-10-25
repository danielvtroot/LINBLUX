#include "objects/Lighting.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace loblox {

Lighting::Lighting(InstanceId id, std::string name)
    : Instance(id, std::move(name)) {
    setSunDirection(sunDirection_);
}

void Lighting::setSunDirection(Vector3 direction) {
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                   direction.z * direction.z);
    if (!std::isfinite(length) || length < 1e-5f) {
        return;
    }
    // Project files store a normalised vector. Re-normalising it on every load
    // introduces a last-bit drift that breaks exact project round-trips, so
    // leave already-unit directions untouched.
    if (std::fabs(length - 1.0f) < 1e-6f) {
        sunDirection_ = direction;
        return;
    }
    sunDirection_ = Vector3Scale(direction, 1.0f / length);
}

void Lighting::setSunIntensity(float intensity) {
    if (!std::isfinite(intensity)) return;
    sunIntensity_ = std::clamp(intensity, 0.0f, 3.0f);
}

void Lighting::setBrightness(float brightness) {
    if (!std::isfinite(brightness)) return;
    brightness_ = std::clamp(brightness, 0.2f, 2.0f);
}

}  // namespace loblox

#include "game/Spawn.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

#include "objects/Part.h"
#include "objects/SpawnLocation.h"
#include "scene/Scene.h"

namespace loblox::game {
namespace {
const Instance::Attribute* FindAttribute(const Part& part, const char* lower, const char* upper) {
    auto it = part.attributes().find(lower);
    if (it != part.attributes().end()) return &it->second;
    if (upper != nullptr) {
        it = part.attributes().find(upper);
        if (it != part.attributes().end()) return &it->second;
    }
    return nullptr;
}

bool Truthy(const Instance::Attribute& value) {
    if (const auto* flag = std::get_if<bool>(&value)) return *flag;
    if (const auto* number = std::get_if<double>(&value))
        return std::isfinite(*number) && *number != 0;
    if (const auto* text = std::get_if<std::string>(&value)) {
        std::string normalized = *text;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on";
    }
    return false;
}
} // namespace

bool IsSpawnPart(const Part& part) {
    if (part.instanceClass() == InstanceClass::SpawnLocation) return true;
    const auto* value = FindAttribute(part, "spawn", "Spawn");
    return value != nullptr && Truthy(*value);
}

double SpawnPriority(const Part& part) {
    const auto* value = FindAttribute(part, "spawn_priority", "SpawnPriority");
    if (value == nullptr) return 0;
    double priority = 0;
    if (const auto* number = std::get_if<double>(value)) priority = *number;
    else if (const auto* flag = std::get_if<bool>(value)) priority = *flag ? 1 : 0;
    else if (const auto* text = std::get_if<std::string>(value)) {
        char* end = nullptr;
        const double parsed = std::strtod(text->c_str(), &end);
        if (end != text->c_str() && end != nullptr && *end == '\0') priority = parsed;
    }
    if (!std::isfinite(priority)) return 0;
    return std::clamp(priority, -1000000.0, 1000000.0);
}

Vector3 SpawnTopCenter(const Part& part) {
    if (part.shape() == PartShape::Sphere) {
        return {part.position().x, part.position().y + part.sphereRadius(), part.position().z};
    }
    const Vector3 half = part.halfExtents();
    Vector3 top = Vector3Add(part.position(),
                             Vector3RotateByQuaternion({0, half.y, 0}, part.rotation()));
    float topY = top.y;
    for (int corner = 0; corner < 8; ++corner) {
        const Vector3 local{(corner & 1 ? 1.f : -1.f) * half.x,
                            (corner & 2 ? 1.f : -1.f) * half.y,
                            (corner & 4 ? 1.f : -1.f) * half.z};
        const Vector3 world = Vector3Add(part.position(),
                                         Vector3RotateByQuaternion(local, part.rotation()));
        topY = std::max(topY, world.y);
    }
    top.y = topY;
    return top;
}

std::vector<SpawnPoint> OrderedSpawnPoints(const Scene& scene) {
    std::vector<SpawnPoint> points;
    for (const Part* part : scene.parts()) {
        if (part == nullptr || !IsSpawnPart(*part)) continue;
        const Vector3 top = SpawnTopCenter(*part);
        if (!std::isfinite(top.x) || !std::isfinite(top.y) || !std::isfinite(top.z)) continue;
        points.push_back({part->id(), SpawnPriority(*part), top});
    }
    std::sort(points.begin(), points.end(), [](const SpawnPoint& a, const SpawnPoint& b) {
        if (a.priority != b.priority) return a.priority > b.priority; // higher wins
        return a.id < b.id; // deterministic tie-break
    });
    return points;
}

} // namespace loblox::game

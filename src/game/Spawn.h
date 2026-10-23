#pragma once

#include <vector>

#include "core/Math.h"
#include "objects/Instance.h"

namespace loblox {
class Part;
class Scene;

namespace game {

struct SpawnPoint {
    InstanceId id = 0;
    double priority = 0;
    Vector3 position{}; // centre of the part's upper surface / top of its bounds
};

// A legacy SpawnLocation is always a spawn point. Any other Part opts in with
// the `spawn` attribute (bool true, non-zero number, or "true"/"1"/"yes"/"on").
bool IsSpawnPart(const Part& part);

// Higher values win. Ties are resolved by the lower InstanceId, so a scene
// always selects the same spawn without relying on pointer/container order.
double SpawnPriority(const Part& part);

// Upper-surface centre for blocks/spheres, including rotation. The returned Y
// is the top of the rotated world-space bounds so a capsule can be placed
// above even a tilted Part without intersecting it.
Vector3 SpawnTopCenter(const Part& part);

std::vector<SpawnPoint> OrderedSpawnPoints(const Scene& scene);

} // namespace game
} // namespace loblox

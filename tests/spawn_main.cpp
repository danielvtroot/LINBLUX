// Headless regression tests for Part-based spawn selection and placement.
#include <cmath>
#include <cstdio>
#include <string>

#include "game/Runtime.h"
#include "game/Spawn.h"
#include "objects/Part.h"
#include "objects/SpawnLocation.h"
#include "scene/Scene.h"

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char* expression, int line) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %d: %s\n", line, expression); }
}
#define CHECK(x) Check((x), #x, __LINE__)
using namespace loblox;
using namespace loblox::game;

Part* Floor(Scene& scene) {
    Part* floor = scene.createPart(PartShape::Block, {0, -1, 0}, {200, 2, 200}, "Floor");
    if (floor) floor->setAnchored(true);
    return floor;
}

Part* SpawnPart(Scene& scene, Vector3 position, Vector3 size, const char* name,
                double priority, bool collides = true) {
    Part* part = scene.createPart(PartShape::Block, position, size, name);
    if (!part) return nullptr;
    part->setAnchored(true);
    part->setCanCollide(collides);
    part->setAttribute("spawn", true);
    part->setAttribute("spawn_priority", priority);
    return part;
}

void TestLegacySpawnLocation() {
    Scene scene;
    Floor(scene);
    auto* legacy = scene.createSpawnLocation({8, 1, 6});
    CHECK(legacy != nullptr);
    if (!legacy) return;
    const Vector3 top = SpawnTopCenter(*legacy);
    RuntimeSession runtime;
    std::string error;
    CHECK(runtime.start(scene, error));
    CHECK(error.empty());
    const Vector3 position = runtime.position();
    const float halfHeight = runtime.settings().collisionHeight * .5f;
    CHECK(std::fabs(position.x - top.x) < .05f);
    CHECK(std::fabs(position.z - top.z) < .05f);
    CHECK(position.y > top.y + halfHeight);
    CHECK(std::fabs(position.y - (top.y + halfHeight + .08f)) < .05f);
    runtime.stop();
}

void TestPriorityAndRotation() {
    Scene scene;
    Floor(scene);
    auto* low = SpawnPart(scene, {10, 1, 0}, {4, 2, 4}, "LowPriority", 1);
    auto* high = SpawnPart(scene, {-6, 4, 2}, {2, 2, 2}, "RotatedHighPriority", 5);
    CHECK(low != nullptr);
    CHECK(high != nullptr);
    if (!high) return;
    high->setRotationEulerDegrees({0, 0, 45});
    const Vector3 expected = SpawnTopCenter(*high);
    RuntimeSession runtime;
    std::string error;
    CHECK(runtime.start(scene, error));
    const Vector3 position = runtime.position();
    const float halfHeight = runtime.settings().collisionHeight * .5f;
    CHECK(std::fabs(position.x - expected.x) < .05f);
    CHECK(std::fabs(position.z - expected.z) < .05f);
    CHECK(position.y > expected.y + halfHeight);
    CHECK(std::fabs(position.y - (expected.y + halfHeight + .08f)) < .05f);
    runtime.stop();
}

void TestDeterministicTie() {
    Scene scene;
    Floor(scene);
    auto* first = SpawnPart(scene, {4, 1, 0}, {2, 2, 2}, "First", 2);
    auto* second = SpawnPart(scene, {-4, 1, 0}, {2, 2, 2}, "Second", 2);
    CHECK(first != nullptr);
    CHECK(second != nullptr);
    if (!first || !second) return;
    const auto points = OrderedSpawnPoints(scene);
    CHECK(points.size() == 2);
    CHECK(points.size() == 2 && points[0].id == first->id());
    CHECK(points.size() == 2 && points[1].id == second->id());
}

void TestFallbackWithoutSpawn() {
    Scene scene;
    Floor(scene);
    RuntimeSession runtime;
    std::string error;
    CHECK(runtime.start(scene, error));
    const Vector3 position = runtime.position();
    CHECK(std::fabs(position.x) < .05f);
    CHECK(std::fabs(position.z) < .05f);
    CHECK(position.y > 0);
    runtime.stop();
}

void TestSmallElevatedAndMarker() {
    Scene scene;
    Floor(scene);
    auto* small = SpawnPart(scene, {20, 50, 0}, {.2f, .2f, .2f}, "SmallElevated", 10);
    auto* marker = SpawnPart(scene, {30, 60, 0}, {2, .2f, 2}, "NonCollidingMarker", 20, false);
    CHECK(small != nullptr);
    CHECK(marker != nullptr);
    if (!marker) return;
    const Vector3 expected = SpawnTopCenter(*marker);
    RuntimeSession runtime;
    std::string error;
    CHECK(runtime.start(scene, error));
    const Vector3 position = runtime.position();
    const float halfHeight = runtime.settings().collisionHeight * .5f;
    CHECK(std::fabs(position.x - expected.x) < .05f);
    CHECK(std::fabs(position.z - expected.z) < .05f);
    CHECK(position.y > expected.y + halfHeight);
    runtime.stop();
}

void TestObstructedCandidateFallsBackToNext() {
    Scene scene;
    Floor(scene);
    auto* blocked = SpawnPart(scene, {0, 1, 0}, {4, 1, 4}, "BlockedSpawn", 10);
    auto* ceiling = scene.createPart(PartShape::Block, {0, 4, 0}, {6, 2, 6}, "Ceiling");
    CHECK(blocked != nullptr);
    CHECK(ceiling != nullptr);
    if (ceiling) ceiling->setAnchored(true);
    auto* clear = SpawnPart(scene, {12, 1, 0}, {4, 1, 4}, "ClearSpawn", 1);
    CHECK(clear != nullptr);
    if (!clear) return;
    RuntimeSession runtime;
    std::string error;
    CHECK(runtime.start(scene, error));
    const Vector3 position = runtime.position();
    CHECK(std::fabs(position.x - 12.f) < .05f);
    CHECK(std::fabs(position.z) < .05f);
    runtime.stop();
}

void TestAllSpawnsObstructed() {
    Scene scene;
    Floor(scene);
    SpawnPart(scene, {0, 1, 0}, {4, 1, 4}, "BlockedSpawn", 10);
    auto* ceiling = scene.createPart(PartShape::Block, {0, 4, 0}, {6, 2, 6}, "Ceiling");
    if (ceiling) ceiling->setAnchored(true);
    RuntimeSession runtime;
    std::string error;
    CHECK(!runtime.start(scene, error));
    CHECK(error.find("obstructed") != std::string::npos);
    CHECK(runtime.state() == RuntimeState::Editing);
}
} // namespace

int main() {
    TestLegacySpawnLocation();
    TestPriorityAndRotation();
    TestDeterministicTie();
    TestFallbackWithoutSpawn();
    TestSmallElevatedAndMarker();
    TestObstructedCandidateFallsBackToNext();
    TestAllSpawnsObstructed();
    std::printf("Spawn tests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

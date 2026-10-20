// Testes headless (sem janela/OpenGL). Executados pelo CTest.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "core/Config.h"
#include "core/Diagnostics.h"
#include "objects/Baseplate.h"
#include "objects/SpawnLocation.h"
#include "editor/Gizmo.h"
#include "editor/GizmoLayout.h"
#include "editor/GizmoMath.h"
#include "editor/TransformSession.h"
#include "physics/PhysicsWorld.h"
#include "rendering/EditorCamera.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"
#include "serialization/SceneLua.h"

namespace {

int gChecks = 0;
int gFailures = 0;
int gSectionFailures = 0;
const char* gSection = "";

void Section(const char* name) {
    gSection = name;
    gSectionFailures = 0;
    std::printf("\n[%s]\n", name);
}

bool TempFilesWithPrefix(const std::string& prefix) {
    std::error_code scanError;
    for (std::filesystem::directory_iterator it(std::filesystem::temp_directory_path(),
                                                 scanError), end;
         !scanError && it != end; it.increment(scanError)) {
        if (it->path().filename().string().rfind(prefix, 0) == 0) return true;
    }
    return false;
}

void CheckTrue(bool condition, const char* expression, int line) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        ++gSectionFailures;
        std::printf("  FAIL linha %d: %s\n", line, expression);
    }
}

void CheckNear(float actual, float expected, float epsilon, const char* expression, int line) {
    ++gChecks;
    if (!(std::fabs(actual - expected) <= epsilon)) {
        ++gFailures;
        ++gSectionFailures;
        std::printf("  FAIL linha %d: %s (obtido %.4f, esperado %.4f +/- %.4f)\n", line,
                    expression, actual, expected, epsilon);
    }
}

void CheckString(const std::string& actual, const std::string& expected, const char* expression,
                 int line) {
    ++gChecks;
    if (actual != expected) {
        ++gFailures;
        ++gSectionFailures;
        std::printf("  FAIL linha %d: %s (obtido '%s', esperado '%s')\n", line, expression,
                    actual.c_str(), expected.c_str());
    }
}

void EndSection() {
    if (gSectionFailures == 0) {
        std::printf("  ok\n");
    }
}

#define CHECK(condition) CheckTrue((condition), #condition, __LINE__)
#define CHECK_NEAR(actual, expected, epsilon) \
    CheckNear((actual), (expected), (epsilon), #actual, __LINE__)
#define CHECK_STR(actual, expected) CheckString((actual), (expected), #actual, __LINE__)

void Simulate(loblox::PhysicsWorld& physics, loblox::Scene& scene, float seconds) {
    const float step = 1.0f / 60.0f;
    const int steps = static_cast<int>(seconds / step) + 1;
    for (int i = 0; i < steps; ++i) {
        physics.simulate(scene, step);
    }
}

void TestDefaultScene() {
    Section("Cena padrao e hierarquia");
    loblox::Scene scene;
    scene.buildDefaultScene();

    CHECK(scene.workspace() != nullptr);
    CHECK(scene.lighting() != nullptr);
    CHECK_STR(scene.workspace()->name(), "Workspace");
    CHECK_STR(scene.lighting()->name(), "Lighting");
    CHECK(scene.workspace()->name() == "Workspace");
    CHECK(scene.workspace()->children().size() == 2);

    loblox::Baseplate* baseplate = nullptr;
    loblox::SpawnLocation* spawn = nullptr;
    for (loblox::Part* part : scene.parts()) {
        if (part->instanceClass() == loblox::InstanceClass::Baseplate) {
            baseplate = static_cast<loblox::Baseplate*>(part);
        } else if (part->instanceClass() == loblox::InstanceClass::SpawnLocation) {
            spawn = static_cast<loblox::SpawnLocation*>(part);
        }
    }
    CHECK(baseplate != nullptr);
    CHECK(spawn != nullptr);
    if (baseplate != nullptr) {
        CHECK(baseplate->anchored());
        CHECK(baseplate->canCollide());
        CHECK(!baseplate->deletable());
        CHECK_STR(baseplate->className(), "Baseplate");
        CHECK_NEAR(baseplate->position().y, -0.5f, 0.001f);
        CHECK(baseplate->parent() == scene.workspace());
    }
    if (spawn != nullptr) {
        CHECK(spawn->anchored());
        CHECK_STR(spawn->className(), "SpawnLocation");
        CHECK(spawn->parent() == scene.workspace());
    }
    CHECK(!scene.remove(baseplate));
    CHECK(scene.parts().size() == 2);
    EndSection();
}

void TestCreateAndRemove() {
    Section("Criacao, nomes, propriedades e remocao");
    loblox::Scene scene;
    scene.buildDefaultScene();

    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 4, 0));
    CHECK(part != nullptr);
    if (part == nullptr) {
        EndSection();
        return;
    }
    CHECK_STR(part->name(), "Part");
    CHECK_STR(part->className(), "Part");
    CHECK(part->parent() == scene.workspace());
    CHECK(scene.find(part->id()) == part);
    CHECK(scene.parts().size() == 3);

    loblox::Part* second = scene.createPart(loblox::PartShape::Sphere, loblox::V3(8, 4, 0));
    CHECK(second != nullptr);
    if (second != nullptr) {
        CHECK_STR(second->name(), "Sphere");
        CHECK(second->id() != part->id());
        CHECK(second->shape() == loblox::PartShape::Sphere);
    }

    part->setPosition(loblox::V3(1, 2, 3));
    part->setSize(loblox::V3(5, 6, 7));
    part->setRotationEulerDegrees(loblox::V3(0, 45, 0));
    part->setColor(Color{10, 20, 30, 255});
    part->setMaterial(loblox::PartMaterial::Wood);
    part->setAnchored(true);
    part->setCanCollide(false);

    CHECK_NEAR(part->position().x, 1.0f, 0.001f);
    CHECK_NEAR(part->size().z, 7.0f, 0.001f);
    CHECK(part->material() == loblox::PartMaterial::Wood);
    CHECK(part->anchored());
    CHECK(!part->canCollide());
    CHECK_NEAR(part->rotationEulerDegrees().y, 45.0f, 0.01f);

    part->setSize(loblox::V3(0.0f, -4.0f, 0.01f));
    CHECK(part->size().x >= loblox::kMinPartSize);
    CHECK(part->size().y >= loblox::kMinPartSize);
    CHECK(part->size().z >= loblox::kMinPartSize);

    scene.select(part);
    CHECK(scene.selection() == part);
    const size_t countBefore = scene.objects().size();
    const loblox::InstanceId removedId = part->id();
    CHECK(scene.remove(part));
    CHECK(scene.objects().size() == countBefore - 1);
    CHECK(scene.selection() == nullptr);
    CHECK(scene.find(removedId) == nullptr);
    EndSection();
}

void TestRemovalListener() {
    Section("Listener de remocao (fisica)");
    loblox::Scene scene;
    loblox::Lighting* lighting = scene.lighting();
    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 4, 0));
    int removedParts = 0;
    scene.addRemovalListener([&removedParts](loblox::Instance* object) {
        if (object->isPart()) {
            ++removedParts;
        }
    });
    CHECK(scene.remove(part));
    CHECK(removedParts == 1);
    CHECK(scene.remove(lighting) == false);
    CHECK(removedParts == 1);
    EndSection();
}

void TestAnchoredAndDynamic() {
    Section("Anchored x dinamico");
    loblox::Scene scene;
    scene.buildDefaultScene();

    loblox::PhysicsWorld physics;
    physics.attach(scene);
    CHECK(physics.bodyCount() == scene.parts().size());

    loblox::Part* anchored = scene.createPart(loblox::PartShape::Block, loblox::V3(-10, 20, 0));
    anchored->setAnchored(true);
    physics.addPart(anchored);
    CHECK(!physics.isDynamic(anchored->id()));
    Simulate(physics, scene, 1.5f);
    CHECK_NEAR(anchored->position().y, 20.0f, 0.001f);

    loblox::Part* dynamic = scene.createPart(loblox::PartShape::Block, loblox::V3(10, 20, 0));
    dynamic->setAnchored(false);
    physics.addPart(dynamic);
    CHECK(physics.isDynamic(dynamic->id()));
    Simulate(physics, scene, 2.5f);
    // Baseplate topo em y=0; caixa 4x4x4 repousa com centro em y=2.
    CHECK_NEAR(dynamic->position().y, 2.0f, 0.25f);
    CHECK(std::fabs(dynamic->position().x) < 25.0f);

    // Alternar anchored no meio da simulacao: deve parar imediatamente.
    dynamic->setAnchored(true);
    physics.applyChanges(dynamic);
    CHECK(!physics.isDynamic(dynamic->id()));
    const float stoppedY = dynamic->position().y;
    Simulate(physics, scene, 1.0f);
    CHECK_NEAR(dynamic->position().y, stoppedY, 0.001f);

    // Voltar a dinamico deve cair de novo.
    dynamic->setAnchored(false);
    dynamic->setPosition(loblox::V3(10, 12, 0));
    physics.applyChanges(dynamic);
    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(dynamic->position().y, 2.0f, 0.25f);
    EndSection();
}

void TestCanCollide() {
    Section("CanCollide");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Part* ghost = scene.createPart(loblox::PartShape::Block, loblox::V3(20, 12, 0));
    ghost->setAnchored(false);
    ghost->setCanCollide(false);
    physics.addPart(ghost);
    Simulate(physics, scene, 2.0f);
    CHECK(ghost->position().y < -5.0f);

    loblox::Part* solid = scene.createPart(loblox::PartShape::Block, loblox::V3(-20, 12, 0));
    solid->setAnchored(false);
    physics.addPart(solid);
    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(solid->position().y, 2.0f, 0.25f);
    EndSection();
}

void TestTransformSyncAndRaycast() {
    Section("Sincronizacao transformacao/fisica e raycast");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 10, 0));
    part->setAnchored(true);
    physics.addPart(part);
    physics.simulate(scene, 1.0f / 60.0f);
    CHECK_NEAR(physics.bodyPosition(part->id()).y, 10.0f, 0.001f);

    part->setPosition(loblox::V3(4, 6, 2));
    physics.syncScene(scene);
    CHECK_NEAR(physics.bodyPosition(part->id()).y, 6.0f, 0.001f);
    physics.simulate(scene, 1.0f / 60.0f);
    CHECK_NEAR(part->position().y, 6.0f, 0.001f);

    loblox::Part* hit = nullptr;
    Vector3 point;
    const bool found = physics.raycast(loblox::V3(4, 20, 2), loblox::V3(0, -1, 0), 30.0f, &hit,
                                       &point);
    CHECK(found);
    CHECK(hit == part);
    if (found) {
        CHECK_NEAR(point.y, 8.0f, 0.1f);
    }

    // Mover a part move o corpo; raio antigo nao deve mais acertar a part.
    part->setPosition(loblox::V3(40, 6, 2));
    physics.syncScene(scene);
    loblox::Part* hitAfter = nullptr;
    physics.raycast(loblox::V3(4, 20, 2), loblox::V3(0, -1, 0), 30.0f, &hitAfter);
    CHECK(hitAfter != part);

    // Sem corpos duplicados ao re-adicionar a mesma Part.
    const size_t count = physics.bodyCount();
    physics.addPart(part);
    CHECK(physics.bodyCount() == count);

    // Exclusao remove o corpo e o raio deixa de acertar.
    const loblox::InstanceId id = part->id();
    scene.remove(part);
    CHECK(!physics.hasBody(id));
    CHECK(physics.bodyCount() == count - 1);
    loblox::Part* hitDeleted = nullptr;
    physics.raycast(loblox::V3(40, 20, 2), loblox::V3(0, -1, 0), 30.0f, &hitDeleted);
    CHECK(hitDeleted != part || hitDeleted == nullptr);
    EndSection();
}

void TestResizeKeepsSingleBody() {
    Section("Redimensionar mantem um corpo unico");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(25, 12, 0));
    part->setAnchored(false);
    physics.addPart(part);
    const size_t bodies = physics.bodyCount();
    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(part->position().y, 2.0f, 0.25f);

    part->setPosition(loblox::V3(25, 14, 0));
    part->setSize(loblox::V3(8, 2, 8));
    physics.simulate(scene, 1.0f / 60.0f);
    CHECK(physics.bodyCount() == bodies);
    CHECK(physics.isDynamic(part->id()));

    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(part->position().y, 1.0f, 0.25f);
    CHECK(physics.bodyCount() == bodies);
    EndSection();
}

void TestGizmoMath() {
    Section("Matematica de gizmos, eixos, planos, escala e rotacao");
    using namespace loblox;
    using namespace loblox::gizmomath;
    const TransformSnapshot start{V3(10, 20, 30), QuatFromEulerDegrees(V3(0, 0, 0)), V3(4, 6, 8)};
    const Vector3 axes[3] = {V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1)};
    for (int i = 0; i < 3; ++i) {
        const Vector3 moved = MoveAxis(start, axes[i], 2.4f, 1.0f);
        CHECK_NEAR(i == 0 ? moved.x : start.position.x, i == 0 ? 12.0f : 10.0f, 0.001f);
        CHECK_NEAR(i == 1 ? moved.y : start.position.y, i == 1 ? 22.0f : 20.0f, 0.001f);
        CHECK_NEAR(i == 2 ? moved.z : start.position.z, i == 2 ? 32.0f : 30.0f, 0.001f);
    }
    const Vector3 xy = MovePlane(start, axes[0], axes[1], V3(2.2f, -1.2f, 9.0f), 1.0f);
    CHECK_NEAR(xy.x, 12.0f, 0.001f); CHECK_NEAR(xy.y, 19.0f, 0.001f); CHECK_NEAR(xy.z, 30.0f, 0.001f);
    const Vector3 xz = MovePlane(start, axes[0], axes[2], V3(1.2f, 9.0f, 3.7f), 1.0f);
    CHECK_NEAR(xz.x, 11.0f, 0.001f); CHECK_NEAR(xz.y, 20.0f, 0.001f); CHECK_NEAR(xz.z, 34.0f, 0.001f);
    const Vector3 yz = MovePlane(start, axes[1], axes[2], V3(9.0f, 2.2f, -2.2f), 1.0f);
    CHECK_NEAR(yz.x, 10.0f, 0.001f); CHECK_NEAR(yz.y, 22.0f, 0.001f); CHECK_NEAR(yz.z, 28.0f, 0.001f);

    const auto plus = ScaleFace(start, axes[0], 1, 2.0f, 0.5f);
    CHECK_NEAR(plus.size.x, 6.0f, 0.001f); CHECK_NEAR(plus.size.y, 6.0f, 0.001f);
    CHECK_NEAR(plus.size.z, 8.0f, 0.001f); CHECK_NEAR(plus.position.x, 11.0f, 0.001f);
    const auto minus = ScaleFace(start, axes[0], -1, -2.0f, 0.5f);
    CHECK_NEAR(minus.size.x, 6.0f, 0.001f); CHECK_NEAR(minus.position.x, 9.0f, 0.001f);
    const auto clamped = ScaleFace({V3(0,0,0), start.rotation, V3(0.5f,2,2)}, axes[1], 1, -20.0f, 0.0f);
    CHECK_NEAR(clamped.size.y, kMinPartSize, 0.001f);
    CHECK_NEAR(clamped.position.y, -0.9f, 0.001f);
    const auto localStart = TransformSnapshot{V3(0,0,0), QuatFromEulerDegrees(V3(0,90,0)), V3(4,4,4)};
    const Vector3 localX = Axis(0, localStart.rotation, true);
    const auto localScale = ScaleFace(localStart, localX, 1, 2.0f, 0.0f);
    CHECK_NEAR(localScale.size.x, 6.0f, 0.001f);
    CHECK_NEAR(localScale.position.z, -1.0f, 0.001f);
    const TransformSnapshot worldRotated{V3(3, 4, 5), QuatFromEulerDegrees(V3(0, 30, 0)),
                                         V3(4, 6, 8)};
    const Vector3 worldXLocalX = Axis(0, worldRotated.rotation, true);
    const auto worldScale = ScaleFaceAlongAxis(worldRotated, 0, 1, false, 0.8660254f, 0.0f);
    CHECK_NEAR(worldScale.size.x, 4.75f, 0.001f);
    CHECK_NEAR(worldScale.size.y, 6.0f, 0.001f);
    CHECK_NEAR(worldScale.size.z, 8.0f, 0.001f);
    CHECK_NEAR(Vector3Distance(Vector3Subtract(worldScale.position,
                                              Vector3Scale(worldXLocalX, worldScale.size.x * 0.5f)),
                               Vector3Subtract(worldRotated.position,
                                               Vector3Scale(worldXLocalX, worldRotated.size.x * 0.5f))),
               0.0f, 0.001f);
    // All six faces use the same snapshot-relative algorithm. Verify that only
    // the selected dimension changes and the opposite local face stays fixed,
    // including for a rotated Part.
    const HandleId scaleIds[3][2] = {
        {HandleId::XNegative, HandleId::XPositive},
        {HandleId::YNegative, HandleId::YPositive},
        {HandleId::ZNegative, HandleId::ZPositive}};
    const TransformSnapshot rotatedBox{V3(7, -3, 5), QuatFromEulerDegrees(V3(23, 37, -16)),
                                       V3(4, 6, 8)};
    for (const TransformSnapshot candidate : {start, rotatedBox}) {
        const Vector3 localAxes[3] = {Axis(0, candidate.rotation, true),
                                      Axis(1, candidate.rotation, true),
                                      Axis(2, candidate.rotation, true)};
        for (bool localSpace : {false, true}) {
            for (int axis = 0; axis < 3; ++axis) {
                for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
                    const int side = sideIndex == 0 ? -1 : 1;
                    int selectedComponent = axis;
                    float alignment = 1.0f;
                    if (!localSpace) {
                        const Vector3 worldAxis = Axis(axis, candidate.rotation, false);
                        float bestAlignment = 0.0f;
                        for (int i = 0; i < 3; ++i) {
                            const float dot = Vector3DotProduct(worldAxis, localAxes[i]);
                            if (std::fabs(dot) > std::fabs(bestAlignment)) {
                                bestAlignment = dot;
                                selectedComponent = i;
                            }
                        }
                        alignment = bestAlignment;
                    }
                    const int localSide = (alignment < 0.0f ? -1 : 1) * side;
                    const TransformSnapshot scaled = ScaleFaceForHandle(
                        candidate, scaleIds[axis][sideIndex], localSpace,
                        static_cast<float>(side) * 2.0f, 0.0f);
                    const float startSize[3] = {candidate.size.x, candidate.size.y, candidate.size.z};
                    const float resultSize[3] = {scaled.size.x, scaled.size.y, scaled.size.z};
                    for (int i = 0; i < 3; ++i) {
                        const float expectedDelta = i == selectedComponent
                            ? 2.0f * std::fabs(alignment) : 0.0f;
                        CHECK_NEAR(resultSize[i], startSize[i] + expectedDelta, 0.001f);
                    }
                    const Vector3 fixedAfter = Vector3Add(
                        scaled.position,
                        Vector3Scale(localAxes[selectedComponent],
                                     -localSide * resultSize[selectedComponent] * 0.5f));
                    const Vector3 fixedBefore = Vector3Add(
                        candidate.position,
                        Vector3Scale(localAxes[selectedComponent],
                                     -localSide * startSize[selectedComponent] * 0.5f));
                    CHECK_NEAR(Vector3Distance(fixedAfter, fixedBefore), 0.0f, 0.001f);
                    CHECK(std::isfinite(scaled.size.x) && std::isfinite(scaled.size.y) &&
                          std::isfinite(scaled.size.z));
                    CHECK(scaled.size.x >= kMinPartSize && scaled.size.y >= kMinPartSize &&
                          scaled.size.z >= kMinPartSize);
                    CHECK(HandleAxis(scaleIds[axis][sideIndex]) == axis);
                    CHECK(HandleSide(scaleIds[axis][sideIndex]) == side);
                }
            }
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        for (int side : {-1, 1}) {
            const Vector3 handle = ScaleHandleCenter(start, axis, side, false, 2.0f);
            CHECK_NEAR(Vector3Distance(handle, start.position),
                       (axis == 0 ? 2.0f : axis == 1 ? 3.0f : 4.0f) + 2.0f, 0.001f);
            const Vector3 localHandle = ScaleHandleCenter(localStart, axis, side, true, 2.0f);
            CHECK_NEAR(Vector3Distance(localHandle, localStart.position), 4.0f, 0.001f);
        }
    }
    const Vector3 tinyHandle = ScaleHandleCenter(
        {V3(0, 0, 0), start.rotation, V3(0.2f, 0.2f, 0.2f)}, 1, 1, false, 2.0f);
    const Vector3 hugeHandle = ScaleHandleCenter(
        {V3(0, 0, 0), start.rotation, V3(1000, 1000, 1000)}, 1, 1, false, 2.0f);
    CHECK_NEAR(tinyHandle.y, 2.1f, 0.001f);
    CHECK_NEAR(hugeHandle.y, 502.0f, 0.001f);
    const float angleAcrossPi = WrappedAngleDelta(179.0f * DEG2RAD, -179.0f * DEG2RAD);
    CHECK_NEAR(angleAcrossPi / DEG2RAD, 2.0f, 0.001f);
    const float angleAcrossMinusPi = WrappedAngleDelta(-179.0f * DEG2RAD, 179.0f * DEG2RAD);
    CHECK_NEAR(angleAcrossMinusPi / DEG2RAD, -2.0f, 0.001f);
    CHECK_NEAR(WrappedAngleDelta(0.75f, 0.75f), 0.0f, 0.0001f);
    const std::array<Vector2, 6> overlapping{};
    const auto separated = SeparateScreenHandles(overlapping, 23.0f);
    for (std::size_t i = 0; i < separated.size(); ++i) {
        for (std::size_t j = i + 1; j < separated.size(); ++j) {
            const float dx = separated[i].x - separated[j].x;
            const float dy = separated[i].y - separated[j].y;
            CHECK(std::sqrt(dx * dx + dy * dy) >= 22.9f);
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        for (int side : {-1, 1}) {
            const int index = axis * 2 + (side > 0 ? 1 : 0);
            CHECK(PickScaleFaceHandle(separated, separated[static_cast<std::size_t>(index)]) ==
                  ScaleFaceHandleId(axis, side));
        }
    }
    CHECK(PickScaleFaceHandle(separated, Vector2{1000.0f, 1000.0f}) == HandleId::None);
    CHECK(ScaleFaceHandleId(3, 1) == HandleId::None);
    const auto uniformSphere = ScaleUniformFace(start, axes[2], -1, -2.0f, 0.0f);
    CHECK_NEAR(uniformSphere.size.x, 6.0f, 0.001f);
    CHECK_NEAR(uniformSphere.size.y, 6.0f, 0.001f);
    CHECK_NEAR(uniformSphere.size.z, 6.0f, 0.001f);
    CHECK_NEAR(uniformSphere.position.z, 29.0f, 0.001f);
    for (int i = 0; i < 3; ++i) {
        const Quaternion rotated = Rotate(start, axes[i], 30.0f * DEG2RAD, 15.0f);
        const Vector3 result = EulerDegreesFromQuat(rotated);
        CHECK(std::isfinite(result.x) && std::isfinite(result.y) && std::isfinite(result.z));
        CHECK_NEAR(result.x, i == 0 ? 30.0f : 0.0f, 0.01f);
        CHECK_NEAR(result.y, i == 1 ? 30.0f : 0.0f, 0.01f);
        CHECK_NEAR(result.z, i == 2 ? 30.0f : 0.0f, 0.01f);
    }
    const TransformSnapshot alreadyRotated{V3(0, 0, 0), QuatFromEulerDegrees(V3(12, 38, -7)),
                                           V3(4, 4, 4)};
    const Vector3 localY = Axis(1, alreadyRotated.rotation, true);
    const Quaternion expectedLocalDelta = QuaternionFromAxisAngle(localY, 20.0f * DEG2RAD);
    const Quaternion combined = Rotate(alreadyRotated, localY, 20.0f * DEG2RAD, 0.0f);
    CHECK(NearlyEqual(combined,
                      CanonicalQuaternion(QuaternionNormalize(
                          QuaternionMultiply(expectedLocalDelta, alreadyRotated.rotation))),
                      0.001f));
    CHECK_NEAR(SnapDelta(23.0f * DEG2RAD, 15.0f * DEG2RAD) / DEG2RAD, 30.0f, 0.001f);
    EndSection();
}

void TestGizmoLayout() {
    Section("Layout screen-space do gizmo (desenho == hit test)");
    using namespace loblox;
    using namespace loblox::gizmolayout;
    Part part(1, "Part", PartShape::Block);
    part.setTransform(loblox::Transform{V3(0, 4, 0), QuatFromEulerDegrees(V3(0, 0, 0)), V3(4, 4, 4)});
    EditorCamera camera;
    const Rectangle viewport{0.0f, 0.0f, 640.0f, 400.0f};

    // Projecao valida a frente, invalida atras do near plane/camera.
    camera.setOrbit(part.position(), 12.0f, 0.75f, 0.48f);
    Vector2 checked{};
    CHECK(camera.projectChecked(part.position(), Vector2{640.0f, 400.0f}, checked));
    const Vector2 plain = camera.project(part.position(), Vector2{640.0f, 400.0f});
    CHECK_NEAR(checked.x, plain.x, 0.5f);
    CHECK_NEAR(checked.y, plain.y, 0.5f);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target(), camera.position()));
    Vector2 ignored{123.0f, 456.0f};
    CHECK(!camera.projectChecked(
        Vector3Add(camera.position(), Vector3Scale(forward, -3.0f)), Vector2{640.0f, 400.0f},
        ignored));
    CHECK_NEAR(ignored.x, 123.0f, 0.001f);
    CHECK(!camera.projectChecked(
        Vector3Add(camera.position(), Vector3Scale(forward, 0.01f)), Vector2{640.0f, 400.0f},
        ignored));
    CHECK(camera.projectChecked(
        Vector3Add(camera.position(), Vector3Scale(forward, 0.5f)), Vector2{640.0f, 400.0f},
        ignored));

    // Tamanho em pixels: acompanha a resolucao e respeita limites.
    const float tiny = PixelLength(Vector2{200.0f, 200.0f});
    const float normal = PixelLength(Vector2{640.0f, 400.0f});
    const float large = PixelLength(Vector2{900.0f, 700.0f});
    const float huge = PixelLength(Vector2{3840.0f, 2160.0f});
    CHECK_NEAR(tiny, 60.0f, 0.01f);
    CHECK_NEAR(huge, 132.0f, 0.01f);
    CHECK(large > normal);
    CHECK(huge >= large);

    // Os seis lados de Scale: posicoes distintas, separadas, na viewport e
    // clicaveis no ponto exato em que sao desenhadas.
    for (float distance : {2.0f, 4.0f, 12.0f, 42.0f, 200.0f}) {
        camera.setOrbit(part.position(), distance, 0.75f, 0.48f);
        const Layout scale = Build(part, ToolMode::Scale, camera, viewport, false);
        CHECK(scale.valid);
        for (int axis = 0; axis < 3; ++axis) {
            for (int side : {-1, 1}) {
                const int index = axis * 2 + (side > 0 ? 1 : 0);
                const Rectangle rect = ScaleHandleRect(scale, index);
                const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
                CHECK(rect.x >= 0.0f && rect.y >= 0.0f && rect.x + rect.width <= 640.0f &&
                      rect.y + rect.height <= 400.0f);
                CHECK(PickScaleHandle(scale, center) ==
                      gizmomath::ScaleFaceHandleId(axis, side));
                for (int other = index + 1; other < 6; ++other) {
                    const Vector2 otherCenter{scale.scaleHandle[other].x,
                                              scale.scaleHandle[other].y};
                    CHECK(Vector2Distance(center, otherCenter) >= 22.0f);
                }
            }
        }
    }

    // Part enorme com camera muito proxima: as alças continuam na viewport.
    part.setTransform(loblox::Transform{V3(0, 4, 0), Quaternion{0, 0, 0, 1}, V3(400, 400, 400)});
    camera.setOrbit(part.position(), 2.0f, 0.75f, 0.48f);
    const Layout hugePart = Build(part, ToolMode::Scale, camera, viewport, false);
    CHECK(hugePart.valid);
    for (int index = 0; index < 6; ++index) {
        const Rectangle rect = ScaleHandleRect(hugePart, index);
        CHECK(rect.x >= 0.0f && rect.y >= 0.0f && rect.x + rect.width <= 640.0f &&
              rect.y + rect.height <= 400.0f);
    }

    // Part pequena: os controles de Move continuam com tamanho de tela.
    part.setTransform(loblox::Transform{V3(0, 4, 0), Quaternion{0, 0, 0, 1}, V3(0.2f, 0.2f, 0.2f)});
    camera.setOrbit(part.position(), 200.0f, 0.0f, 0.0f);
    const Layout smallPart = Build(part, ToolMode::Move, camera, viewport, false);
    CHECK(smallPart.valid);
    for (int axis = 0; axis < 3; ++axis) {
        const float distance = Vector2Distance(smallPart.center,
                                               smallPart.axisButton[axis]);
        CHECK(distance >= 60.0f - 0.5f && distance <= 132.5f);
        const Rectangle rect = AxisButtonRect(smallPart, axis);
        const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
        if (PickPlane(smallPart, center) < 0) CHECK(PickAxisButton(smallPart, center) == axis);
    }

    // Rotacao: aneis existem, marcadores sao clicaveis e longe deles nao ha hit.
    part.setTransform(loblox::Transform{V3(3, 5, -2), QuatFromEulerDegrees(V3(23, 37, -16)),
                                V3(4, 6, 8)});
    for (float distance : {2.0f, 12.0f, 200.0f}) {
        for (float pitch : {-1.2f, 0.0f, 1.2f}) {
            camera.setOrbit(part.position(), distance, 0.75f, pitch);
            const Layout rotate = Build(part, ToolMode::Rotate, camera, viewport, true);
            CHECK(rotate.valid);
            for (int axis = 0; axis < 3; ++axis) {
                CHECK(!rotate.ring[axis].empty());
                const Rectangle rect = RotateMarkerRect(rotate, axis);
                const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
                CHECK(PickRotateMarker(rotate, center) == axis);
            }
        }
    }
    CHECK(PickRotateRing(smallPart, Vector2{-50.0f, -50.0f}) == -1);

    // Build e puro: duas chamadas produzem exatamente o mesmo layout, premissa
    // para desenho e picking usarem a mesma geometria.
    camera.setOrbit(part.position(), 9.0f, 0.9f, 0.4f);
    const Layout first = Build(part, ToolMode::Scale, camera, viewport, false);
    const Layout second = Build(part, ToolMode::Scale, camera, viewport, false);
    CHECK(first.valid && second.valid);
    for (int index = 0; index < 6; ++index) {
        CHECK_NEAR(first.scaleHandle[index].x, second.scaleHandle[index].x, 0.0001f);
        CHECK_NEAR(first.scaleHandle[index].y, second.scaleHandle[index].y, 0.0001f);
    }

    // Pivo atras da camera: nenhuma geometria valida e nenhum picking.
    const float yaw = 0.4f;
    const float pitch = 0.3f;
    const Vector3 direction{std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                            std::cos(pitch) * std::cos(yaw)};
    camera.setOrbit(Vector3Subtract(part.position(), Vector3Scale(direction, 2.5f)), 2.0f, yaw,
                    pitch);
    const Layout behind = Build(part, ToolMode::Move, camera, viewport, false);
    CHECK(!behind.valid);
    CHECK(PickAxisButton(behind, Vector2{320.0f, 200.0f}) == -1);
    CHECK(PickScaleHandle(behind, Vector2{320.0f, 200.0f}) == gizmomath::HandleId::None);
    EndSection();
}

void TestGizmoSessionLifecycle() {
    Section("Sessao de transformacao: prioridade, confirmacao, cancelamento e fisica");
    using namespace loblox;
    Scene scene;
    Part* part = scene.createPart(PartShape::Block, V3(0, 20, 0), V3(8, 2, 4), "SessionPart");
    part->setAnchored(false);
    PhysicsWorld physics;
    physics.attach(scene);
    Gizmo gizmo;
    TransformSession session(gizmo, physics);
    EditorCamera camera;
    const Rectangle viewport{0.0f, 0.0f, 640.0f, 400.0f};
    const loblox::Transform start = part->transform();
    camera.setOrbit(part->position(), 14.0f, 0.75f, 0.35f);

    const auto layout = gizmolayout::Build(*part, ToolMode::Move, camera, viewport, false);
    CHECK(layout.valid);
    int axis = -1;
    Vector2 pressPoint{};
    for (int candidate = 0; candidate < 3; ++candidate) {
        const Rectangle rect = gizmolayout::AxisButtonRect(layout, candidate);
        const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
        if (gizmolayout::PickPlane(layout, center) < 0 &&
            gizmolayout::PickAxisButton(layout, center) == candidate) {
            axis = candidate;
            pressPoint = center;
            break;
        }
    }
    CHECK(axis >= 0);
    if (axis < 0) { EndSection(); return; }

    const Vector2 direction = Vector2Normalize(
        Vector2Subtract(pressPoint, layout.center));
    const Vector2 dragPoint{std::clamp(pressPoint.x + direction.x * 55.0f, 5.0f, 635.0f),
                            std::clamp(pressPoint.y + direction.y * 55.0f, 5.0f, 395.0f)};

    const auto makeInput = [&](Vector2 mouse, bool pressed, bool down, bool focused = true) {
        GizmoInput input;
        input.mousePosition = mouse;
        input.leftPressed = pressed;
        input.leftDown = down;
        input.windowFocused = focused;
        return input;
    };
    const auto resetPart = [&]() {
        part->setTransform(start);
        physics.simulate(scene, 0.0f);
        camera.setOrbit(part->position(), 14.0f, 0.75f, 0.35f);
    };
    const auto begin = [&](Gizmo& targetGizmo, TransformSession& targetSession) {
        (void)targetGizmo.update(ToolMode::Move, part, camera, viewport, true, true,
                                 makeInput(pressPoint, false, false));
        return targetSession.update(ToolMode::Move, part, camera, viewport, true, true,
                                    makeInput(pressPoint, true, true));
    };

    resetPart();
    GizmoUpdateResult result = begin(gizmo, session);
    CHECK(result.started);
    CHECK(result.consumed);
    CHECK(!result.ended);
    CHECK(result.partId == part->id());
    CHECK(session.active());
    CHECK(session.partId() == part->id());
    CHECK(physics.isPartEditing(part->id()));
    CHECK(gizmo.hoveringHandle());

    result = session.update(ToolMode::Move, part, camera, viewport, true, true,
                            makeInput(dragPoint, false, true));
    CHECK(result.consumed);
    CHECK(!result.started);
    CHECK(Vector3Distance(part->position(), start.position) > 0.01f);
    CHECK(physics.isPartEditing(part->id()));

    result = session.update(ToolMode::Move, part, camera, viewport, true, true,
                            makeInput(dragPoint, false, false));
    CHECK(result.ended);
    CHECK(!result.cancelled);
    CHECK(result.partId == part->id());
    CHECK(!session.active());
    CHECK(!physics.isPartEditing(part->id()));
    const Vector3 releasedPosition = part->position();

    resetPart();
    result = begin(gizmo, session);
    CHECK(result.started && physics.isPartEditing(part->id()));
    (void)session.update(ToolMode::Move, part, camera, viewport, true, true,
                         makeInput(dragPoint, false, true));
    result = session.update(ToolMode::Move, part, camera, viewport, true, true,
                            makeInput(dragPoint, false, true, false));
    CHECK(result.ended);
    CHECK(result.cancelled);
    CHECK(NearlyEqual(part->position(), start.position, 0.001f));
    CHECK(!physics.isPartEditing(part->id()));

    resetPart();
    result = begin(gizmo, session);
    CHECK(result.started);
    (void)session.update(ToolMode::Move, part, camera, viewport, true, true,
                         makeInput(dragPoint, false, true));
    result = session.update(ToolMode::Select, part, camera, viewport, true, true,
                            makeInput(dragPoint, false, true));
    CHECK(result.ended);
    CHECK(result.cancelled);
    CHECK(NearlyEqual(part->position(), start.position, 0.001f));
    CHECK(!physics.isPartEditing(part->id()));

    resetPart();
    result = begin(gizmo, session);
    CHECK(result.started);
    (void)session.update(ToolMode::Move, part, camera, viewport, true, true,
                         makeInput(dragPoint, false, true));
    session.cancel(part, true);
    CHECK(!session.active());
    CHECK(NearlyEqual(part->position(), start.position, 0.001f));
    CHECK(!physics.isPartEditing(part->id()));

    {
        resetPart();
        Gizmo temporaryGizmo;
        TransformSession temporarySession(temporaryGizmo, physics);
        result = begin(temporaryGizmo, temporarySession);
        CHECK(result.started);
        CHECK(physics.isPartEditing(part->id()));
    }
    CHECK(!physics.isPartEditing(part->id()));

    // O clique que inicia um arraste e consumido: a selecao por raycast nao
    // pode trocar a Part no mesmo evento.
    resetPart();
    result = begin(gizmo, session);
    CHECK(result.consumed);
    CHECK(result.partId == part->id());
    session.cancel(part, false);
    CHECK(!physics.isPartEditing(part->id()));
    CHECK(std::isfinite(releasedPosition.x) && std::isfinite(releasedPosition.y) &&
          std::isfinite(releasedPosition.z));
    EndSection();
}

void TestEditorCameraMath() {
    Section("Matematica de navegacao da camera");
    using namespace loblox;
    EditorCamera camera;
    const Vector3 initialTarget = camera.target();
    const Vector3 initialPosition = camera.position();
    camera.moveFree(V3(0, 0, 1), 0.1f, false);
    CHECK(Vector3Distance(initialTarget, camera.target()) > 2.9f);
    CHECK(Vector3Distance(initialPosition, camera.position()) > 2.9f);
    const Vector3 normalTarget = camera.target();
    camera.moveFree(V3(1, 0, 0), 0.1f, true);
    CHECK(Vector3Distance(normalTarget, camera.target()) > 8.9f);
    EditorCamera sixtyFps;
    EditorCamera oneTwentyFps;
    for (int i = 0; i < 60; ++i) sixtyFps.moveFree(V3(1, 0, 1), 1.0f / 60.0f, false);
    for (int i = 0; i < 120; ++i) oneTwentyFps.moveFree(V3(1, 0, 1), 1.0f / 120.0f, false);
    CHECK(NearlyEqual(sixtyFps.target(), oneTwentyFps.target(), 0.001f));
    const Vector3 beforeOrbitTarget = camera.target();
    const Vector3 beforeOrbitPosition = camera.position();
    camera.rotateBy(Vector2{80.0f, 20000.0f});
    CHECK(NearlyEqual(beforeOrbitTarget, camera.target()));
    CHECK(!NearlyEqual(beforeOrbitPosition, camera.position()));
    CHECK(std::fabs(camera.pitch()) < 1.46f);
    camera.zoomBy(500.0f);
    CHECK(camera.distance() >= 2.0f);
    camera.zoomBy(-1000.0f);
    CHECK(camera.distance() <= 2000.0f);
    camera.focusOn(V3(100, 20, -30), 10.0f);
    CHECK(NearlyEqual(camera.target(), V3(100, 20, -30)));
    CHECK(camera.distance() > 20.0f);
    camera.zoomBy(500.0f);
    CHECK(camera.distance() >= 12.49f);
    EndSection();
}

void TestInputRouting() {
    Section("Roteamento: UI, sessoes, navegacao e atalhos");
    using namespace loblox;
    EditorInput input;
    InputContext context;
    context.windowFocused = context.viewportVisible = context.hovered = context.focused = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Viewport);
    input.rightPressed = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Camera);
    context.transforming = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Transform);
    context.hovered = false;
    CHECK(RouteInput(input, context).owner == InputOwner::Transform);
    context.popup = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Cancel);
    context.popup = false;
    context.text = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Cancel);
    context.text = false;
    input.escape = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Cancel);
    input.escape = false;
    context.windowFocused = false;
    CHECK(RouteInput(input, context).owner == InputOwner::Cancel);
    context.windowFocused = true;
    context.transforming = false;
    context.navigating = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Camera);
    CHECK(!RouteInput(input, context).editorKeys);
    context.navigating = false;
    input.rightPressed = false;
    input.rightDown = true;
    context.hovered = true;
    CHECK(RouteInput(input, context).owner == InputOwner::UI);
    input.rightDown = false;
    context.widget = true;
    CHECK(RouteInput(input, context).owner == InputOwner::UI);
    CHECK(!RouteInput(input, context).globalKeys);
    context.widget = false;
    context.viewportVisible = false;
    CHECK(RouteInput(input, context).owner == InputOwner::UI);
    context.navigating = true;
    CHECK(RouteInput(input, context).owner == InputOwner::Cancel);
    EndSection();
}

void TestGizmoHandleDrags() {
    Section("Handles reais: begin/update/cancel em vistas obliquas e axiais");
    using namespace loblox;
    Scene scene;
    Part* part = scene.createPart(PartShape::Block, V3(0, 20, 0), V3(8, 2, 4));
    part->setAnchored(false);
    PhysicsWorld physics;
    physics.attach(scene);
    EditorCamera camera;
    const Rectangle viewport{100, 80, 640, 400};
    const loblox::Transform initial = part->transform();
    for (const Vector2 orbit : {Vector2{0.75f, 0.4f}, Vector2{0, 0},
                                Vector2{PI / 2, 0}, Vector2{0.5f, 1.4f}}) {
        camera.setOrbit(initial.position, 18, orbit.x, orbit.y);
        for (ToolMode mode : {ToolMode::Move, ToolMode::Scale, ToolMode::Rotate}) {
            const int handles = mode == ToolMode::Scale ? 6 : 3;
            for (int index = 0; index < handles; ++index) {
                part->setTransform(initial);
                physics.syncScene(scene);
                Gizmo gizmo;
                TransformSession session(gizmo, physics);
                const auto layout = gizmolayout::Build(*part, mode, camera, viewport, false);
                const Vector2 point = mode == ToolMode::Move ? layout.axisButton[index] :
                    mode == ToolMode::Scale ? layout.scaleHandle[index] : layout.rotateMarker[index];
                GizmoInput input;
                input.mousePosition = {point.x + viewport.x, point.y + viewport.y};
                input.leftPressed = input.leftDown = input.shiftDown = true;
                const auto result = session.update(mode, part, camera, viewport, true, true, input);
                if (!result.started) std::printf("  start failed: mode=%s handle=%d yaw=%.2f pitch=%.2f\n",
                    ToolModeName(mode), index, orbit.x, orbit.y);
                CHECK(result.started);
                CHECK(result.consumed);
                if (!result.started) continue;
                CHECK(physics.isPartEditing(part->id()));
                input.leftPressed = false;
                const Vector2 delta = mode == ToolMode::Rotate ? Vector2{27, 19} :
                    Vector2Scale(layout.axisDirection[mode == ToolMode::Scale ? index / 2 : index], 35);
                input.mousePosition = Vector2Add(input.mousePosition, delta);
                session.update(mode, part, camera, viewport, true, true, input);
                const bool changed = mode == ToolMode::Move ?
                    Vector3Distance(part->position(), initial.position) > 0.01f :
                    mode == ToolMode::Scale ? Vector3Distance(part->size(), initial.size) > 0.01f :
                    std::fabs(Vector4DotProduct(part->rotation(), initial.rotation)) < 0.99999f;
                if (!changed) std::printf("  update failed: mode=%s handle=%d yaw=%.2f pitch=%.2f\n",
                    ToolModeName(mode), index, orbit.x, orbit.y);
                CHECK(changed);
                if (mode != ToolMode::Scale) CHECK(NearlyEqual(part->size(), initial.size));
                if (mode == ToolMode::Rotate) CHECK(NearlyEqual(part->position(), initial.position));
                CHECK(std::isfinite(Vector3Length(part->position())));
                CHECK(std::isfinite(Vector3Length(part->size())));
                session.cancel(part, true);
                CHECK(!physics.isPartEditing(part->id()));
                CHECK(NearlyEqual(part->position(), initial.position));
                CHECK(NearlyEqual(part->size(), initial.size));
            }
        }
    }
    EndSection();
}

void TestPhysicsEditSuspension() {
    Section("Fisica suspensa durante edicao de gizmo");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);
    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 15, 0));
    physics.simulate(scene, 1.0f / 60.0f);
    CHECK(part != nullptr);
    if (part == nullptr) { EndSection(); return; }
    const std::size_t bodyCount = physics.bodyCount();
    physics.setPartEditing(part->id(), true);
    CHECK(physics.isPartEditing(part->id()));
    part->setPosition(loblox::V3(8, 25, 4));
    physics.simulate(scene, 0.1f);
    CHECK_NEAR(part->position().y, 25.0f, 0.001f);
    CHECK_NEAR(physics.bodyPosition(part->id()).y, 25.0f, 0.001f);
    CHECK(physics.bodyCount() == bodyCount);
    physics.setPartEditing(part->id(), false);
    physics.simulate(scene, 0.1f);
    CHECK(!physics.isPartEditing(part->id()));
    CHECK(part->position().y < 25.0f);
    CHECK(physics.bodyCount() == bodyCount);
    EndSection();
}

void TestLuaDeclarativeSceneRoundTrip() {
    Section("Exportar/importar Lua declarativo sem runtime");
    using namespace loblox;
    Scene source;
    source.buildDefaultScene();
    Part* part = source.createPart(PartShape::Block, V3(12, 7, -4), V3(3, 5, 7), "Lua Part");
    CHECK(part != nullptr);
    if (part == nullptr) { EndSection(); return; }
    part->setRotationEulerDegrees(V3(12, 38, -7));
    part->setColor(Color{22, 44, 66, 255});
    part->setMaterial(PartMaterial::Metal);
    part->setAnchored(true);
    part->setCanCollide(false);
    source.lighting()->setSkyColor(Color{15, 30, 45, 255});
    source.lighting()->setAmbientColor(Color{55, 66, 77, 255});
    source.lighting()->setSunDirection(V3(-0.2f, -1.0f, 0.1f));
    source.lighting()->setSunIntensity(1.6f);
    source.lighting()->setBrightness(0.8f);

    const std::string text = scenelua::toLua(source);
    CHECK(text.find("format = \"loblox-scene\"") != std::string::npos);
    CHECK(text.find("Lua Part") != std::string::npos);
    Scene loaded;
    loaded.buildDefaultScene();
    std::string error;
    CHECK(scenelua::fromLua(loaded, text, error));
    CHECK(error.empty());
    CHECK(loaded.parts().size() == source.parts().size());
    const Part* copy = loaded.findPart(part->id());
    CHECK(copy != nullptr);
    if (copy != nullptr) {
        CHECK_STR(copy->name(), "Lua Part");
        CHECK(copy->instanceClass() == InstanceClass::Part);
        CHECK(NearlyEqual(copy->position(), part->position()));
        CHECK(NearlyEqual(copy->size(), part->size()));
        CHECK(NearlyEqual(copy->rotation(), part->rotation(), 0.001f));
        CHECK(copy->color().r == 22 && copy->color().g == 44 && copy->color().b == 66);
        CHECK(copy->material() == PartMaterial::Metal && copy->anchored() && !copy->canCollide());
        CHECK(copy->parent() == loaded.workspace());
    }
    CHECK(loaded.lighting()->skyColor().r == 15);
    CHECK(loaded.lighting()->ambientColor().g == 66);
    CHECK_NEAR(loaded.lighting()->brightness(), 0.8f, 0.001f);
    CHECK_NEAR(loaded.lighting()->sunIntensity(), 1.6f, 0.001f);
    CHECK(loaded.find(source.workspace()->id()) == loaded.workspace());
    CHECK(loaded.findPart(source.parts()[0]->id())->instanceClass() == InstanceClass::Baseplate);
    CHECK(loaded.findPart(source.parts()[1]->id())->instanceClass() == InstanceClass::SpawnLocation);

    const std::size_t count = loaded.parts().size();
    const std::string malicious = text + "os.execute(\\\"touch /tmp/loblox_lua_must_not_run\\\")\n";
    CHECK(!scenelua::fromLua(loaded, malicious, error));
    CHECK(loaded.parts().size() == count);
    std::string unknownClass = "return { format = \"loblox-scene\", version = 1, objects = { { id = 9, class = \"Script\", name = \"x\", parent = 1 } } }";
    CHECK(!scenelua::fromLua(loaded, unknownClass, error));
    CHECK(loaded.parts().size() == count);
    std::string unknownProperty = text;
    const std::string anchorField = "anchored = true";
    const std::size_t fieldPosition = unknownProperty.find(anchorField);
    CHECK(fieldPosition != std::string::npos);
    if (fieldPosition != std::string::npos) unknownProperty.insert(fieldPosition, "script = \"return 1\", ");
    CHECK(!scenelua::fromLua(loaded, unknownProperty, error));
    CHECK(loaded.parts().size() == count);
    std::string invalidNextId = text;
    const std::size_t nextIdPosition = invalidNextId.find("nextId = ");
    CHECK(nextIdPosition != std::string::npos);
    if (nextIdPosition != std::string::npos) {
        const std::size_t valueStart = nextIdPosition + std::string("nextId = ").size();
        const std::size_t valueEnd = invalidNextId.find(',', valueStart);
        invalidNextId.replace(valueStart, valueEnd - valueStart, "-1");
    }
    CHECK(!scenelua::fromLua(loaded, invalidNextId, error));
    CHECK(loaded.parts().size() == count);

    Scene fileLoaded;
    fileLoaded.buildDefaultScene();
    PhysicsWorld physics;
    physics.attach(fileLoaded);
    const auto path = std::filesystem::temp_directory_path() / "loblox_scene_test.lua";
    CHECK(scenelua::save(source, path.string(), error));
    CHECK(!std::filesystem::exists(path.string() + ".tmp"));
    CHECK(!TempFilesWithPrefix("loblox_scene_test.lua.tmp."));
    CHECK(scenelua::load(fileLoaded, path.string(), error));
    physics.rebuildAll(fileLoaded);
    CHECK(physics.bodyCount() == fileLoaded.parts().size());
    CHECK(fileLoaded.parts().size() == source.parts().size());
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    EndSection();
}

void TestProjectRoundTrip() {
    Section("Salvar/carregar JSON");
    CHECK(std::filesystem::path(loblox::DefaultProjectPath()).is_absolute());
    loblox::Scene scene;
    scene.buildDefaultScene();

    loblox::Part* block = scene.createPart(loblox::PartShape::Block, loblox::V3(3, 5, -2),
                                           loblox::V3(6, 2, 4), "MinhaPeca");
    block->setRotationEulerDegrees(loblox::V3(10, 20, 30));
    block->setColor(Color{200, 50, 25, 255});
    block->setMaterial(loblox::PartMaterial::Metal);
    block->setAnchored(false);

    loblox::Part* sphere = scene.createPart(loblox::PartShape::Sphere, loblox::V3(-7, 3, 1),
                                            loblox::V3(3, 3, 3), "Bola");
    sphere->setCanCollide(false);
    sphere->setAnchored(true);
    sphere->setColor(Color{20, 220, 90, 255});
    sphere->setMaterial(loblox::PartMaterial::Neon);

    scene.lighting()->setSkyColor(Color{30, 40, 90, 255});
    scene.lighting()->setSunIntensity(1.7f);
    scene.lighting()->setBrightness(0.8f);

    const std::string path = "/tmp/loblox_test_project.json";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path + ".tmp", ignored);

    std::string error;
    CHECK(loblox::projectio::save(scene, path, error));
    CHECK(error.empty());
    CHECK(std::filesystem::exists(path));
    CHECK(!std::filesystem::exists(path + ".tmp"));
    CHECK(!TempFilesWithPrefix("loblox_test_project.json.tmp."));

    // A failed replace (destination is a directory) must not destroy its contents.
    const auto blockedPath = std::filesystem::temp_directory_path() / "loblox-blocked-save.json";
    const auto markerPath = blockedPath / "keep.txt";
    std::error_code filesystemError;
    std::filesystem::remove_all(blockedPath, filesystemError);
    std::filesystem::create_directories(blockedPath);
    { std::ofstream marker(markerPath); marker << "preserve"; }
    CHECK(!loblox::projectio::save(scene, blockedPath.string(), error));
    CHECK(!error.empty());
    CHECK(!TempFilesWithPrefix("loblox-blocked-save.json.tmp."));
    std::ifstream marker(markerPath);
    std::string markerContents;
    marker >> markerContents;
    CHECK_STR(markerContents, "preserve");
    std::filesystem::remove_all(blockedPath, filesystemError);
    CHECK(!loblox::projectio::save(
        scene, "/proc/loblox-denied-save/project.json", error));
    CHECK(!error.empty());

    std::ifstream in(path);
    std::ostringstream text;
    text << in.rdbuf();
    CHECK(text.str().find("\"format\": \"loblox-project\"") != std::string::npos);
    CHECK(text.str().find("\"version\": 1") != std::string::npos);

    loblox::Scene loaded;
    CHECK(loblox::projectio::load(loaded, path, error));
    CHECK(error.empty());
    CHECK(loaded.parts().size() == scene.parts().size());
    CHECK(loaded.objects().size() == scene.objects().size());
    CHECK_STR(loaded.workspace()->name(), scene.workspace()->name());
    CHECK_STR(loaded.lighting()->name(), scene.lighting()->name());

    loblox::Part* loadedBlock = loaded.findPart(block->id());
    CHECK(loadedBlock != nullptr);
    if (loadedBlock != nullptr) {
        CHECK_STR(loadedBlock->name(), "MinhaPeca");
        CHECK(loadedBlock->shape() == loblox::PartShape::Block);
        CHECK_NEAR(loadedBlock->position().x, 3.0f, 0.001f);
        CHECK_NEAR(loadedBlock->position().y, 5.0f, 0.001f);
        CHECK_NEAR(loadedBlock->position().z, -2.0f, 0.001f);
        CHECK_NEAR(loadedBlock->size().x, 6.0f, 0.001f);
        CHECK_NEAR(loadedBlock->size().z, 4.0f, 0.001f);
        CHECK_NEAR(loadedBlock->rotationEulerDegrees().y, 20.0f, 0.01f);
        CHECK_NEAR(static_cast<float>(loadedBlock->color().r), 200.0f, 0.5f);
        CHECK(loadedBlock->material() == loblox::PartMaterial::Metal);
        CHECK(!loadedBlock->anchored());
        CHECK(loadedBlock->canCollide());
        CHECK(loadedBlock->parent() == loaded.workspace());
    }

    loblox::Part* loadedSphere = loaded.findPart(sphere->id());
    CHECK(loadedSphere != nullptr);
    if (loadedSphere != nullptr) {
        CHECK(loadedSphere->shape() == loblox::PartShape::Sphere);
        CHECK(!loadedSphere->canCollide());
        CHECK(loadedSphere->material() == loblox::PartMaterial::Neon);
    }

    CHECK_NEAR(static_cast<float>(loaded.lighting()->skyColor().b), 90.0f, 0.5f);
    CHECK_NEAR(loaded.lighting()->sunIntensity(), 1.7f, 0.001f);
    CHECK_NEAR(loaded.lighting()->brightness(), 0.8f, 0.001f);

    // Saving the same path again and reopening preserves the modified state.
    loadedBlock = loaded.findPart(block->id());
    CHECK(loadedBlock != nullptr);
    if (loadedBlock != nullptr) {
        loadedBlock->setPosition(loblox::V3(12, 7, -4));
        loadedBlock->setRotationEulerDegrees(loblox::V3(-15, 65, 20));
        loadedBlock->setSize(loblox::V3(8, 3, 5));
        loadedBlock->setColor(Color{12, 34, 56, 255});
        loadedBlock->setMaterial(loblox::PartMaterial::Wood);
        loadedBlock->setAnchored(true);
        loadedBlock->setCanCollide(false);
        CHECK(loblox::projectio::save(loaded, path, error));
        loblox::Scene reopened;
        CHECK(loblox::projectio::load(reopened, path, error));
        const loblox::Part* persisted = reopened.findPart(block->id());
        CHECK(persisted != nullptr);
        if (persisted != nullptr) {
            CHECK(loblox::NearlyEqual(persisted->position(), loblox::V3(12, 7, -4)));
            CHECK(loblox::NearlyEqual(persisted->size(), loblox::V3(8, 3, 5)));
            CHECK(loblox::NearlyEqual(persisted->rotation(), loadedBlock->rotation(), 0.001f));
            CHECK(persisted->color().r == 12 && persisted->color().g == 34 &&
                  persisted->color().b == 56);
            CHECK(persisted->material() == loblox::PartMaterial::Wood);
            CHECK(persisted->anchored() && !persisted->canCollide());
            CHECK(persisted->parent() == reopened.workspace());
        }
        CHECK(reopened.find(block->id()) != nullptr);
        CHECK(reopened.find(sphere->id()) != nullptr);
        // Keep the original round-trip physics assertions independent of the
        // edited state used above to verify a second save.
        loadedBlock->setTransform(block->transform());
        loadedBlock->setAnchored(false);
        loadedBlock->setCanCollide(true);
    }

    // A cena recarregada precisa reconstruir corpos fisicos coerentes.
    loblox::PhysicsWorld physics;
    physics.attach(loaded);
    CHECK(physics.bodyCount() == loaded.parts().size());
    loblox::Part* falling = loaded.findPart(block->id());
    CHECK(falling != nullptr);
    if (falling != nullptr) {
        CHECK(physics.isDynamic(falling->id()));
        Simulate(physics, loaded, 2.5f);
        CHECK(falling->position().y < 5.0f);
        CHECK(falling->position().y > -5.0f);
    }

    // Ids continuam unicos depois de carregar.
    loblox::Part* extra = loaded.createPart(loblox::PartShape::Block, loblox::V3(0, 10, 0));
    CHECK(extra != nullptr);
    if (extra != nullptr) {
        CHECK(loaded.findPart(extra->id()) == extra);
        CHECK(extra->id() != block->id());
        CHECK(extra->id() != sphere->id());
    }
    EndSection();
}

void TestProjectValidation() {
    Section("Validacao de projetos invalidos");
    loblox::Scene scene;
    scene.buildDefaultScene();
    const size_t originalParts = scene.parts().size();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    std::string error;
    CHECK(!loblox::projectio::fromJson(scene, "{ isto nao e json", error));
    CHECK(!error.empty());
    CHECK(scene.parts().size() == originalParts);

    CHECK(!loblox::projectio::fromJson(scene, "{\"format\":\"loblox-project\",\"version\":99,\"objects\":[]}", error));
    CHECK(error.find("versao") != std::string::npos);

    CHECK(!loblox::projectio::fromJson(scene,
                                      "{\"format\":\"loblox-project\",\"version\":1,"
                                      "\"objects\":[{\"id\":1,\"class\":\"Workspace\","
                                      "\"name\":\"Workspace\",\"parent\":0}]}",
                                      error));
    CHECK(error.find("Lighting") != std::string::npos);

    // Duas Parts com o mesmo id.
    const std::string duplicateId =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":1,"class":"Workspace","name":"Workspace","parent":0},
        {"id":2,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":3,"class":"Part","name":"A","parent":1,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true},
        {"id":3,"class":"Part","name":"B","parent":1,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, duplicateId, error));
    CHECK(error.find("duplicado") != std::string::npos);

    // Tamanho invalido.
    const std::string badSize =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":1,"class":"Workspace","name":"Workspace","parent":0},
        {"id":2,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":3,"class":"Part","name":"A","parent":1,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,-4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, badSize, error));
    CHECK(error.find("tamanho") != std::string::npos || error.find("size") != std::string::npos);

    // Part fora do Workspace (parent aponta para Lighting).
    const std::string badParent =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":1,"class":"Workspace","name":"Workspace","parent":0},
        {"id":2,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":3,"class":"Part","name":"A","parent":2,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, badParent, error));

    // Id de Part conflitando com um servico da cena atual: rejeitar antes de
    // limpar a cena (o arquivo tem ids unicos, mas usa Workspace/Lighting 7/8).
    const std::string serviceConflict =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":7,"class":"Workspace","name":"Workspace","parent":0},
        {"id":8,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":1,"class":"Part","name":"Conflito","parent":7,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, serviceConflict, error));
    CHECK(error.find("servico") != std::string::npos);

    // Falha nao deve deixar a cena vazia nem corpos orfaos.
    CHECK(scene.parts().size() == originalParts);
    CHECK(physics.bodyCount() == originalParts);

    // Arquivo inexistente.
    CHECK(!loblox::projectio::load(scene, "/tmp/nao_existe_loblox.json", error));
    CHECK(!error.empty());
    EndSection();
}


void TestInvariantsAndDiagnostics() {
    Section("Invariantes, saneamento de NaN e diagnostico JSONL");
    using namespace loblox;
    std::string error;
    Scene scene;
    scene.buildDefaultScene();
    CHECK(scene.validate(error));

    Part* part = scene.parts().front();
    const Vector3 originalPosition = part->position();
    const Vector3 originalSize = part->size();
    const Quaternion originalRotation = part->rotation();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    part->setPosition(Vector3{nan, 2.0f, 3.0f});
    CHECK(NearlyEqual(part->position(), originalPosition, 0.001f));
    part->setSize(Vector3{nan, 2.0f, 3.0f});
    CHECK(NearlyEqual(part->size(), originalSize, 0.001f));
    part->setRotation(Quaternion{nan, 0.0f, 0.0f, 1.0f});
    CHECK(NearlyEqual(part->rotation().x, originalRotation.x, 0.001f));
    CHECK(NearlyEqual(part->rotation().w, originalRotation.w, 0.001f));
    part->setTransform(loblox::Transform{originalPosition, originalRotation,
                                 Vector3{originalSize.x, nan, originalSize.z}});
    CHECK(NearlyEqual(part->position(), originalPosition, 0.001f));
    CHECK(NearlyEqual(part->size(), originalSize, 0.001f));
    CHECK(scene.validate(error));

    // Ids continuam unicos mesmo quando o chamador tenta forcar um conflito.
    CHECK(scene.createPart(PartShape::Block, V3(0, 2, 0), V3(2, 2, 2),
                           "Conflito", part->id()) == nullptr);
    Part* deletable = scene.createPart(PartShape::Block, V3(0, 2, 0), V3(2, 2, 2),
                                        "Deletable");
    CHECK(deletable != nullptr);
    scene.select(deletable);
    CHECK(scene.remove(deletable));
    CHECK(scene.selection() == nullptr);
    CHECK(scene.validate(error));

    // Corpo ausente e corpo orfao sao detectados; sync/remocao restauram a
    // correspondencia exata.
    Scene physicsScene;
    Part* first = physicsScene.createPart(PartShape::Block, V3(0, 4, 0), V3(4, 4, 4), "First");
    PhysicsWorld physics;
    physics.attach(physicsScene);
    CHECK(physics.validate(physicsScene, error));
    Part* late = physicsScene.createPart(PartShape::Block, V3(6, 4, 0), V3(2, 2, 2), "Late");
    CHECK(!physics.validate(physicsScene, error));
    CHECK(error.find("sem corpo") != std::string::npos);
    physics.simulate(physicsScene, 0.0f);
    CHECK(physics.validate(physicsScene, error));
    const InstanceId lateId = late->id();
    physics.setPartEditing(lateId, true);
    CHECK(physics.isPartEditing(lateId));
    CHECK(physics.validate(physicsScene, error));
    CHECK(physicsScene.remove(late));
    CHECK(!physics.isPartEditing(lateId));
    CHECK(physics.validate(physicsScene, error));
    CHECK(physicsScene.remove(first));
    CHECK(physics.validate(physicsScene, error));

    const auto logPath =
        std::filesystem::temp_directory_path() / "loblox_diagnostics_test.jsonl";
    std::error_code ignored;
    std::filesystem::remove(logPath, ignored);
    diag::configure(logPath.string());
    diag::Event event;
    event.level = diag::Level::Error;
    event.subsystem = "test";
    event.name = "invariant_probe";
    event.correlationId = diag::newCorrelationId();
    event.entityId = 42;
    event.file = "projeto.json";
    event.durationMs = 12.5;
    event.result = "error";
    event.error = "falha controlada";
    event.context = {{"check", "jsonl"}, {"count", 3}};
    diag::log(event);
    CHECK(std::filesystem::exists(logPath));
    std::ifstream input(logPath);
    std::string line;
    CHECK(static_cast<bool>(std::getline(input, line)));
    bool parsed = false;
    try {
        const nlohmann::json record = nlohmann::json::parse(line);
        parsed = true;
        CHECK(record.value("level", "") == "error");
        CHECK(record.value("subsystem", "") == "test");
        CHECK(record.value("event", "") == "invariant_probe");
        CHECK(record.value("correlation_id", "") == event.correlationId);
        CHECK(record.value("entity_id", 0) == 42);
        CHECK(record.value("file", "") == "projeto.json");
        CHECK_NEAR(record.value("duration_ms", 0.0), 12.5, 0.001);
        CHECK(record.value("result", "") == "error");
        CHECK(record.value("error", "") == "falha controlada");
        CHECK(record.contains("timestamp"));
        CHECK(record["context"].value("check", "") == "jsonl");
    } catch (const std::exception&) {
        CHECK(parsed);
    }
    std::filesystem::remove(logPath, ignored);
    EndSection();
}

void TestTransactionalLoad() {
    Section("Carregamento transacional e sincronizacao da fisica");
    using namespace loblox;
    Scene source;
    source.createPart(PartShape::Block, V3(1, 6, 2), V3(5, 2, 3), "Candidato");
    const auto path = std::filesystem::temp_directory_path() / "loblox_transactional_load.json";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::string error;
    CHECK(projectio::save(source, path.string(), error));

    Scene target;
    target.buildDefaultScene();
    PhysicsWorld physics;
    physics.attach(target);
    target.select(target.workspace());
    const std::size_t oldBodies = physics.bodyCount();
    CHECK(oldBodies > 0);
    CHECK(projectio::load(target, path.string(), error));
    CHECK(error.empty());
    CHECK(target.parts().size() == 1);
    CHECK(target.selection() == nullptr);
    // A troca avisa a fisica sobre as Parts antigas; o chamador reconstroi os
    // corpos da cena nova (EditorUI faz isso via onSceneLoaded).
    CHECK(physics.bodyCount() == 0);
    physics.rebuildAll(target);
    CHECK(physics.bodyCount() == target.parts().size());
    CHECK(target.findPart(target.parts().front()->id()) != nullptr);

    // Uma falha posterior nao pode desmontar a cena valida que acabou de
    // entrar nem remover os corpos correspondentes.
    CHECK(!projectio::fromJson(target, "{\"format\":\"loblox-project\"", error));
    CHECK(!error.empty());
    CHECK(target.parts().size() == 1);
    CHECK(physics.bodyCount() == 1);
    std::filesystem::remove(path, ignored);
    EndSection();
}

void TestBaseplatePhysics() {
    Section("Baseplate na fisica e protecao");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Baseplate* baseplate = nullptr;
    for (loblox::Part* part : scene.parts()) {
        if (part->instanceClass() == loblox::InstanceClass::Baseplate) {
            baseplate = static_cast<loblox::Baseplate*>(part);
        }
    }
    CHECK(baseplate != nullptr);
    if (baseplate == nullptr) {
        EndSection();
        return;
    }
    Simulate(physics, scene, 1.0f);
    CHECK_NEAR(baseplate->position().y, -0.5f, 0.001f);

    // Nao pode ser desancorada nem perder colisao: a API protege alem da UI.
    baseplate->setAnchored(false);
    baseplate->setCanCollide(false);
    CHECK(baseplate->anchored());
    CHECK(baseplate->canCollide());
    physics.syncScene(scene);
    Simulate(physics, scene, 1.0f);
    CHECK_NEAR(baseplate->position().y, -0.5f, 0.001f);

    // O editor pode mover a baseplate manualmente (ancorada continua estatica).
    baseplate->setPosition(loblox::V3(0, -2.0f, 0));
    physics.applyChanges(baseplate);
    Simulate(physics, scene, 0.5f);
    CHECK_NEAR(baseplate->position().y, -2.0f, 0.001f);

    // Exclusao protegida mesmo com listener de fisica registrado.
    CHECK(!scene.remove(baseplate));
    CHECK(physics.hasBody(baseplate->id()));
    EndSection();
}

void TestClearObjects() {
    Section("Limpeza de cena (carregar projeto)");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);
    scene.createPart(loblox::PartShape::Block, loblox::V3(0, 4, 0));
    scene.createPart(loblox::PartShape::Sphere, loblox::V3(6, 4, 0));
    // O sync cria corpos que faltam (partes criadas pela UI/loader).
    physics.simulate(scene, 0.0f);
    CHECK(physics.bodyCount() == 4);

    scene.clearObjects();
    CHECK(scene.parts().empty());
    CHECK(scene.workspace()->children().empty());
    CHECK(physics.bodyCount() == 0);
    CHECK(scene.selection() == nullptr);

    // A cena continua utilizavel.
    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 2, 0));
    CHECK(part != nullptr);
    physics.addPart(part);
    CHECK(physics.bodyCount() == 1);
    EndSection();
}

}  // namespace

int main() {
    std::printf("loblox tests\n============\n");
    TestDefaultScene();
    TestCreateAndRemove();
    TestRemovalListener();
    TestAnchoredAndDynamic();
    TestCanCollide();
    TestTransformSyncAndRaycast();
    TestResizeKeepsSingleBody();
    TestGizmoMath();
    TestGizmoLayout();
    TestGizmoSessionLifecycle();
    TestEditorCameraMath();
    TestInputRouting();
    TestGizmoHandleDrags();
    TestPhysicsEditSuspension();
    TestLuaDeclarativeSceneRoundTrip();
    TestProjectRoundTrip();
    TestProjectValidation();
    TestInvariantsAndDiagnostics();
    TestTransactionalLoad();
    TestBaseplatePhysics();
    TestClearObjects();

    std::printf("\n%d checagens, %d falhas\n", gChecks, gFailures);
    if (gFailures != 0) {
        std::printf("RESULTADO: FALHOU\n");
        return 1;
    }
    std::printf("RESULTADO: OK\n");
    return 0;
}

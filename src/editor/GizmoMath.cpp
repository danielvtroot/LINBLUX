#include "editor/GizmoMath.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace loblox::gizmomath {

int HandleAxis(HandleId id) {
    switch (id) {
        case HandleId::XAxis: case HandleId::XPositive: case HandleId::XNegative:
        case HandleId::RotateX: return 0;
        case HandleId::YAxis: case HandleId::YPositive: case HandleId::YNegative:
        case HandleId::RotateY: return 1;
        case HandleId::ZAxis: case HandleId::ZPositive: case HandleId::ZNegative:
        case HandleId::RotateZ: return 2;
        default: return -1;
    }
}

int HandleSide(HandleId id) {
    switch (id) {
        case HandleId::XPositive: case HandleId::YPositive: case HandleId::ZPositive: return 1;
        case HandleId::XNegative: case HandleId::YNegative: case HandleId::ZNegative: return -1;
        default: return 0;
    }
}

bool IsScaleFace(HandleId id) { return HandleSide(id) != 0 && HandleAxis(id) >= 0; }

HandleId ScaleFaceHandleId(int axisIndex, int side) {
    if (axisIndex < 0 || axisIndex > 2 || side == 0) return HandleId::None;
    static constexpr HandleId ids[3][2] = {
        {HandleId::XNegative, HandleId::XPositive},
        {HandleId::YNegative, HandleId::YPositive},
        {HandleId::ZNegative, HandleId::ZPositive}};
    return ids[axisIndex][side > 0 ? 1 : 0];
}

HandleId PickScaleFaceHandle(const std::array<Vector2, 6>& centers, Vector2 mouse,
                             float halfWidth, float halfHeight) {
    HandleId picked = HandleId::None;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        for (int side : {-1, 1}) {
            const int index = axis * 2 + (side > 0 ? 1 : 0);
            const Vector2 center = centers[static_cast<std::size_t>(index)];
            const float dx = std::fabs(mouse.x - center.x);
            const float dy = std::fabs(mouse.y - center.y);
            if (dx > halfWidth || dy > halfHeight) continue;
            const float distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                picked = ScaleFaceHandleId(axis, side);
            }
        }
    }
    return picked;
}

Vector3 Axis(int index, Quaternion rotation, bool local) {
    const Vector3 basis = index == 0 ? Vector3{1, 0, 0}
                         : index == 1 ? Vector3{0, 1, 0}
                                      : Vector3{0, 0, 1};
    return local ? Vector3Normalize(Vector3RotateByQuaternion(basis, rotation)) : basis;
}

float SnapDelta(float delta, float increment) {
    if (!std::isfinite(delta)) return 0.0f;
    if (!std::isfinite(increment) || increment <= 0.0f) return delta;
    return std::round(delta / increment) * increment;
}

Vector3 MoveAxis(const TransformSnapshot& start, Vector3 axis, float delta,
                 float snapIncrement) {
    const float snapped = SnapDelta(delta, snapIncrement);
    return Vector3Add(start.position, Vector3Scale(Vector3Normalize(axis), snapped));
}

Vector3 MovePlane(const TransformSnapshot& start, Vector3 axisA, Vector3 axisB,
                  Vector3 worldDelta, float snapIncrement) {
    axisA = Vector3Normalize(axisA);
    axisB = Vector3Normalize(axisB);
    const float a = SnapDelta(Vector3DotProduct(worldDelta, axisA), snapIncrement);
    const float b = SnapDelta(Vector3DotProduct(worldDelta, axisB), snapIncrement);
    return Vector3Add(start.position,
                      Vector3Add(Vector3Scale(axisA, a), Vector3Scale(axisB, b)));
}

TransformSnapshot ScaleFace(const TransformSnapshot& start, Vector3 axis, int side,
                            float delta, float snapIncrement, float minimumSize) {
    TransformSnapshot result = start;
    if (!std::isfinite(delta) || !std::isfinite(minimumSize) || minimumSize <= 0.0f ||
        Vector3LengthSqr(axis) < 1e-8f) return result;
    axis = Vector3Normalize(axis);
    side = side < 0 ? -1 : 1;
    const Vector3 localAxes[3] = {Axis(0, start.rotation, true), Axis(1, start.rotation, true),
                                  Axis(2, start.rotation, true)};
    int component = 0;
    float bestDot = -1.0f;
    for (int i = 0; i < 3; ++i) {
        const float dot = std::fabs(Vector3DotProduct(axis, localAxes[i]));
        if (dot > bestDot) { bestDot = dot; component = i; }
    }
    const float signedDelta = SnapDelta(delta, snapIncrement) * static_cast<float>(side);
    float& sizeComponent = component == 0 ? result.size.x
                           : component == 1 ? result.size.y : result.size.z;
    const float startComponent = component == 0 ? start.size.x
                                 : component == 1 ? start.size.y : start.size.z;
    sizeComponent = std::max(minimumSize, startComponent + signedDelta);
    const float actualDelta = sizeComponent - startComponent;
    result.position = Vector3Add(start.position,
                                 Vector3Scale(localAxes[component],
                                              0.5f * static_cast<float>(side) * actualDelta));
    return result;
}

TransformSnapshot ScaleFaceAlongAxis(const TransformSnapshot& start, int axisIndex, int side,
                                     bool localSpace, float delta, float snapIncrement,
                                     float minimumSize) {
    if (axisIndex < 0 || axisIndex > 2 || !std::isfinite(delta)) return start;
    if (localSpace) {
        return ScaleFace(start, Axis(axisIndex, start.rotation, true), side, delta,
                         snapIncrement, minimumSize);
    }

    const Vector3 worldAxis = Axis(axisIndex, start.rotation, false);
    int component = 0;
    float alignment = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float candidate = Vector3DotProduct(worldAxis, Axis(i, start.rotation, true));
        if (std::fabs(candidate) > std::fabs(alignment)) {
            alignment = candidate;
            component = i;
        }
    }
    // Keep the sign of the projection: a world-positive axis may point toward
    // the local-negative face. Dropping that sign made half of the handles on
    // rotated Parts shrink when dragged outward. Dividing by the projection
    // also amplified resize; the local face displacement is delta * alignment.
    if (std::fabs(alignment) < 1e-5f) return start;
    const int localSide = (alignment < 0.0f ? -1 : 1) * (side < 0 ? -1 : 1);
    return ScaleFace(start, Axis(component, start.rotation, true), localSide,
                     delta * alignment, snapIncrement, minimumSize);
}

TransformSnapshot ScaleFaceForHandle(const TransformSnapshot& start, HandleId handle,
                                     bool localSpace, float delta, float snapIncrement,
                                     float minimumSize) {
    if (!IsScaleFace(handle)) return start;
    return ScaleFaceAlongAxis(start, HandleAxis(handle), HandleSide(handle), localSpace,
                              delta, snapIncrement, minimumSize);
}

TransformSnapshot ScaleUniformFace(const TransformSnapshot& start, Vector3 axis, int side,
                                   float delta, float snapIncrement, float minimumSize) {
    TransformSnapshot result = start;
    if (!std::isfinite(delta) || !std::isfinite(minimumSize) || minimumSize <= 0.0f ||
        Vector3LengthSqr(axis) < 1e-8f) return result;
    axis = Vector3Normalize(axis);
    side = side < 0 ? -1 : 1;
    const float oldDiameter = std::max(minimumSize, start.size.x);
    const float newDiameter = std::max(minimumSize,
        oldDiameter + static_cast<float>(side) * SnapDelta(delta, snapIncrement));
    result.size = Vector3{newDiameter, newDiameter, newDiameter};
    result.position = Vector3Add(start.position,
        Vector3Scale(axis, static_cast<float>(side) * (newDiameter - oldDiameter) * 0.5f));
    return result;
}

TransformSnapshot ScaleCenteredFace(const TransformSnapshot& start, int axisIndex, int side,
                                    bool localSpace, float delta, float snapIncrement,
                                    float minimumSize) {
    TransformSnapshot result = start;
    if (axisIndex < 0 || axisIndex > 2 || !std::isfinite(delta) ||
        !std::isfinite(minimumSize) || minimumSize <= 0.0f) return result;
    int component = axisIndex;
    int localSide = side < 0 ? -1 : 1;
    if (!localSpace) {
        // Mesma regra de ScaleFaceAlongAxis: em World o eixo escolhido pode
        // apontar para a face local oposta.
        const Vector3 worldAxis = Axis(axisIndex, start.rotation, false);
        float alignment = 0.0f;
        component = 0;
        for (int i = 0; i < 3; ++i) {
            const float candidate = Vector3DotProduct(worldAxis, Axis(i, start.rotation, true));
            if (std::fabs(candidate) > std::fabs(alignment)) {
                alignment = candidate;
                component = i;
            }
        }
        if (std::fabs(alignment) < 1e-5f) return result;
        if (alignment < 0.0f) localSide = -localSide;
    }
    float& size = component == 0 ? result.size.x
                 : component == 1 ? result.size.y : result.size.z;
    const float previous = component == 0 ? start.size.x
                         : component == 1 ? start.size.y : start.size.z;
    const float signedDelta = SnapDelta(delta, snapIncrement) * static_cast<float>(localSide);
    // Cresce (ou encolhe) pelos dois lados: o centro nao se move.
    size = std::max(minimumSize, previous + 2.0f * signedDelta);
    return result;
}

Vector3 ScaleHandleCenter(const TransformSnapshot& start, int axisIndex, int side,
                          bool localSpace, float handleOffset) {
    if (axisIndex < 0 || axisIndex > 2) return start.position;
    const Vector3 axis = Axis(axisIndex, start.rotation, localSpace);
    if (Vector3LengthSqr(axis) < 1e-8f) return start.position;
    const int sign = side < 0 ? -1 : 1;
    const Vector3 localAxes[3] = {Axis(0, start.rotation, true), Axis(1, start.rotation, true),
                                  Axis(2, start.rotation, true)};
    const float half[3] = {0.5f * start.size.x, 0.5f * start.size.y,
                           0.5f * start.size.z};
    float support = 0.0f;
    for (int i = 0; i < 3; ++i)
        support += half[i] * std::fabs(Vector3DotProduct(axis, localAxes[i]));
    return Vector3Add(start.position,
                      Vector3Scale(axis, static_cast<float>(sign) * (support + std::max(0.0f, handleOffset))));
}

float WrappedAngleDelta(float startAngle, float currentAngle) {
    constexpr float twoPi = 2.0f * PI;
    float delta = std::fmod(currentAngle - startAngle + PI, twoPi);
    if (delta < 0.0f) delta += twoPi;
    return delta - PI;
}

void SeparateScreenPoints(Vector2* points, int count, float minimumDistance) {
    if (points == nullptr || count < 2) return;
    const float minimum = std::max(0.0f, minimumDistance);
    if (minimum <= 0.0f) return;
    for (int pass = 0; pass < 96; ++pass) {
        bool changed = false;
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                Vector2 delta{points[j].x - points[i].x, points[j].y - points[i].y};
                float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (distance >= minimum) continue;
                if (distance < 1e-3f) {
                    const float angle = static_cast<float>(i * 13 + j * 7) * 0.6180339f;
                    delta = Vector2{std::cos(angle), std::sin(angle)};
                    distance = 1.0f;
                }
                const float push = (minimum - distance) * 0.5f / distance;
                delta.x *= push;
                delta.y *= push;
                points[i].x -= delta.x;
                points[i].y -= delta.y;
                points[j].x += delta.x;
                points[j].y += delta.y;
                changed = true;
            }
        }
        if (!changed) break;
    }
}

std::array<Vector2, 6> SeparateScreenHandles(const std::array<Vector2, 6>& input,
                                             float minimumDistance) {
    std::array<Vector2, 6> result = input;
    SeparateScreenPoints(result.data(), static_cast<int>(result.size()), minimumDistance);
    return result;
}

Quaternion Rotate(const TransformSnapshot& start, Vector3 axis, float radians,
                  float snapDegrees) {
    if (Vector3LengthSqr(axis) < 1e-8f || !std::isfinite(radians)) return start.rotation;
    const float increment = snapDegrees > 0.0f ? snapDegrees * DEG2RAD : 0.0f;
    const float angle = SnapDelta(radians, increment);
    const Quaternion delta = QuaternionFromAxisAngle(Vector3Normalize(axis), angle);
    return CanonicalQuaternion(QuaternionNormalize(QuaternionMultiply(delta, start.rotation)));
}

}  // namespace loblox::gizmomath

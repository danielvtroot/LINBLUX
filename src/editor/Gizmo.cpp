#include "editor/Gizmo.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "rendering/EditorCamera.h"

namespace loblox {

namespace {

struct AxisDef {
    Vector3 direction;
    Color color;
};

const AxisDef kAxes[3] = {
    {{1.0f, 0.0f, 0.0f}, Color{235, 80, 80, 255}},
    {{0.0f, 1.0f, 0.0f}, Color{90, 220, 110, 255}},
    {{0.0f, 0.0f, 1.0f}, Color{90, 150, 255, 255}},
};

const Color kActiveColor{255, 215, 60, 255};
constexpr float kGizmoPixelRadius = 11.0f;

float AxisLength(const Part& part) {
    const Vector3 size = part.size();
    const float largest = std::max(size.x, std::max(size.y, size.z));
    return std::clamp(largest * 0.85f + 2.2f, 2.6f, 14.0f);
}

Vector2 WorldToScreenGlobal(const EditorCamera& camera, Rectangle viewport, Vector3 world) {
    const Vector2 local = camera.project(world, Vector2{viewport.width, viewport.height});
    return Vector2{viewport.x + local.x, viewport.y + local.y};
}

float DistanceToSegment(Vector2 point, Vector2 a, Vector2 b) {
    const Vector2 ab{b.x - a.x, b.y - a.y};
    const Vector2 ap{point.x - a.x, point.y - a.y};
    const float lengthSquared = ab.x * ab.x + ab.y * ab.y;
    float t = 0.0f;
    if (lengthSquared > 1e-6f) {
        t = std::clamp((ap.x * ab.x + ap.y * ab.y) / lengthSquared, 0.0f, 1.0f);
    }
    const Vector2 closest{a.x + ab.x * t, a.y + ab.y * t};
    return std::sqrt((point.x - closest.x) * (point.x - closest.x) +
                     (point.y - closest.y) * (point.y - closest.y));
}

// Parametro t do ponto mais proximo da linha (origin + t*axis) ao raio.
bool ClosestParamOnAxis(Vector3 rayOrigin, Vector3 rayDirection, Vector3 axisOrigin,
                        Vector3 axisDirection, float& outT) {
    const Vector3 w0 = Vector3Subtract(rayOrigin, axisOrigin);
    const float a = Vector3DotProduct(rayDirection, rayDirection);
    const float b = Vector3DotProduct(rayDirection, axisDirection);
    const float c = Vector3DotProduct(axisDirection, axisDirection);
    const float d = Vector3DotProduct(rayDirection, w0);
    const float e = Vector3DotProduct(axisDirection, w0);
    const float denominator = a * c - b * b;
    if (std::fabs(denominator) < 1e-5f) {
        return false;
    }
    outT = (a * e - b * d) / denominator;
    return true;
}

bool RayPlane(Vector3 rayOrigin, Vector3 rayDirection, Vector3 planePoint, Vector3 planeNormal,
              Vector3& hit) {
    const float denominator = Vector3DotProduct(rayDirection, planeNormal);
    if (std::fabs(denominator) < 1e-5f) {
        return false;
    }
    const float t = Vector3DotProduct(Vector3Subtract(planePoint, rayOrigin), planeNormal) /
                    denominator;
    if (t < 0.0f) {
        return false;
    }
    hit = Vector3Add(rayOrigin, Vector3Scale(rayDirection, t));
    return true;
}

void PerpendicularBasis(Vector3 axis, Vector3& u, Vector3& v) {
    if (std::fabs(axis.y) < 0.9f) {
        u = Vector3Normalize(Vector3CrossProduct(axis, Vector3{0, 1, 0}));
    } else {
        u = Vector3Normalize(Vector3CrossProduct(axis, Vector3{1, 0, 0}));
    }
    v = Vector3Normalize(Vector3CrossProduct(axis, u));
}

float RingDistance(const EditorCamera& camera, Rectangle viewport, Vector3 center, Vector3 axis,
                   float radius, Vector2 mouse) {
    Vector3 u;
    Vector3 v;
    PerpendicularBasis(axis, u, v);
    constexpr int segments = 48;
    float best = 1e9f;
    Vector2 previous = WorldToScreenGlobal(
        camera, viewport,
        Vector3Add(center, Vector3Add(Vector3Scale(u, radius), Vector3Scale(v, 0.0f))));
    for (int i = 1; i <= segments; ++i) {
        const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        const Vector3 point =
            Vector3Add(center, Vector3Add(Vector3Scale(u, std::cos(angle) * radius),
                                          Vector3Scale(v, std::sin(angle) * radius)));
        const Vector2 screen = WorldToScreenGlobal(camera, viewport, point);
        best = std::min(best, DistanceToSegment(mouse, previous, screen));
        previous = screen;
    }
    return best;
}

float WrapAngle(float angle) {
    while (angle > PI) {
        angle -= 2.0f * PI;
    }
    while (angle < -PI) {
        angle += 2.0f * PI;
    }
    return angle;
}

}  // namespace

void Gizmo::cancel() {
    dragging_ = false;
    activeAxis_ = -1;
}

bool Gizmo::update(ToolMode mode, Part* part, const EditorCamera& camera, Rectangle viewport,
                   bool hovered, bool allowInput) {
    if (mode == ToolMode::Select) {
        cancel();
        return false;
    }
    const bool leftDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool leftPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool leftReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    const Vector2 mouse = GetMousePosition();
    const Vector2 localMouse{mouse.x - viewport.x, mouse.y - viewport.y};

    if (!dragging_) {
        if (part == nullptr || !leftPressed || !hovered || !allowInput ||
            viewport.width < 8.0f || viewport.height < 8.0f) {
            return false;
        }

        const Ray ray = camera.rayFromScreen(localMouse, Vector2{viewport.width, viewport.height});
        const Vector3 center = part->position();

        if (mode == ToolMode::Rotate) {
            const float radius =
                std::clamp(AxisLength(*part) * 0.85f, 1.8f, 12.0f);
            int bestAxis = -1;
            float bestDistance = kGizmoPixelRadius;
            for (int i = 0; i < 3; ++i) {
                const float distance = RingDistance(camera, viewport, center,
                                                    kAxes[i].direction, radius, mouse);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestAxis = i;
                }
            }
            if (bestAxis < 0) {
                return false;
            }
            activeAxis_ = bestAxis;
            dragging_ = true;
            startRotation_ = part->rotation();

            Vector3 hit;
            if (!RayPlane(ray.position, ray.direction, center, kAxes[bestAxis].direction, hit)) {
                cancel();
                return false;
            }
            Vector3 u;
            Vector3 v;
            PerpendicularBasis(kAxes[bestAxis].direction, u, v);
            const Vector3 offset = Vector3Subtract(hit, center);
            startAngle_ = std::atan2(Vector3DotProduct(offset, u), Vector3DotProduct(offset, v));
            return true;
        }

        const float length = AxisLength(*part);
        int bestAxis = -1;
        float bestDistance = kGizmoPixelRadius;
        for (int i = 0; i < 3; ++i) {
            const Vector2 start = WorldToScreenGlobal(camera, viewport, center);
            const Vector2 end = WorldToScreenGlobal(
                camera, viewport, Vector3Add(center, Vector3Scale(kAxes[i].direction, length)));
            const float distance = DistanceToSegment(mouse, start, end);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestAxis = i;
            }
        }
        if (bestAxis < 0) {
            return false;
        }

        float parameter = 0.0f;
        if (!ClosestParamOnAxis(ray.position, ray.direction, center,
                                kAxes[bestAxis].direction, parameter)) {
            return false;
        }

        activeAxis_ = bestAxis;
        dragging_ = true;
        startPosition_ = part->position();
        startSize_ = part->size();
        startRotation_ = part->rotation();
        startParam_ = parameter;
        return true;
    }

    // Arrasto em andamento.
    if (!leftDown) {
        (void)leftReleased;
        cancel();
        return false;
    }
    if (part == nullptr) {
        cancel();
        return false;
    }

    const Ray ray = camera.rayFromScreen(localMouse, Vector2{viewport.width, viewport.height});
    const Vector3 center = startPosition_;
    const Vector3 axis = kAxes[activeAxis_].direction;

    if (mode == ToolMode::Rotate) {
        Vector3 hit;
        if (!RayPlane(ray.position, ray.direction, center, axis, hit)) {
            return true;
        }
        Vector3 u;
        Vector3 v;
        PerpendicularBasis(axis, u, v);
        const Vector3 offset = Vector3Subtract(hit, center);
        const float angle = std::atan2(Vector3DotProduct(offset, u), Vector3DotProduct(offset, v));
        const float delta = WrapAngle(angle - startAngle_);
        const Quaternion rotation = QuaternionFromAxisAngle(axis, delta);
        part->setRotation(QuaternionNormalize(QuaternionMultiply(rotation, startRotation_)));
        return true;
    }

    float parameter = 0.0f;
    if (!ClosestParamOnAxis(ray.position, ray.direction, center, axis, parameter)) {
        return true;
    }
    const float delta = parameter - startParam_;

    if (mode == ToolMode::Move) {
        part->setPosition(Vector3Add(startPosition_, Vector3Scale(axis, delta)));
    } else if (mode == ToolMode::Scale) {
        if (part->shape() == PartShape::Sphere) {
            const float diameter = std::max(kMinPartSize, startSize_.x + 2.0f * delta);
            part->setSize(Vector3{diameter, diameter, diameter});
        } else {
            Vector3 size = startSize_;
            if (std::fabs(axis.x) > 0.5f) {
                size.x = startSize_.x + 2.0f * delta;
            } else if (std::fabs(axis.y) > 0.5f) {
                size.y = startSize_.y + 2.0f * delta;
            } else {
                size.z = startSize_.z + 2.0f * delta;
            }
            part->setSize(size);
        }
    }
    return true;
}

void Gizmo::draw(const Part& part, ToolMode mode) const {
    if (mode == ToolMode::Select) {
        return;
    }
    const Vector3 center = part.position();
    const float length = AxisLength(part);

    rlDisableDepthTest();

    if (mode == ToolMode::Rotate) {
        const float radius = std::clamp(length * 0.85f, 1.8f, 12.0f);
        constexpr int segments = 48;
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            Vector3 u;
            Vector3 v;
            PerpendicularBasis(kAxes[axisIndex].direction, u, v);
            const Color color = activeAxis_ == axisIndex ? kActiveColor : kAxes[axisIndex].color;
            Vector3 previous = Vector3Add(center, Vector3Scale(u, radius));
            for (int i = 1; i <= segments; ++i) {
                const float angle = 2.0f * PI * static_cast<float>(i) /
                                    static_cast<float>(segments);
                const Vector3 point =
                    Vector3Add(center, Vector3Add(Vector3Scale(u, std::cos(angle) * radius),
                                                  Vector3Scale(v, std::sin(angle) * radius)));
                DrawLine3D(previous, point, color);
                previous = point;
            }
        }
        rlEnableDepthTest();
        return;
    }

    const float thickness = mode == ToolMode::Scale ? 0.11f : 0.08f;
    for (int i = 0; i < 3; ++i) {
        const Vector3 end = Vector3Add(center, Vector3Scale(kAxes[i].direction, length));
        const Color color = activeAxis_ == i ? kActiveColor : kAxes[i].color;
        DrawCylinderEx(center, end, thickness, thickness, 6, color);
        if (mode == ToolMode::Move) {
            const Vector3 tip = Vector3Add(end, Vector3Scale(kAxes[i].direction, 0.75f));
            DrawCylinderEx(end, tip, 0.24f, 0.0f, 8, color);
        } else {
            DrawCubeV(end, Vector3{0.55f, 0.55f, 0.55f}, color);
            DrawCubeWiresV(end, Vector3{0.56f, 0.56f, 0.56f}, Color{20, 20, 20, 255});
        }
    }
    if (mode == ToolMode::Scale) {
        DrawCubeV(center, Vector3{0.4f, 0.4f, 0.4f}, Color{240, 240, 240, 255});
    }

    rlEnableDepthTest();
}

}  // namespace loblox

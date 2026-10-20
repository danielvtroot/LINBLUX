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
const Vector3 kPlaneAxisA[3] = {{1, 0, 0}, {1, 0, 0}, {0, 1, 0}};
const Vector3 kPlaneAxisB[3] = {{0, 1, 0}, {0, 0, 1}, {0, 0, 1}};
const Vector3 kPlaneNormal[3] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}};
const Color kPlaneColor[3] = {
    {90, 150, 255, 150}, {90, 220, 110, 150}, {235, 80, 80, 150}};
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

bool AxisPlaneParam(const Ray& ray, Vector3 center, Vector3 axis, Vector3 viewForward,
                    float& parameter) {
    // Plane contains the selected axis and faces the camera. This stays stable
    // even when the ray is nearly parallel to the axis, where closest-line
    // dragging becomes undefined.
    Vector3 normal = Vector3Subtract(viewForward, Vector3Scale(axis,
                                      Vector3DotProduct(viewForward, axis)));
    if (Vector3LengthSqr(normal) < 1e-5f) {
        normal = Vector3CrossProduct(axis, Vector3{0.0f, 1.0f, 0.0f});
        if (Vector3LengthSqr(normal) < 1e-5f) {
            normal = Vector3CrossProduct(axis, Vector3{1.0f, 0.0f, 0.0f});
        }
    }
    Vector3 hit;
    if (!RayPlane(ray.position, ray.direction, center, Vector3Normalize(normal), hit)) {
        return false;
    }
    parameter = Vector3DotProduct(Vector3Subtract(hit, center), axis);
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
    activePlane_ = -1;
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
        if (mode == ToolMode::Move) {
            int bestPlane = -1;
            float bestDistance = 10.0f;
            for (int i = 0; i < 3; ++i) {
                const Vector3 planeCenter = Vector3Add(
                    center, Vector3Scale(Vector3Add(kPlaneAxisA[i], kPlaneAxisB[i]),
                                         length * 0.24f));
                const Vector2 screen = WorldToScreenGlobal(camera, viewport, planeCenter);
                const float dx = mouse.x - screen.x;
                const float dy = mouse.y - screen.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestPlane = i;
                }
            }
            if (bestPlane >= 0 && RayPlane(ray.position, ray.direction, center,
                                           kPlaneNormal[bestPlane], startPlaneHit_)) {
                activePlane_ = bestPlane;
                activeAxis_ = -1;
                dragging_ = true;
                startPosition_ = part->position();
                startSize_ = part->size();
                startRotation_ = part->rotation();
                return true;
            }
        }
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
        const Vector3 viewForward = Vector3Normalize(Vector3Subtract(camera.target(), camera.position()));
        if (!AxisPlaneParam(ray, center, kAxes[bestAxis].direction, viewForward, parameter)) {
            return false;
        }

        activeAxis_ = bestAxis;
        activePlane_ = -1;
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
    if (mode == ToolMode::Move && activePlane_ >= 0) {
        Vector3 hit;
        if (!RayPlane(ray.position, ray.direction, center, kPlaneNormal[activePlane_], hit)) {
            return true;
        }
        Vector3 delta = Vector3Subtract(hit, startPlaneHit_);
        delta = Vector3Subtract(
            delta, Vector3Scale(kPlaneNormal[activePlane_],
                                Vector3DotProduct(delta, kPlaneNormal[activePlane_])));
        part->setPosition(Vector3Add(startPosition_, delta));
        return true;
    }
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
    const Vector3 viewForward = Vector3Normalize(Vector3Subtract(camera.target(), camera.position()));
    if (!AxisPlaneParam(ray, center, axis, viewForward, parameter)) {
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

    if (mode == ToolMode::Move) {
        const float offset = length * 0.24f;
        const float half = length * 0.075f;
        for (int i = 0; i < 3; ++i) {
            const Vector3 c = Vector3Add(
                center, Vector3Scale(Vector3Add(kPlaneAxisA[i], kPlaneAxisB[i]), offset));
            const Vector3 a = Vector3Scale(kPlaneAxisA[i], half);
            const Vector3 b = Vector3Scale(kPlaneAxisB[i], half);
            const Vector3 p0 = Vector3Subtract(Vector3Subtract(c, a), b);
            const Vector3 p1 = Vector3Add(Vector3Subtract(c, b), a);
            const Vector3 p2 = Vector3Add(Vector3Add(c, a), b);
            const Vector3 p3 = Vector3Add(Vector3Subtract(c, a), b);
            const Color color = activePlane_ == i ? kActiveColor : kPlaneColor[i];
            DrawTriangle3D(p0, p1, p2, color);
            DrawTriangle3D(p0, p2, p3, color);
            DrawLine3D(p0, p1, Color{235, 238, 245, 230});
            DrawLine3D(p1, p2, Color{235, 238, 245, 230});
            DrawLine3D(p2, p3, Color{235, 238, 245, 230});
            DrawLine3D(p3, p0, Color{235, 238, 245, 230});
        }
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

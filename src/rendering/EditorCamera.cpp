#include "rendering/EditorCamera.h"

#include <algorithm>
#include <cmath>

namespace loblox {

namespace {
constexpr float kMinPitch = -1.45f;
constexpr float kMaxPitch = 1.45f;
constexpr float kMinDistance = 2.0f;
constexpr float kMaxDistance = 600.0f;
}  // namespace

EditorCamera::EditorCamera() = default;

void EditorCamera::reset() {
    target_ = Vector3{0.0f, 2.0f, 0.0f};
    distance_ = 42.0f;
    yaw_ = 0.75f;
    pitch_ = 0.48f;
}

Vector3 EditorCamera::position() const {
    const float cosPitch = std::cos(pitch_);
    return Vector3Add(
        target_, Vector3Scale(Vector3{cosPitch * std::sin(yaw_), std::sin(pitch_),
                                      cosPitch * std::cos(yaw_)},
                              distance_));
}

Camera3D EditorCamera::camera() const {
    Camera3D view{};
    view.position = position();
    view.target = target_;
    view.up = Vector3{0.0f, 1.0f, 0.0f};
    view.fovy = fovDegrees_;
    view.projection = CAMERA_PERSPECTIVE;
    return view;
}

void EditorCamera::focusOn(Vector3 point, float radius) {
    target_ = point;
    distance_ = std::clamp(radius * 3.2f, 4.0f, 400.0f);
}

void EditorCamera::handleInput(Vector2 viewportSize, bool hovered, bool allowMouse) {
    if (!hovered || !allowMouse) {
        return;
    }
    const Vector2 delta = GetMouseDelta();
    const bool rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    const bool middleDown = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (rightDown && !shift) {
        yaw_ -= delta.x * 0.0065f;
        pitch_ = std::clamp(pitch_ + delta.y * 0.0065f, kMinPitch, kMaxPitch);
    } else if (middleDown || (rightDown && shift)) {
        const Vector3 forward =
            Vector3Normalize(Vector3Subtract(target_, position()));
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{0, 1, 0}));
        const Vector3 up = Vector3CrossProduct(right, forward);
        const float scale = distance_ * 0.0016f;
        target_ = Vector3Subtract(target_, Vector3Scale(right, delta.x * scale));
        target_ = Vector3Add(target_, Vector3Scale(up, delta.y * scale));
    }

    const float wheel = GetMouseWheelMove();
    if (std::fabs(wheel) > 0.0001f) {
        distance_ = std::clamp(distance_ * std::pow(0.88f, wheel), kMinDistance, kMaxDistance);
    }

    (void)viewportSize;
}

Ray EditorCamera::rayFromScreen(Vector2 localMouse, Vector2 viewportSize) const {
    const float width = std::max(1.0f, viewportSize.x);
    const float height = std::max(1.0f, viewportSize.y);
    const Vector3 eye = position();
    const Vector3 forward = Vector3Normalize(Vector3Subtract(target_, eye));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{0, 1, 0}));
    const Vector3 up = Vector3CrossProduct(right, forward);

    const float aspect = width / height;
    const float tanHalf = std::tan(fovDegrees_ * DEG2RAD * 0.5f);
    const float ndcX = (2.0f * localMouse.x / width - 1.0f) * aspect * tanHalf;
    const float ndcY = (1.0f - 2.0f * localMouse.y / height) * tanHalf;

    Vector3 direction = Vector3Add(forward, Vector3Scale(right, ndcX));
    direction = Vector3Add(direction, Vector3Scale(up, ndcY));
    Ray ray{};
    ray.position = eye;
    ray.direction = Vector3Normalize(direction);
    return ray;
}

Vector2 EditorCamera::project(Vector3 world, Vector2 viewportSize) const {
    const Vector2 screen = GetWorldToScreenEx(world, camera(), static_cast<int>(viewportSize.x),
                                              static_cast<int>(viewportSize.y));
    return screen;
}

}  // namespace loblox

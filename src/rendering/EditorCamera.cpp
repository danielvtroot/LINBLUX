#include "rendering/EditorCamera.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"

namespace loblox {

namespace {
constexpr float kMinPitch = -1.45f;
constexpr float kMaxPitch = 1.45f;
constexpr float kMinDistance = 2.0f;
constexpr float kMaxDistance = 2000.0f;
}  // namespace

EditorCamera::EditorCamera() = default;

EditorCamera::~EditorCamera() { releaseCapture(); }

void EditorCamera::reset() {
    target_ = Vector3{0.0f, 2.0f, 0.0f};
    distance_ = 42.0f;
    yaw_ = 0.75f;
    pitch_ = 0.48f;
    minimumOrbitDistance_ = kMinDistance;
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

float EditorCamera::depthOf(Vector3 world) const {
    const Vector3 forward = Vector3Normalize(Vector3Subtract(target_, position()));
    return Vector3DotProduct(Vector3Subtract(world, position()), forward);
}

float EditorCamera::worldUnitsPerPixel(Vector3 world, float viewportHeight) const {
    const float depth = std::max(kNearClipDistance, depthOf(world));
    const float tanHalf = std::tan(fovDegrees_ * DEG2RAD * 0.5f);
    return 2.0f * depth * tanHalf / std::max(1.0f, viewportHeight);
}

void EditorCamera::setOrbit(Vector3 target, float distance, float yaw, float pitch) {
    target_ = target;
    distance_ = std::clamp(distance, kMinDistance, kMaxDistance);
    minimumOrbitDistance_ = kMinDistance;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, kMinPitch, kMaxPitch);
}

void EditorCamera::focusOn(Vector3 point, float radius) {
    target_ = point;
    const float safeRadius = std::max(radius, 0.1f);
    const float comfortableDistance = safeRadius / std::sin(fovDegrees_ * DEG2RAD * 0.5f) * 1.15f;
    minimumOrbitDistance_ = std::min(kMaxDistance, std::max(kMinDistance, safeRadius * 1.25f));
    distance_ = std::clamp(comfortableDistance, minimumOrbitDistance_, kMaxDistance);
}

void EditorCamera::releaseCapture() {
    if (!cursorCaptured_) return;
    EnableCursor();
    SetMousePosition(static_cast<int>(cursorRestorePosition_.x),
                     static_cast<int>(cursorRestorePosition_.y));
    cursorCaptured_ = false;
}

void EditorCamera::rotateBy(Vector2 mouseDelta) {
    yaw_ -= mouseDelta.x * lookSensitivity_;
    pitch_ = std::clamp(pitch_ + mouseDelta.y * lookSensitivity_, kMinPitch, kMaxPitch);
}

void EditorCamera::setNavigationSettings(float moveSpeed, float lookSensitivity) {
    if (std::isfinite(moveSpeed)) moveSpeed_ = std::clamp(moveSpeed, 1.0f, 200.0f);
    if (std::isfinite(lookSensitivity))
        lookSensitivity_ = std::clamp(lookSensitivity, 0.0005f, 0.03f);
}

void EditorCamera::zoomBy(float wheelDelta) {
    distance_ = std::clamp(distance_ * std::pow(0.88f, wheelDelta), minimumOrbitDistance_, kMaxDistance);
}

void EditorCamera::moveFree(Vector3 localInput, float deltaSeconds, bool fast) {
    const Vector3 forward{ -std::sin(yaw_), 0.0f, -std::cos(yaw_) };
    const Vector3 right{ std::cos(yaw_), 0.0f, -std::sin(yaw_) };
    Vector3 movement = Vector3Add(Vector3Scale(forward, localInput.z),
                                  Vector3Scale(right, localInput.x));
    movement.y += localInput.y;
    const float magnitude = Vector3Length(movement);
    if (magnitude < 1e-5f) return;
    const float speed = fast ? moveSpeed_ * 3.0f : moveSpeed_;
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.1f);
    const Vector3 step = Vector3Scale(movement, speed * dt / magnitude);
    target_ = Vector3Add(target_, step);
}

void EditorCamera::handleInput(Vector2 viewportSize, bool hovered, bool focused,
                               bool allowKeyboard, bool allowMouse, bool interactionBlocked,
                               float deltaSeconds) {
    const bool windowFocused = IsWindowFocused();
    const bool rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    const bool rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    bool capturedThisFrame = false;

    if (!windowFocused || interactionBlocked || (!hovered && !cursorCaptured_)) {
        releaseCapture();
    }
    if (rightPressed && hovered && windowFocused && allowMouse && !interactionBlocked) {
        cursorRestorePosition_ = GetMousePosition();
        DisableCursor();
        cursorCaptured_ = true;
        capturedThisFrame = true;
    }
    if (cursorCaptured_ && (!rightDown || !windowFocused || !hovered || interactionBlocked)) {
        releaseCapture();
    }

    if (cursorCaptured_ && !capturedThisFrame) {
        rotateBy(GetMouseDelta());
    }

    if (!cursorCaptured_ && hovered && allowMouse && !interactionBlocked &&
        IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        const Vector2 delta = GetMouseDelta();
        const Vector3 forward = Vector3Normalize(Vector3Subtract(target_, position()));
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{0, 1, 0}));
        const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
        const float worldPerPixel = 2.0f * distance_ * std::tan(fovDegrees_ * DEG2RAD * 0.5f) /
                                    std::max(1.0f, viewportSize.y);
        target_ = Vector3Subtract(target_, Vector3Scale(right, delta.x * worldPerPixel));
        target_ = Vector3Add(target_, Vector3Scale(up, delta.y * worldPerPixel));
    }

    if (focused && allowKeyboard && !interactionBlocked) {
        Vector3 input{0.0f, 0.0f, 0.0f};
        if (IsKeyDown(KEY_D)) input.x += 1.0f;
        if (IsKeyDown(KEY_A)) input.x -= 1.0f;
        if (IsKeyDown(KEY_W)) input.z += 1.0f;
        if (IsKeyDown(KEY_S)) input.z -= 1.0f;
        if (IsKeyDown(KEY_E)) input.y += 1.0f;
        if (IsKeyDown(KEY_Q)) input.y -= 1.0f;
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        moveFree(input, deltaSeconds, shift);
    }

    if (hovered && allowMouse && !interactionBlocked && !cursorCaptured_) {
        zoomBy(GetMouseWheelMove());
    }
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

bool EditorCamera::projectChecked(Vector3 world, Vector2 viewportSize, Vector2& out) const {
    const float width = std::max(1.0f, viewportSize.x);
    const float height = std::max(1.0f, viewportSize.y);
    const Vector3 eye = position();
    const Vector3 forward = Vector3Normalize(Vector3Subtract(target_, eye));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{0.0f, 1.0f, 0.0f}));
    const Vector3 up = Vector3CrossProduct(right, forward);
    const Vector3 relative = Vector3Subtract(world, eye);
    const float depth = Vector3DotProduct(relative, forward);
    // Um ponto atras da camera produziria w negativo e coordenadas espelhadas.
    if (!std::isfinite(depth) || depth <= kNearClipDistance) return false;
    const float tanHalf = std::tan(fovDegrees_ * DEG2RAD * 0.5f);
    if (!(tanHalf > 0.0f)) return false;
    const float aspect = width / height;
    const float ndcX = Vector3DotProduct(relative, right) / (depth * tanHalf * aspect);
    const float ndcY = Vector3DotProduct(relative, up) / (depth * tanHalf);
    const Vector2 screen{(ndcX * 0.5f + 0.5f) * width, (0.5f - ndcY * 0.5f) * height};
    if (!std::isfinite(screen.x) || !std::isfinite(screen.y)) return false;
    out = screen;
    return true;
}

}  // namespace loblox

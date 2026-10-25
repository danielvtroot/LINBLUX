#include "rendering/EditorCamera.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"
#include "core/Diagnostics.h"

namespace loblox {

namespace {
constexpr float kMinPitch = -1.45f;
constexpr float kMaxPitch = 1.45f;
constexpr float kMinDistance = 2.0f;
constexpr float kMaxDistance = 2000.0f;
// A cursor warp (focus loss, capture release) can produce a single huge
// pointer delta. Clamping it keeps one bad frame from throwing the orbit.
constexpr float kMaxPointerDelta = 240.0f;
constexpr float kMaxWheelDelta = 20.0f;

bool Finite(Vector2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}
bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}  // namespace

EditorCamera::EditorCamera() = default;

EditorCamera::~EditorCamera() { releaseCapture(); }

void EditorCamera::reset() {
    releaseCapture();
    navigationMode_ = NavigationMode::FPS;
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
    if (!Finite(target) || !std::isfinite(distance) || !std::isfinite(yaw) ||
        !std::isfinite(pitch)) return;
    target_ = target;
    distance_ = std::clamp(distance, kMinDistance, kMaxDistance);
    minimumOrbitDistance_ = kMinDistance;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, kMinPitch, kMaxPitch);
}

void EditorCamera::focusOn(Vector3 point, float radius) {
    if (!Finite(point) || !std::isfinite(radius)) return;
    target_ = point;
    const float safeRadius = std::max(radius, 0.1f);
    const float comfortableDistance = safeRadius / std::sin(fovDegrees_ * DEG2RAD * 0.5f) * 1.15f;
    minimumOrbitDistance_ = std::min(kMaxDistance, std::max(kMinDistance, safeRadius * 1.25f));
    distance_ = std::clamp(comfortableDistance, minimumOrbitDistance_, kMaxDistance);
}

void EditorCamera::releaseCapture() {
    if (!cursorCaptured()) return;
    EnableCursor();
    SetMousePosition(static_cast<int>(cursorRestorePosition_.x),
                     static_cast<int>(cursorRestorePosition_.y));
    gesture_ = Gesture::None;
    diag::Event event;
    event.subsystem = "camera";
    event.name = "capture_end";
    diag::log(event);
}

void EditorCamera::rotateBy(Vector2 mouseDelta) {
    if (!Finite(mouseDelta)) return;
    const Vector3 eye = position();
    const Vector2 delta{std::clamp(mouseDelta.x, -kMaxPointerDelta, kMaxPointerDelta),
                        std::clamp(mouseDelta.y, -kMaxPointerDelta, kMaxPointerDelta)};
    yaw_ -= delta.x * lookSensitivity_;
    pitch_ = std::clamp(pitch_ + delta.y * lookSensitivity_, kMinPitch, kMaxPitch);
    if (navigationMode_ == NavigationMode::FPS) {
        // Keep the eye fixed; move the look target, not the camera, when
        // yaw/pitch change. Focus deliberately leaves its pivot fixed instead.
        const Vector3 offset = Vector3Scale(Vector3{
            std::cos(pitch_) * std::sin(yaw_), std::sin(pitch_),
            std::cos(pitch_) * std::cos(yaw_)}, distance_);
        target_ = Vector3Subtract(eye, offset);
    }
}

void EditorCamera::setNavigationMode(NavigationMode mode) {
    if (mode == navigationMode_) return;
    releaseCapture();
    navigationMode_ = mode; // No transform change: switching never teleports.
    diag::Event event; event.subsystem="camera"; event.name="editor_mode_change";
    event.context={{"mode",mode==NavigationMode::FPS?"fps":"focus"}};diag::log(event);
}

void EditorCamera::setNavigationSettings(float moveSpeed, float lookSensitivity) {
    if (std::isfinite(moveSpeed)) moveSpeed_ = std::clamp(moveSpeed, 1.0f, 200.0f);
    if (std::isfinite(lookSensitivity))
        lookSensitivity_ = std::clamp(lookSensitivity, 0.0005f, 0.03f);
}

void EditorCamera::zoomBy(float wheelDelta) {
    if (!std::isfinite(wheelDelta)) return;
    wheelDelta = std::clamp(wheelDelta, -kMaxWheelDelta, kMaxWheelDelta);
    distance_ = std::clamp(distance_ * std::pow(0.88f, wheelDelta), minimumOrbitDistance_, kMaxDistance);
}

void EditorCamera::moveFree(Vector3 localInput, float deltaSeconds, bool fast) {
    if (!Finite(localInput) || !std::isfinite(deltaSeconds)) return;
    const Vector3 forward = Vector3Normalize(Vector3Subtract(target_, position()));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{0, 1, 0}));
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

void EditorCamera::handleInput(const EditorInput& input, Vector2 viewportSize,
                               bool navigation, bool keyboard, bool zoom, float deltaSeconds) {
    bool capturedThisFrame = false;
    if (!navigation) releaseCapture();
    if (navigation && !cursorCaptured() && Finite(input.mouse) &&
        (input.rightPressed || input.middlePressed)) {
        cursorRestorePosition_ = input.mouse;
        DisableCursor();
        gesture_ = input.rightPressed ? Gesture::Look : Gesture::Pan;
        capturedThisFrame = true;
        diag::Event event;
        event.subsystem = "camera";
        event.name = "capture_begin";
        event.context = {{"gesture", gesture_ == Gesture::Look ? "look" : "pan"}};
        diag::log(event);
    }
    if ((gesture_ == Gesture::Look && !input.rightDown) ||
        (gesture_ == Gesture::Pan && !input.middleDown)) {
        releaseCapture();
    }
    Vector2 delta = Finite(input.delta) ? input.delta : Vector2{};
    delta.x = std::clamp(delta.x, -kMaxPointerDelta, kMaxPointerDelta);
    delta.y = std::clamp(delta.y, -kMaxPointerDelta, kMaxPointerDelta);
    if (gesture_ == Gesture::Look && !capturedThisFrame) {
        rotateBy(delta);
    }
    if (gesture_ == Gesture::Pan && !capturedThisFrame) {
        const Vector3 forward = Vector3Normalize(Vector3Subtract(target_, position()));
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, Vector3{0, 1, 0}));
        const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
        const float viewportHeight = std::isfinite(viewportSize.y) ? viewportSize.y : 1.0f;
        const float worldPerPixel = 2.0f * distance_ * std::tan(fovDegrees_ * DEG2RAD * 0.5f) /
                                    std::max(1.0f, viewportHeight);
        target_ = Vector3Subtract(target_, Vector3Scale(right, delta.x * worldPerPixel));
        target_ = Vector3Add(target_, Vector3Scale(up, delta.y * worldPerPixel));
    }

    if (keyboard && !input.modified && !capturedThisFrame) {
        moveFree(input.movement, deltaSeconds, input.shift);
    }
    if (zoom && !cursorCaptured()) {
        zoomBy(input.wheel);
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

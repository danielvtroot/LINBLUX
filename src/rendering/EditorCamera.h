#pragma once

#include "core/Math.h"

namespace loblox {

// Camera orbital de editor. Nao conhece ImGui: recebe apenas estado de mouse e
// se o viewport esta sob o cursor.
class EditorCamera {
public:
    EditorCamera();

    Camera3D camera() const;
    Vector3 position() const;
    Vector3 target() const { return target_; }
    float distance() const { return distance_; }

    void setTarget(Vector3 target) { target_ = target; }
    void focusOn(Vector3 point, float radius);
    void reset();

    void handleInput(Vector2 viewportSize, bool hovered, bool allowMouse);

    // Coordenadas locais do viewport (0..width, 0..height).
    Ray rayFromScreen(Vector2 localMouse, Vector2 viewportSize) const;
    Vector2 project(Vector3 world, Vector2 viewportSize) const;

private:
    Vector3 target_{0.0f, 2.0f, 0.0f};
    float distance_ = 42.0f;
    float yaw_ = 0.75f;
    float pitch_ = 0.48f;
    float fovDegrees_ = 45.0f;
};

}  // namespace loblox

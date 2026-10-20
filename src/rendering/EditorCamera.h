#pragma once

#include "core/Math.h"

namespace loblox {

// Camera orbital de editor. Nao conhece ImGui: recebe apenas estado de mouse e
// se o viewport esta sob o cursor.
class EditorCamera {
public:
    EditorCamera();
    ~EditorCamera();

    Camera3D camera() const;
    Vector3 position() const;
    Vector3 target() const { return target_; }
    float distance() const { return distance_; }
    float fovDegrees() const { return fovDegrees_; }
    float moveSpeed() const { return moveSpeed_; }
    float lookSensitivity() const { return lookSensitivity_; }

    // Profundidade em espaco de visao: negativa atras da camera.
    float depthOf(Vector3 world) const;
    // Unidades de mundo por pixel na profundidade do ponto.
    float worldUnitsPerPixel(Vector3 world, float viewportHeight) const;

    void setTarget(Vector3 target) { target_ = target; }
    // Estado orbital deterministico (testes e futuras visoes predefinidas).
    // Distancia e pitch respeitam os mesmos limites da navegacao normal.
    void setOrbit(Vector3 target, float distance, float yaw, float pitch);
    void focusOn(Vector3 point, float radius);
    void reset();
    void releaseCapture();
    bool cursorCaptured() const { return cursorCaptured_; }
    float pitch() const { return pitch_; }
    void rotateBy(Vector2 mouseDelta);
    void zoomBy(float wheelDelta);
    void moveFree(Vector3 localInput, float deltaSeconds, bool fast);
    void setNavigationSettings(float moveSpeed, float lookSensitivity);

    void handleInput(Vector2 viewportSize, bool hovered, bool focused,
                     bool allowKeyboard, bool allowMouse, bool interactionBlocked,
                     float deltaSeconds);

    // Coordenadas locais do viewport (0..width, 0..height).
    Ray rayFromScreen(Vector2 localMouse, Vector2 viewportSize) const;
    // Projecao sem validacao: pontos atras da camera voltam espelhados.
    // Overlays devem usar projectChecked.
    Vector2 project(Vector3 world, Vector2 viewportSize) const;
    // Projecao perspectiva valida somente a frente do near plane. Retorna
    // false sem escrever em out para pontos atras da camera, sobre o plano
    // proximo ou com coordenadas nao finitas.
    bool projectChecked(Vector3 world, Vector2 viewportSize, Vector2& out) const;

    // Near plane usado pela projecao 3D da raylib (RL_CULL_DISTANCE_NEAR).
    static constexpr float kNearClipDistance = 0.05f;

private:
    Vector3 target_{0.0f, 2.0f, 0.0f};
    float distance_ = 42.0f;
    float yaw_ = 0.75f;
    float pitch_ = 0.48f;
    float fovDegrees_ = 45.0f;
    float minimumOrbitDistance_ = 2.0f;
    float moveSpeed_ = 30.0f;
    float lookSensitivity_ = 0.0065f;
    bool cursorCaptured_ = false;
    Vector2 cursorRestorePosition_{0.0f, 0.0f};
};

}  // namespace loblox

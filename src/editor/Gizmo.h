#pragma once

#include "core/Math.h"
#include "editor/Tool.h"
#include "objects/Part.h"

namespace loblox {

class EditorCamera;

// Gizmos em eixos do mundo. Nao depende de ImGui: usa o mouse do raylib e so
// recebe input quando a UI informa que o viewport esta disponivel.
class Gizmo {
public:
    // Retorna true quando o clique foi consumido pelo gizmo.
    bool update(ToolMode mode, Part* part, const EditorCamera& camera, Rectangle viewport,
                bool hovered, bool allowInput);

    void draw(const Part& part, ToolMode mode) const;

    bool dragging() const { return dragging_; }
    int activeAxis() const { return activeAxis_; }
    void cancel();

private:
    int activeAxis_ = -1;
    int activePlane_ = -1;
    bool dragging_ = false;
    Vector3 startPosition_{0.0f, 0.0f, 0.0f};
    Vector3 startSize_{1.0f, 1.0f, 1.0f};
    Quaternion startRotation_{0.0f, 0.0f, 0.0f, 1.0f};
    float startParam_ = 0.0f;
    Vector3 startPlaneHit_{0.0f, 0.0f, 0.0f};
    float startAngle_ = 0.0f;
};

}  // namespace loblox

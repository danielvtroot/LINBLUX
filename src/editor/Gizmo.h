#pragma once

#include "core/Math.h"
#include "editor/GizmoMath.h"
#include "editor/Tool.h"
#include "objects/Part.h"

namespace loblox {

class EditorCamera;

class Gizmo {
public:
    bool update(ToolMode mode, Part* part, const EditorCamera& camera, Rectangle viewport,
                bool hovered, bool allowInput);
    void drawOverlay(const Part& part, ToolMode mode, const EditorCamera& camera,
                     Rectangle viewport) const;
    bool dragging() const { return dragging_; }
    bool consumedLastInput() const { return consumedLastInput_; }
    bool hoveringHandle() const { return hoveredHandle_ != gizmomath::HandleId::None; }
    gizmomath::HandleId hoveredHandle() const { return hoveredHandle_; }
    int activeAxis() const { return activeAxis_; }
    void cancel(Part* part = nullptr, bool restoreTransform = false);

    void setLocalSpace(bool local) { localSpace_ = local; }
    bool localSpace() const { return localSpace_; }
    void setSnap(float move, float scale, float rotateDegrees) {
        moveSnap_ = move; scaleSnap_ = scale; rotateSnapDegrees_ = rotateDegrees;
    }
    float moveSnap() const { return moveSnap_; }
    float scaleSnap() const { return scaleSnap_; }
    float rotateSnapDegrees() const { return rotateSnapDegrees_; }

private:
    enum class Handle { None, Axis, Plane, ScaleFace, RotateRing };

    Handle handle_ = Handle::None;
    gizmomath::HandleId activeHandle_ = gizmomath::HandleId::None;
    gizmomath::HandleId hoveredHandle_ = gizmomath::HandleId::None;
    int activeAxis_ = -1;
    int activePlane_ = -1;
    bool dragging_ = false;
    bool consumedLastInput_ = false;
    bool localSpace_ = false;
    float moveSnap_ = 1.0f;
    float scaleSnap_ = 0.5f;
    float rotateSnapDegrees_ = 15.0f;
    InstanceId draggedPartId_ = kInvalidInstanceId;
    Vector3 startPlaneHit_{0.0f, 0.0f, 0.0f};
    float startParam_ = 0.0f;
    float startAngle_ = 0.0f;
    Transform startTransform_{};
};

}  // namespace loblox

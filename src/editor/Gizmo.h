#pragma once

#include "core/Math.h"
#include "editor/GizmoMath.h"
#include "editor/GizmoLayout.h"
#include "editor/Tool.h"
#include "objects/Part.h"

namespace loblox {

class EditorCamera;

// Entrada sintetica: permite testar o ciclo de vida do gizmo sem janela e
// impede que o hit test e o arraste leiam estados globais divergentes.
struct GizmoInput {
    Vector2 mousePosition{0.0f, 0.0f};
    bool leftPressed = false;
    bool leftDown = false;
    bool windowFocused = true;
    bool shiftDown = false;
    bool altDown = false;   // escala centrada (as duas faces)
    float occlusionDistance=INFINITY;
    bool surfaceSnap = false; // Ctrl: encaixe geométrico durante Mover
    bool cursorSurface = false;
    Ray cursorRay{};
    bool alignToNormal = false;
    const char* surfaceIssue = nullptr;
    gizmomath::SnapFeature feature = gizmomath::SnapFeature::Face;
    const std::vector<gizmomath::SurfaceBox>* movingBoxes = nullptr;
    const std::vector<gizmomath::SurfaceBox>* targetBoxes = nullptr;
};

// Resultado de um unico update. `partId` identifica a sessao encerrada mesmo
// depois de o Gizmo limpar seu estado interno.
struct GizmoUpdateResult {
    bool consumed = false;
    bool started = false;
    bool ended = false;
    bool cancelled = false;
    InstanceId partId = kInvalidInstanceId;
    const char* error = nullptr;
};

class Gizmo {
public:
    GizmoUpdateResult update(ToolMode mode, Part* part, const EditorCamera& camera,
                             Rectangle viewport, bool hovered, bool allowInput,
                             const GizmoInput& input);
    void drawOverlay(const Part& part, ToolMode mode, const EditorCamera& camera,
                     Rectangle viewport) const;
    bool dragging() const { return dragging_; }
    InstanceId sessionPartId() const { return draggedPartId_; }
    bool consumedLastInput() const { return consumedLastInput_; }
    // Transformacao no instante em que o arraste comecou. O readout numerico e
    // a edicao por texto sao sempre calculados a partir dela, nunca do frame
    // anterior.
    const Transform& startTransform() const { return startTransform_; }
    ToolMode dragTool() const { return dragTool_; }
    int activeHandleSide() const { return gizmomath::HandleSide(activeHandle_); }
    bool hoveringHandle() const { return hoveredHandle_ != gizmomath::HandleId::None; }
    gizmomath::HandleId hoveredHandle() const { return hoveredHandle_; }
    gizmomath::HandleId activeHandle() const { return activeHandle_; }
    const gizmomath::SurfaceSnap& surfaceContact() const { return surfaceContact_; }
    int activeAxis() const { return activeAxis_; }
    int activePlane() const { return activePlane_; }
    // Signed, unwrapped (and snapped) sweep of the active rotate drag. The
    // animation editor uses it to keep a deliberate >180 degree turn that a
    // pair of key quaternions cannot express on its own.
    float rotationArcRadians() const { return appliedRotation_; }
    // Retained after cancel() so the caller can inspect the finished drag.
    Vector3 rotationRingAxis() const { return appliedAxis_; }
    void cancel(Part* part = nullptr, bool restoreTransform = false);

    void setLocalSpace(bool local) { localSpace_ = local; }
    bool localSpace() const { return localSpace_; }
    void setGridMode(gizmomath::GridMode mode) {gridMode_=mode;}
    gizmomath::GridMode gridMode() const {return gridMode_;}
    void setSnap(float move, float scale, float rotateDegrees) {
        moveSnap_ = move; scaleSnap_ = scale; rotateSnapDegrees_ = rotateDegrees;
    }
    float moveSnap() const { return moveSnap_; }
    float scaleSnap() const { return scaleSnap_; }
    float rotateSnapDegrees() const { return rotateSnapDegrees_; }
    // Modo de escala fixado no inicio do arraste: mudar Alt no meio nao troca
    // a matematica debaixo do usuario.
    bool centeredScale() const { return centeredScale_; }
    void setCenteredScaleOption(bool value){centerScaleOption_=value;}
    bool centeredScaleOption() const {return centerScaleOption_;}
    Vector3 faceAnchor() const {return faceAnchorWorld_;}
    void setPixelScale(float value) { if(std::isfinite(value))pixelScale_=std::clamp(value,.75f,1.75f); }
    float pixelScale() const { return pixelScale_; }
    gizmolayout::Settings visualSettings() const {
        gizmolayout::Settings s;const float v=pixelScale_;
        s.basePixelLength*=v;s.minPixelLength*=v;s.maxPixelLength*=v;
        s.axisButtonHalf*=v;s.planeButtonHalf*=v;s.scaleHandleHalfWidth*=v;s.scaleHandleHalfHeight*=v;
        s.scaleHandleSeparation*=v;s.rotateMarkerHalfWidth*=v;s.rotateMarkerHalfHeight*=v;
        s.axisLineTolerance*=v;s.ringTolerance*=v;
        return s;
    }

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
    gizmomath::GridMode gridMode_=gizmomath::GridMode::Relative;
    bool centeredScale_ = false,centerScaleOption_=false;
    Vector3 faceAnchorLocal_{},faceAnchorWorld_{},faceGrab_{};
    float pixelScale_=1;
    bool dragLocalSpace_ = false;
    Vector3 dragOrigin_{},axisViewForward_{};
    Rectangle dragViewport_{};
    float moveSnap_ = 1.0f;
    float scaleSnap_ = 0.5f;
    float rotateSnapDegrees_ = 15.0f;
    InstanceId draggedPartId_ = kInvalidInstanceId;
    Vector3 startPlaneHit_{0.0f, 0.0f, 0.0f};
    float startParam_ = 0.0f;
    float startAngle_ = 0.0f;
    float accumulatedAngle_ = 0.0f;
    float appliedRotation_ = 0.0f; // accumulatedAngle_ after the current snap
    Vector3 appliedAxis_{1, 0, 0};
    enum class DragProjection { World, Screen };
    DragProjection dragProjection_ = DragProjection::World;
    ToolMode dragTool_ = ToolMode::Select;
    Vector2 startMouse_{}, screenDirection_{}, secondScreenDirection_{};
    float screenUnits_ = 1.0f;
    Transform startTransform_{};
    gizmomath::SurfaceSnap surfaceContact_{};
    void snapPosition(Part& part,Vector3 candidate,Vector3 axisA,Vector3 axisB,
                      const GizmoInput& input,float tolerance);
};

}  // namespace loblox

#include "editor/Gizmo.h"

#include <algorithm>
#include <cmath>

#include "editor/GizmoLayout.h"
#include "editor/GizmoMath.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "rendering/EditorCamera.h"

namespace loblox {
namespace {

const Color kAxisColors[3] = {{193, 119, 125, 255}, {126, 169, 148, 255}, {124, 153, 192, 255}};
const Color kPlaneColors[3] = {{75, 145, 235, 125}, {68, 185, 106, 125}, {230, 90, 78, 125}};
const Color kHighlight{239, 58, 81, 255};
const Color kOutline{32, 38, 46, 255};
void Triangle2D(Vector2 a,Vector2 b,Vector2 c,Color color) {
    const auto u=Vector2Subtract(b,a),v=Vector2Subtract(c,a);
    if(u.x*v.y-u.y*v.x>0)std::swap(b,c);
    DrawTriangle(a,b,c,color);
}

Vector3 AxisVector(int axis, const Part& part, bool local) {
    return gizmomath::Axis(axis, part.rotation(), local);
}

Vector3 PlaneAxisA(int plane, const Part& part, bool local) {
    static const int indices[3] = {0, 0, 1};
    return AxisVector(indices[plane], part, local);
}

Vector3 PlaneAxisB(int plane, const Part& part, bool local) {
    static const int indices[3] = {1, 2, 2};
    return AxisVector(indices[plane], part, local);
}

bool RayPlane(const Ray& ray, Vector3 point, Vector3 normal, Vector3& hit) {
    const float denominator = Vector3DotProduct(ray.direction, normal);
    if (std::fabs(denominator) < .05f) return false;
    const float t = Vector3DotProduct(Vector3Subtract(point, ray.position), normal) / denominator;
    if (t < 0.0f) return false;
    hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    return true;
}

bool AxisParameter(const Ray& ray, Vector3 center, Vector3 axis, Vector3 viewForward,
                   float& parameter) {
    Vector3 normal = Vector3Subtract(viewForward,
                                     Vector3Scale(axis, Vector3DotProduct(viewForward, axis)));
    if (Vector3LengthSqr(normal) < 1e-5f) {
        normal = Vector3CrossProduct(axis, Vector3{0, 1, 0});
        if (Vector3LengthSqr(normal) < 1e-5f)
            normal = Vector3CrossProduct(axis, Vector3{1, 0, 0});
    }
    Vector3 hit;
    if (!RayPlane(ray, center, Vector3Normalize(normal), hit)) return false;
    parameter = Vector3DotProduct(Vector3Subtract(hit, center), axis);
    return true;
}

void PerpendicularBasis(Vector3 axis, Vector3& u, Vector3& v) {
    const Vector3 helper = std::fabs(axis.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    u = Vector3Normalize(Vector3CrossProduct(axis, helper));
    v = Vector3Normalize(Vector3CrossProduct(axis, u));
}

float AngleOnPlane(Vector3 offset, Vector3 axis) {
    Vector3 u, v;
    PerpendicularBasis(axis, u, v);
    return std::atan2(Vector3DotProduct(offset, v), Vector3DotProduct(offset, u));
}

gizmomath::HandleId AxisHandleId(int axis, int side = 1) {
    return gizmomath::MoveHandleId(axis, side);
}

gizmomath::HandleId PlaneHandleId(int plane) {
    return plane == 0 ? gizmomath::HandleId::XYPlane
         : plane == 1 ? gizmomath::HandleId::XZPlane : gizmomath::HandleId::YZPlane;
}

gizmomath::HandleId RotateHandleId(int axis) {
    return axis == 0 ? gizmomath::HandleId::RotateX
         : axis == 1 ? gizmomath::HandleId::RotateY : gizmomath::HandleId::RotateZ;
}

}  // namespace

void Gizmo::cancel(Part* part, bool restoreTransform) {
    if (restoreTransform && dragging_ && part != nullptr && part->id() == draggedPartId_) {
        part->setTransform(startTransform_);
    }
    dragging_ = false;
    handle_ = Handle::None;
    activeAxis_ = activePlane_ = -1;
    activeHandle_ = gizmomath::HandleId::None;
    hoveredHandle_ = gizmomath::HandleId::None;
    draggedPartId_ = kInvalidInstanceId;
    surfaceContact_ = {};
}

void Gizmo::snapPosition(Part& part,Vector3 candidate,Vector3 a,Vector3 b,
                         const GizmoInput& input,float tolerance) {
    const auto previous=surfaceContact_.target;
    surfaceContact_={};
    candidate=gizmomath::SnapGrid(candidate,startTransform_.position,a,b,
        input.shiftDown?0:moveSnap_,gridMode_);
    if(input.surfaceSnap&&input.surfaceIssue) {
        surfaceContact_.reason=input.surfaceIssue;part.setPosition(candidate);return;
    }
    // Um clique sem movimento não desloca o objeto para uma face próxima.
    if(input.surfaceSnap&&input.targetBoxes&&Vector2Distance(input.mousePosition,
        Vector2Add(startMouse_,{dragViewport_.x,dragViewport_.y}))>=2) {
        std::vector<gizmomath::SurfaceBox> moving;
        if(input.movingBoxes)moving=*input.movingBoxes;
        else if(part.shape()==PartShape::Block)moving.push_back({{part.position(),part.rotation(),part.size()},part.id()});
        const auto delta=Vector3Subtract(candidate,part.position());
        for(auto& box:moving)box.pose.position=Vector3Add(box.pose.position,delta);
        surfaceContact_=input.cursorSurface?gizmomath::SnapUnderCursor(moving,*input.targetBoxes,candidate,
            input.cursorRay,input.feature,input.alignToNormal,part.rotation(),previous):
            gizmomath::SnapSurfaces(moving,*input.targetBoxes,a,b,tolerance);
        if(surfaceContact_.blocked)return; // Retain last accepted pose, with visible reason.
        if(surfaceContact_.snapped) {
            candidate=Vector3Add(candidate,surfaceContact_.correction);
            part.setRotation(QuaternionMultiply(surfaceContact_.rotationDelta,part.rotation()));
        }
    }
    part.setPosition(candidate);
}

GizmoUpdateResult Gizmo::update(ToolMode mode, Part* part, const EditorCamera& camera,
                                Rectangle viewport, bool hovered, bool allowInput,
                                const GizmoInput& input) {
    GizmoUpdateResult result;
    const auto settings=visualSettings();
    const auto previousHover=hoveredHandle_;
    consumedLastInput_ = false;
    if (!dragging_) hoveredHandle_ = gizmomath::HandleId::None;
    const auto finish = [this](GizmoUpdateResult value) {
        consumedLastInput_ = value.consumed;
        return value;
    };
    const auto endSession = [this, &result](Part* sessionPart, bool restore,
                                            bool cancelled) {
        result.ended = true;
        result.cancelled = cancelled;
        result.partId = draggedPartId_;
        result.consumed = true;
        cancel(sessionPart, restore);
        return result;
    };

    if (dragging_ && (!input.windowFocused || mode != dragTool_ ||
        viewport.x!=dragViewport_.x||viewport.y!=dragViewport_.y||viewport.width!=dragViewport_.width||viewport.height!=dragViewport_.height)) {
        return finish(endSession(part, true, true));
    }
    if (mode == ToolMode::Select || viewport.width < 8 || viewport.height < 8) {
        if (dragging_) return finish(endSession(part, true, true));
        return finish(result);
    }

    // Todo hit test usa coordenadas locais da viewport; o layout tambem.
    const Vector2 localMouse{input.mousePosition.x - viewport.x,
                             input.mousePosition.y - viewport.y};
    const Ray ray = camera.rayFromScreen(localMouse, Vector2{viewport.width, viewport.height});
    if (dragging_) {
        result.consumed = true;
        if (part == nullptr || part->id() != draggedPartId_) {
            return finish(endSession(nullptr, false, true));
        }
        if (!input.leftDown) {
            // Soltar o botao e o unico caminho que confirma o transform atual.
            return finish(endSession(part, false, false));
        }

        const Vector3 center = startTransform_.position;
        const Vector3 axis = gizmomath::Axis(activeAxis_, startTransform_.rotation, dragLocalSpace_);
        if (handle_ == Handle::Axis || handle_ == Handle::ScaleFace) {
            float parameter = 0.0f;
            if (dragProjection_ == DragProjection::World &&
                !AxisParameter(ray, dragOrigin_, axis, axisViewForward_, parameter)) {
                result.error = "axis_intersection_unavailable";
                return finish(result);
            }
            const float delta = dragProjection_ == DragProjection::Screen ?
                Vector2DotProduct(Vector2Subtract(localMouse, startMouse_), screenDirection_) * screenUnits_ :
                parameter - startParam_;
            if (handle_ == Handle::Axis) {
                snapPosition(*part,gizmomath::MoveAxis(
                    {startTransform_.position, startTransform_.rotation, startTransform_.size},
                    axis, delta, 0),axis,{},input,
                    std::clamp(camera.worldUnitsPerPixel(center,viewport.height)*10,.02f,2.f));
            } else {
                const auto snapshot = gizmomath::TransformSnapshot{
                    startTransform_.position, startTransform_.rotation, startTransform_.size};
                const float snap = input.shiftDown ? 0.0f : scaleSnap_;
                if (!gizmomath::IsScaleFace(activeHandle_)) return finish(result);
                const auto transformed = part->shape() == PartShape::Sphere
                    ? gizmomath::ScaleUniformFace(snapshot, axis,
                                                  gizmomath::HandleSide(activeHandle_), delta, snap)
                    : centeredScale_
                        ? gizmomath::ScaleCenteredFace(snapshot, gizmomath::HandleAxis(activeHandle_),
                                                       gizmomath::HandleSide(activeHandle_),
                                                       dragLocalSpace_, delta, snap)
                        : gizmomath::ScaleFaceForHandle(snapshot, activeHandle_, dragLocalSpace_,
                                                        delta, snap);
                part->setTransform(Transform{centeredScale_&&part->shape()==PartShape::Sphere?startTransform_.position:transformed.position, startTransform_.rotation,
                                             transformed.size});
                const auto size=gizmolayout::MakeSnapshot(*part).size;
                faceAnchorWorld_=Vector3Add(part->position(),Vector3RotateByQuaternion({faceAnchorLocal_.x*size.x,faceAnchorLocal_.y*size.y,faceAnchorLocal_.z*size.z},part->rotation()));
            }
            return finish(result);
        }
        if (handle_ == Handle::Plane) {
            const Vector3 a = PlaneAxisA(activePlane_, *part, dragLocalSpace_);
            const Vector3 b = PlaneAxisB(activePlane_, *part, dragLocalSpace_);
            const Vector3 normal = Vector3Normalize(Vector3CrossProduct(a, b));
            Vector3 delta;
            if (dragProjection_ == DragProjection::Screen) {
                const Vector2 screenDelta = Vector2Subtract(localMouse, startMouse_);
                const float determinant = screenDirection_.x * secondScreenDirection_.y -
                                          screenDirection_.y * secondScreenDirection_.x;
                const float da = (screenDelta.x * secondScreenDirection_.y -
                                  screenDelta.y * secondScreenDirection_.x) / determinant;
                const float db = (screenDirection_.x * screenDelta.y -
                                  screenDirection_.y * screenDelta.x) / determinant;
                delta = Vector3Scale(Vector3Add(Vector3Scale(a, da), Vector3Scale(b, db)), screenUnits_);
            } else {
                Vector3 hit;
                if (!RayPlane(ray, center, normal, hit)) {
                    result.error = "plane_intersection_unavailable";
                    return finish(result);
                }
                delta = Vector3Subtract(hit, startPlaneHit_);
            }
            snapPosition(*part,gizmomath::MovePlane(
                {startTransform_.position, startTransform_.rotation, startTransform_.size}, a, b,
                delta, 0),a,b,input,
                std::clamp(camera.worldUnitsPerPixel(center,viewport.height)*10,.02f,2.f));
            return finish(result);
        }
        if (handle_ == Handle::RotateRing) {
            if (dragProjection_ == DragProjection::Screen) {
                accumulatedAngle_ = Vector2DotProduct(Vector2Subtract(localMouse, startMouse_),
                                                       screenDirection_) * screenUnits_;
            } else {
                Vector3 hit;
                if (!RayPlane(ray, center, axis, hit)) {
                    result.error = "rotation_intersection_unavailable";
                    return finish(result);
                }
                const Vector3 offset = Vector3Subtract(hit, center);
                if (Vector3LengthSqr(offset) < 1e-5f) {
                    result.error = "rotation_at_pivot";
                    return finish(result);
                }
                const float angle = AngleOnPlane(offset, axis);
                accumulatedAngle_ += gizmomath::WrappedAngleDelta(startAngle_, angle);
                startAngle_ = angle;
            }
            const float snap = input.shiftDown ? 0.0f : rotateSnapDegrees_;
            appliedRotation_ = gizmomath::SnapDelta(accumulatedAngle_, snap * DEG2RAD);
            appliedAxis_ = axis;
            part->setRotation(gizmomath::Rotate(
                {startTransform_.position, startTransform_.rotation, startTransform_.size},
                axis, accumulatedAngle_, snap));
            return finish(result);
        }
        return finish(result);
    }

    hoveredHandle_ = gizmomath::HandleId::None;
    if (part == nullptr || !hovered || !allowInput) return finish(result);
    const gizmolayout::Layout layout =
        gizmolayout::Build(*part, mode==ToolMode::Move&&faceMove()?ToolMode::Scale:mode, camera, viewport, localSpace_,settings);
    if (!layout.valid) return finish(result);

    gizmolayout::FacePick facePick;
    int pickedAxis = -1;
    int pickedSide = 1;
    int pickedPlane = -1;
    Handle pickedHandle = Handle::None;

    if (mode == ToolMode::Move && !faceMove()) {
        // Arrowheads win over shafts; planar areas win over shafts too.
        auto id = gizmolayout::PickMoveArrow(layout, localMouse, settings);
        if (id != gizmomath::HandleId::None) {
            pickedAxis = gizmomath::HandleAxis(id);
            pickedSide = gizmomath::HandleSide(id) < 0 ? -1 : 1;
            pickedHandle = Handle::Axis;
        } else {
            pickedPlane = gizmolayout::PickPlane(layout, localMouse, settings);
            if (pickedPlane >= 0) {
                pickedHandle = Handle::Plane;
            } else {
                id = gizmolayout::PickMoveShaft(layout, localMouse, settings);
                if (id != gizmomath::HandleId::None) {
                    pickedAxis = gizmomath::HandleAxis(id);
                    pickedSide = gizmomath::HandleSide(id) < 0 ? -1 : 1;
                    pickedHandle = Handle::Axis;
                }
            }
        }
    } else if (mode == ToolMode::Scale || (mode==ToolMode::Move&&faceMove())) {
        // The one visible grip is interactive too, including its protruding
        // tip outside the projected face. Its triangles are the drawn mesh.
        if(previousHover!=gizmomath::HandleId::None && faceIndex_>=0){
            const int index=faceIndex_;
            std::array<gizmolayout::Mesh,3> controls{};const auto normal=mode==ToolMode::Move?Vector3Scale(gizmomath::Axis(gizmomath::HandleAxis(previousHover),part->rotation(),localSpace_),float(gizmomath::HandleSide(previousHover))):layout.faces[index].normal;
            controls[0]=gizmolayout::FaceGrip(*part,camera,viewport,faceAnchorWorld_,normal);
            if(gizmolayout::PickMesh(controls,localMouse)==0)facePick={index,faceAnchorWorld_,Vector3Distance(camera.position(),faceAnchorWorld_)};
        }
        if(facePick.index<0)facePick=gizmolayout::FaceUnderCursor(layout,localMouse);
        if(facePick.distance>input.occlusionDistance+.002f)facePick.index=-1;
        const auto id=facePick.index<0?gizmomath::HandleId::None:gizmomath::ScaleFaceHandleId(facePick.index/2,facePick.index%2?1:-1);
        if (gizmomath::IsScaleFace(id)) {
            const auto& face=layout.faces[facePick.index];
            faceAnchorWorld_=layout.sphere||Vector2Distance(face.screen,localMouse)>150?facePick.point:face.anchor;
            auto offset=Vector3RotateByQuaternion(Vector3Subtract(faceAnchorWorld_,part->position()),QuaternionInvert(part->rotation()));
            const auto size=gizmolayout::MakeSnapshot(*part).size;
            faceAnchorLocal_={offset.x/size.x,offset.y/size.y,offset.z/size.z};faceGrab_=facePick.point;
            pickedAxis = gizmomath::HandleAxis(id);
            pickedSide = gizmomath::HandleSide(id);
            faceIndex_=facePick.index;
            pickedHandle = mode==ToolMode::Scale?Handle::ScaleFace:Handle::Axis;
            if(mode==ToolMode::Move&&!localSpace_){
                // World mode constrains to the world axis closest to this face.
                pickedAxis=0;for(int a=1;a<3;++a)if(std::fabs((&face.normal.x)[a])>std::fabs((&face.normal.x)[pickedAxis]))pickedAxis=a;
                pickedSide=(&face.normal.x)[pickedAxis]>=0?1:-1;
            }
        }
    } else if (mode == ToolMode::Rotate) {
        pickedAxis = gizmolayout::PickRotateMarker(layout, localMouse,settings);
        if (pickedAxis >= 0) {
            pickedHandle = Handle::RotateRing;
        } else {
            pickedAxis = gizmolayout::PickRotateRing(layout, localMouse,settings);
            if (pickedAxis >= 0) pickedHandle = Handle::RotateRing;
        }
    }

    if (pickedHandle == Handle::None) return finish(result);
    hoveredHandle_ = pickedHandle == Handle::ScaleFace ? gizmomath::ScaleFaceHandleId(pickedAxis, pickedSide)
                    : pickedHandle == Handle::RotateRing ? RotateHandleId(pickedAxis)
                    : pickedHandle == Handle::Plane ? PlaneHandleId(pickedPlane)
                    : pickedHandle == Handle::Axis ? AxisHandleId(pickedAxis, pickedSide)
                    : gizmomath::HandleId::None;
    if (!input.leftPressed) return finish(result);

    handle_ = pickedHandle;
    activeHandle_ = hoveredHandle_;
    activeAxis_ = pickedHandle == Handle::Plane ? -1 : gizmomath::HandleAxis(activeHandle_);
    activePlane_ = pickedPlane;
    draggedPartId_ = part->id();
    startTransform_ = part->transform();
    dragLocalSpace_=localSpace_||mode==ToolMode::Scale;dragViewport_=viewport;
    dragTool_ = mode;
    startMouse_ = localMouse;
    dragProjection_ = DragProjection::World;
    accumulatedAngle_ = 0;
    centeredScale_ = (input.altDown||centerScaleOption_) && mode == ToolMode::Scale;
    dragging_ = true;
    surfaceContact_ = {};

    if (handle_ == Handle::Axis || handle_ == Handle::ScaleFace) {
        const Vector3 axis = gizmomath::Axis(activeAxis_, startTransform_.rotation, dragLocalSpace_);
        // Direct face gestures use the clicked surface as their projection origin.
        dragOrigin_=(handle_==Handle::ScaleFace||(mode==ToolMode::Move&&faceMove()))?faceGrab_:startTransform_.position;
        const Vector3 viewForward = Vector3Normalize(Vector3Subtract(dragOrigin_,camera.position()));
        axisViewForward_=viewForward;
        // Looking down an axis makes ray/axis intersection singular. The
        // visible screen-space handle remains usable through its shared layout.
        if (std::fabs(Vector3DotProduct(axis, viewForward)) > 0.94f ||
            !AxisParameter(ray, dragOrigin_, axis, viewForward, startParam_)) {
            dragProjection_ = DragProjection::Screen;
            screenDirection_ = layout.axisDirection[activeAxis_];
            if(handle_==Handle::ScaleFace||(mode==ToolMode::Move&&faceMove())) {
                Vector2 a,b;const Vector2 size{viewport.width,viewport.height};
                if(camera.projectChecked(dragOrigin_,size,a)&&camera.projectChecked(Vector3Add(dragOrigin_,Vector3Scale(axis,camera.worldUnitsPerPixel(dragOrigin_,viewport.height)*64)),size,b)&&Vector2Distance(a,b)>4)
                    screenDirection_=Vector2Normalize(Vector2Subtract(b,a));
                else screenDirection_={0,float(-pickedSide)};
            }
            screenUnits_ = camera.worldUnitsPerPixel(dragOrigin_, viewport.height);
        }
    } else if (handle_ == Handle::Plane) {
        const Vector3 a = PlaneAxisA(activePlane_, *part, localSpace_);
        const Vector3 b = PlaneAxisB(activePlane_, *part, localSpace_);
        const Vector3 normal = Vector3Normalize(Vector3CrossProduct(a, b));
        const Vector3 viewForward = Vector3Normalize(Vector3Subtract(camera.target(), camera.position()));
        if (std::fabs(Vector3DotProduct(normal, viewForward)) < 0.15f ||
            !RayPlane(ray, startTransform_.position, normal, startPlaneHit_)) {
            dragProjection_ = DragProjection::Screen;
            const int axes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
            screenDirection_ = layout.axisDirection[axes[activePlane_][0]];
            secondScreenDirection_ = layout.axisDirection[axes[activePlane_][1]];
            const float determinant = screenDirection_.x * secondScreenDirection_.y -
                                      screenDirection_.y * secondScreenDirection_.x;
            if (std::fabs(determinant) < 0.2f) {
                const Vector2 perpendicular{-screenDirection_.y, screenDirection_.x};
                secondScreenDirection_ = Vector2Scale(perpendicular, determinant < 0 ? -1.0f : 1.0f);
            }
            screenUnits_ = camera.worldUnitsPerPixel(startTransform_.position, viewport.height);
        }
    } else if (handle_ == Handle::RotateRing) {
        const Vector3 axis = gizmomath::Axis(activeAxis_, startTransform_.rotation, localSpace_);
        Vector3 hit;
        const Vector3 viewForward = Vector3Normalize(Vector3Subtract(camera.target(), camera.position()));
        const float facing = Vector3DotProduct(axis, viewForward);
        if (std::fabs(facing) < 0.15f || !RayPlane(ray, startTransform_.position, axis, hit) ||
            Vector3LengthSqr(Vector3Subtract(hit, startTransform_.position)) < 1e-5f) {
            // Edge-on rings have no invertible ray/plane map. Freeze a screen
            // tangent for this session; do not switch mappings midway through.
            dragProjection_ = DragProjection::Screen;
            const Vector2 radial = Vector2Subtract(localMouse, layout.center);
            screenDirection_ = Vector2Normalize(Vector2{-radial.y, radial.x});
            if (Vector2LengthSqr(screenDirection_) < 0.5f) screenDirection_ = {1, 0};
            if (facing > 0) screenDirection_ = Vector2Negate(screenDirection_);
            screenUnits_ = 1.0f / std::max(1.0f, layout.ringRadiusPx);
        } else {
            startAngle_ = AngleOnPlane(Vector3Subtract(hit, startTransform_.position), axis);
        }
    }
    result.consumed = true;
    result.started = true;
    result.partId = draggedPartId_;
    return finish(result);
}

void Gizmo::drawOverlay(const Part& part, ToolMode mode, const EditorCamera& camera,
                       Rectangle viewport,bool showMeasurement) const {
    if (mode == ToolMode::Select) return;
    // Layout puro: o desenho abaixo usa exatamente as mesmas posicoes que o
    // hit test de update() usa. Nenhuma coordenada invalida e gerada.
    const auto settings=visualSettings();
    const gizmolayout::Layout layout =
        gizmolayout::Build(part, mode==ToolMode::Move&&faceMove()?ToolMode::Scale:mode, camera, viewport, localSpace_,settings);
    if (!layout.valid) return;

    if(surfaceContact_.snapped&&part.shape()==PartShape::Block) {
        BeginMode3D(camera.camera());rlDrawRenderBatchActive();rlDisableDepthMask();
        const auto corners=part.bounds().corners();
        const int faces[6][4]={{0,1,2,3},{4,7,6,5},{0,4,5,1},{3,2,6,7},{0,3,7,4},{1,5,6,2}};
        for(const auto& f:faces){DrawTriangle3D(corners[f[0]],corners[f[1]],corners[f[2]],{60,215,170,45});
            DrawTriangle3D(corners[f[0]],corners[f[2]],corners[f[3]],{60,215,170,45});}
        rlDrawRenderBatchActive();rlEnableDepthMask();EndMode3D();
    }

    const auto faceHandle=dragging_?activeHandle_:hoveredHandle_;
    const Color graphite{101,108,121,255},accent{237,53,76,255};
    const auto isActive=[&](gizmomath::HandleId id){return dragging_?activeHandle_==id:hoveredHandle_==id;};
    struct Painted {gizmolayout::Triangle triangle;Color color;};
    std::vector<Painted> paint;
    const auto mesh=[&](const gizmolayout::Mesh& value,Color color){for(const auto& t:value.triangles)paint.push_back({t,color});};
    Vector2 readout=layout.center;
    const bool directFace=mode==ToolMode::Scale||(mode==ToolMode::Move&&faceMove());
    if(directFace){
        if(faceHandle==gizmomath::HandleId::None||faceIndex_<0)return;
        const int index=faceIndex_;const auto& face=layout.faces[index];
        const auto size=gizmolayout::MakeSnapshot(part).size;
        const auto anchor=Vector3Add(part.position(),Vector3RotateByQuaternion({faceAnchorLocal_.x*size.x,faceAnchorLocal_.y*size.y,faceAnchorLocal_.z*size.z},part.rotation()));
        camera.projectChecked(anchor,{viewport.width,viewport.height},readout);
        BeginMode3D(camera.camera());rlDrawRenderBatchActive();rlEnableDepthTest();rlDisableDepthMask();rlDisableBackfaceCulling();
        if(!layout.sphere&&face.visible){
            const auto offset=Vector3Scale(face.normal,camera.worldUnitsPerPixel(anchor,viewport.height)*.3f);
            for(size_t i=1;i+1<face.polygon.size();++i)DrawTriangle3D(Vector3Add(face.polygon[0],offset),Vector3Add(face.polygon[i],offset),Vector3Add(face.polygon[i+1],offset),{237,53,76,14});
            for(size_t i=0;i<face.polygon.size();++i)DrawLine3D(Vector3Add(face.polygon[i],offset),Vector3Add(face.polygon[(i+1)%face.polygon.size()],offset),{237,80,99,210});
        }
        rlDrawRenderBatchActive();EndMode3D();
        const auto normal=mode==ToolMode::Move?Vector3Scale(gizmomath::Axis(gizmomath::HandleAxis(faceHandle),part.rotation(),dragging_?dragLocalSpace_:localSpace_),float(gizmomath::HandleSide(faceHandle))):face.normal;
        mesh(gizmolayout::FaceGrip(part,camera,viewport,anchor,normal),accent);
        // Preview only the original active face; never draw diagonal debug axes.
        if(dragging_&&!layout.sphere){Part original(0,"preview",PartShape::Block);original.setTransform(startTransform_);
            const auto before=gizmolayout::Build(original,ToolMode::Scale,camera,viewport,true,settings);const auto& old=before.faces[index];
            rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();
            for(size_t i=0;i<old.projected.size();++i)DrawLineEx(old.projected[i],old.projected[(i+1)%old.projected.size()],1,{146,151,161,110});
        }
    }else if(mode==ToolMode::Move){
        for(int axis=0;axis<3;++axis){const bool active=isActive(AxisHandleId(axis,1));
            mesh(layout.shaft[axis],active?accent:graphite);
            const Color tint[3]={{170,103,111,255},{104,149,130,255},{106,135,175,255}};
            mesh(layout.arrow[axis],active?accent:tint[axis]);
        }
        rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();
        for(int plane=0;plane<3;++plane)if(layout.planeVisible[plane]){
            const auto& q=layout.planeCorners[plane];const auto color=isActive(PlaneHandleId(plane))?accent:Color{125,131,144,180};
            Triangle2D(q[0],q[1],q[2],{50,55,65,110});Triangle2D(q[0],q[2],q[3],{50,55,65,110});
            for(int i=0;i<4;++i)DrawLineEx(q[i],q[(i+1)%4],1.25f,color);
        }
    }else if(mode==ToolMode::Rotate){
        for(int axis=0;axis<3;++axis)mesh(layout.rotation[axis],isActive(RotateHandleId(axis))?accent:graphite);
    }
    // These are the same world-space triangles projected by the picker.
    // Painter order gives self-occlusion without allowing the selected solid
    // to hide its own transform tools. Face picking still obeys scene occlusion.
    std::stable_sort(paint.begin(),paint.end(),[](const Painted& a,const Painted& b){return a.triangle.depth>b.triangle.depth;});
    BeginMode3D(camera.camera());rlDrawRenderBatchActive();
    if(directFace)rlEnableDepthTest();else rlDisableDepthTest();
    rlDisableDepthMask();rlDisableBackfaceCulling();
    for(const auto& p:paint){const auto& t=p.triangle.world;auto normal=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(t[1],t[0]),Vector3Subtract(t[2],t[0])));
        const auto view=Vector3Normalize(Vector3Subtract(camera.position(),t[0]));
        const float facing=std::fabs(Vector3DotProduct(normal,view));
        const float shade=.48f+.25f*std::fabs(Vector3DotProduct(normal,Vector3Normalize({-.3f,.8f,.6f})))+.8f*std::pow(facing,6.f);
        Color color{(unsigned char)std::min(255.f,p.color.r*shade),(unsigned char)std::min(255.f,p.color.g*shade),(unsigned char)std::min(255.f,p.color.b*shade),p.color.a};
        DrawTriangle3D(t[0],t[1],t[2],color);
    }
    if(mode==ToolMode::Move&&!directFace)DrawSphere(part.position(),camera.worldUnitsPerPixel(part.position(),viewport.height)*5,{215,218,223,255});
    rlDrawRenderBatchActive();EndMode3D();rlDisableDepthTest();rlDisableDepthMask();
    if(dragging_&&showMeasurement){
        const auto delta=Vector3Subtract(part.position(),startTransform_.position);
        const char* text=mode==ToolMode::Scale?TextFormat("%.2f  x  %.2f  x  %.2f",part.size().x,part.size().y,part.size().z):
            mode==ToolMode::Rotate?TextFormat("%.1f graus",appliedRotation_*RAD2DEG):TextFormat("%.3f unidades",Vector3Length(delta));
        const float width=std::min(viewport.width-8,float(MeasureText(text,19)+26));
        const float x=std::clamp(readout.x+40,4.f,std::max(4.f,viewport.width-width-4));
        const float y=std::clamp(readout.y+36,4.f,std::max(4.f,viewport.height-54));
        DrawRectangleRounded({x,y,width,48},.15f,4,{27,30,36,245});DrawRectangle(int(x),int(y+7),3,34,accent);
        DrawText(mode==ToolMode::Scale?(centeredScale_?"Escala centrada":"Escala por face"):mode==ToolMode::Rotate?"Rotacao":"Deslocamento",int(x+12),int(y+5),12,{184,190,202,255});
        DrawText(text,int(x+12),int(y+22),19,{244,245,248,255});
    }
    if(surfaceContact_.snapped) {
        Vector2 p,q;
        const Vector2 size{viewport.width,viewport.height};
        const auto end=Vector3Add(surfaceContact_.point,Vector3Scale(surfaceContact_.normal,
            camera.worldUnitsPerPixel(surfaceContact_.point,viewport.height)*24));
        if(camera.projectChecked(surfaceContact_.point,size,p)&&camera.projectChecked(end,size,q)) {
            DrawCircleV(p,7,{16,18,22,230});DrawCircleLinesV(p,7,{242,70,78,255});
            DrawLineEx(p,q,2,{242,70,78,255});
            const float x=std::clamp(p.x+14,4.f,std::max(4.f,viewport.width-82));
            const float y=std::clamp(p.y-27,4.f,std::max(4.f,viewport.height-26));
            DrawRectangleRounded({x,y,78,23},.18f,4,{23,24,29,235});
            DrawText("Encaixe",int(x+8),int(y+4),16,{255,225,225,255});
        }
    }

    // Descarrega antes de restaurar: garante que nenhum vertice do gizmo seja
    // rasterizado com o depth test da cena ativo.
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    // Estado esperado pela raylib fora do modo 3D (EndMode3D ja desliga o
    // depth test); nao deixamos o depth test ligado para a UI.
    rlDisableDepthTest();
}

}  // namespace loblox

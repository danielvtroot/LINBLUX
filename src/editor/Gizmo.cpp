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

const Color kAxisColors[3] = {{235, 72, 66, 255}, {62, 174, 92, 255}, {55, 125, 225, 255}};
const Color kPlaneColors[3] = {{75, 145, 235, 125}, {68, 185, 106, 125}, {230, 90, 78, 125}};
const Color kHighlight{255, 195, 42, 255};
const Color kOutline{32, 38, 46, 255};

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
    if (std::fabs(denominator) < 1e-5f) return false;
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

gizmomath::HandleId AxisHandleId(int axis) {
    return axis == 0 ? gizmomath::HandleId::XAxis
         : axis == 1 ? gizmomath::HandleId::YAxis : gizmomath::HandleId::ZAxis;
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
}

GizmoUpdateResult Gizmo::update(ToolMode mode, Part* part, const EditorCamera& camera,
                                Rectangle viewport, bool hovered, bool allowInput,
                                const GizmoInput& input) {
    GizmoUpdateResult result;
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

    if (dragging_ && (!input.windowFocused || mode != dragTool_)) {
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
        const Vector3 axis = gizmomath::Axis(activeAxis_, startTransform_.rotation, localSpace_);
        if (handle_ == Handle::Axis || handle_ == Handle::ScaleFace) {
            const Vector3 viewForward = Vector3Normalize(Vector3Subtract(camera.target(), camera.position()));
            float parameter = 0.0f;
            if (dragProjection_ == DragProjection::World &&
                !AxisParameter(ray, center, axis, viewForward, parameter)) return finish(result);
            const float delta = dragProjection_ == DragProjection::Screen ?
                Vector2DotProduct(Vector2Subtract(localMouse, startMouse_), screenDirection_) * screenUnits_ :
                parameter - startParam_;
            if (handle_ == Handle::Axis) {
                part->setPosition(gizmomath::MoveAxis(
                    {startTransform_.position, startTransform_.rotation, startTransform_.size},
                    axis, delta, input.shiftDown ? 0.0f : moveSnap_));
            } else {
                const auto snapshot = gizmomath::TransformSnapshot{
                    startTransform_.position, startTransform_.rotation, startTransform_.size};
                const float snap = input.shiftDown ? 0.0f : scaleSnap_;
                if (!gizmomath::IsScaleFace(activeHandle_)) return finish(result);
                const auto transformed = part->shape() == PartShape::Sphere
                    ? gizmomath::ScaleUniformFace(snapshot, axis,
                                                  gizmomath::HandleSide(activeHandle_), delta, snap)
                    : gizmomath::ScaleFaceForHandle(snapshot, activeHandle_, localSpace_,
                                                    delta, snap);
                part->setTransform(Transform{transformed.position, startTransform_.rotation,
                                             transformed.size});
            }
            return finish(result);
        }
        if (handle_ == Handle::Plane) {
            const Vector3 a = PlaneAxisA(activePlane_, *part, localSpace_);
            const Vector3 b = PlaneAxisB(activePlane_, *part, localSpace_);
            const Vector3 normal = Vector3Normalize(Vector3CrossProduct(a, b));
            Vector3 hit;
            if (!RayPlane(ray, center, normal, hit)) return finish(result);
            const Vector3 delta = Vector3Subtract(hit, startPlaneHit_);
            part->setPosition(gizmomath::MovePlane(
                {startTransform_.position, startTransform_.rotation, startTransform_.size}, a, b,
                delta, input.shiftDown ? 0.0f : moveSnap_));
            return finish(result);
        }
        if (handle_ == Handle::RotateRing) {
            if (dragProjection_ == DragProjection::Screen) {
                accumulatedAngle_ = Vector2DotProduct(Vector2Subtract(localMouse, startMouse_),
                                                       screenDirection_) * screenUnits_;
            } else {
                Vector3 hit;
                if (!RayPlane(ray, center, axis, hit)) return finish(result);
                const Vector3 offset = Vector3Subtract(hit, center);
                if (Vector3LengthSqr(offset) < 1e-5f) return finish(result);
                const float angle = AngleOnPlane(offset, axis);
                accumulatedAngle_ += gizmomath::WrappedAngleDelta(startAngle_, angle);
                startAngle_ = angle;
            }
            const float snap = input.shiftDown ? 0.0f : rotateSnapDegrees_;
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
        gizmolayout::Build(*part, mode, camera, viewport, localSpace_);
    if (!layout.valid) return finish(result);

    int pickedAxis = -1;
    int pickedSide = 1;
    int pickedPlane = -1;
    Handle pickedHandle = Handle::None;

    if (mode == ToolMode::Move) {
        // Prioridade: planos (mais proximos do centro), botoes, depois linhas.
        pickedPlane = gizmolayout::PickPlane(layout, localMouse);
        if (pickedPlane >= 0) {
            pickedHandle = Handle::Plane;
        } else {
            pickedAxis = gizmolayout::PickAxisButton(layout, localMouse);
            if (pickedAxis >= 0) {
                pickedHandle = Handle::Axis;
            } else {
                pickedAxis = gizmolayout::PickAxisLine(layout, localMouse);
                if (pickedAxis >= 0) pickedHandle = Handle::Axis;
            }
        }
    } else if (mode == ToolMode::Scale) {
        const gizmomath::HandleId id = gizmolayout::PickScaleHandle(layout, localMouse);
        if (gizmomath::IsScaleFace(id)) {
            pickedAxis = gizmomath::HandleAxis(id);
            pickedSide = gizmomath::HandleSide(id);
            pickedHandle = Handle::ScaleFace;
        }
    } else if (mode == ToolMode::Rotate) {
        pickedAxis = gizmolayout::PickRotateMarker(layout, localMouse);
        if (pickedAxis >= 0) {
            pickedHandle = Handle::RotateRing;
        } else {
            pickedAxis = gizmolayout::PickRotateRing(layout, localMouse);
            if (pickedAxis >= 0) pickedHandle = Handle::RotateRing;
        }
    }

    if (pickedHandle == Handle::None) return finish(result);
    hoveredHandle_ = pickedHandle == Handle::ScaleFace ? gizmomath::ScaleFaceHandleId(pickedAxis, pickedSide)
                    : pickedHandle == Handle::RotateRing ? RotateHandleId(pickedAxis)
                    : pickedHandle == Handle::Plane ? PlaneHandleId(pickedPlane)
                    : pickedHandle == Handle::Axis ? AxisHandleId(pickedAxis)
                    : gizmomath::HandleId::None;
    if (!input.leftPressed) return finish(result);

    handle_ = pickedHandle;
    activeHandle_ = hoveredHandle_;
    activeAxis_ = pickedHandle == Handle::Plane ? -1 : gizmomath::HandleAxis(activeHandle_);
    activePlane_ = pickedPlane;
    draggedPartId_ = part->id();
    startTransform_ = part->transform();
    dragTool_ = mode;
    startMouse_ = localMouse;
    dragProjection_ = DragProjection::World;
    accumulatedAngle_ = 0;
    dragging_ = true;

    const auto failedStart = [&]() {
        cancel(part, false);
        // O clique era do gizmo: nao pode cair na selecao de Part.
        result.consumed = true;
        return finish(result);
    };
    if (handle_ == Handle::Axis || handle_ == Handle::ScaleFace) {
        const Vector3 axis = gizmomath::Axis(activeAxis_, startTransform_.rotation, localSpace_);
        const Vector3 viewForward = Vector3Normalize(Vector3Subtract(camera.target(), camera.position()));
        // Looking down an axis makes ray/axis intersection singular. The
        // visible screen-space handle remains usable through its shared layout.
        if (std::fabs(Vector3DotProduct(axis, viewForward)) > 0.98f ||
            !AxisParameter(ray, startTransform_.position, axis, viewForward, startParam_)) {
            dragProjection_ = DragProjection::Screen;
            screenDirection_ = layout.axisDirection[activeAxis_];
            screenUnits_ = camera.worldUnitsPerPixel(startTransform_.position, viewport.height);
        }
    } else if (handle_ == Handle::Plane) {
        const Vector3 a = PlaneAxisA(activePlane_, *part, localSpace_);
        const Vector3 b = PlaneAxisB(activePlane_, *part, localSpace_);
        if (!RayPlane(ray, startTransform_.position, Vector3Normalize(Vector3CrossProduct(a, b)),
                      startPlaneHit_)) {
            return failedStart();
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
                       Rectangle viewport) const {
    if (mode == ToolMode::Select) return;
    // Layout puro: o desenho abaixo usa exatamente as mesmas posicoes que o
    // hit test de update() usa. Nenhuma coordenada invalida e gerada.
    const gizmolayout::Layout layout =
        gizmolayout::Build(part, mode, camera, viewport, localSpace_);
    if (!layout.valid) return;

    // Passe proprio do gizmo, independente da profundidade da cena:
    //  1. descarrega o batch da cena (nenhum vertice antigo muda de estado);
    //  2. isola depth test e depth write;
    //  3. desenha os manipuladores;
    //  4. descarrega o batch do gizmo AINDA com a profundidade isolada;
    //  5. restaura o estado.
    // O passo 4 e obrigatorio: rlgl acumula vertices e so aplica o estado de
    // profundidade no flush. Restaurar antes do flush fazia a Part ocluir os
    // controles (primitivas 2D ficam no far plane da projecao ortografica).
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();

    const Vector2 center = layout.center;
    const auto highlighted = [this](gizmomath::HandleId id) {
        return dragging_ ? activeHandle_ == id : hoveredHandle_ == id;
    };

    // Ordem de desenho: linhas/guias primeiro, depois areas e por ultimo os
    // botoes com o texto. Assim um elemento desenhado depois nunca cobre o
    // centro clicavel de outro handle.
    if (mode == ToolMode::Move) {
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            const Color color = highlighted(AxisHandleId(axisIndex))
                                    ? kHighlight : kAxisColors[axisIndex];
            DrawLineEx(center, layout.axisButton[static_cast<std::size_t>(axisIndex)], 4.0f,
                       color);
        }
        const char* planeLabels[3] = {"XY", "XZ", "YZ"};
        for (int plane = 0; plane < 3; ++plane) {
            const Rectangle box = gizmolayout::PlaneButtonRect(layout, plane);
            const Vector2 point{box.x + box.width * 0.5f, box.y + box.height * 0.5f};
            const Color color = highlighted(PlaneHandleId(plane)) ? kHighlight : kPlaneColors[plane];
            DrawRectangleRounded(box, 0.12f, 3, color);
            DrawRectangleLinesEx(box, 1.5f, kAxisColors[plane]);
            DrawText(planeLabels[plane], static_cast<int>(point.x - 7.0f),
                     static_cast<int>(point.y - 4.0f), 9, Color{255, 255, 255, 255});
        }
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            const Color color = highlighted(AxisHandleId(axisIndex))
                                    ? kHighlight : kAxisColors[axisIndex];
            const Rectangle button = gizmolayout::AxisButtonRect(layout, axisIndex);
            DrawRectangleRounded(button, 0.5f, 8, color);
            DrawRectangleRoundedLinesEx(button, 0.5f, 8, 1.5f, kOutline);
            const char label[2] = {static_cast<char>('X' + axisIndex), '\0'};
            DrawText(label, static_cast<int>(button.x + button.width * 0.5f - 4.0f),
                     static_cast<int>(button.y + button.height * 0.5f - 6.0f), 12,
                     Color{255, 255, 255, 255});
        }
    } else if (mode == ToolMode::Rotate) {
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            const Color color = highlighted(RotateHandleId(axisIndex))
                                    ? kHighlight : kAxisColors[axisIndex];
            for (const gizmolayout::Segment& segment : layout.ring[static_cast<std::size_t>(axisIndex)]) {
                DrawLineEx(segment.a, segment.b, 7.0f, Color{31, 38, 48, 235});
                DrawLineEx(segment.a, segment.b, highlighted(RotateHandleId(axisIndex)) ? 4.5f : 3.0f,
                           color);
            }
        }
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            const Vector2 anchor = layout.rotateAnchor[static_cast<std::size_t>(axisIndex)];
            const Vector2 marker = layout.rotateMarker[static_cast<std::size_t>(axisIndex)];
            if (layout.rotateAnchorValid[static_cast<std::size_t>(axisIndex)] &&
                Vector2Distance(anchor, marker) > 3.0f) {
                DrawLineEx(anchor, marker, 2.0f, Color{31, 38, 48, 235});
            }
        }
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            const Color color = highlighted(RotateHandleId(axisIndex))
                                    ? kHighlight : kAxisColors[axisIndex];
            const Rectangle button = gizmolayout::RotateMarkerRect(layout, axisIndex);
            DrawRectangleRounded(button, 0.2f, 4, color);
            DrawRectangleRoundedLinesEx(button, 0.2f, 4, 1.5f, kOutline);
            const char label[2] = {static_cast<char>('X' + axisIndex), '\0'};
            DrawText(label, static_cast<int>(button.x + button.width * 0.5f - 4.0f),
                     static_cast<int>(button.y + button.height * 0.5f - 6.0f), 12,
                     Color{255, 255, 255, 255});
        }
    } else if (mode == ToolMode::Scale) {
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            for (int side : {-1, 1}) {
                const int index = axisIndex * 2 + (side > 0 ? 1 : 0);
                if (!layout.scaleAnchorValid[static_cast<std::size_t>(index)]) continue;
                const Vector2 anchor = layout.scaleAnchor[static_cast<std::size_t>(index)];
                const Vector2 handle = layout.scaleHandle[static_cast<std::size_t>(index)];
                if (Vector2Distance(anchor, handle) > 3.0f) {
                    DrawLineEx(anchor, handle, 1.5f, Color{45, 52, 62, 210});
                }
            }
        }
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            for (int side : {-1, 1}) {
                const int index = axisIndex * 2 + (side > 0 ? 1 : 0);
                const bool highlight =
                    highlighted(gizmomath::ScaleFaceHandleId(axisIndex, side));
                const Color color = highlight ? kHighlight : kAxisColors[axisIndex];
                const Rectangle box = gizmolayout::ScaleHandleRect(layout, index);
                DrawRectangleRounded(box, 0.18f, 3, color);
                DrawRectangleLinesEx(box, 1.5f, Color{35, 40, 48, 255});
                const char label[3] = {static_cast<char>('X' + axisIndex),
                                       static_cast<char>(side > 0 ? '+' : '-'), '\0'};
                const int textWidth = MeasureText(label, 11);
                DrawText(label, static_cast<int>(box.x + box.width * 0.5f - textWidth * 0.5f),
                         static_cast<int>(box.y + box.height * 0.5f - 5.0f), 11,
                         Color{255, 255, 255, 255});
            }
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

#include "editor/GizmoLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "raylib.h"
#include "raymath.h"

#include "rendering/EditorCamera.h"

namespace loblox::gizmolayout {
namespace {

float Distance(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool PointInQuad(Vector2 point, const std::array<Vector2, 4>& corners) {
    bool positive = false, negative = false;
    for (int i = 0; i < 4; ++i) {
        const auto edge = Vector2Subtract(corners[(i + 1) % 4], corners[i]);
        const auto offset = Vector2Subtract(point, corners[i]);
        const float cross = edge.x * offset.y - edge.y * offset.x;
        positive |= cross > 1e-3f;
        negative |= cross < -1e-3f;
    }
    return !(positive && negative);
}

bool InsideViewport(Vector2 point, Vector2 viewport, float margin) {
    return point.x >= -margin && point.y >= -margin && point.x <= viewport.x + margin &&
           point.y <= viewport.y + margin;
}

Vector2 ClampInside(Vector2 point, Vector2 viewport, float margin, float halfWidth,
                    float halfHeight) {
    const float minX = margin + halfWidth;
    const float maxX = viewport.x - margin - halfWidth;
    const float minY = margin + halfHeight;
    const float maxY = viewport.y - margin - halfHeight;
    point.x = maxX < minX ? viewport.x * 0.5f : std::clamp(point.x, minX, maxX);
    point.y = maxY < minY ? viewport.y * 0.5f : std::clamp(point.y, minY, maxY);
    return point;
}

// Direcao em tela do eixo no pivo. O delta projetado de um ponto deslocado de
// uma unidade de gizmo no mundo da a direcao correta inclusive em perspectiva.
Vector2 AxisDirectionOnScreen(const EditorCamera& camera, Vector2 viewport, Vector3 pivot,
                              Vector3 axis, float probe, Vector2 fallback, bool& degenerate) {
    degenerate = true;
    if (Vector3LengthSqr(axis) < 1e-8f) return fallback;
    const Vector3 a = Vector3Add(pivot, Vector3Scale(Vector3Normalize(axis),
                                                     std::max(probe, 1e-4f)));
    Vector2 projected{};
    if (!camera.projectChecked(a, viewport, projected)) return fallback;
    Vector2 start{};
    if (!camera.projectChecked(pivot, viewport, start)) return fallback;
    const Vector2 raw=Vector2Subtract(projected,start);
    if (Vector2Length(raw)<4.f) return fallback;
    const Vector2 delta=Vector2Normalize(raw);
    degenerate = false;
    return delta;
}

void EmitSegments(const std::vector<Vector2>& run, std::vector<Segment>& segments) {
    for (std::size_t i = 1; i < run.size(); ++i) {
        segments.push_back(Segment{run[i - 1], run[i]});
    }
}

}  // namespace

float PixelLength(Vector2 viewportSize, const Settings& settings) {
    const float smallest = std::min(viewportSize.x, viewportSize.y);
    const float reference = std::max(1.0f, settings.referenceViewportMin);
    float length = settings.basePixelLength * (smallest / reference);
    if (!std::isfinite(length) || length <= 0.0f) length = settings.basePixelLength;
    return std::clamp(length, settings.minPixelLength, settings.maxPixelLength);
}

gizmomath::TransformSnapshot MakeSnapshot(const Part& part) {
    Vector3 size = part.size();
    if (part.shape() == PartShape::Sphere) {
        const float diameter = part.sphereRadius() * 2.0f;
        size = Vector3{diameter, diameter, diameter};
    }
    return {part.position(), part.rotation(), size};
}

Layout Build(const Part& part, ToolMode mode, const EditorCamera& camera, Rectangle viewport,
             bool localSpace, const Settings& settings) {
    Layout layout;
    if (mode == ToolMode::Select) return layout;
    // Scale handles identify actual local faces. World/Local only changes
    // the Move/Rotate frame; reassigning size components is never a resize.
    if(mode==ToolMode::Scale)localSpace=true;
    const Vector2 viewportSize{std::max(1.0f, viewport.width), std::max(1.0f, viewport.height)};
    const Vector3 pivot = part.position();
    if (!camera.projectChecked(pivot, viewportSize, layout.center)) {
        // Sem pivo projetavel nao existe geometria valida para desenhar.
        return layout;
    }
    layout.valid = true;
    layout.pixelLength = PixelLength(viewportSize, settings);
    layout.ringRadiusPx = layout.pixelLength * settings.ringRadiusFactor;
    layout.centerInViewport = InsideViewport(layout.center, viewportSize, 0.0f);

    const float worldPerPixel = camera.worldUnitsPerPixel(pivot, viewportSize.y);
    const float maxRadius = settings.maxHandleRadiusFactor *
                            std::min(viewportSize.x, viewportSize.y);
    const auto snapshot = MakeSnapshot(part);

    // Direcoes dos eixos em tela (mesmo em Local, a projecao e do mundo).
    const Vector2 fallbacks[3] = {{0.94f, 0.34f}, {0.0f, -1.0f}, {-0.94f, 0.34f}};
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        const Vector3 axis = gizmomath::Axis(axisIndex, part.rotation(), localSpace);
        bool degenerate = false;
        layout.axisDirection[axisIndex] = AxisDirectionOnScreen(
            camera, viewportSize, pivot, axis, worldPerPixel * layout.pixelLength,
            fallbacks[axisIndex], degenerate);
        layout.axisDirectionFallback[axisIndex] = degenerate;
    }

    // Move always has both signs. Separate nearly collinear projections by
    // length, preserving the projected axes through the pivot (never rotating
    // a handle just to make room). Picking and drawing use these same points.
    float lengths[3] = {layout.pixelLength, layout.pixelLength, layout.pixelLength};
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        for (int previous = 0; previous < axisIndex; ++previous) {
            if (std::fabs(Vector2DotProduct(layout.axisDirection[axisIndex],
                                           layout.axisDirection[previous])) > .94f) {
                lengths[axisIndex] = std::max(lengths[axisIndex],
                    lengths[previous] + settings.axisButtonHalf * 2.5f);
            }
        }
        for (int side : {-1, 1}) {
            const Vector2 direction = Vector2Scale(layout.axisDirection[axisIndex], float(side));
            float length = lengths[axisIndex];
            if (layout.centerInViewport) {
                const float margin = settings.viewportMargin + settings.axisButtonHalf;
                if (direction.x > 1e-5f) length = std::min(length, (viewportSize.x - margin - layout.center.x) / direction.x);
                if (direction.x < -1e-5f) length = std::min(length, (margin - layout.center.x) / direction.x);
                if (direction.y > 1e-5f) length = std::min(length, (viewportSize.y - margin - layout.center.y) / direction.y);
                if (direction.y < -1e-5f) length = std::min(length, (margin - layout.center.y) / direction.y);
            }
            const auto button = Vector2Add(layout.center, Vector2Scale(direction, std::max(0.f, length)));
            layout.moveHandle[axisIndex * 2 + (side > 0)] = button;
            if (side > 0) layout.axisButton[axisIndex] = button;
        }
    }
    const int planeAxes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    for (int plane = 0; plane < 3; ++plane) {
        const Vector2 diagonal = Vector2Add(layout.axisDirection[planeAxes[plane][0]],
                                            layout.axisDirection[planeAxes[plane][1]]);
        Vector2 point = Vector2Add(
            layout.center, Vector2Scale(diagonal, settings.planeOffsetFactor * layout.pixelLength));
        if (layout.centerInViewport) {
            point = ClampInside(point, viewportSize, settings.viewportMargin,
                                settings.planeButtonHalf, settings.planeButtonHalf);
        }
        layout.planeCenter[plane] = point;
        const auto a=layout.axisDirection[planeAxes[plane][0]],b=layout.axisDirection[planeAxes[plane][1]];
        const auto normal=Vector3CrossProduct(gizmomath::Axis(planeAxes[plane][0],part.rotation(),localSpace),
            gizmomath::Axis(planeAxes[plane][1],part.rotation(),localSpace));
        const auto facing=Vector3Normalize(Vector3Subtract(camera.position(),pivot));
        layout.planeVisible[plane]=std::fabs(a.x*b.y-a.y*b.x)>.25f&&std::fabs(Vector3DotProduct(normal,facing))>.16f;
        const float r=settings.planeButtonHalf;
        layout.planeCorners[plane]={Vector2Subtract(point,Vector2Scale(Vector2Add(a,b),r)),
            Vector2Add(point,Vector2Scale(Vector2Subtract(a,b),r)),
            Vector2Add(point,Vector2Scale(Vector2Add(a,b),r)),
            Vector2Add(point,Vector2Scale(Vector2Subtract(b,a),r))};
    }

    // Scale: ancora na face (mundo) e icone em tela. Faces fora da viewport
    // ficam no raio maximo, na direcao do eixo, com guia ate a ancora.
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        for (int side : {-1, 1}) {
            const int index = axisIndex * 2 + (side > 0 ? 1 : 0);
            const Vector3 anchorWorld = gizmomath::ScaleHandleCenter(
                snapshot, axisIndex, side, localSpace,
                worldPerPixel * settings.scaleHandleOffsetPx);
            Vector2 anchorScreen{};
            const bool anchorValid = camera.projectChecked(anchorWorld, viewportSize, anchorScreen);
            layout.scaleAnchor[index] = anchorScreen;
            layout.scaleAnchorValid[index] = anchorValid;

            Vector2 place = anchorValid
                                ? anchorScreen
                                : Vector2Add(layout.center,
                                             Vector2Scale(layout.axisDirection[axisIndex],
                                                          static_cast<float>(side) * maxRadius));
            const Vector2 fromCenter = Vector2Subtract(place, layout.center);
            const float distance = Vector2Length(fromCenter);
            if (std::isfinite(distance) && distance > maxRadius) {
                place = Vector2Add(layout.center, Vector2Scale(fromCenter, maxRadius / distance));
            }
            layout.scaleHandle[index] = place;
        }
    }
    if (layout.centerInViewport) {
        const float reserved = settings.viewportMargin + settings.scaleHandleSeparation * 0.5f;
        for (Vector2& point : layout.scaleHandle) {
            point = ClampInside(point, viewportSize, reserved, settings.scaleHandleHalfWidth,
                                settings.scaleHandleHalfHeight);
        }
    }
    gizmomath::SeparateScreenPoints(layout.scaleHandle.data(),
                                    static_cast<int>(layout.scaleHandle.size()),
                                    settings.scaleHandleSeparation);
    if (layout.centerInViewport) {
        for (Vector2& point : layout.scaleHandle) {
            point = ClampInside(point, viewportSize, settings.viewportMargin,
                                settings.scaleHandleHalfWidth, settings.scaleHandleHalfHeight);
        }
    }

    // Rotate: anel amostrado e projetado. Runs quebrados quando um ponto cai
    // atras do near plane; nenhuma coordenada invalida entra na lista.
    const float ringRadiusWorld = worldPerPixel * layout.ringRadiusPx;
    const float maxRingScreenJump = layout.ringRadiusPx * 2.0f + 8.0f;
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        const Vector3 axis = gizmomath::Axis(axisIndex, part.rotation(), localSpace);
        Vector3 u, v;
        const Vector3 helper = std::fabs(axis.y) < 0.9f ? Vector3{0.0f, 1.0f, 0.0f}
                                                       : Vector3{1.0f, 0.0f, 0.0f};
        u = Vector3Normalize(Vector3CrossProduct(axis, helper));
        v = Vector3Normalize(Vector3CrossProduct(axis, u));

        std::vector<Vector2> run;
        run.reserve(static_cast<std::size_t>(settings.ringSegments) + 1);
        for (int step = 0; step <= settings.ringSegments; ++step) {
            const float angle = 2.0f * PI * static_cast<float>(step) /
                                static_cast<float>(std::max(1, settings.ringSegments));
            const Vector3 world = Vector3Add(
                pivot, Vector3Add(Vector3Scale(u, std::cos(angle) * ringRadiusWorld),
                                  Vector3Scale(v, std::sin(angle) * ringRadiusWorld)));
            Vector2 point{};
            const bool front=Vector3DotProduct(Vector3Subtract(world,pivot),Vector3Subtract(camera.position(),pivot))>=-1e-4f;
            const bool valid = front&&camera.projectChecked(world, viewportSize, point) &&
                               Distance(point, layout.center) <=
                                   layout.pixelLength * 12.0f;
            if (valid) {
                if (!run.empty() && Distance(run.back(), point) > maxRingScreenJump) {
                    EmitSegments(run, layout.ring[axisIndex]);
                    run.clear();
                }
                run.push_back(point);
            } else if (!run.empty()) {
                EmitSegments(run, layout.ring[axisIndex]);
                run.clear();
            }
        }
        if (!run.empty()) EmitSegments(run, layout.ring[axisIndex]);

        // Marcador do eixo: procura um ponto do anel a frente da camera.
        const Vector3 candidates[4] = {
            Vector3Add(pivot, Vector3Scale(u, ringRadiusWorld)),
            Vector3Add(pivot, Vector3Scale(u, -ringRadiusWorld)),
            Vector3Add(pivot, Vector3Scale(v, ringRadiusWorld)),
            Vector3Add(pivot, Vector3Scale(v, -ringRadiusWorld))};
        Vector2 marker{};
        bool anchorValid = false;
        for (const Vector3& candidate : candidates) {
            if(Vector3DotProduct(Vector3Subtract(candidate,pivot),Vector3Subtract(camera.position(),pivot)) < -1e-4f)continue;
            if (camera.projectChecked(candidate, viewportSize, marker)) {
                anchorValid = true;
                break;
            }
        }
        if (!anchorValid) {
            marker = Vector2Add(layout.center,
                                Vector2Scale(layout.axisDirection[axisIndex],
                                             layout.ringRadiusPx));
        }
        layout.rotateAnchor[axisIndex] = marker;
        layout.rotateAnchorValid[axisIndex] = anchorValid;
        layout.rotateMarker[axisIndex] = marker;
    }
    if (layout.centerInViewport) {
        const float reserved = settings.viewportMargin + settings.rotateMarkerSeparation * 0.5f;
        for (Vector2& point : layout.rotateMarker) {
            point = ClampInside(point, viewportSize, reserved, settings.rotateMarkerHalfWidth,
                                settings.rotateMarkerHalfHeight);
        }
    }
    // Duas rodadas de separacao + clamp para que o limite da viewport nao
    // volte a sobrepor os marcadores.
    for (int pass = 0; pass < 2; ++pass) {
        gizmomath::SeparateScreenPoints(layout.rotateMarker.data(),
                                        static_cast<int>(layout.rotateMarker.size()),
                                        settings.rotateMarkerSeparation);
        if (!layout.centerInViewport) break;
        for (Vector2& point : layout.rotateMarker) {
            point = ClampInside(point, viewportSize, settings.viewportMargin,
                                settings.rotateMarkerHalfWidth,
                                settings.rotateMarkerHalfHeight);
        }
    }

    return layout;
}

Rectangle AxisButtonRect(const Layout& layout, int axis, const Settings& settings) {
    const Vector2 center = layout.axisButton[static_cast<std::size_t>(axis)];
    return Rectangle{center.x - settings.axisButtonHalf, center.y - settings.axisButtonHalf,
                     settings.axisButtonHalf * 2.0f, settings.axisButtonHalf * 2.0f};
}

Rectangle PlaneButtonRect(const Layout& layout, int plane, const Settings& settings) {
    const Vector2 center = layout.planeCenter[static_cast<std::size_t>(plane)];
    return Rectangle{center.x - settings.planeButtonHalf, center.y - settings.planeButtonHalf,
                     settings.planeButtonHalf * 2.0f, settings.planeButtonHalf * 2.0f};
}

Rectangle RotateMarkerRect(const Layout& layout, int axis, const Settings& settings) {
    const Vector2 center = layout.rotateMarker[static_cast<std::size_t>(axis)];
    return Rectangle{center.x - settings.rotateMarkerHalfWidth,
                     center.y - settings.rotateMarkerHalfHeight,
                     settings.rotateMarkerHalfWidth * 2.0f,
                     settings.rotateMarkerHalfHeight * 2.0f};
}

Rectangle ScaleHandleRect(const Layout& layout, int index, const Settings& settings) {
    const Vector2 center = layout.scaleHandle[static_cast<std::size_t>(index)];
    return Rectangle{center.x - settings.scaleHandleHalfWidth,
                     center.y - settings.scaleHandleHalfHeight,
                     settings.scaleHandleHalfWidth * 2.0f,
                     settings.scaleHandleHalfHeight * 2.0f};
}

int PickPlane(const Layout& layout, Vector2 mouse, const Settings&) {
    int picked = -1;
    float best = std::numeric_limits<float>::infinity();
    for (int plane = 0; plane < 3; ++plane) {
        if(!layout.planeVisible[plane])continue;
        const Vector2 center = layout.planeCenter[static_cast<std::size_t>(plane)];
        const float dx = std::fabs(mouse.x - center.x);
        const float dy = std::fabs(mouse.y - center.y);
        const auto& q=layout.planeCorners[plane];
        if (!PointInQuad(mouse, q)) continue;
        const float distance = dx * dx + dy * dy;
        if (distance < best) {
            best = distance;
            picked = plane;
        }
    }
    return picked;
}

int PickAxisButton(const Layout& layout, Vector2 mouse, const Settings& settings) {
    int picked = -1;
    float best = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        const Vector2 center = layout.axisButton[static_cast<std::size_t>(axis)];
        const float dx = std::fabs(mouse.x - center.x);
        const float dy = std::fabs(mouse.y - center.y);
        if (dx > settings.axisButtonHalf || dy > settings.axisButtonHalf) continue;
        const float distance = dx * dx + dy * dy;
        if (distance < best) {
            best = distance;
            picked = axis;
        }
    }
    return picked;
}

int PickAxisLine(const Layout& layout, Vector2 mouse, const Settings& settings) {
    int picked = -1;
    float best = settings.axisLineTolerance;
    for (int axis = 0; axis < 3; ++axis) {
        const Vector2 a = layout.center;
        const Vector2 b = layout.axisButton[static_cast<std::size_t>(axis)];
        const Vector2 segment{b.x - a.x, b.y - a.y};
        const Vector2 ap{mouse.x - a.x, mouse.y - a.y};
        const float lengthSqr = segment.x * segment.x + segment.y * segment.y;
        const float t = lengthSqr > 1e-6f
                            ? std::clamp((ap.x * segment.x + ap.y * segment.y) / lengthSqr, 0.0f,
                                         1.0f)
                            : 0.0f;
        const Vector2 closest{a.x + segment.x * t, a.y + segment.y * t};
        const float distance = Distance(mouse, closest);
        if (distance < best) {
            best = distance;
            picked = axis;
        }
    }
    return picked;
}

gizmomath::HandleId PickMoveArrow(const Layout& layout, Vector2 mouse, const Settings& settings) {
    float best = settings.axisButtonHalf;
    auto result = gizmomath::HandleId::None;
    for (int axis = 0; axis < 3; ++axis) for (int side : {-1, 1}) {
        const float distance = Distance(mouse, layout.moveHandle[axis * 2 + (side > 0)]);
        if (distance < best) {
            best = distance;
            result = gizmomath::MoveHandleId(axis, side);
        }
    }
    return result;
}

gizmomath::HandleId PickMoveShaft(const Layout& layout, Vector2 mouse, const Settings& settings) {
    // The pivot is not an implicit free-drag handle.
    if (Distance(mouse, layout.center) < settings.axisLineTolerance * 1.5f)
        return gizmomath::HandleId::None;
    float best = settings.axisLineTolerance;
    auto result = gizmomath::HandleId::None;
    for (int axis = 0; axis < 3; ++axis) for (int side : {-1, 1}) {
        const auto segment = Vector2Subtract(layout.moveHandle[axis * 2 + (side > 0)], layout.center);
        const float lengthSqr = Vector2LengthSqr(segment);
        const float t = lengthSqr > 1e-6f
            ? std::clamp(Vector2DotProduct(Vector2Subtract(mouse, layout.center), segment) / lengthSqr, 0.f, 1.f) : 0.f;
        const float distance = Distance(mouse, Vector2Add(layout.center, Vector2Scale(segment, t)));
        if (distance < best) {
            best = distance;
            result = gizmomath::MoveHandleId(axis, side);
        }
    }
    return result;
}

int PickRotateMarker(const Layout& layout, Vector2 mouse, const Settings& settings) {
    int picked = -1;
    float best = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        const Vector2 center = layout.rotateMarker[static_cast<std::size_t>(axis)];
        const float dx = std::fabs(mouse.x - center.x);
        const float dy = std::fabs(mouse.y - center.y);
        if (dx > settings.rotateMarkerHalfWidth || dy > settings.rotateMarkerHalfHeight) continue;
        const float distance = dx * dx + dy * dy;
        if (distance < best) {
            best = distance;
            picked = axis;
        }
    }
    return picked;
}

int PickRotateRing(const Layout& layout, Vector2 mouse, const Settings& settings) {
    int picked = -1;
    float best = settings.ringTolerance;
    for (int axis = 0; axis < 3; ++axis) {
        for (const Segment& segment : layout.ring[static_cast<std::size_t>(axis)]) {
            const Vector2 ab{segment.b.x - segment.a.x, segment.b.y - segment.a.y};
            const Vector2 ap{mouse.x - segment.a.x, mouse.y - segment.a.y};
            const float lengthSqr = ab.x * ab.x + ab.y * ab.y;
            const float t = lengthSqr > 1e-6f
                                ? std::clamp((ap.x * ab.x + ap.y * ab.y) / lengthSqr, 0.0f, 1.0f)
                                : 0.0f;
            const Vector2 closest{segment.a.x + ab.x * t, segment.a.y + ab.y * t};
            const float distance = Distance(mouse, closest);
            if (distance < best) {
                best = distance;
                picked = axis;
            }
        }
    }
    return picked;
}

gizmomath::HandleId PickScaleHandle(const Layout& layout, Vector2 mouse,
                                    const Settings& settings) {
    int best=-1;float distance=settings.scaleHandleHalfWidth;
    for(int i=0;i<6;++i){const float d=Vector2Distance(mouse,layout.scaleHandle[i]);if(d<=distance){distance=d;best=i;}}
    return best<0?gizmomath::HandleId::None:gizmomath::ScaleFaceHandleId(best/2,best%2?1:-1);
}

}  // namespace loblox::gizmolayout

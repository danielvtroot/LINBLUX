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
    if(mode==ToolMode::Select)return layout;
    if(mode==ToolMode::Scale)localSpace=true;
    const Vector2 size{std::max(1.f,viewport.width),std::max(1.f,viewport.height)};
    const auto pivot=part.position();
    if(!camera.projectChecked(pivot,size,layout.center))return layout;
    layout.valid=true;layout.centerInViewport=InsideViewport(layout.center,size,0);
    layout.pixelLength=PixelLength(size,settings);layout.ringRadiusPx=layout.pixelLength*settings.ringRadiusFactor;
    const float units=camera.worldUnitsPerPixel(pivot,size.y);
    const Vector2 fallbacks[3]={{.94f,.34f},{0,-1},{-.94f,.34f}};
    for(int axis=0;axis<3;++axis)
        layout.axisDirection[axis]=AxisDirectionOnScreen(camera,size,pivot,gizmomath::Axis(axis,part.rotation(),localSpace),
            units*layout.pixelLength,fallbacks[axis],layout.axisDirectionFallback[axis]);
    float radii[3]={layout.pixelLength,layout.pixelLength,layout.pixelLength};
    for(int axis=0;axis<3;++axis){
        for(int previous=0;previous<axis;++previous)
            if(std::fabs(Vector2DotProduct(layout.axisDirection[axis],layout.axisDirection[previous]))>.86f)
                radii[axis]=std::min(radii[axis],radii[previous]-settings.scaleHandleSeparation);
        radii[axis]=std::max(18.f,radii[axis]);
        for(int side:{-1,1}){
            const int index=axis*2+(side>0);
            auto point=Vector2Add(layout.center,Vector2Scale(layout.axisDirection[axis],side*radii[axis]));
            point=ClampInside(point,size,settings.viewportMargin,settings.axisButtonHalf,settings.axisButtonHalf);
            layout.moveHandle[index]=point;layout.moveVisible[index]=side>0;
            if(side>0)layout.axisButton[axis]=point;
            layout.scaleHandle[index]=ClampInside(Vector2Add(layout.center,Vector2Scale(layout.axisDirection[axis],side*radii[axis]*.84f)),
                size,settings.viewportMargin,settings.scaleHandleHalfWidth,settings.scaleHandleHalfHeight);
        }
    }
    // Separate compact face controls, never distribute them using Part extents.
    for(int pass=0;pass<4;++pass){
        gizmomath::SeparateScreenPoints(layout.scaleHandle.data(),6,settings.scaleHandleSeparation);
        for(auto& point:layout.scaleHandle)point=ClampInside(point,size,settings.viewportMargin,settings.scaleHandleHalfWidth,settings.scaleHandleHalfHeight);
    }
    const int axes[3][2]={{0,1},{0,2},{1,2}};
    for(int plane=0;plane<3;++plane){
        const auto a=layout.axisDirection[axes[plane][0]],b=layout.axisDirection[axes[plane][1]];
        const auto point=Vector2Add(layout.center,Vector2Scale(Vector2Add(a,b),settings.planeOffsetFactor*layout.pixelLength));
        layout.planeCenter[plane]=point;
        const auto normal=Vector3CrossProduct(gizmomath::Axis(axes[plane][0],part.rotation(),localSpace),gizmomath::Axis(axes[plane][1],part.rotation(),localSpace));
        layout.planeVisible[plane]=std::fabs(a.x*b.y-a.y*b.x)>.3f&&std::fabs(Vector3DotProduct(normal,Vector3Normalize(Vector3Subtract(camera.position(),pivot))))>.18f;
        const float r=settings.planeButtonHalf;
        layout.planeCorners[plane]={Vector2Subtract(point,Vector2Scale(Vector2Add(a,b),r)),
            Vector2Add(point,Vector2Scale(Vector2Subtract(a,b),r)),Vector2Add(point,Vector2Scale(Vector2Add(a,b),r)),Vector2Add(point,Vector2Scale(Vector2Subtract(b,a),r))};
    }
    for(int axisIndex=0;axisIndex<3;++axisIndex){
        const auto axis=gizmomath::Axis(axisIndex,part.rotation(),localSpace);
        const auto helper=std::fabs(axis.y)<.9f?Vector3{0,1,0}:Vector3{1,0,0};
        const auto u=Vector3Normalize(Vector3CrossProduct(axis,helper)),v=Vector3Normalize(Vector3CrossProduct(axis,u));
        std::vector<Vector2> run;
        for(int step=0;step<=settings.ringSegments;++step){
            const float angle=2*PI*step/std::max(1,settings.ringSegments);
            const auto world=Vector3Add(pivot,Vector3Scale(Vector3Add(Vector3Scale(u,std::cos(angle)),Vector3Scale(v,std::sin(angle))),units*layout.ringRadiusPx));
            Vector2 point{};
            const bool front=Vector3DotProduct(Vector3Subtract(world,pivot),Vector3Subtract(camera.position(),pivot))>=-1e-4f;
            const bool valid=front&&camera.projectChecked(world,size,point)&&Distance(point,layout.center)<layout.pixelLength*1.5f;
            if(valid)run.push_back(point);else{EmitSegments(run,layout.ring[axisIndex]);run.clear();}
        }
        EmitSegments(run,layout.ring[axisIndex]);
        float best=-1;
        for(const auto& segment:layout.ring[axisIndex]){
            const auto point=Vector2Scale(Vector2Add(segment.a,segment.b),.5f);
            if(!InsideViewport(point,size,-settings.rotateMarkerHalfWidth-3))continue;
            float score=Distance(point,layout.center);
            for(int other=0;other<axisIndex;++other)score=std::min(score,Distance(point,layout.rotateMarker[other]));
            if(score>best){best=score;layout.rotateMarker[axisIndex]=point;}
        }
        if(best<0)layout.rotateMarker[axisIndex]=ClampInside(layout.center,size,settings.viewportMargin,settings.rotateMarkerHalfWidth,settings.rotateMarkerHalfHeight);
        layout.rotateAnchor[axisIndex]=layout.rotateMarker[axisIndex];layout.rotateAnchorValid[axisIndex]=best>=0;
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

std::array<Vector2,3> MoveArrowCorners(const Layout& layout,int axis,const Settings& settings) {
    const auto p=layout.moveHandle[axis*2+1],d=Vector2Normalize(Vector2Subtract(p,layout.center));const Vector2 n{-d.y,d.x};
    const float scale=settings.axisButtonHalf/10;
    return {Vector2Add(p,Vector2Scale(d,7*scale)),Vector2Add(Vector2Subtract(p,Vector2Scale(d,5*scale)),Vector2Scale(n,5*scale)),Vector2Subtract(Vector2Subtract(p,Vector2Scale(d,5*scale)),Vector2Scale(n,5*scale))};
}
gizmomath::HandleId PickMoveArrow(const Layout& layout, Vector2 mouse,const Settings& settings) {
    for(int axis=0;axis<3;++axis){const auto p=MoveArrowCorners(layout,axis,settings);
        if(CheckCollisionPointTriangle(mouse,p[0],p[1],p[2]))return gizmomath::MoveHandleId(axis,1);}
    return gizmomath::HandleId::None;
}

gizmomath::HandleId PickMoveShaft(const Layout& layout, Vector2 mouse, const Settings& settings) {
    // The pivot is not an implicit free-drag handle.
    if (Distance(mouse, layout.center) < settings.axisLineTolerance * 1.5f)
        return gizmomath::HandleId::None;
    float best = settings.axisLineTolerance;
    auto result = gizmomath::HandleId::None;
    for (int axis = 0; axis < 3; ++axis) for (int side : {-1, 1}) {
        if(!layout.moveVisible[axis*2+(side>0)])continue;
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
    int best=-1;float distance=std::numeric_limits<float>::infinity();
    for(int i=0;i<6;++i){if(!CheckCollisionPointRec(mouse,ScaleHandleRect(layout,i,settings)))continue;
        const float d=Vector2Distance(mouse,layout.scaleHandle[i]);if(d<distance){distance=d;best=i;}}
    return best<0?gizmomath::HandleId::None:gizmomath::ScaleFaceHandleId(best/2,best%2?1:-1);
}

}  // namespace loblox::gizmolayout

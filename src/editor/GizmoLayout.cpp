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

namespace {
void triangle(Mesh& mesh,Vector3 a,Vector3 b,Vector3 c,const EditorCamera& camera,Vector2 size){
    Triangle t;t.world={a,b,c};
    if(!camera.projectChecked(a,size,t.screen[0])||!camera.projectChecked(b,size,t.screen[1])||!camera.projectChecked(c,size,t.screen[2]))return;
    t.depth=(camera.depthOf(a)+camera.depthOf(b)+camera.depthOf(c))/3;mesh.triangles.push_back(t);
}
void cylinder(Mesh& mesh,Vector3 a,Vector3 b,float ra,float rb,const EditorCamera& camera,Vector2 size,int sides=12){
    const auto axis=Vector3Normalize(Vector3Subtract(b,a));const auto helper=std::fabs(axis.y)<.9f?Vector3{0,1,0}:Vector3{1,0,0};
    const auto u=Vector3Normalize(Vector3CrossProduct(axis,helper)),v=Vector3CrossProduct(axis,u);
    for(int i=0;i<sides;++i){const float x=2*PI*i/sides,y=2*PI*(i+1)/sides;
        const auto r=Vector3Add(Vector3Scale(u,std::cos(x)),Vector3Scale(v,std::sin(x))),q=Vector3Add(Vector3Scale(u,std::cos(y)),Vector3Scale(v,std::sin(y)));
        const auto p0=Vector3Add(a,Vector3Scale(r,ra)),p1=Vector3Add(a,Vector3Scale(q,ra)),p2=Vector3Add(b,Vector3Scale(q,rb)),p3=Vector3Add(b,Vector3Scale(r,rb));
        triangle(mesh,p0,p1,p2,camera,size);triangle(mesh,p0,p2,p3,camera,size);triangle(mesh,a,p1,p0,camera,size);if(rb>0)triangle(mesh,b,p3,p2,camera,size);
    }
}
std::vector<Vector3> clipFace(std::vector<Vector3> points,const EditorCamera& camera,Vector2 size){
    const auto view=camera.camera();const auto f=Vector3Normalize(Vector3Subtract(view.target,view.position));
    const auto r=Vector3Normalize(Vector3CrossProduct(f,view.up)),u=Vector3CrossProduct(r,f);
    const float h=std::tan(view.fovy*DEG2RAD*.5f),w=h*size.x/size.y;
    const Vector3 normals[5]={f,Vector3Add(r,Vector3Scale(f,w)),Vector3Add(Vector3Negate(r),Vector3Scale(f,w)),Vector3Add(u,Vector3Scale(f,h)),Vector3Add(Vector3Negate(u),Vector3Scale(f,h))};
    for(int plane=0;plane<5&&!points.empty();++plane){std::vector<Vector3> next;const auto n=normals[plane];
        const auto distance=[&](Vector3 p){return Vector3DotProduct(Vector3Subtract(p,view.position),n)-(plane==0?.11f:0.f);};
        auto a=points.back();float da=distance(a);
        for(auto b:points){const float db=distance(b);if((da>=0)!=(db>=0))next.push_back(Vector3Lerp(a,b,da/(da-db)));if(db>=0)next.push_back(b);a=b;da=db;}points=std::move(next);
    }return points;
}
float depthAt(Vector2 p,const Triangle& t){
    const auto a=t.screen[0],b=t.screen[1],c=t.screen[2];const float den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
    if(std::fabs(den)<1e-6f)return INFINITY;
    const float x=((b.y-c.y)*(p.x-c.x)+(c.x-b.x)*(p.y-c.y))/den,y=((c.y-a.y)*(p.x-c.x)+(a.x-c.x)*(p.y-c.y))/den;
    return x>=-.001f&&y>=-.001f&&x+y<=1.001f?t.depth:INFINITY;
}
Vector2 probe(const Mesh& mesh,Vector2 center){
    Vector2 result=center;float score=-1;
    for(const auto& t:mesh.triangles){const auto p=Vector2Scale(Vector2Add(Vector2Add(t.screen[0],t.screen[1]),t.screen[2]),1.f/3);
        const float value=Vector2Distance(p,center);if(value>score){score=value;result=p;}}
    return result;
}
}
int PickMesh(const std::array<Mesh,3>& meshes,Vector2 mouse){int best=-1;float depth=INFINITY;
    for(int axis=0;axis<3;++axis)for(const auto& t:meshes[axis].triangles){const float d=depthAt(mouse,t);if(d<depth){depth=d;best=axis;}}return best;
}
FacePick FaceUnderCursor(const Layout& layout,Vector2 mouse){
    FacePick hit;if(mouse.x<0||mouse.y<0||mouse.x>layout.viewport.x||mouse.y>layout.viewport.y)return hit;
    const auto ray=GetScreenToWorldRayEx(mouse,layout.camera,int(layout.viewport.x),int(layout.viewport.y));
    if(layout.sphere){const auto h=GetRayCollisionSphere(ray,layout.pivot,layout.sphereRadius);if(!h.hit)return hit;
        float best=-1;for(int index=0;index<6;++index){const float dot=Vector3DotProduct(h.normal,layout.faces[index].normal);if(dot>best){best=dot;hit.index=index;}}
        hit.point=h.point;hit.distance=h.distance;return hit;}
    for(int i=0;i<6;++i){const auto& face=layout.faces[i];if(!face.visible)continue;
        const float den=Vector3DotProduct(ray.direction,face.normal);if(std::fabs(den)<1e-6f)continue;
        const float distance=Vector3DotProduct(Vector3Subtract(face.center,ray.position),face.normal)/den;if(distance<0||distance>=hit.distance)continue;
        bool inside=true;float sign=0;for(size_t k=0;k<face.projected.size();++k){auto a=face.projected[k],b=face.projected[(k+1)%face.projected.size()];const float cross=(b.x-a.x)*(mouse.y-a.y)-(b.y-a.y)*(mouse.x-a.x);if(std::fabs(cross)>.1f){if(sign*cross<0){inside=false;break;}sign=cross;}}
        if(inside){hit={i,Vector3Add(ray.position,Vector3Scale(ray.direction,distance)),distance};}
    }return hit;
}
Mesh FaceGrip(const Part&,const EditorCamera& camera,Rectangle viewport,Vector3 point,Vector3 normal){
    Mesh result;const Vector2 size{viewport.width,viewport.height};const float unit=camera.worldUnitsPerPixel(point,size.y);
    const auto start=Vector3Add(point,Vector3Scale(normal,unit*2)),end=Vector3Add(point,Vector3Scale(normal,unit*33));
    cylinder(result,start,end,unit*4,unit*4,camera,size);
    cylinder(result,end,Vector3Add(point,Vector3Scale(normal,unit*54)),unit*12,0,camera,size,20);return result;
}
Layout Build(const Part& part,ToolMode mode,const EditorCamera& camera,Rectangle viewport,bool localSpace,const Settings& settings){
    Layout layout;if(mode==ToolMode::Select)return layout;
    layout.camera=camera.camera();layout.viewport={std::max(1.f,viewport.width),std::max(1.f,viewport.height)};layout.pivot=part.position();
    layout.pixelLength=PixelLength(layout.viewport,settings);layout.ringRadiusPx=layout.pixelLength*.86f;
    const bool pivotValid=camera.projectChecked(part.position(),layout.viewport,layout.center);layout.centerInViewport=pivotValid&&InsideViewport(layout.center,layout.viewport,0);
    const auto snapshot=MakeSnapshot(part);const auto corners=OrientedBox{snapshot.position,snapshot.rotation,HalfExtents(snapshot.size)}.corners();
    const int indices[6][4]={{0,4,7,3},{1,2,6,5},{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2}};
    layout.sphere=part.shape()==PartShape::Sphere;layout.sphereRadius=part.sphereRadius();
    for(int index=0;index<6;++index){auto& face=layout.faces[index];const int axis=index/2,side=index%2?1:-1;
        face.normal=Vector3Scale(gizmomath::Axis(axis,part.rotation(),true),float(side));face.center=Vector3Add(part.position(),Vector3Scale(face.normal,(&snapshot.size.x)[axis]*.5f));
        if(Vector3DotProduct(face.normal,Vector3Subtract(camera.position(),face.center))<=0)continue;
        std::vector<Vector3> polygon;for(int i:indices[index])polygon.push_back(corners[i]);face.polygon=clipFace(std::move(polygon),camera,layout.viewport);
        for(const auto p:face.polygon){Vector2 q;if(camera.projectChecked(p,layout.viewport,q))face.projected.push_back(q);}
        if(face.projected.size()<3)continue;
        face.visible=true;layout.valid=true;Vector2 anchor{};
        if(!camera.projectChecked(face.center,layout.viewport,anchor)||!InsideViewport(anchor,layout.viewport,-24)){
            anchor={};for(auto p:face.projected)anchor=Vector2Add(anchor,p);anchor=Vector2Scale(anchor,1.f/face.projected.size());}
        const auto ray=camera.rayFromScreen(anchor,layout.viewport);const float den=Vector3DotProduct(ray.direction,face.normal);
        face.anchor=std::fabs(den)>1e-6f?Vector3Add(ray.position,Vector3Scale(ray.direction,Vector3DotProduct(Vector3Subtract(face.center,ray.position),face.normal)/den)):face.center;
        face.screen=anchor;layout.scaleHandle[index]=anchor;
    }
    if(mode==ToolMode::Scale)return layout;
    layout.valid=pivotValid;if(!layout.valid)return layout;
    const float unit=camera.worldUnitsPerPixel(part.position(),layout.viewport.y);
    const Vector2 fallbacks[3]={{.94f,.34f},{0,-1},{-.94f,.34f}};
    for(int axis=0;axis<3;++axis){const auto direction=gizmomath::Axis(axis,part.rotation(),localSpace);
        layout.axisDirection[axis]=AxisDirectionOnScreen(camera,layout.viewport,part.position(),direction,unit*layout.pixelLength,fallbacks[axis],layout.axisDirectionFallback[axis]);
        if(mode==ToolMode::Move){
            const auto base=Vector3Add(part.position(),Vector3Scale(direction,unit*13)),neck=Vector3Add(part.position(),Vector3Scale(direction,unit*(layout.pixelLength-22))),tip=Vector3Add(part.position(),Vector3Scale(direction,unit*layout.pixelLength));
            cylinder(layout.shaft[axis],base,neck,unit*3.5f,unit*3.5f,camera,layout.viewport);
            cylinder(layout.arrow[axis],neck,tip,unit*10,0,camera,layout.viewport,20);
            layout.moveVisible[axis*2+1]=true;layout.moveHandle[axis*2+1]=probe(layout.arrow[axis],layout.center);layout.axisButton[axis]=layout.moveHandle[axis*2+1];
        }else{
            const auto helper=std::fabs(direction.y)<.9f?Vector3{0,1,0}:Vector3{1,0,0};const auto u=Vector3Normalize(Vector3CrossProduct(direction,helper)),v=Vector3CrossProduct(direction,u);
            bool previousValid=false;Vector3 previous{};Vector2 prior{};
            for(int step=0;step<=settings.ringSegments;++step){const float angle=2*PI*step/settings.ringSegments;
                const auto world=Vector3Add(part.position(),Vector3Scale(Vector3Add(Vector3Scale(u,std::cos(angle)),Vector3Scale(v,std::sin(angle))),unit*layout.ringRadiusPx));Vector2 screen{};
                const bool valid=Vector3DotProduct(Vector3Subtract(world,part.position()),Vector3Subtract(camera.position(),part.position()))>=0&&camera.projectChecked(world,layout.viewport,screen);
                if(valid&&previousValid){layout.ring[axis].push_back({prior,screen});cylinder(layout.rotation[axis],previous,world,unit*2.7f,unit*2.7f,camera,layout.viewport,6);}
                previous=world;prior=screen;previousValid=valid;
            }
            layout.rotateMarker[axis]=probe(layout.rotation[axis],layout.center);
        }
    }
    const int axes[3][2]={{0,1},{0,2},{1,2}};
    for(int plane=0;plane<3;++plane){const auto a=gizmomath::Axis(axes[plane][0],part.rotation(),localSpace),b=gizmomath::Axis(axes[plane][1],part.rotation(),localSpace);const auto n=Vector3CrossProduct(a,b);
        layout.planeVisible[plane]=std::fabs(Vector3DotProduct(Vector3Normalize(Vector3Subtract(camera.position(),part.position())),n))>.32f;
        const float low=unit*23,high=unit*37;const Vector3 world[4]={Vector3Add(part.position(),Vector3Add(Vector3Scale(a,low),Vector3Scale(b,low))),Vector3Add(part.position(),Vector3Add(Vector3Scale(a,high),Vector3Scale(b,low))),Vector3Add(part.position(),Vector3Add(Vector3Scale(a,high),Vector3Scale(b,high))),Vector3Add(part.position(),Vector3Add(Vector3Scale(a,low),Vector3Scale(b,high)))};
        layout.planeCenter[plane]={};for(int k=0;k<4;++k){if(!camera.projectChecked(world[k],layout.viewport,layout.planeCorners[plane][k]))layout.planeVisible[plane]=false;layout.planeCenter[plane]=Vector2Add(layout.planeCenter[plane],Vector2Scale(layout.planeCorners[plane][k],.25f));}
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
gizmomath::HandleId PickMoveArrow(const Layout& layout,Vector2 mouse,const Settings&) {const int axis=PickMesh(layout.arrow,mouse);return axis<0?gizmomath::HandleId::None:gizmomath::MoveHandleId(axis,1);}
gizmomath::HandleId PickMoveShaft(const Layout& layout,Vector2 mouse,const Settings&) {const int axis=PickMesh(layout.shaft,mouse);return axis<0?gizmomath::HandleId::None:gizmomath::MoveHandleId(axis,1);}

int PickRotateMarker(const Layout& layout,Vector2 mouse,const Settings&) {return PickMesh(layout.rotation,mouse);}
int PickRotateRing(const Layout& layout,Vector2 mouse,const Settings&) {return PickMesh(layout.rotation,mouse);}
gizmomath::HandleId PickScaleHandle(const Layout& layout,Vector2 mouse,const Settings&) {const auto face=FaceUnderCursor(layout,mouse);return face.index<0?gizmomath::HandleId::None:gizmomath::ScaleFaceHandleId(face.index/2,face.index%2?1:-1);}
} // namespace loblox::gizmolayout

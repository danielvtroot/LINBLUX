#include "editor/GizmoMath.h"
#include "core/Geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace loblox::gizmomath {

namespace {
std::array<Vector3,3> BoxAxes(const TransformSnapshot& box) {
    const auto q=UnitQuaternion(box.rotation);
    return {Vector3RotateByQuaternion({1,0,0},q),Vector3RotateByQuaternion({0,1,0},q),
            Vector3RotateByQuaternion({0,0,1},q)};
}
float Radius(const TransformSnapshot& box,Vector3 axis) {
    const auto basis=BoxAxes(box);const auto h=HalfExtents(box.size);
    return std::fabs(Vector3DotProduct(axis,basis[0]))*h.x+
           std::fabs(Vector3DotProduct(axis,basis[1]))*h.y+
           std::fabs(Vector3DotProduct(axis,basis[2]))*h.z;
}
bool ValidBox(const TransformSnapshot& b) {
    for(float f:{b.position.x,b.position.y,b.position.z,b.size.x,b.size.y,b.size.z,
                 b.rotation.x,b.rotation.y,b.rotation.z,b.rotation.w})if(!std::isfinite(f))return false;
    return b.size.x>0&&b.size.y>0&&b.size.z>0&&QuaternionDotProduct(b.rotation,b.rotation)>1e-10f;
}
bool NearBoxes(const TransformSnapshot& a,const TransformSnapshot& b,float margin) {
    const auto d=Vector3Subtract(a.position,b.position);
    for(int i=0;i<3;++i){Vector3 n{};(&n.x)[i]=1;
        if(std::fabs((&d.x)[i])>Radius(a,n)+Radius(b,n)+margin)return false;}
    return true;
}
// Tolerância absoluta pequena com parcela proporcional à precisão float nas coordenadas.
float ContactEpsilon(const TransformSnapshot& a,const TransformSnapshot& b) {
    float scale=1;
    for(float x:{a.position.x,a.position.y,a.position.z,b.position.x,b.position.y,b.position.z,
                 a.size.x,a.size.y,a.size.z,b.size.x,b.size.y,b.size.z})scale=std::max(scale,std::fabs(x));
    return std::max(1e-5f,scale*4*std::numeric_limits<float>::epsilon());
}
}

float BoxSeparation(const TransformSnapshot& a,const TransformSnapshot& b) {
    if(!ValidBox(a)||!ValidBox(b))return std::numeric_limits<float>::infinity();
    const auto aa=BoxAxes(a),bb=BoxAxes(b);const auto delta=Vector3Subtract(a.position,b.position);
    float gap=-std::numeric_limits<float>::infinity();
    const auto test=[&](Vector3 n){if(Vector3LengthSqr(n)<1e-10f)return;
        n=Vector3Normalize(n);gap=std::max(gap,std::fabs(Vector3DotProduct(delta,n))-Radius(a,n)-Radius(b,n));};
    for(auto n:aa)test(n);for(auto n:bb)test(n);
    for(auto u:aa)for(auto v:bb)test(Vector3CrossProduct(u,v));
    return gap;
}

SurfaceSnap SnapSurfaces(const std::vector<SurfaceBox>& moving,const std::vector<SurfaceBox>& targets,
                         Vector3 axisA,Vector3 axisB,float tolerance) {
    SurfaceSnap best;
    if(!std::isfinite(tolerance)||tolerance<=0||Vector3LengthSqr(axisA)<1e-8f)return best;
    axisA=Vector3Normalize(axisA);
    axisB=Vector3Subtract(axisB,Vector3Scale(axisA,Vector3DotProduct(axisA,axisB)));
    axisB=Vector3LengthSqr(axisB)>1e-8f?Vector3Normalize(axisB):Vector3{};
    float distance=tolerance+1e-6f;
    for(const auto& source:moving)for(const auto& target:targets) {
        if(source.id==target.id||!ValidBox(source.pose)||!ValidBox(target.pose)||
           !NearBoxes(source.pose,target.pose,tolerance))continue;
        const auto sa=BoxAxes(source.pose),ta=BoxAxes(target.pose);
        for(int face=0;face<6;++face) {
            auto normal=face<3?ta[face]:sa[face-3];
            const auto centers=Vector3Subtract(source.pose.position,target.pose.position);
            if(Vector3DotProduct(centers,normal)<0)normal=Vector3Negate(normal);
            const float gap=Vector3DotProduct(centers,normal)-Radius(source.pose,normal)-Radius(target.pose,normal);
            const auto projected=Vector3Add(Vector3Scale(axisA,Vector3DotProduct(normal,axisA)),
                                            Vector3Scale(axisB,Vector3DotProduct(normal,axisB)));
            const float denominator=Vector3LengthSqr(projected);if(denominator<1e-6f)continue;
            const auto correction=Vector3Scale(projected,-gap/denominator);
            const float length=Vector3Length(correction);if(length>distance)continue;
            auto candidate=source.pose;candidate.position=Vector3Add(candidate.position,correction);
            const float epsilon=ContactEpsilon(candidate,target.pose);
            if(std::fabs(BoxSeparation(candidate,target.pose))>epsilon)continue;
            // Não encaixar uma face deixando outra peça do conjunto dentro de um obstáculo.
            bool safe=true;
            for(const auto& member:moving) {
                auto pose=member.pose;pose.position=Vector3Add(pose.position,correction);
                for(const auto& obstacle:targets)if(member.id!=obstacle.id&&ValidBox(obstacle.pose)&&
                    NearBoxes(pose,obstacle.pose,0)&&BoxSeparation(pose,obstacle.pose)<-ContactEpsilon(pose,obstacle.pose)){
                    safe=false;break;
                }
                if(!safe)break;
            }
            if(!safe)continue;
            // Projeta o centro na face alvo e limita-o ao retângulo para indicar o contato.
            auto point=Vector3Scale(Vector3Add(candidate.position,target.pose.position),.5f);
            const auto offset=Vector3Subtract(point,target.pose.position);point=target.pose.position;
            for(int i=0;i<3;++i)point=Vector3Add(point,Vector3Scale(ta[i],std::clamp(Vector3DotProduct(offset,ta[i]),
                -(&target.pose.size.x)[i]*.5f,(&target.pose.size.x)[i]*.5f)));
            point=Vector3Add(point,Vector3Scale(normal,Radius(target.pose,normal)-Vector3DotProduct(Vector3Subtract(point,target.pose.position),normal)));
            best={true,correction,point,normal,target.id};distance=length;
        }
    }
    return best;
}

Vector3 SnapGrid(Vector3 position,Vector3 origin,Vector3 a,Vector3 b,float step,GridMode mode) {
    if(!(step>0)||!std::isfinite(step))return position;
    const Vector3 base=mode==GridMode::Absolute?Vector3{}:origin;
    auto delta=Vector3Subtract(position,base);
    // Quantize only the active world/local basis, preserving inactive axes.
    for(auto axis:{a,b})if(Vector3LengthSqr(axis)>1e-8f) {
        axis=Vector3Normalize(axis);const float d=Vector3DotProduct(delta,axis);
        position=Vector3Add(position,Vector3Scale(axis,SnapDelta(d,step)-d));
    }
    return position;
}

SurfaceSnap SnapUnderCursor(const std::vector<SurfaceBox>& moving,
    const std::vector<SurfaceBox>& targets,Vector3 reference,Ray ray,SnapFeature feature,
    bool alignNormal,Quaternion orientation,std::uint64_t previous,float offset,float grid,bool closestFace) {
    SurfaceSnap out;if(!std::isfinite(offset)||!std::isfinite(grid))return out;
    if(moving.empty()) {out.reason="Esferas: encaixe de caixa indisponível";return out;}
    for(const auto& member:moving)if(!ValidBox(member.pose))return out;
    for(float value:{reference.x,reference.y,reference.z,ray.position.x,ray.position.y,ray.position.z,
        ray.direction.x,ray.direction.y,ray.direction.z})if(!std::isfinite(value))return out;
    if(Vector3LengthSqr(ray.direction)<1e-8f)return out;
    ray.direction=Vector3Normalize(ray.direction);
    const SurfaceBox* target=nullptr;RayCollision closest{};closest.distance=INFINITY;
    for(const auto& box:targets) {
        if(!ValidBox(box.pose))continue;
        bool own=false;for(const auto& member:moving)own|=member.id==box.id;if(own)continue;
        const auto hit=OrientedBox{box.pose.position,box.pose.rotation,HalfExtents(box.pose.size)}.intersect(ray);
        // Stable ties: retain previous target only within a tiny depth band.
        if(hit.hit&&(hit.distance<closest.distance-.001f ||
            (std::fabs(hit.distance-closest.distance)<=.001f&&(!target||box.id==previous||
             (target->id!=previous&&box.id<target->id))))) {target=&box;closest=hit;}
    }
    if(!target)return out;
    out.target=target->id;out.point=closest.point;out.normal=Vector3Normalize(closest.normal);
    if(alignNormal) {
        auto up=Vector3RotateByQuaternion({0,1,0},orientation);
        if(closestFace){float best=-1;for(int i=0;i<3;++i){const auto axis=Axis(i,orientation,true);const float dot=Vector3DotProduct(axis,out.normal);
            if(std::fabs(dot)>best){best=std::fabs(dot);up=Vector3Scale(axis,dot<0?-1.f:1.f);}}}
        out.rotationDelta=Vector3DotProduct(up,out.normal)<-.99999f?
            QuaternionFromAxisAngle(Vector3RotateByQuaternion({1,0,0},orientation),PI):
            QuaternionFromVector3ToVector3(up,out.normal);
    }
    const auto axes=BoxAxes(target->pose);const auto half=HalfExtents(target->pose.size);
    auto local=Vector3RotateByQuaternion(Vector3Subtract(closest.point,target->pose.position),QuaternionInvert(target->pose.rotation));
    int face=0;for(int i=1;i<3;++i)if(std::fabs(Vector3DotProduct(out.normal,axes[i]))>
        std::fabs(Vector3DotProduct(out.normal,axes[face])))face=i;
    if(grid>0){for(int i=0;i<3;++i)if(i!=face)(&local.x)[i]=std::clamp(SnapDelta((&local.x)[i],grid),-(&half.x)[i],(&half.x)[i]);
        out.point=Vector3Add(target->pose.position,Vector3RotateByQuaternion(local,target->pose.rotation));}
    int targetEdge=-1;
    if(feature!=SnapFeature::Face) {
        int edge=-1;float distance=INFINITY;
        for(int i=0;i<3;++i)if(i!=face) {
            const float d=(&half.x)[i]-std::fabs((&local.x)[i]);
            if(d<distance){distance=d;edge=i;}
        }
        targetEdge=edge;
        for(int i=0;i<3;++i)if(i!=face&&(feature==SnapFeature::Vertex||i==edge))
            (&local.x)[i]=std::copysign((&half.x)[i],(&local.x)[i]);
        out.point=Vector3Add(target->pose.position,Vector3RotateByQuaternion(local,target->pose.rotation));
    }
    std::vector<SurfaceBox> poses=moving;
    float support=-INFINITY;
    for(auto& member:poses) {
        member.pose.position=Vector3Add(reference,Vector3RotateByQuaternion(Vector3Subtract(member.pose.position,reference),out.rotationDelta));
        member.pose.rotation=UnitQuaternion(QuaternionMultiply(out.rotationDelta,member.pose.rotation));
        support=std::max(support,Radius(member.pose,out.normal)-Vector3DotProduct(Vector3Subtract(member.pose.position,reference),out.normal));
    }
    Vector3 desired=Vector3Add(out.point,Vector3Scale(out.normal,support));
    if(feature==SnapFeature::Face||closestFace) {
        float nearest=INFINITY;Vector3 anchor{};
        for(const auto& member:poses) {
            Vector3 sum{};int count=0;
            for(const auto corner:OrientedBox{member.pose.position,member.pose.rotation,HalfExtents(member.pose.size)}.corners())
                if(std::fabs(Vector3DotProduct(Vector3Subtract(corner,reference),out.normal)+support)<=.001f) {
                    sum=Vector3Add(sum,corner);++count;
                }
            if(count) {const auto point=Vector3Scale(sum,1.f/count);
                const float distance=Vector3DistanceSqr(point,out.point);
                if(distance<nearest){nearest=distance;anchor=point;}}
        }
        if(std::isfinite(nearest))desired=Vector3Add(reference,Vector3Subtract(out.point,anchor));
    }
    if(feature!=SnapFeature::Face&&!closestFace) {
        // Align the nearest support vertex/edge of the moving selection with
        // the requested target feature, without ever entering the target.
        float nearest=INFINITY;Vector3 anchor{};
        for(const auto& member:poses)for(const auto corner:OrientedBox{member.pose.position,member.pose.rotation,HalfExtents(member.pose.size)}.corners()) {
            if(std::fabs(Vector3DotProduct(Vector3Subtract(corner,reference),out.normal)+support)>.001f)continue;
            const float d=Vector3DistanceSqr(corner,out.point);if(d<nearest){nearest=d;anchor=corner;}
        }
        if(std::isfinite(nearest))desired=Vector3Add(reference,Vector3Subtract(out.point,anchor));
    }
    if(closestFace&&feature!=SnapFeature::Face){
        // Construction edges align outer support planes, keeping the wall
        // inside the floor boundary instead of jumping to a distant corner.
        for(int i=0;i<3;++i)if(i!=face&&(feature==SnapFeature::Vertex||i==targetEdge)){
            const auto outward=Vector3Scale(axes[i],std::copysign(1.f,(&local.x)[i]));float extent=-INFINITY;
            for(const auto& member:poses)extent=std::max(extent,Vector3DotProduct(member.pose.position,outward)+Radius(member.pose,outward));
            const float correction=Vector3DotProduct(out.point,outward)-extent-Vector3DotProduct(Vector3Subtract(desired,reference),outward);
            desired=Vector3Add(desired,Vector3Scale(outward,correction));
        }
    }
    desired=Vector3Add(desired,Vector3Scale(out.normal,offset));
    out.correction=Vector3Subtract(desired,reference);
    for(auto& member:poses) {
        member.pose.position=Vector3Add(member.pose.position,out.correction);
        for(const auto& obstacle:targets)if(member.id!=obstacle.id&&ValidBox(obstacle.pose)&&
            NearBoxes(member.pose,obstacle.pose,0)&&BoxSeparation(member.pose,obstacle.pose)<-ContactEpsilon(member.pose,obstacle.pose)) {
            out.blocked=true;out.blocker=obstacle.id;out.reason="Interseção: afaste o cursor ou desative o encaixe";return out;
        }
    }
    out.snapped=true;return out;
}

int HandleAxis(HandleId id) {
    switch (id) {
        case HandleId::XAxis: case HandleId::XPositive: case HandleId::XNegative:
        case HandleId::RotateX: case HandleId::XNegativeAxis: return 0;
        case HandleId::YAxis: case HandleId::YPositive: case HandleId::YNegative:
        case HandleId::RotateY: case HandleId::YNegativeAxis: return 1;
        case HandleId::ZAxis: case HandleId::ZPositive: case HandleId::ZNegative:
        case HandleId::RotateZ: case HandleId::ZNegativeAxis: return 2;
        default: return -1;
    }
}

int HandleSide(HandleId id) {
    switch (id) {
        case HandleId::XPositive: case HandleId::YPositive: case HandleId::ZPositive: return 1;
        case HandleId::XNegative: case HandleId::YNegative: case HandleId::ZNegative:
        case HandleId::XNegativeAxis: case HandleId::YNegativeAxis: case HandleId::ZNegativeAxis: return -1;
        default: return 0;
    }
}

bool IsScaleFace(HandleId id) {
    return id >= HandleId::XPositive && id <= HandleId::ZNegative;
}

HandleId MoveHandleId(int axisIndex, int side) {
    if (axisIndex < 0 || axisIndex > 2 || side == 0) return HandleId::None;
    static constexpr HandleId ids[3][2] = {
        {HandleId::XNegativeAxis, HandleId::XAxis},
        {HandleId::YNegativeAxis, HandleId::YAxis},
        {HandleId::ZNegativeAxis, HandleId::ZAxis}};
    return ids[axisIndex][side > 0 ? 1 : 0];
}

HandleId ScaleFaceHandleId(int axisIndex, int side) {
    if (axisIndex < 0 || axisIndex > 2 || side == 0) return HandleId::None;
    static constexpr HandleId ids[3][2] = {
        {HandleId::XNegative, HandleId::XPositive},
        {HandleId::YNegative, HandleId::YPositive},
        {HandleId::ZNegative, HandleId::ZPositive}};
    return ids[axisIndex][side > 0 ? 1 : 0];
}

HandleId PickScaleFaceHandle(const std::array<Vector2, 6>& centers, Vector2 mouse,
                             float halfWidth, float halfHeight) {
    HandleId picked = HandleId::None;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        for (int side : {-1, 1}) {
            const int index = axis * 2 + (side > 0 ? 1 : 0);
            const Vector2 center = centers[static_cast<std::size_t>(index)];
            const float dx = std::fabs(mouse.x - center.x);
            const float dy = std::fabs(mouse.y - center.y);
            if (dx > halfWidth || dy > halfHeight) continue;
            const float distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                picked = ScaleFaceHandleId(axis, side);
            }
        }
    }
    return picked;
}

Vector3 Axis(int index, Quaternion rotation, bool local) {
    const Vector3 basis = index == 0 ? Vector3{1, 0, 0}
                         : index == 1 ? Vector3{0, 1, 0}
                                      : Vector3{0, 0, 1};
    return local ? Vector3Normalize(Vector3RotateByQuaternion(basis, rotation)) : basis;
}

float SnapDelta(float delta, float increment) {
    if (!std::isfinite(delta)) return 0.0f;
    if (!std::isfinite(increment) || increment <= 0.0f) return delta;
    return std::round(delta / increment) * increment;
}

Vector3 MoveAxis(const TransformSnapshot& start, Vector3 axis, float delta,
                 float snapIncrement) {
    const float snapped = SnapDelta(delta, snapIncrement);
    return Vector3Add(start.position, Vector3Scale(Vector3Normalize(axis), snapped));
}

Vector3 MovePlane(const TransformSnapshot& start, Vector3 axisA, Vector3 axisB,
                  Vector3 worldDelta, float snapIncrement) {
    axisA = Vector3Normalize(axisA);
    axisB = Vector3Normalize(axisB);
    const float a = SnapDelta(Vector3DotProduct(worldDelta, axisA), snapIncrement);
    const float b = SnapDelta(Vector3DotProduct(worldDelta, axisB), snapIncrement);
    return Vector3Add(start.position,
                      Vector3Add(Vector3Scale(axisA, a), Vector3Scale(axisB, b)));
}

TransformSnapshot ScaleFace(const TransformSnapshot& start, Vector3 axis, int side,
                            float delta, float snapIncrement, float minimumSize) {
    TransformSnapshot result = start;
    if (!std::isfinite(delta) || !std::isfinite(minimumSize) || minimumSize <= 0.0f ||
        Vector3LengthSqr(axis) < 1e-8f) return result;
    axis = Vector3Normalize(axis);
    side = side < 0 ? -1 : 1;
    const Vector3 localAxes[3] = {Axis(0, start.rotation, true), Axis(1, start.rotation, true),
                                  Axis(2, start.rotation, true)};
    int component = 0;
    float bestDot = -1.0f;
    for (int i = 0; i < 3; ++i) {
        const float dot = std::fabs(Vector3DotProduct(axis, localAxes[i]));
        if (dot > bestDot) { bestDot = dot; component = i; }
    }
    const float signedDelta = SnapDelta(delta, snapIncrement) * static_cast<float>(side);
    float& sizeComponent = component == 0 ? result.size.x
                           : component == 1 ? result.size.y : result.size.z;
    const float startComponent = component == 0 ? start.size.x
                                 : component == 1 ? start.size.y : start.size.z;
    sizeComponent = std::max(minimumSize, startComponent + signedDelta);
    const float actualDelta = sizeComponent - startComponent;
    result.position = Vector3Add(start.position,
                                 Vector3Scale(localAxes[component],
                                              0.5f * static_cast<float>(side) * actualDelta));
    return result;
}

TransformSnapshot ScaleFaceAlongAxis(const TransformSnapshot& start, int axisIndex, int side,
                                     bool localSpace, float delta, float snapIncrement,
                                     float minimumSize) {
    if (axisIndex < 0 || axisIndex > 2 || !std::isfinite(delta)) return start;
    if (localSpace) {
        return ScaleFace(start, Axis(axisIndex, start.rotation, true), side, delta,
                         snapIncrement, minimumSize);
    }

    const Vector3 worldAxis = Axis(axisIndex, start.rotation, false);
    int component = 0;
    float alignment = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float candidate = Vector3DotProduct(worldAxis, Axis(i, start.rotation, true));
        if (std::fabs(candidate) > std::fabs(alignment)) {
            alignment = candidate;
            component = i;
        }
    }
    // Keep the sign of the projection: a world-positive axis may point toward
    // the local-negative face. Dropping that sign made half of the handles on
    // rotated Parts shrink when dragged outward. Dividing by the projection
    // also amplified resize; the local face displacement is delta * alignment.
    if (std::fabs(alignment) < 1e-5f) return start;
    const int localSide = (alignment < 0.0f ? -1 : 1) * (side < 0 ? -1 : 1);
    return ScaleFace(start, Axis(component, start.rotation, true), localSide,
                     delta * alignment, snapIncrement, minimumSize);
}

TransformSnapshot ScaleFaceForHandle(const TransformSnapshot& start, HandleId handle,
                                     bool localSpace, float delta, float snapIncrement,
                                     float minimumSize) {
    if (!IsScaleFace(handle)) return start;
    return ScaleFaceAlongAxis(start, HandleAxis(handle), HandleSide(handle), localSpace,
                              delta, snapIncrement, minimumSize);
}

TransformSnapshot ScaleUniformFace(const TransformSnapshot& start, Vector3 axis, int side,
                                   float delta, float snapIncrement, float minimumSize) {
    TransformSnapshot result = start;
    if (!std::isfinite(delta) || !std::isfinite(minimumSize) || minimumSize <= 0.0f ||
        Vector3LengthSqr(axis) < 1e-8f) return result;
    axis = Vector3Normalize(axis);
    side = side < 0 ? -1 : 1;
    const float oldDiameter = std::max(minimumSize, std::min({start.size.x,start.size.y,start.size.z}));
    const float newDiameter = std::max(minimumSize,
        oldDiameter + static_cast<float>(side) * SnapDelta(delta, snapIncrement));
    result.size = Vector3{newDiameter, newDiameter, newDiameter};
    result.position = Vector3Add(start.position,
        Vector3Scale(axis, static_cast<float>(side) * (newDiameter - oldDiameter) * 0.5f));
    return result;
}

TransformSnapshot ScaleCenteredFace(const TransformSnapshot& start, int axisIndex, int side,
                                    bool localSpace, float delta, float snapIncrement,
                                    float minimumSize) {
    TransformSnapshot result = start;
    if (axisIndex < 0 || axisIndex > 2 || !std::isfinite(delta) ||
        !std::isfinite(minimumSize) || minimumSize <= 0.0f) return result;
    int component = axisIndex;
    int localSide = side < 0 ? -1 : 1;
    if (!localSpace) {
        // Mesma regra de ScaleFaceAlongAxis: em World o eixo escolhido pode
        // apontar para a face local oposta.
        const Vector3 worldAxis = Axis(axisIndex, start.rotation, false);
        float alignment = 0.0f;
        component = 0;
        for (int i = 0; i < 3; ++i) {
            const float candidate = Vector3DotProduct(worldAxis, Axis(i, start.rotation, true));
            if (std::fabs(candidate) > std::fabs(alignment)) {
                alignment = candidate;
                component = i;
            }
        }
        if (std::fabs(alignment) < 1e-5f) return result;
        if (alignment < 0.0f) localSide = -localSide;
    }
    float& size = component == 0 ? result.size.x
                 : component == 1 ? result.size.y : result.size.z;
    const float previous = component == 0 ? start.size.x
                         : component == 1 ? start.size.y : start.size.z;
    const float signedDelta = SnapDelta(delta, snapIncrement) * static_cast<float>(localSide);
    // Cresce (ou encolhe) pelos dois lados: o centro nao se move.
    size = std::max(minimumSize, previous + 2.0f * signedDelta);
    return result;
}

Vector3 ScaleHandleCenter(const TransformSnapshot& start, int axisIndex, int side,
                          bool localSpace, float handleOffset) {
    if (axisIndex < 0 || axisIndex > 2) return start.position;
    const Vector3 axis = Axis(axisIndex, start.rotation, localSpace);
    if (Vector3LengthSqr(axis) < 1e-8f) return start.position;
    const int sign = side < 0 ? -1 : 1;
    const Vector3 localAxes[3] = {Axis(0, start.rotation, true), Axis(1, start.rotation, true),
                                  Axis(2, start.rotation, true)};
    const float half[3] = {0.5f * start.size.x, 0.5f * start.size.y,
                           0.5f * start.size.z};
    float support = 0.0f;
    for (int i = 0; i < 3; ++i)
        support += half[i] * std::fabs(Vector3DotProduct(axis, localAxes[i]));
    return Vector3Add(start.position,
                      Vector3Scale(axis, static_cast<float>(sign) * (support + std::max(0.0f, handleOffset))));
}

float WrappedAngleDelta(float startAngle, float currentAngle) {
    constexpr float twoPi = 2.0f * PI;
    float delta = std::fmod(currentAngle - startAngle + PI, twoPi);
    if (delta < 0.0f) delta += twoPi;
    return delta - PI;
}

void SeparateScreenPoints(Vector2* points, int count, float minimumDistance) {
    if (points == nullptr || count < 2) return;
    const float minimum = std::max(0.0f, minimumDistance);
    if (minimum <= 0.0f) return;
    for (int pass = 0; pass < 96; ++pass) {
        bool changed = false;
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                Vector2 delta{points[j].x - points[i].x, points[j].y - points[i].y};
                float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (distance >= minimum) continue;
                if (distance < 1e-3f) {
                    const float angle = static_cast<float>(i * 13 + j * 7) * 0.6180339f;
                    delta = Vector2{std::cos(angle), std::sin(angle)};
                    distance = 1.0f;
                }
                const float push = (minimum - distance) * 0.5f / distance;
                delta.x *= push;
                delta.y *= push;
                points[i].x -= delta.x;
                points[i].y -= delta.y;
                points[j].x += delta.x;
                points[j].y += delta.y;
                changed = true;
            }
        }
        if (!changed) break;
    }
}

std::array<Vector2, 6> SeparateScreenHandles(const std::array<Vector2, 6>& input,
                                             float minimumDistance) {
    std::array<Vector2, 6> result = input;
    SeparateScreenPoints(result.data(), static_cast<int>(result.size()), minimumDistance);
    return result;
}

Quaternion Rotate(const TransformSnapshot& start, Vector3 axis, float radians,
                  float snapDegrees) {
    if (Vector3LengthSqr(axis) < 1e-8f || !std::isfinite(radians)) return start.rotation;
    const float increment = snapDegrees > 0.0f ? snapDegrees * DEG2RAD : 0.0f;
    const float angle = SnapDelta(radians, increment);
    const Quaternion delta = QuaternionFromAxisAngle(Vector3Normalize(axis), angle);
    return CanonicalQuaternion(QuaternionNormalize(QuaternionMultiply(delta, start.rotation)));
}

}  // namespace loblox::gizmomath

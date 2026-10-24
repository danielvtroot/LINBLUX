#pragma once
#include <array>
#include "core/Math.h"

namespace loblox {
// Same local box for mesh vertices, selection and margin-inclusive Bullet
// bounds. Parents are organizational in scene v3; Part transforms are world.
struct OrientedBox {
    Vector3 center{};
    Quaternion rotation{0,0,0,1};
    Vector3 half{};
    std::array<Vector3,8> corners() const {
        const Vector3 signs[]={{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1},
                               {-1,1,-1},{1,1,-1},{1,1,1},{-1,1,1}};
        std::array<Vector3,8> result;
        for(int i=0;i<8;++i)result[i]=Vector3Add(center,Vector3RotateByQuaternion(
            Vector3Multiply(signs[i],half),rotation));
        return result;
    }
    RayCollision intersect(Ray ray) const {
        const auto inverse=QuaternionInvert(rotation);
        Ray local{Vector3RotateByQuaternion(Vector3Subtract(ray.position,center),inverse),
                  Vector3RotateByQuaternion(ray.direction,inverse)};
        auto hit=GetRayCollisionBox(local,{Vector3Negate(half),half});
        if(hit.hit){hit.point=Vector3Add(center,Vector3RotateByQuaternion(hit.point,rotation));
            hit.normal=Vector3RotateByQuaternion(hit.normal,rotation);}
        return hit;
    }
};
}

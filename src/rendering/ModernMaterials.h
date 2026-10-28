#pragma once
#include "rendering/RenderSettings.h"
#include "objects/Part.h"
#include <vector>
#include <utility>
namespace loblox {
class Lighting;
class ModernMaterials {
public:
    ~ModernMaterials();
    bool draw(const Part& part,Camera3D camera,const Lighting* lighting,RenderQuality quality);
    bool wireSphere(Vector3 center,float radius,Quaternion rotation,Color color);
    bool ready() const {return ready_;}
private:
    bool initialize();
    bool attempted_=false,ready_=false;
    Shader shader_{};Mesh cube_{},sphere_{};Material material_{};
    std::vector<std::pair<Vector3,Vector3>> sphereGuides_;
};
}

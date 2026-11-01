#pragma once
#include "rendering/RenderSettings.h"
#include "objects/Part.h"
#include "rendering/DirectionalShadow.h"
#include <vector>
#include <utility>
namespace loblox {
class Lighting;
class ModernMaterials {
public:
    ~ModernMaterials();
    bool draw(const Part& part,Camera3D camera,const Lighting* lighting,RenderQuality quality,bool transparent=false);
    void prepareShadows(const std::vector<ShadowCaster>& casters,Camera3D camera,const Lighting* light,RenderQuality quality,float aspect,ShadowQuality shadowQuality);
    bool shadowActive() const {return shadow_.active();}
    bool wireSphere(Vector3 center,float radius,Quaternion rotation,Color color);
    bool ready() const {return ready_;}
    bool available() {return initialize();}
    // Shared by the modern shader and the CPU legacy fallback so both paths
    // show the same measured construction surface.
    Texture2D constructionGridTexture();
private:
    bool initialize();
    DirectionalShadow shadow_;
    bool attempted_=false,ready_=false;
    bool gridTextureAttempted_=false;
    Texture2D gridTexture_{};
    Shader shader_{};Mesh cube_{},sphere_{};Material material_{};
    std::vector<std::pair<Vector3,Vector3>> sphereGuides_;
};
}

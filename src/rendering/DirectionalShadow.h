#pragma once
#include "objects/Part.h"
#include "rendering/RenderSettings.h"
#include <vector>
namespace loblox {
class Lighting;
struct ShadowCaster {
    Vector3 position{},size{};Quaternion rotation{0,0,0,1};bool sphere=false;
    const Part* part=nullptr;
};
class DirectionalShadow {
public:
    ~DirectionalShadow();
    void update(const std::vector<ShadowCaster>& casters,Camera3D camera,const Lighting* lighting,
                bool enabled,const Mesh& cube,const Mesh& sphere,float aspect,ShadowQuality quality);
    bool active() const {return active_;}
    unsigned texture() const {return depth_;}
    Matrix matrix() const {return matrix_;}
    float normalBias() const {return normalBias_;}
    float depthBiasScale() const {return depthBiasScale_;}
    int resolution() const {return resolution_;}
    int taps() const {return resolution_>1024?16:quality_==ShadowQuality::Economy?4:8;}
    float distance() const {return distance_;}
private:
    bool initialize(int resolution);
    void release();
    bool active_=false,failed_=false,highUnavailable_=false;
    int resolution_=0;
    float stableSpan_=0,distance_=80;
    ShadowQuality quality_=ShadowQuality::Soft;
    unsigned framebuffer_=0,depth_=0;
    Shader shader_{};Material material_{};
    Matrix matrix_{};
    float normalBias_=.01f,depthBiasScale_=0;
};
}

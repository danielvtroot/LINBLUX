#pragma once
#include "objects/Part.h"
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
                bool enabled,const Mesh& cube,const Mesh& sphere);
    bool active() const {return active_;}
    unsigned texture() const {return depth_;}
    Matrix matrix() const {return matrix_;}
    float normalBias() const {return normalBias_;}
private:
    bool initialize();
    void release();
    bool active_=false,failed_=false;
    unsigned framebuffer_=0,depth_=0;
    Shader shader_{};Material material_{};
    Matrix matrix_{};
    float normalBias_=.01f;
};
}

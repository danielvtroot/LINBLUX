#pragma once
#include "rendering/GraphicsGL.h"
#include "rendering/RenderSettings.h"
#include <memory>
namespace loblox {
// One scene-only resolve, shared by Studio and standalone Player.
class SceneTarget {
public:
    ~SceneTarget();
    bool begin(int width,int height,Antialiasing requested,RenderQuality quality);
    void finish();
    Antialiasing actual() const {return actual_;}
private:
    void release();
    bool allocate(int width,int height,bool multisample);
    RenderTexture2D resolved_{};
    unsigned multisample_=0,color_=0,depth_=0;
    Shader fxaa_{};
    bool shaderTried_=false;
    Antialiasing actual_=Antialiasing::None;
    std::unique_ptr<FrameState> saved_;
};
}

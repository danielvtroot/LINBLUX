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
    // Amostras realmente concedidas pelo driver (0 = alvo sem MSAA).
    int samples() const {return samples_;}
private:
    void release();
    bool allocate(int width,int height,int samples);
    bool tryAllocate(int width,int height,int samples);
    RenderTexture2D resolved_{};
    unsigned multisample_=0,color_=0,depth_=0;
    int samples_=0;
    Shader fxaa_{};
    bool shaderTried_=false,msaaUnavailable_=false;
    Antialiasing actual_=Antialiasing::None;
    std::unique_ptr<FrameState> saved_;
};
}

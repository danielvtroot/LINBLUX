#include "rendering/BrandLogo3D.h"
#include "game/ImageAssets.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
namespace loblox {
BrandLogo3D::~BrandLogo3D() {
    if(target_.id)UnloadRenderTexture(target_);
    if(model_.meshCount)UnloadModel(model_);
}
bool BrandLogo3D::update(float dt,int physicalSize) {
    if(!tried_) {
        tried_=true;
        const auto path=game::BuiltinAssetPath("midia/3d/logo-3d.glb");
        if(!path.empty())model_=LoadModel(path.c_str());
        if(model_.meshCount) {
            const auto box=GetModelBoundingBox(model_);
            center_=Vector3Scale(Vector3Add(box.min,box.max),.5f);
            radius_=Vector3Distance(box.min,box.max)*.5f;
            if(!std::isfinite(radius_)||radius_<.00001f){UnloadModel(model_);model_={};}
        }
    }
    if(!model_.meshCount)return false;
    // Supersample the small brand once, never one framebuffer per placement.
    const int size=std::clamp(physicalSize*4,128,2048);
    if(target_.texture.width!=size) {
        if(target_.id)UnloadRenderTexture(target_);
        target_=LoadRenderTexture(size,size);
        if(!target_.id)return false;
        SetTextureFilter(target_.texture,TEXTURE_FILTER_BILINEAR);
        elapsed_=1;
    }
    dt=std::clamp(dt,0.f,.1f);elapsed_+=dt;
    angle_=std::fmod(angle_+dt*60.f,360.f);
    // Draw each presented frame: dropping frames here causes visible judder
    // when the Studio frame interval straddles the 60 Hz threshold.
    elapsed_=0;
    // Rotate about the bounding centre, preserving the source proportions.
    model_.transform=MatrixMultiply(MatrixMultiply(MatrixTranslate(-center_.x,-center_.y,-center_.z),MatrixRotateX(PI*.5f)),MatrixRotateY(angle_*DEG2RAD));
    Camera3D camera{{0,0,radius_*3},{0,0,0},{0,1,0},radius_*2.3f,CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target_);ClearBackground(BLANK);
    BeginMode3D(camera);DrawModel(model_,{0,0,0},1,WHITE);EndMode3D();
    EndTextureMode();
    return true;
}
void BrandLogo3D::draw(Rectangle bounds,Texture2D fallback) const {
    const auto texture=target_.id?target_.texture:fallback;
    if(!texture.id)return;
    const float side=std::min(bounds.width,bounds.height);
    Rectangle destination{bounds.x+(bounds.width-side)*.5f,bounds.y+(bounds.height-side)*.5f,side,side};
    DrawTexturePro(texture,{0,0,float(texture.width),float(target_.id?-texture.height:texture.height)},destination,{0,0},0,WHITE);
}
}

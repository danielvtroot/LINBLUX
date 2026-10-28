#include "rendering/GraphicsGL.h"
#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "raymath.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <chrono>
using namespace loblox;
namespace {
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
int delta(Image a,Image b,int threshold=6){int count=0;for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x){auto p=GetImageColor(a,x,y),q=GetImageColor(b,x,y);count+=std::abs(int(p.r)-q.r)+std::abs(int(p.g)-q.g)+std::abs(int(p.b)-q.b)>threshold;}return count;}
int luminance(Color c){return int(c.r)+c.g+c.b;}
}
#include "shadow_acceptance.h"
int main(){
    if(const char* path=std::getenv("LINBLUX_SHADOW_ACCEPTANCE"))return CaptureCharacterShadows(path);
    SetTraceLogLevel(LOG_WARNING);InitWindow(640,400,"LINBLUX depth quality");if(!IsWindowReady())return 77;
    const auto out=std::filesystem::path(__FILE__).parent_path().parent_path()/".turbo/logs/mega3d-phase4/depth";std::filesystem::create_directories(out);
    {
    Scene scene;auto* light=scene.lighting();light->setSkyEnabled(false);light->setSunDirection({.7f,-1,.4f});
    auto* floor=scene.createPart(PartShape::Block,{0,-.5f,0},{40,1,40},"Floor");floor->setColor({160,160,160,255});
    auto* block=scene.createPart(PartShape::Block,{0,2,0},{2,4,2},"Caster");block->setColor({160,160,160,255});
    SceneRenderer renderer;renderer.setAntialiasing(Antialiasing::None);
    Camera3D camera{{12,12,18},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    auto target=LoadRenderTexture(640,400);
    const auto render=[&](InstanceId selected=0,bool guides=false){
        BeginTextureMode(target);GLint outer=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&outer);
        renderer.drawScene(scene,camera,target.texture.width,target.texture.height,selected);
        GLint restored=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&restored);CHECK(outer==restored);
        GLboolean mask=GL_FALSE;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);CHECK(mask==GL_TRUE);
        if(guides)renderer.drawEditorGuides(scene,camera,{true,false,false});
        CHECK(glGetError()==GL_NO_ERROR);EndTextureMode();auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    light->setShadowsEnabled(false);auto unshadowed=render();light->setShadowsEnabled(true);auto shadowed=render();CHECK(renderer.shadowsActive());CHECK(delta(unshadowed,shadowed)>150);
    const auto point=GetWorldToScreenEx({2,0,1},camera,640,400);auto lit=GetImageColor(unshadowed,int(point.x),int(point.y)),dark=GetImageColor(shadowed,int(point.x),int(point.y));
    std::printf("Contact shadow RGB sum: off=%d on=%d at %.1f,%.1f\n",luminance(lit),luminance(dark),point.x,point.y);CHECK(luminance(dark)+50<luminance(lit));
    ExportImage(unshadowed,(out/"shadow-off.png").c_str());ExportImage(shadowed,(out/"shadow-on.png").c_str());
    light->setSunDirection({-.7f,-1,-.4f});auto moved=render();CHECK(delta(shadowed,moved)>150);UnloadImage(moved);light->setSunDirection({.7f,-1,.4f});
    light->setShadowStrength(0);auto zero=render();CHECK(!renderer.shadowsActive());CHECK(delta(unshadowed,zero)==0);UnloadImage(zero);light->setShadowStrength(1);
    light->setShadowDistance(16);auto distant=render();CHECK(!renderer.shadowsActive());UnloadImage(distant);light->setShadowDistance(80);
    renderer.setQuality(RenderQuality::Low);auto low=render();CHECK(!renderer.shadowsActive());UnloadImage(low);renderer.setQuality(RenderQuality::Medium);
    block->setPosition({100,2,0});auto plane=render();light->setShadowsEnabled(false);auto planeOff=render();ExportImage(plane,(out/"plane-shadow.png").c_str());ExportImage(planeOff,(out/"plane-off.png").c_str());std::printf("Plane acne difference: %d pixels\n",delta(plane,planeOff));CHECK(delta(plane,planeOff)==0);UnloadImage(plane);UnloadImage(planeOff);block->setPosition({0,2,0});
    setenv("LINBLUX_DISABLE_SHADOWS","1",1);{SceneRenderer fallback;fallback.setAntialiasing(Antialiasing::None);light->setShadowsEnabled(true);BeginTextureMode(target);fallback.drawScene(scene,camera,640,400,0);EndTextureMode();CHECK(!fallback.shadowsActive());}unsetenv("LINBLUX_DISABLE_SHADOWS");
    UnloadImage(unshadowed);UnloadImage(shadowed);
    // Sorting must depend on camera distance, not authored insertion order.
    scene.clearObjects();light->setShadowsEnabled(false);camera={{0,0,12},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    const auto layers=[&](bool reverse){for(int k=0;k<2;++k){const int i=reverse?1-k:k;auto* p=scene.createPart(PartShape::Block,{0,0,float(i)*2},{5,5,1},"Glass");p->setColor(i?Color{30,70,240,128}:Color{240,40,20,128});}};
    layers(false);auto a=render();scene.clearObjects();layers(true);auto b=render();CHECK(delta(a,b)==0);
    auto selected=render(scene.parts().front()->id());CHECK(delta(b,selected)>100);UnloadImage(selected);
    ExportImage(b,(out/"transparent-sorted.png").c_str());UnloadImage(a);UnloadImage(b);
    // Nested target resolve preserves scene depth for a later F8 pass.
    scene.clearObjects();floor=scene.createPart(PartShape::Block,{0,0,0},{7,7,1},"Occluder");floor->setColor({90,130,170,255});floor->setRotationEulerDegrees({0,0,15});
    block=scene.createPart(PartShape::Block,{0,0,-4},{2,2,2},"Hidden");
    renderer.setAntialiasing(Antialiasing::None);auto native=render();auto nativeGuides=render(0,true);
    for(auto mode:{Antialiasing::FXAA,Antialiasing::MSAA4}){
        renderer.setAntialiasing(mode);auto clean=render();CHECK(renderer.actualAntialiasing()==mode);auto guides=render(0,true);
        // Hidden center stays untouched; only the visible outer silhouette has guides.
        CHECK(luminance(GetImageColor(clean,320,200))==luminance(GetImageColor(guides,320,200)));
        CHECK(luminance(GetImageColor(clean,320,200))==luminance(GetImageColor(native,320,200)));
        CHECK(delta(native,clean)>40);ExportImage(clean,(out/(mode==Antialiasing::FXAA?"fxaa.png":"msaa4.png")).c_str());UnloadImage(clean);UnloadImage(guides);
    }
    UnloadImage(native);UnloadImage(nativeGuides);
    setenv("LINBLUX_DISABLE_MSAA","1",1);{SceneRenderer fallback;fallback.setAntialiasing(Antialiasing::MSAA4);BeginTextureMode(target);fallback.drawScene(scene,camera,640,400,0);EndTextureMode();CHECK(fallback.actualAntialiasing()==Antialiasing::FXAA);}unsetenv("LINBLUX_DISABLE_MSAA");
    setenv("LINBLUX_DISABLE_MSAA","1",1);setenv("LINBLUX_DISABLE_FXAA","1",1);{SceneRenderer fallback;fallback.setAntialiasing(Antialiasing::MSAA4);BeginTextureMode(target);fallback.drawScene(scene,camera,640,400,0);EndTextureMode();CHECK(fallback.actualAntialiasing()==Antialiasing::None);}unsetenv("LINBLUX_DISABLE_MSAA");unsetenv("LINBLUX_DISABLE_FXAA");
    UnloadRenderTexture(target);
    scene.clearObjects();floor=scene.createPart(PartShape::Block,{0,-.5f,0},{40,1,40},"Floor");
    for(int i=0;i<6;++i){auto* p=scene.createPart(PartShape::Sphere,{float(i%3-1)*6,3,float(i/3)*7-3},{4,4,4},"Material");std::string error;CHECK(p->setMaterial3D(MaterialPreset3D(i,{180,170,150,255}),error));}
    camera={{18,16,28},{0,2,0},{0,1,0},45,CAMERA_PERSPECTIVE};light->setShadowsEnabled(true);
    // Small repeatable software benchmark, not a hardware/GPU performance claim.
    for(auto size:{Vector2{1366,768},Vector2{1920,1080},Vector2{1366,768}}){
        target=LoadRenderTexture(int(size.x),int(size.y));
        for(auto quality:{RenderQuality::Low,RenderQuality::Medium,RenderQuality::High}){
            renderer.setQuality(quality);renderer.setAntialiasing(Antialiasing::Auto);
            for(int frame=0;frame<2;++frame){auto warm=render();UnloadImage(warm);}glFinish();const auto start=std::chrono::steady_clock::now();
            for(int frame=0;frame<3;++frame){BeginTextureMode(target);renderer.drawScene(scene,camera,int(size.x),int(size.y),0);EndTextureMode();}glFinish();
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/3;
            std::printf("Benchmark %dx%d quality=%d AA=%d shadows=%d %.2f ms/frame (llvmpipe)\n",int(size.x),int(size.y),int(quality),int(renderer.actualAntialiasing()),renderer.shadowsActive(),ms);
            CHECK(renderer.actualAntialiasing()==(quality==RenderQuality::Low?Antialiasing::None:quality==RenderQuality::Medium?Antialiasing::FXAA:Antialiasing::MSAA4));CHECK(renderer.shadowsActive()==(quality!=RenderQuality::Low));
        }UnloadRenderTexture(target);
    }
    }
    game::ClearImageTextures();CHECK(glGetError()==GL_NO_ERROR);CloseWindow();std::printf("Depth quality: %d checks, %d failures\n",checks,failures);return failures?1:0;
}

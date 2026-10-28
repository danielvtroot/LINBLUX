#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "raymath.h"
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace loblox;
namespace {int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
int differences(Image a,Image b){int n=0;for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x){auto p=GetImageColor(a,x,y),q=GetImageColor(b,x,y);n+=std::abs(int(p.r)-q.r)+std::abs(int(p.g)-q.g)+std::abs(int(p.b)-q.b)>6;}return n;}
}
int main(){
    SetTraceLogLevel(LOG_WARNING);InitWindow(640,400,"LINBLUX materials regression");if(!IsWindowReady())return 77;
    const auto out=std::filesystem::path(__FILE__).parent_path().parent_path()/".turbo/logs/mega3d-phase3/materials";
    std::filesystem::create_directories(out);
    {
    Scene scene;scene.lighting()->setSkyEnabled(false);std::string error;
    auto* sphere=scene.createPart(PartShape::Sphere,{0,0,0},{6,6,6},"Smooth sphere");sphere->setColor({160,160,160,255});
    SceneRenderer renderer;Camera3D camera{{0,0,12},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    auto target=LoadRenderTexture(640,400);
    const auto render=[&](RenderQuality quality,bool authored=true){
        renderer.setQuality(quality);BeginTextureMode(target);renderer.drawScene(scene,camera,640,400,0,nullptr,authored);EndTextureMode();
        auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    auto legacy=render(RenderQuality::Legacy);auto medium=render(RenderQuality::Medium);
    CHECK(renderer.modernReady());CHECK(differences(legacy,medium)>10000);
    std::set<int> colors;for(int x=240;x<400;++x){auto p=GetImageColor(medium,x,195);colors.insert((p.r<<16)|(p.g<<8)|p.b);}
    CHECK(colors.size()>35);std::printf("Smooth sphere: %zu distinct samples/160\n",colors.size());
    auto player=render(RenderQuality::Medium,false);CHECK(differences(medium,player)==0);
    auto restored=render(RenderQuality::Legacy);CHECK(differences(legacy,restored)==0);
    UnloadImage(legacy);UnloadImage(medium);UnloadImage(player);UnloadImage(restored);
    // Actual image asset path, including premultiplication. Opacity must not
    // darken the sampled albedo a second time before alpha composition.
    scene.clearObjects();auto* part=scene.createPart(PartShape::Block,{0,0,0},{4,4,1},"Texture");
    const auto make=[&](const char* name,Color c){auto image=GenImageColor(8,8,c);const auto path=out/name;ExportImage(image,path.c_str());UnloadImage(image);return path.string();};
    const auto opaque=make("opaque.png",{220,30,180,255}),half=make("half.png",{220,30,180,128});
    auto m=MaterialPreset3D(0,WHITE);m.baseColorTexture=opaque;CHECK(part->setMaterial3D(m,error));
    auto full=render(RenderQuality::Medium);m.baseColorTexture=half;CHECK(part->setMaterial3D(m,error));auto alpha=render(RenderQuality::Medium);
    part->setPosition({100,0,0});auto empty=render(RenderQuality::Medium);part->setPosition({0,0,0});
    auto f=GetImageColor(full,320,200),a=GetImageColor(alpha,320,200),b=GetImageColor(empty,320,200);
    std::printf("Material alpha pixels: full=%d,%d,%d half=%d,%d,%d background=%d,%d,%d\n",f.r,f.g,f.b,a.r,a.g,a.b,b.r,b.g,b.b);
    CHECK(std::abs(int(a.r)-(int(f.r)+b.r)/2)<7&&std::abs(int(a.g)-(int(f.g)+b.g)/2)<7&&std::abs(int(a.b)-(int(f.b)+b.b)/2)<7);
    CHECK(f.r>f.g*2&&f.b>f.g*2);
    UnloadImage(full);UnloadImage(alpha);UnloadImage(empty);
    m.baseColorTexture=opaque;m.normalTexture=make("normal.png",{210,128,220,255});
    m.metallicRoughnessTexture=make("mr.png",{255,100,255,255});m.occlusionTexture=make("ao.png",{150,150,150,255});
    m.metallic=.8f;CHECK(part->setMaterial3D(m,error));auto maps=render(RenderQuality::High);CHECK(renderer.modernReady());
    auto noMaps=m;noMaps.normalTexture.clear();noMaps.occlusionTexture.clear();noMaps.metallicRoughnessTexture.clear();
    CHECK(part->setMaterial3D(noMaps,error));auto plain=render(RenderQuality::High);CHECK(differences(maps,plain)>1000);
    UnloadImage(maps);UnloadImage(plain);
    // Every profile, two resolutions, same scene/camera shared with Player.
    scene.clearObjects();auto* floor=scene.createPart(PartShape::Block,{0,-.5f,0},{40,1,40},"Floor");floor->setColor({130,135,145,255});
    for(int i=0;i<6;++i){auto* p=scene.createPart(PartShape::Sphere,{float(i%3-1)*6,3,float(i/3)*7-3},{4,4,4},MaterialPresetName(i));
        auto preset=MaterialPreset3D(i,{180,170,150,255});CHECK(p->setMaterial3D(preset,error));}
    camera={{18,16,28},{0,2,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    UnloadRenderTexture(target);
    for(const auto size:{Vector2{1366,768},Vector2{1920,1080}}){
        target=LoadRenderTexture(int(size.x),int(size.y));
        for(auto quality:{RenderQuality::Legacy,RenderQuality::Low,RenderQuality::Medium,RenderQuality::High}){
            renderer.setQuality(quality);BeginTextureMode(target);renderer.drawScene(scene,camera,int(size.x),int(size.y),0,nullptr,false);EndTextureMode();
            auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
            const auto path=out/(std::to_string(int(size.x))+"-quality-"+std::to_string(int(quality))+".png");CHECK(ExportImage(image,path.c_str()));UnloadImage(image);
        }
        UnloadRenderTexture(target);
    }
    // Force a real shader-init failure; the default path must match Legacy.
    target=LoadRenderTexture(640,400);renderer.setQuality(RenderQuality::Legacy);
    BeginTextureMode(target);renderer.drawScene(scene,camera,640,400,0);EndTextureMode();auto expected=LoadImageFromTexture(target.texture);
    setenv("LINBLUX_DISABLE_MATERIAL_SHADER","1",1);
    {SceneRenderer fallback;fallback.setQuality(RenderQuality::High);BeginTextureMode(target);fallback.drawScene(scene,camera,640,400,0);EndTextureMode();
        auto actual=LoadImageFromTexture(target.texture);CHECK(!fallback.modernReady());CHECK(differences(expected,actual)==0);UnloadImage(actual);}
    unsetenv("LINBLUX_DISABLE_MATERIAL_SHADER");UnloadImage(expected);UnloadRenderTexture(target);
    }
    game::ClearImageTextures();CloseWindow();std::printf("Material render: %d checks, %d failures; artifacts: %s\n",checks,failures,out.c_str());return failures?1:0;
}

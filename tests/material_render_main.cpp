#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "raymath.h"
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <algorithm>
#include <cmath>
#include <vector>
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
    Scene scene;scene.lighting()->setSkyEnabled(false);
    scene.lighting()->setSunDirection({-.5f,-.8f,-.6f});std::string error;
    auto* sphere=scene.createPart(PartShape::Sphere,{0,0,0},{6,6,6},"Smooth sphere");sphere->setColor({160,160,160,255});
    SceneRenderer renderer;renderer.setAntialiasing(Antialiasing::None);Camera3D camera{{0,0,12},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    auto target=LoadRenderTexture(640,400);
    const auto render=[&](RenderQuality quality,bool authored=true){
        renderer.setQuality(quality);BeginTextureMode(target);renderer.drawScene(scene,camera,640,400,0,nullptr,authored);EndTextureMode();
        auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    auto legacy=render(RenderQuality::Legacy);auto medium=render(RenderQuality::Medium);
    CHECK(renderer.modernReady());CHECK(differences(legacy,medium)>10000);
    ExportImage(legacy,(out/"sphere-legacy.png").c_str());ExportImage(medium,(out/"sphere-medium.png").c_str());
    std::set<int> colors;for(int x=240;x<400;++x){auto p=GetImageColor(medium,x,195);colors.insert((p.r<<16)|(p.g<<8)|p.b);}
    CHECK(colors.size()>35);std::printf("Smooth sphere: %zu distinct samples/160\n",colors.size());
    auto player=render(RenderQuality::Medium,false);CHECK(differences(medium,player)==0);
    sphere->setRotationEulerDegrees({15,25,35});
    auto rotated=render(RenderQuality::Medium);
    BeginTextureMode(target);renderer.drawScene(scene,camera,640,400,0);
    renderer.drawEditorGuides(scene,camera,{true,false,false});EndTextureMode();
    auto guides=LoadImageFromTexture(target.texture);ImageFlipVertical(&guides);
    CHECK(differences(rotated,guides)>100);
    ExportImage(guides,(out/"sphere-guides.png").c_str());
    auto afterGuides=render(RenderQuality::Medium);CHECK(differences(rotated,afterGuides)==0);
    UnloadImage(rotated);UnloadImage(guides);UnloadImage(afterGuides);sphere->setRotationEulerDegrees({0,0,0});
    auto restored=render(RenderQuality::Legacy);CHECK(differences(legacy,restored)==0);
    UnloadImage(legacy);UnloadImage(medium);UnloadImage(player);UnloadImage(restored);
    // Construction surface: physical tiling, rotation, explicit overrides and
    // the CPU fallback. Compare surface=1 against surface=2 with the same
    // material so only the measured grid can explain the difference.
    scene.clearObjects();
    auto* plate=scene.createBaseplate({0,-.5f,0},{8,1,8},"Grid plate");
    plate->setColor({150,150,150,255});
    Material3D gridMaterial=LegacyMaterial3D(PartMaterial::SmoothPlastic,plate->color());
    gridMaterial.surface=1;CHECK(plate->setMaterial3D(gridMaterial,error));
    camera={{0,20,0},{0,0,0},{0,0,-1},20,CAMERA_ORTHOGRAPHIC};
    const auto lineCenters=[&](Image withGrid,Image smooth,int row) {
        std::vector<float> centers;bool inside=false;int start=0;
        for(int x=2;x<638;++x) {
            const auto a=GetImageColor(withGrid,x,row),b=GetImageColor(smooth,x,row);
            const int da=int(a.r)+a.g+a.b,db=int(b.r)+b.g+b.b;
            const bool dark=da+6<db;
            if(dark&&!inside){inside=true;start=x;}
            else if(!dark&&inside){inside=false;centers.push_back((start+x-1)*.5f);}
        }
        if(inside)centers.push_back((start+637)*.5f);
        return centers;
    };
    const auto medianSpacing=[](const std::vector<float>& centers) {
        std::vector<float> gaps;
        for(size_t i=1;i<centers.size();++i)gaps.push_back(centers[i]-centers[i-1]);
        if(gaps.empty())return 0.f;
        std::sort(gaps.begin(),gaps.end());return gaps[gaps.size()/2];
    };
    auto grid8=render(RenderQuality::Medium);
    gridMaterial.surface=2;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto smooth8=render(RenderQuality::Medium);
    CHECK(differences(grid8,smooth8)>1000);
    const auto centers8=lineCenters(grid8,smooth8,200);
    std::printf("Construction grid 8: %zu lines, spacing %.2f px\n",centers8.size(),medianSpacing(centers8));
    CHECK(centers8.size()>=6);CHECK(medianSpacing(centers8)>18&&medianSpacing(centers8)<22);
    ExportImage(grid8,(out/"grid-8-medium.png").c_str());ExportImage(smooth8,(out/"grid-8-smooth.png").c_str());

    // Resizing must add squares instead of stretching the existing ones.
    plate->setSize({16,1,16});
    gridMaterial.surface=1;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto grid16=render(RenderQuality::Medium);
    gridMaterial.surface=2;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto smooth16=render(RenderQuality::Medium);
    const auto centers16=lineCenters(grid16,smooth16,200);
    std::printf("Construction grid 16: %zu lines, spacing %.2f px\n",centers16.size(),medianSpacing(centers16));
    CHECK(centers16.size()>=12);CHECK(std::fabs(medianSpacing(centers16)-medianSpacing(centers8))<2.f);
    ExportImage(grid16,(out/"grid-16-medium.png").c_str());

    // Rotation follows the Part transform.
    plate->setSize({8,1,8});plate->setRotationEulerDegrees({0,30,0});
    gridMaterial.surface=1;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto gridRotated=render(RenderQuality::Medium);
    gridMaterial.surface=2;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto smoothRotated=render(RenderQuality::Medium);
    CHECK(differences(gridRotated,smoothRotated)>1000);
    ExportImage(gridRotated,(out/"grid-rotated-medium.png").c_str());
    plate->setRotationEulerDegrees({0,0,0});

    // Square scale and line intensity.
    gridMaterial.surface=1;gridMaterial.gridScale=2;gridMaterial.gridIntensity=1;
    CHECK(plate->setMaterial3D(gridMaterial,error));
    auto gridScale2=render(RenderQuality::Medium);
    gridMaterial.surface=2;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto smoothScale2=render(RenderQuality::Medium);
    const auto centersScale2=lineCenters(gridScale2,smoothScale2,200);
    std::printf("Construction grid scale 2: %zu lines, spacing %.2f px\n",centersScale2.size(),medianSpacing(centersScale2));
    CHECK(centersScale2.size()>=4);CHECK(medianSpacing(centersScale2)>38&&medianSpacing(centersScale2)<42);
    gridMaterial.surface=1;gridMaterial.gridScale=1;gridMaterial.gridIntensity=0;
    CHECK(plate->setMaterial3D(gridMaterial,error));
    auto gridOff=render(RenderQuality::Medium);
    gridMaterial.surface=2;gridMaterial.gridIntensity=1;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto smoothOff=render(RenderQuality::Medium);
    CHECK(differences(gridOff,smoothOff)==0);

    // An authored albedo texture replaces the construction surface.
    auto magenta=GenImageColor(8,8,{220,30,180,255});
    const auto magentaPath=out/"grid-override.png";ExportImage(magenta,magentaPath.c_str());UnloadImage(magenta);
    gridMaterial.surface=1;gridMaterial.baseColorTexture=magentaPath.string();
    CHECK(plate->setMaterial3D(gridMaterial,error));
    auto textureGrid=render(RenderQuality::Medium);
    gridMaterial.surface=2;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto textureSmooth=render(RenderQuality::Medium);
    CHECK(differences(textureGrid,textureSmooth)==0);
    gridMaterial.baseColorTexture.clear();gridMaterial.surface=1;gridMaterial.gridIntensity=1;
    CHECK(plate->setMaterial3D(gridMaterial,error));

    // The CPU fallback draws the same surface.
    auto legacyGrid=render(RenderQuality::Legacy);
    gridMaterial.surface=2;CHECK(plate->setMaterial3D(gridMaterial,error));
    auto legacySmooth=render(RenderQuality::Legacy);
    CHECK(differences(legacyGrid,legacySmooth)>1000);
    ExportImage(legacyGrid,(out/"grid-legacy.png").c_str());

    // Both sides of a cube: all six faces use the construction surface.
    scene.clearObjects();
    auto* cube=scene.createPart(PartShape::Block,{0,2,0},{4,4,4},"Grid cube");
    cube->setColor({150,150,150,255});
    auto cubeMaterial=LegacyMaterial3D(PartMaterial::SmoothPlastic,cube->color());
    cubeMaterial.surface=1;CHECK(cube->setMaterial3D(cubeMaterial,error));
    camera={{8,6,8},{0,2,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    auto cubeGridFront=render(RenderQuality::Medium);
    cubeMaterial.surface=2;CHECK(cube->setMaterial3D(cubeMaterial,error));
    auto cubeSmoothFront=render(RenderQuality::Medium);
    CHECK(differences(cubeGridFront,cubeSmoothFront)>1000);
    ExportImage(cubeGridFront,(out/"grid-cube-front.png").c_str());
    camera={{-8,6,-8},{0,2,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    cubeMaterial.surface=1;CHECK(cube->setMaterial3D(cubeMaterial,error));
    auto cubeGridBack=render(RenderQuality::Medium);
    cubeMaterial.surface=2;CHECK(cube->setMaterial3D(cubeMaterial,error));
    auto cubeSmoothBack=render(RenderQuality::Medium);
    CHECK(differences(cubeGridBack,cubeSmoothBack)>1000);
    ExportImage(cubeGridBack,(out/"grid-cube-back.png").c_str());

    UnloadImage(grid8);UnloadImage(smooth8);UnloadImage(grid16);UnloadImage(smooth16);
    UnloadImage(gridRotated);UnloadImage(smoothRotated);UnloadImage(gridScale2);UnloadImage(smoothScale2);
    UnloadImage(gridOff);UnloadImage(smoothOff);UnloadImage(textureGrid);UnloadImage(textureSmooth);
    UnloadImage(legacyGrid);UnloadImage(legacySmooth);UnloadImage(cubeGridFront);UnloadImage(cubeSmoothFront);
    UnloadImage(cubeGridBack);UnloadImage(cubeSmoothBack);
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
    {SceneRenderer fallback;fallback.setAntialiasing(Antialiasing::None);fallback.setQuality(RenderQuality::High);BeginTextureMode(target);fallback.drawScene(scene,camera,640,400,0);EndTextureMode();
        auto actual=LoadImageFromTexture(target.texture);CHECK(!fallback.modernReady());ExportImage(expected,(out/"fallback-expected.png").c_str());ExportImage(actual,(out/"fallback-actual.png").c_str());std::printf("Fallback differences: %d\n",differences(expected,actual));CHECK(differences(expected,actual)==0);UnloadImage(actual);}
    unsetenv("LINBLUX_DISABLE_MATERIAL_SHADER");UnloadImage(expected);UnloadRenderTexture(target);
    }
    game::ClearImageTextures();CloseWindow();std::printf("Material render: %d checks, %d failures; artifacts: %s\n",checks,failures,out.c_str());return failures?1:0;
}

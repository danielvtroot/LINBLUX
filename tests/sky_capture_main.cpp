// Deterministic production-renderer fixtures; no personal projects are loaded.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include "raylib.h"
#include "rendering/SceneRenderer.h"
#include "objects/Lighting.h"
#include "scene/Scene.h"
#include "core/Diagnostics.h"
using namespace loblox;
int failures=0,checks=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}}while(0)
int main() {
    char temp[]="/tmp/loblox-sky-capture-XXXXXX";const char* dir=mkdtemp(temp);if(!dir)return 1;
    diag::configure(std::string(dir)+"/events.jsonl");
    SetTraceLogLevel(LOG_WARNING);InitWindow(640,360,"LINBLUX procedural sky regression");SetExitKey(KEY_NULL);
    {
        Scene scene;auto* lighting=scene.lighting();lighting->setSkyEnabled(true);
        lighting->setCloudDensity(0);lighting->setAtmosphereStrength(.3f);
        SceneRenderer renderer;const auto target=LoadRenderTexture(640,360);
        auto render=[&](Camera3D camera,const char* name) {
            BeginDrawing();BeginTextureMode(target);
            renderer.drawScene(scene,camera,640,360,kInvalidInstanceId,nullptr,false);
            EndTextureMode();EndDrawing();
            auto result=LoadImageFromTexture(target.texture);
            ImageFlipVertical(&result); // RenderTexture readback has OpenGL's bottom-left origin
            ExportImage(result,(std::string(dir)+"/"+name+".png").c_str());return result;
        };
        Camera3D camera{{0,5,0},{0,5,-1},{0,1,0},60,CAMERA_PERSPECTIVE};
        auto horizon=render(camera,"horizon");
        const auto top=GetImageColor(horizon,320,50),bottom=GetImageColor(horizon,320,310);
        CHECK(std::abs(int(top.r)-int(bottom.r))+std::abs(int(top.g)-int(bottom.g))+std::abs(int(top.b)-int(bottom.b))>30);
        const auto centre=GetImageColor(horizon,320,180);
        camera.position={300,500,700};camera.target={300,500,699};
        auto translated=render(camera,"translated");
        const auto translatedCentre=GetImageColor(translated,320,180);
        CHECK(std::abs(int(centre.r)-int(translatedCentre.r))<=1);
        CHECK(std::abs(int(centre.g)-int(translatedCentre.g))<=1);
        camera.target={300,501,699};auto up=render(camera,"zenith");
        const auto upCentre=GetImageColor(up,320,180);
        CHECK(std::abs(int(centre.r)-int(upCentre.r))+std::abs(int(centre.g)-int(upCentre.g))>15);
        lighting->setCloudDensity(.8f);auto clouds=render(camera,"clouds");
        lighting->setSkyEnabled(false);auto disabled=render(camera,"disabled");
        const auto disabledCentre=GetImageColor(disabled,320,180);
        CHECK(std::abs(int(upCentre.r)-int(disabledCentre.r))+std::abs(int(upCentre.g)-int(disabledCentre.g))>15);
        for(auto image:{horizon,translated,up,clouds,disabled})UnloadImage(image);
        UnloadRenderTexture(target);
    } // renderer releases shader/GPU resources before CloseWindow
    CloseWindow();std::printf("Sky: %d checks, %d failures; captures %s\n",checks,failures,dir);return failures?1:0;
}

#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include <cstdio>
#include <cstdlib>

// Reproduces the production Decal null-optional-key crash and verifies that
// a newly applied image is opaque by default, not merely "doesn't crash".
int main() {
    char temp[]="/tmp/loblox-decal-render-XXXXXX";const char* dir=mkdtemp(temp);if(!dir)return 1;
    SetTraceLogLevel(LOG_WARNING);InitWindow(256,256,"Loblox decal regression");
    int failures=0,checks=0;
    {
        const std::string file=std::string(dir)+"/sample.png";
        Image image=GenImageColor(8,8,{240,30,180,255});ExportImage(image,file.c_str());UnloadImage(image);
        loblox::Scene scene;scene.lighting()->setSkyEnabled(false);
        auto* part=scene.createPart(loblox::PartShape::Block,{0,0,0},{2,2,2},"ImagePart");part->setAnchored(true);
        Camera3D camera{{0,0,-6},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
        loblox::SceneRenderer renderer;auto target=LoadRenderTexture(256,256);
        auto render=[&](const char* capture)->Color {
            BeginDrawing();BeginTextureMode(target);
            renderer.drawScene(scene,camera,256,256,loblox::kInvalidInstanceId);
            EndTextureMode();EndDrawing();
            auto image=LoadImageFromTexture(target.texture);const Color pixel=GetImageColor(image,128,128);
            if(capture)ExportImage(image,(std::string(dir)+"/"+capture+".png").c_str());
            UnloadImage(image);return pixel;
        };
        const Color blank=render(nullptr);
        for(const char* field:{"decal_image","texture_image"}) {
            part->setAttribute(field,file);const Color pixel=render(field);
            // Decals are lit in the production renderer. Prove the sampled texel
            // is opaque, keeps the magenta hue, and is clearly not the bare part.
            const int distance=std::abs(int(pixel.r)-int(blank.r))+std::abs(int(pixel.g)-int(blank.g))+std::abs(int(pixel.b)-int(blank.b));
            ++checks;
            if(!(pixel.a==255 && pixel.r>150 && pixel.g<90 && pixel.b>110 &&
                 pixel.r>pixel.g*2 && pixel.b>pixel.g*2 && distance>80)) {
                ++failures;std::fprintf(stderr,"%s: pixel=%d,%d,%d,%d blank=%d,%d,%d,%d distance=%d\n",
                    field,pixel.r,pixel.g,pixel.b,pixel.a,blank.r,blank.g,blank.b,blank.a,distance);
            }
            part->setAttribute(field,std::string());
        }
        UnloadRenderTexture(target);
    }
    loblox::game::ClearImageTextures();CloseWindow();
    std::printf("Decal render: %d checks, %d failures; captures: %s\n",checks,failures,dir);return failures?1:0;
}

#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "game/Inventory.h"
#include "physics/PhysicsWorld.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

// Two independent regressions live here:
//  1. the production Decal null-optional-key crash + default opacity;
//  2. the decal ORIENTATION on the 3D surface. A uniform magenta image proves
//     the quad exists and nothing else: mirrored text looked exactly the same.
//     Only an asymmetric fixture read back through the real renderer can tell
//     "imagem natural" from "imagem espelhada".
namespace {

// 2:1, quatro cantos nomeados, seta para a direita e barra no topo.
Image OrientationImage() {
    Image image = GenImageColor(64, 32, WHITE);
    ImageDrawRectangle(&image, 0, 0, 32, 16, Color{230, 25, 25, 255});    // topo-esquerda
    ImageDrawRectangle(&image, 32, 0, 32, 16, Color{25, 70, 235, 255});   // topo-direita
    ImageDrawRectangle(&image, 0, 16, 32, 16, Color{30, 190, 55, 255});   // baixo-esquerda
    ImageDrawRectangle(&image, 32, 16, 32, 16, Color{245, 210, 30, 255}); // baixo-direita
    ImageDrawRectangle(&image, 0, 0, 64, 2, Color{0, 235, 235, 255});     // marca do topo
    ImageDrawRectangle(&image, 20, 15, 16, 3, WHITE);                     // haste da seta
    ImageDrawTriangle(&image, Vector2{36, 10}, Vector2{36, 23}, Vector2{48, 16}, WHITE);
    return image;
}

enum class Hue { Unknown, Red, Yellow, Green, Blue };

const char* HueName(Hue hue) {
    switch (hue) {
        case Hue::Red: return "vermelho";
        case Hue::Yellow: return "amarelo";
        case Hue::Green: return "verde";
        case Hue::Blue: return "azul";
        default: return "indefinido";
    }
}

// Hue em vez de valores RGB: o decal é iluminado pela face, então a
// luminosidade muda e a matiz não. A Part nua é cinza (saturação baixa) e cai
// em Unknown, o que falha como deve.
Hue Classify(Color c) {
    const float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    const float delta = mx - mn;
    if (mx < 0.12f || delta < 0.12f) return Hue::Unknown;
    float hue = 0.0f;
    if (mx == r) hue = 60.0f * std::fmod((g - b) / delta + 6.0f, 6.0f);
    else if (mx == g) hue = 60.0f * ((b - r) / delta + 2.0f);
    else hue = 60.0f * ((r - g) / delta + 4.0f);
    if (hue >= 330.0f || hue < 45.0f) return Hue::Red;
    if (hue < 90.0f) return Hue::Yellow;
    if (hue < 195.0f) return Hue::Green;
    return Hue::Blue;
}

}  // namespace

int main() {
    char temp[]="/tmp/loblox-decal-render-XXXXXX";const char* dir=mkdtemp(temp);if(!dir)return 1;
    SetTraceLogLevel(LOG_WARNING);InitWindow(256,256,"LINBLUX decal regression");
    int failures=0,checks=0;
    {
        const std::string file=std::string(dir)+"/sample.png";
        Image image=GenImageColor(8,8,{240,30,180,255});ExportImage(image,file.c_str());UnloadImage(image);
        const std::string oriented=std::string(dir)+"/orientation.png";
        const Image fixture=OrientationImage();ExportImage(fixture,oriented.c_str());UnloadImage(fixture);
        loblox::Scene scene;scene.lighting()->setSkyEnabled(false);
        auto* part=scene.createPart(loblox::PartShape::Block,{0,0,0},{2,2,2},"ImagePart");part->setAnchored(true);
        Camera3D camera{{0,0,-6},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
        loblox::SceneRenderer renderer;auto target=LoadRenderTexture(256,256);
        auto renderFull=[&](const char* capture,Camera3D cam)->Image {
            BeginDrawing();BeginTextureMode(target);
            renderer.drawScene(scene,cam,256,256,loblox::kInvalidInstanceId);
            EndTextureMode();EndDrawing();
            auto shot=LoadImageFromTexture(target.texture);
            // LoadImageFromTexture devolve a textura de render espelhada na
            // vertical; a tela (e o JPEG original) tem a linha 0 no topo.
            ImageFlipVertical(&shot);
            if(capture)ExportImage(shot,(std::string(dir)+"/"+capture+".png").c_str());
            return shot;
        };
        auto render=[&](const char* capture)->Color {
            const Image shot=renderFull(capture,camera);
            const Color pixel=GetImageColor(shot,shot.width/2,shot.height/2);
            UnloadImage(shot);return pixel;
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
        // ---------- Orientação sobre a superfície (imagem assimétrica) ----------
        // Cada face é olhada por FORA com "para cima" = v da imagem. A direita
        // do observador é cross(-normal, up); sem espelhamento, o canto
        // superior-esquerdo da IMAGEM (vermelho) tem de cair exatamente aí.
        struct View { const char* name; Vector3 normal; Vector3 up; };
        const View views[6]={
            {"Front", {0,0,-1}, {0,1,0}},
            {"Back",  {0,0, 1}, {0,1,0}},
            {"Left",  {-1,0,0}, {0,1,0}},
            {"Right", {1,0, 0}, {0,1,0}},
            {"Top",   {0,1,0},  {0,0,-1}},
            {"Bottom",{0,-1,0}, {0,0,1}},
        };
        auto checkOrientation=[&](const std::string& tag,const Vector3& normalIn,const Vector3& upIn,
                                  const Quaternion& rotation,bool flip,float halfExtent,
                                  Hue tl,Hue tr,Hue bl,Hue br) {
            const Vector3 normal=Vector3RotateByQuaternion(normalIn,rotation);
            const Vector3 up=Vector3RotateByQuaternion(upIn,rotation);
            const Vector3 right=Vector3Normalize(Vector3CrossProduct(Vector3Negate(normal),up));
            const Vector3 plane=Vector3Scale(normal,halfExtent+0.021f);
            const Camera3D view{{normal.x*(halfExtent+3.0f),normal.y*(halfExtent+3.0f),normal.z*(halfExtent+3.0f)},
                                {0,0,0},up,45,CAMERA_PERSPECTIVE};
            part->setAttribute("decal_flip_u",flip);
            const Image shot=renderFull(tag.c_str(),view);
            struct Probe { float r,u; Hue want; const char* label; };
            const float offset=halfExtent*0.62f;
            const Probe probes[4]={{-offset, offset,tl,"sup-esq"},{offset, offset,tr,"sup-dir"},
                                   {-offset,-offset,bl,"inf-esq"},{offset,-offset,br,"inf-dir"}};
            for(const Probe& probe:probes) {
                const Vector3 world=Vector3Add(plane,Vector3Add(Vector3Scale(right,probe.r),
                                                                Vector3Scale(up,probe.u)));
                const Vector2 screen=GetWorldToScreen(world,view);
                const int x=int(std::lround(screen.x)),y=int(std::lround(screen.y));
                ++checks;
                if(x<1||y<1||x>=shot.width-1||y>=shot.height-1) {
                    ++failures;std::fprintf(stderr,"%s %s: fora do framebuffer (%d,%d)\n",
                        tag.c_str(),probe.label,x,y);continue;
                }
                const Color pixel=GetImageColor(shot,x,y);
                const Hue found=Classify(pixel);
                if(found!=probe.want) {
                    ++failures;std::fprintf(stderr,
                        "%s %s: esperado %s, obtido %s (%d,%d,%d) em %d,%d\n",
                        tag.c_str(),probe.label,HueName(probe.want),HueName(found),
                        pixel.r,pixel.g,pixel.b,x,y);
                }
            }
            UnloadImage(shot);
        };
        part->setAttribute("decal_image",oriented);
        part->setAttribute("decal_flip_u",false);
        for(const View& view:views) {
            part->setAttribute("decal_face",std::string(view.name));
            checkOrientation(std::string("face-")+view.name,view.normal,view.up,
                             QuaternionIdentity(),false,1.0f,
                             Hue::Red,Hue::Blue,Hue::Green,Hue::Yellow);
        }
        // Espelhamento proposital: só troca esquerda/direita.
        part->setAttribute("decal_face",std::string("Front"));
        checkOrientation("flip-front",Vector3{0,0,-1},Vector3{0,1,0},QuaternionIdentity(),true,1.0f,
                         Hue::Blue,Hue::Red,Hue::Yellow,Hue::Green);
        // Part rotacionada: a face continua legível e natural para quem olha.
        const Quaternion turned=QuaternionFromAxisAngle(Vector3{0,1,0},35.0f*DEG2RAD);
        const Quaternion previousRotation=part->rotation();
        part->setRotation(turned);
        checkOrientation("rotated-front",Vector3{0,0,-1},Vector3{0,1,0},turned,false,1.0f,
                         Hue::Red,Hue::Blue,Hue::Green,Hue::Yellow);
        part->setRotation(previousRotation);
        part->setAttribute("decal_flip_u",false);
        // O componente Surface (aba Recursos) usa o mesmo quad.
        {
            auto* surface=scene.createSurface(loblox::InstanceClass::Decal,part,"Decal orientado");
            auto properties=surface->properties();properties.image=oriented;properties.face=1;
            surface->setProperties(properties);
            part->removeAttribute("decal_image");part->removeAttribute("decal_face");
            checkOrientation("surface-back",Vector3{0,0,1},Vector3{0,1,0},QuaternionIdentity(),false,1.0f,
                             Hue::Red,Hue::Blue,Hue::Green,Hue::Yellow);
            scene.remove(surface);
        }
        // ---------- Proporção preservada (Ajustar / Preencher) ----------
        // A fixture é 2:1 e a face é quadrada. Sem `decal_fit` a imagem é
        // esticada (padrão dos arquivos antigos); com "fit" ela cabe com
        // sobra transparente; com "fill" cobre a face cortando o excesso.
        {
            const Camera3D front{{0,0,-4},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
            auto frontPixel=[&](const char* tag,float x,float y)->Color {
                const Image shot=renderFull(tag,front);
                const Vector2 point=GetWorldToScreen(Vector3{x,y,-1.021f},front);
                const Color pixel=GetImageColor(shot,
                    std::clamp(int(std::lround(point.x)),1,shot.width-2),
                    std::clamp(int(std::lround(point.y)),1,shot.height-2));
                UnloadImage(shot);return pixel;
            };
            part->setAttribute("decal_image",oriented);
            part->setAttribute("decal_face",std::string("Front"));
            part->setAttribute("decal_flip_u",false);
            part->removeAttribute("decal_fit");
            const Color stretched=frontPixel("fit-estica",0.30f,0.86f);++checks;
            if(Classify(stretched)==Hue::Unknown){++failures;std::fprintf(stderr,
                "sem decal_fit: a imagem deve cobrir o topo da face (%d,%d,%d)\n",stretched.r,stretched.g,stretched.b);}
            part->setAttribute("decal_fit",std::string("fit"));
            const Color fittedEdge=frontPixel("fit-ajustar",0.30f,0.86f);
            const Color fittedCenter=frontPixel(nullptr,0.30f,0.20f);++checks;
            if(Classify(fittedEdge)!=Hue::Unknown){++failures;std::fprintf(stderr,
                "ajustar: a sobra deveria ser o fundo da face, veio %s\n",HueName(Classify(fittedEdge)));}
            ++checks;
            if(Classify(fittedCenter)==Hue::Unknown){++failures;std::fprintf(stderr,
                "ajustar: a imagem deveria continuar visível no centro\n");}
            part->setAttribute("decal_fit",std::string("fill"));
            const Color filledEdge=frontPixel("fit-preencher",0.30f,0.86f);
            const Color filledSide=frontPixel(nullptr,0.90f,0.10f);++checks;
            if(Classify(filledEdge)==Hue::Unknown||Classify(filledSide)==Hue::Unknown){++failures;std::fprintf(stderr,
                "preencher: a imagem deve cobrir topo e laterais (%d,%d,%d)/(%d,%d,%d)\n",
                filledEdge.r,filledEdge.g,filledEdge.b,filledSide.r,filledSide.g,filledSide.b);}
        }
        part->removeAttribute("decal_image");part->removeAttribute("decal_flip_u");
        part->removeAttribute("decal_fit");part->removeAttribute("decal_face");
        // The selectable Surface component must draw actual pixels too.
        for(const auto kind:{loblox::InstanceClass::Decal,loblox::InstanceClass::Texture}) {
            auto* surface=scene.createSurface(kind,part,"Surface");auto properties=surface->properties();properties.image=file;
            surface->setProperties(properties);const Color pixel=render("surface");++checks;
            if(!(pixel.r>150&&pixel.g<90&&pixel.b>110)){++failures;std::fprintf(stderr,"Surface not rendered: %d,%d,%d\n",pixel.r,pixel.g,pixel.b);}
            properties.transparency=1;surface->setProperties(properties);const Color hidden=render(nullptr);++checks;
            if(std::abs(int(hidden.r)-int(blank.r))>3||std::abs(int(hidden.g)-int(blank.g))>3){++failures;std::fprintf(stderr,"Surface transparency ignored\n");}
            scene.remove(surface);
        }
        // Inventory visibility must hide decals too, without changing decal data.
        part->setPosition({20,0,0});const auto background=render(nullptr);part->setPosition({0,0,0});
        auto* surface=scene.createSurface(loblox::InstanceClass::Decal,part,"Adesivo do item");auto properties=surface->properties();properties.image=file;surface->setProperties(properties);
        std::string error;auto* model=scene.group({part->id()},"Item de teste",error);
        loblox::PhysicsWorld physics;physics.attach(scene);loblox::game::Inventory inventory(scene,physics);inventory.registerType("teste",model->id());
        const auto templatePixel=render("inventory-prototype");++checks;
        if(std::abs(int(templatePixel.r)-int(background.r))>3||std::abs(int(templatePixel.g)-int(background.g))>3){++failures;std::fprintf(stderr,"Hidden prototype decal leaked\n");}
        const auto item=inventory.spawn("teste");const auto worldPixel=render("inventory-world");++checks;
        if(!(worldPixel.r>150&&worldPixel.g<90&&worldPixel.b>110)){++failures;std::fprintf(stderr,"World item decal missing\n");}
        inventory.collect(item);const auto bagPixel=render("inventory-stored");++checks;
        if(std::abs(int(bagPixel.r)-int(background.r))>3||std::abs(int(bagPixel.g)-int(background.g))>3){++failures;std::fprintf(stderr,"Stored item decal leaked\n");}
        UnloadRenderTexture(target);
    }
    loblox::game::ClearImageTextures();CloseWindow();
    std::printf("Decal render: %d checks, %d failures; captures: %s\n",checks,failures,dir);return failures?1:0;
}

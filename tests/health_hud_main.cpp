#include "game/Runtime.h"
#include "rendering/ExperienceOverlay.h"
#include <filesystem>
#include <stdexcept>
#include <cstdio>
using namespace loblox;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){
 SetTraceLogLevel(LOG_ERROR);SetConfigFlags(FLAG_WINDOW_RESIZABLE);InitWindow(1280,720,"LINBLUX — vida");
 if(!IsWindowReady())return 1;SetTargetFPS(60);int result=0;
 try{
  Scene scene;scene.buildDefaultScene();std::string error;game::RuntimeSession runtime;check(runtime.start(scene,error),error.c_str());
  int red=0;
  // 1. Nenhum pixel vermelho do círculo antigo em nenhum quadro.
  for(int rep=0;rep<3;++rep){
   BeginDrawing();ClearBackground({75,85,95,255});DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight());EndDrawing();
   auto image=LoadImageFromScreen();int red=0;
   for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){auto c=GetImageColor(image,x,y);if(c.r>150&&c.g<75&&c.b<100)red++;}
   UnloadImage(image);check(red==0,"círculo vermelho antigo ainda presente");
  }
  // 2. Lógica de interpolação (transição suave), independente do host de render.
  check(runtime.takeDamage(60,error),"dano");
  float first=runtime.healthHud().sample(runtime.health(),100,1.f/60);check(first>.4f&&first<1,"dano não interpola suavemente");
  for(int i=0;i<90;++i)runtime.healthHud().sample(runtime.health(),100,1.f/60);
  check(runtime.healthHud().sample(runtime.health(),100,1.f/60)>0.3f&&runtime.healthHud().sample(runtime.health(),100,1.f/60)<0.5f,"valor segue dano real");
  check(runtime.heal(30,error),"cura");
  float before=runtime.healthHud().sample(runtime.health(),100,1.f/60);
  for(int i=0;i<60;++i)runtime.healthHud().sample(runtime.health(),100,1.f/60);
  check(runtime.healthHud().sample(runtime.health(),100,1.f/60)>before,"cura aumenta o valor mostrado");
  check(runtime.takeDamage(100,error),"morte");check(runtime.health()==0,"morte zera vida");
  check(runtime.resetCharacter(error),"respawn");check(runtime.health()==100,"respawn restaura vida");
  // 3. HUD oculto não pinta na região da barra de vida (topo-esquerda).
  const int hr0=20,hr1=255,hv0=33,hv1=48;
  runtime.healthHud().visible=false;
  BeginDrawing();ClearBackground({75,85,95,255});DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight());EndDrawing();
  auto image=LoadImageFromScreen();int painted=0;
  for(int y=hv0;y<hv1&&y<image.height;++y)for(int x=hr0;x<hr1&&x<image.width;++x){auto c=GetImageColor(image,x,y);if(c.r>200&&c.g>200&&c.b>200)painted++;}
  UnloadImage(image);check(painted==0,"HUD oculto ainda pinta");
  runtime.healthHud().visible=true;runtime.pause();runtime.resume();
  // 4. Diferentes resoluções não quebram.
  for(auto size:{Vector2{1280,720},Vector2{1600,900},Vector2{1920,1080}}){
   SetWindowSize(int(size.x),int(size.y));
   BeginDrawing();ClearBackground({75,85,95,255});DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight());EndDrawing();
   image=LoadImageFromScreen();red=0;
   for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){auto c=GetImageColor(image,x,y);if(c.r>150&&c.g<75&&c.b<100)red++;}
   UnloadImage(image);std::string msg="vermelho antigo na resolução "+std::to_string(int(size.x));check(red==0,msg.c_str());
  }
  // 5. Lua não oculta o HUD padrão.
  auto* script=scene.createScript(InstanceClass::Script,scene.ensureService(InstanceClass::ServerScriptService),"Hud.lua");script->source="linblux.ui.health_hud(false)";
  check(runtime.start(scene,error),"Lua HUD start");runtime.update(1.f/60,{});check(!runtime.healthHud().visible&&runtime.scripts().errorCount()==0,"Lua não pode ocultar HUD padrão");runtime.stop(false);
  scene.removeById(script->id());check(runtime.start(scene,error),"restart");check(runtime.healthHud().visible,"preferência HUD vazou entre Play");runtime.stop(false);
  puts("HUD: sem vermelho, interpolação dano/cura/morte/respawn, oculto, resoluções, Lua hide: OK");
 }catch(const std::exception& e){fprintf(stderr,"HUD: %s\n",e.what());result=1;}
 ClearExperienceOverlayResources();CloseWindow();return result;
}

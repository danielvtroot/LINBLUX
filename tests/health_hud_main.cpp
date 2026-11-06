#include "game/Runtime.h"
#include "rendering/ExperienceOverlay.h"
#include <filesystem>
#include <stdexcept>
#include <cstdio>
#include <algorithm>
using namespace loblox;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){
 SetTraceLogLevel(LOG_ERROR);SetConfigFlags(FLAG_WINDOW_RESIZABLE);InitWindow(1280,720,"LINBLUX — barra de vida");
 if(!IsWindowReady())return 1;SetTargetFPS(60);int result=0;
 try{
  Scene scene;scene.buildDefaultScene();std::string error;game::RuntimeSession runtime;check(runtime.start(scene,error),error.c_str());
  auto render=[&](){for(int i=0;i<2;++i){BeginDrawing();ClearBackground({75,85,95,255});DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight());EndDrawing();}
   auto image=LoadImageFromScreen();int filled=0;
   const float scale=std::clamp(GetScreenHeight()/720.f,.75f,1.5f);
   const int x0=int(GetScreenWidth()-260*scale),y0=int(28*scale);
   for(int y=y0;y<y0+int(8*scale);++y)for(int x=x0;x<x0+int(180*scale);++x){
    auto c=GetImageColor(image,x,y);filled+=c.r>205&&c.g>205&&c.b>205;
   }
   UnloadImage(image);return filled;};
  const int full=render();check(full>700,"full health bar not painted");
  check(runtime.takeDamage(60,error),"damage");float first=runtime.healthHud().sample(runtime.health(),100,.016f);check(first>.4f&&first<1,"damage animation does not interpolate");
  for(int i=0;i<90;++i)runtime.healthHud().sample(runtime.health(),100,1.f/60);
  const int damaged=render();check(damaged>100&&damaged<full*.6f,"bar does not follow real damage");
  check(runtime.heal(30,error),"heal");for(int i=0;i<90;++i)runtime.healthHud().sample(runtime.health(),100,1.f/60);check(render()>damaged,"healing did not increase bar");
  check(runtime.takeDamage(100,error),"death");check(render()==0&&runtime.health()==0,"death must show zero");
  check(runtime.resetCharacter(error),"respawn");check(render()>=full-20&&runtime.health()==100,"respawn did not restore real health");
  std::filesystem::create_directories(".turbo/logs");
  for(auto size:{Vector2{1280,720},Vector2{1600,900},Vector2{1920,1080}}){SetWindowSize(int(size.x),int(size.y));for(int i=0;i<4;++i)render();check(render()>700,"resized health bar missing");auto image=LoadImageFromScreen();const auto path=".turbo/logs/health-hud-"+std::to_string(int(size.x))+".png";ExportImage(image,path.c_str());UnloadImage(image);}
  runtime.healthHud().visible=false;check(render()==0,"hidden HUD still painted");runtime.pause();runtime.healthHud().visible=true;check(render()==0,"HUD leaks into pause");runtime.resume();check(render()>700,"HUD missing after resume");runtime.stop(false);
  auto* script=scene.createScript(InstanceClass::Script,scene.ensureService(InstanceClass::ServerScriptService),"Hud.lua");script->source="linblux.ui.health_hud(false)";
  check(runtime.start(scene,error),"Lua HUD start");runtime.update(1.f/60,{});check(!runtime.healthHud().visible&&runtime.scripts().errorCount()==0,"Lua cannot hide default HUD");runtime.stop(false);
  scene.removeById(script->id());check(runtime.start(scene,error),"restart");check(runtime.healthHud().visible,"HUD preference leaked across Play");runtime.stop(false);
  puts("HUD: real damage/heal/death/respawn, interpolation, Lua hide, pause, restart, 720p/900p/1080p: OK");
 }catch(const std::exception& e){fprintf(stderr,"HUD: %s\n",e.what());result=1;}
 ClearExperienceOverlayResources();CloseWindow();return result;
}

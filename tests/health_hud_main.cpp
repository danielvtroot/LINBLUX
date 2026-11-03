#include "game/Runtime.h"
#include "rendering/ExperienceOverlay.h"
#include <filesystem>
#include <stdexcept>
#include <cstdio>
using namespace loblox;
static int countBright(Image image,int x0,int y0,int x1,int y1){
 int n=0;
 for(int y=y0;y<y1&&y<image.height;++y)
  for(int x=x0;x<x1&&x<image.width;++x){
   auto c=GetImageColor(image,x,y);
   if(c.r>200&&c.g>200&&c.b>200)n++;
  }
 return n;
}
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){
 SetTraceLogLevel(LOG_ERROR);SetConfigFlags(FLAG_WINDOW_RESIZABLE);InitWindow(1280,720,"LINBLUX — vida");
 if(!IsWindowReady())return 1;SetTargetFPS(60);int result=0;
 try{
  Scene scene;scene.buildDefaultScene();std::string error;game::RuntimeSession runtime;check(runtime.start(scene,error),error.c_str());
  fprintf(stderr,"HUD-DBG state=%d health=%.0f maxHealth=%.0f visible=%d\n",(int)runtime.state(),runtime.health(),runtime.settings().maxHealth,runtime.healthHud().visible?1:0);
  // Barra de vida no topo-esquerda: x [20,250], y [35,45] (720p, scale=1).
  auto region=[&](float scale){return std::array<int,4>{int(20*scale),int(255*scale),int(33*scale),int(48*scale)};};
  float scale=1.f;auto reg=region(scale);
  auto renderAndCount=[&](){
   BeginDrawing();ClearBackground({75,85,95,255});DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight());EndDrawing();
   auto image=LoadImageFromScreen();int n=countBright(image,reg[0],reg[2],reg[1],reg[3]);UnloadImage(image);return n;
  };
  const int full=renderAndCount();fprintf(stderr,"HUD-DBG full bright=%d\n",full);
  int red=0,brightAny=0,bx0=9999,by0=9999,bx1=-1,by1=-1;
  {
   BeginDrawing();ClearBackground({75,85,95,255});DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight());EndDrawing();
   auto image=LoadImageFromScreen();
   for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){auto c=GetImageColor(image,x,y);if(c.r>150&&c.g<75&&c.b<100)red++;if(c.r>200&&c.g>200&&c.b>200){brightAny++;bx0=std::min(bx0,x);by0=std::min(by0,y);bx1=std::max(bx1,x);by1=std::max(by1,y);}}
   UnloadImage(image);
  }
  fprintf(stderr,"HUD-DBG red=%d brightAny=%d brightBox=[%d,%d]-[%d,%d]\n",red,brightAny,bx0,by0,bx1,by1);
  {
   BeginDrawing();ClearBackground({75,85,95,255});DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight());EndDrawing();
   auto image=LoadImageFromScreen();
   auto c1=GetImageColor(image,100,40);auto c2=GetImageColor(image,100,365);
   fprintf(stderr,"HUD-DBG px@(100,40)=%d,%d,%d px@(100,365)=%d,%d,%d\n",c1.r,c1.g,c1.b,c2.r,c2.g,c2.b);
   float fr=runtime.healthHud().sample(runtime.health(),runtime.settings().maxHealth,1.f/60);
   fprintf(stderr,"HUD-DBG fraction=%.3f\n",fr);
   UnloadImage(image);
  }
  check(full>20,"barra de vida não pintada (cheia)");
  check(red==0,"círculo vermelho antigo ainda presente");
  check(runtime.takeDamage(60,error),"dano");float first=runtime.healthHud().sample(runtime.health(),100,.016f);check(first>.4f&&first<1,"interpolação de dano não ocorre");
  for(int i=0;i<90;++i)runtime.healthHud().sample(runtime.health(),100,1.f/60);
  const int damaged=renderAndCount();check(damaged>0&&damaged<full,"barra não encolhe com dano");
  check(runtime.heal(30,error),"cura");for(int i=0;i<90;++i)runtime.healthHud().sample(runtime.health(),100,1.f/60);check(renderAndCount()>damaged,"cura não aumentou barra");
  check(runtime.takeDamage(100,error),"morte");check(renderAndCount()==0&&runtime.health()==0,"morte deve mostrar zero");
  check(runtime.resetCharacter(error),"respawn");check(renderAndCount()>=full-5&&runtime.health()==100,"respawn não restaurou vida");
  std::filesystem::create_directories(".turbo/logs");
  for(auto size:{Vector2{1280,720},Vector2{1600,900},Vector2{1920,1080}}){
   SetWindowSize(int(size.x),int(size.y));scale=std::clamp(size.y/720.f,.75f,1.5f);reg=region(scale);
   for(int i=0;i<4;++i)renderAndCount();check(renderAndCount()>20,"barra ausente ao redimensionar");
   auto image=LoadImageFromScreen();const auto path=".turbo/logs/health-hud-"+std::to_string(int(size.x))+".png";ExportImage(image,path.c_str());UnloadImage(image);
  }
  runtime.healthHud().visible=false;check(renderAndCount()==0,"HUD oculto ainda pintado");
  runtime.pause();runtime.healthHud().visible=true;check(renderAndCount()==0,"HUD vaza na pausa");
  runtime.resume();check(renderAndCount()>20,"HUD sumiu após retomar");runtime.stop(false);
  auto* script=scene.createScript(InstanceClass::Script,scene.ensureService(InstanceClass::ServerScriptService),"Hud.lua");script->source="linblux.ui.health_hud(false)";
  check(runtime.start(scene,error),"Lua HUD start");runtime.update(1.f/60,{});check(!runtime.healthHud().visible&&runtime.scripts().errorCount()==0,"Lua não pode ocultar HUD padrão");runtime.stop(false);
  scene.removeById(script->id());check(runtime.start(scene,error),"restart");check(runtime.healthHud().visible,"preferência HUD vazou entre Play");runtime.stop(false);
  puts("HUD: barra mono, dano/cura/morte/respawn, interpolação, Lua hide, pause, restart, 720p/900p/1080p: OK");
 }catch(const std::exception& e){fprintf(stderr,"HUD: %s\n",e.what());result=1;}
 ClearExperienceOverlayResources();CloseWindow();return result;
}

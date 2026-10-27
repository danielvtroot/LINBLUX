#include "rendering/PlayerMenu.h"
#include "game/Runtime.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace loblox {
bool PlayerMenu::draw(game::RuntimeSession& runtime,int width,int height,Font font,Texture2D logo,const std::string& title){
 const bool paused=open_;
 progress_=std::clamp(progress_+(paused?1.f:-1.f)*std::min(GetFrameTime(),.05f)/.18f,0.f,1.f);
 if(progress_<=0)return false;
 const float s=std::clamp(height/720.f,.75f,1.5f),w=std::min(width-16.f,std::clamp(width*.55f,480*s,840.f)),side=170*s;
 const float t=progress_*progress_*(3-2*progress_),left=-w*(1-t);
 const Color white{242,238,242,255},muted{185,177,185,255},red{215,38,63,255},panel{23,23,28,247};
 DrawRectangle(0,0,width,height,{0,0,0,(unsigned char)(90*t)});DrawRectangle(int(left),0,int(w),height,panel);
 DrawRectangle(int(left),0,int(side),height,{15,15,19,250});
 auto text=[&](const std::string& value,float x,float y,float size,Color color){DrawTextEx(font,value.c_str(),{x,y},size*s,s,color);};
 const auto mouse=GetMousePosition();int current=0;bool quit=false;
 if(paused){if(IsKeyPressed(KEY_TAB)||IsKeyPressed(KEY_DOWN))focus_=(focus_+1)%std::max(1,count_);if(IsKeyPressed(KEY_UP))focus_=(focus_+std::max(1,count_)-1)%std::max(1,count_);}
 auto button=[&](const std::string& label,Rectangle r,bool active=false){
  const int id=current++;const bool hover=paused&&CheckCollisionPointRec(mouse,r);
  DrawRectangleRounded(r,.12f,6,active?Color{91,35,47,255}:hover?Color{60,48,56,255}:Color{32,31,37,255});
  if(paused&&id==focus_)DrawRectangleRoundedLines(r,.12f,6,red);
  text(label,r.x+10*s,r.y+(r.height-17*s)/2,16,active?white:muted);
  return paused&&((hover&&IsMouseButtonReleased(MOUSE_BUTTON_LEFT))||(id==focus_&&IsKeyPressed(KEY_ENTER)));
 };
 if(logo.id)DrawTexturePro(logo,{0,0,float(logo.width),float(logo.height)},{left+16*s,18*s,34*s,34*s},{0,0},0,WHITE);
 text("LINBLUX",left+58*s,26*s,17,white);
 if(button("Continuar",{left+10*s,76*s,side-20*s,36*s},true)){open_=false;runtime.resume();}
 const char* pages[]={"Jogador","Câmera","Controles","Áudio","Vídeo","Ajuda"};
 for(int i=0;i<6;++i)if(button(pages[i],{left+10*s,(126+42*i)*s,side-20*s,36*s},page_==i)){page_=i;scroll_=0;}
 if(button("Sair",{left+10*s,height-54*s,side-20*s,38*s}))quit=true;
 const float x=left+side+22*s,available=w-side-44*s;
 std::string heading=title;while(!heading.empty()&&MeasureTextEx(font,heading.c_str(),17*s,s).x>available-42*s)heading.pop_back();
 text(heading,x,26*s,17,white);
 if(button("×",{left+w-48*s,18*s,32*s,32*s})){open_=false;runtime.resume();}
 DrawLine(int(x),int(66*s),int(left+w-18*s),int(66*s),{70,52,60,255});
 const Rectangle content{x,76*s,available,height-90*s};
 if(paused&&CheckCollisionPointRec(mouse,content))scroll_=std::clamp(scroll_-GetMouseWheelMove()*36*s,0.f,250*s);
 BeginScissorMode(int(content.x),int(content.y),int(content.width),int(content.height));
 float y=90*s-scroll_;
 auto line=[&](const std::string& value){text(value,x,y,16,muted);y+=30*s;};
 auto action=[&](const std::string& label){const bool clicked=button(label,{x,y,available,36*s});y+=48*s;return clicked;};
 auto slider=[&](const char* label,float& value,float min,float max){
  char b[100];std::snprintf(b,sizeof(b),"%s: %.2f",label,value);line(b);
  const int id=current++;Rectangle track{x,y,available,22*s};const bool hover=paused&&CheckCollisionPointRec(mouse,track)&&CheckCollisionPointRec(mouse,content);
  if(hover&&IsMouseButtonDown(MOUSE_BUTTON_LEFT))value=min+std::clamp((mouse.x-x)/available,0.f,1.f)*(max-min);
  if(paused&&id==focus_){if(IsKeyPressed(KEY_LEFT))value=std::max(min,value-(max-min)/20);if(IsKeyPressed(KEY_RIGHT))value=std::min(max,value+(max-min)/20);}
  DrawRectangle(int(x),int(y+9*s),int(available),int(4*s),{65,56,64,255});
  DrawRectangle(int(x),int(y+9*s),int(available*std::clamp((value-min)/std::max(.0001f,max-min),0.f,1.f)),int(4*s),red);
  DrawCircleV({x+available*std::clamp((value-min)/std::max(.0001f,max-min),0.f,1.f),y+11*s},6*s,white);y+=46*s;
 };
 auto& settings=runtime.settings();
 if(page_==0){line(runtime.rig().name);line("Vida: "+std::to_string(int(runtime.health()))+" / "+std::to_string(int(settings.maxHealth)));if(action("Renascer personagem")){std::string error;runtime.resetCharacter(error);}}
 if(page_==1){
  const char* modes[]={"Livre","Acompanhar","Primeira pessoa"};
  if(action(std::string("Modo: ")+modes[int(settings.cameraMode)]))settings.cameraMode=game::CameraMode((int(settings.cameraMode)+1)%3);
  const float oldDistance=settings.cameraDistance;slider("Distância",settings.cameraDistance,settings.minZoom,settings.maxZoom);
  if(settings.cameraDistance!=oldDistance)runtime.playerCamera().setDistance(settings.cameraDistance);
  if(action(settings.cameraLock?"Trava permanente: ativada":"Trava permanente: desativada"))settings.cameraLock=!settings.cameraLock;
  if(action(settings.enableShiftLock?"Shift Lock: ativado":"Shift Lock: desativado"))settings.enableShiftLock=!settings.enableShiftLock;
  if(action(settings.allowFirstPerson?"Primeira pessoa: permitida":"Primeira pessoa: bloqueada"))settings.allowFirstPerson=!settings.allowFirstPerson;
 }
 if(page_==2){slider("Sensibilidade",settings.sensitivity,.0001f,.03f);line("WASD: mover · Espaço: pular");line("Botão direito: girar câmera");line("E: segurar para interagir");line("1–9: equipar · Delete: soltar");line("PgUp / PgDn: trocar página");line("Esc: abrir e fechar menu");}
 if(page_==3){if(IsAudioDeviceReady()){float volume=GetMasterVolume();slider("Volume geral",volume,0,1);if(paused)SetMasterVolume(volume);}else line("Dispositivo de áudio indisponível.");}
 if(page_==4){if(action(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)?"Sair da tela cheia":"Entrar em tela cheia"))ToggleBorderlessWindowed();line("Ajuste a resolução pela janela.");}
 if(page_==5){line("LINBLUX Player · Alpha em desenvolvimento");line("Experiências locais com Lua e Bullet.");line("Use Esc para retornar ao jogo.");line("Menu protegido: scripts não o removem.");}
 EndScissorMode();count_=std::max(1,current);focus_=std::clamp(focus_,0,count_-1);return quit;
}
}

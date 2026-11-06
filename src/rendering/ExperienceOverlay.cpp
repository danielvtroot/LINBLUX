#include "rendering/ExperienceOverlay.h"
#include "game/Runtime.h"
#include "rendering/UiControls.h"
#include "ui/UiFont.h"
#include "game/ImageAssets.h"
#include <algorithm>
#include <cstdio>
namespace loblox {
namespace {
// Cada tamanho usa o atlas rasterizado nesse tamanho; o parametro Font continua
// valendo como reserva quando nao ha TTF no sistema.
Font Face(Font fallback,float size){
    const Font face=ui::Text(size);
    return face.texture.id==GetFontDefault().texture.id?fallback:face;
}
}
Font ExperienceOverlayFont(){return ui::SystemFont();}
void ClearExperienceOverlayResources(){ui::ReleaseSystemFont();}

void DrawHealthHud(const game::RuntimeSession& runtime,int width,int height,Font font){
    if(runtime.state()!=game::RuntimeState::Playing||!runtime.healthHud().visible)return;
    const float scale=std::clamp(height/720.f,.75f,1.5f);
    const float fraction=runtime.healthHud().sample(runtime.health(),runtime.settings().maxHealth,GetFrameTime());
    // Barra compacta monocromática — área segura superior-direita
    const float barW=180.f*scale,barH=8.f*scale,pad=10.f*scale;
    const float px=width-barW-pad*2.f-60.f*scale,py=16.f*scale;
    const float panelW=barW+pad*2.f+56.f*scale,panelH=barH+24.f*scale;
    // Fundo preto translúcido
    DrawRectangleRounded({px-pad,py-pad*.5f,panelW,panelH},.18f,6,{0,0,0,180});
    // Rótulo VIDA e valor em tamanho de corpo: 10/12px ampliados de um atlas
    // 32px eram o texto borrado do HUD.
    const int labelPx=ui::FontPixels(13.f*scale),valuePx=ui::FontPixels(16.f*scale);
    DrawTextEx(Face(font,13.f*scale),"VIDA",{px,py-1.f*scale},float(labelPx),scale,{220,220,220,200});
    ui::ProgressBar({px,py+12.f*scale,barW,barH},fraction);
    // Valor numérico
    char value[32];std::snprintf(value,sizeof(value),"%.0f",runtime.health());
    const auto face=Face(font,16.f*scale);
    const auto mv=MeasureTextEx(face,value,float(valuePx),scale);
    DrawTextEx(face,value,{px+barW+8.f*scale,py+12.f*scale+(barH-mv.y)/2.f},float(valuePx),scale,{255,255,255,220});
}

void DrawExperienceOverlay(const game::RuntimeSession& runtime,int width,int height,Font font){
    if(runtime.state()!=game::RuntimeState::Playing)return;
    DrawHealthHud(runtime,width,height,font);
    const float scale=std::clamp(height/720.f,1.f,1.4f);const Color panel{15,15,19,225},focus{220,220,220,220},text{242,242,245,255},muted{185,185,195,255};
    auto label=[&](const std::string& value,float x,float y,float size,Color color){
        DrawTextEx(Face(font,size*scale),value.c_str(),{x,y},float(ui::FontPixels(size*scale)),scale,color);};
    auto fit=[&](std::string value,float available,float size){
        const auto face=Face(font,size*scale);const float px=float(ui::FontPixels(size*scale));
        while(!value.empty()&&MeasureTextEx(face,(value+"...").c_str(),px,scale).x>available){size_t i=value.size()-1;while(i&&(static_cast<unsigned char>(value[i])&0xc0)==0x80)--i;value.resize(i);}return value;};
    std::vector<InstanceId> slots;for(auto id:runtime.inventory().list())if(runtime.inventory().info(id).equipable)slots.push_back(id);
    const unsigned first=runtime.hotbarPage()*9,visible=first<slots.size()?unsigned(std::min<size_t>(9,slots.size()-first)):0;
    if(visible){const float slot=std::min(104.f*scale,(width-32.f)/visible-6*scale),gap=6*scale,total=visible*slot+(visible-1)*gap;
        const float start=(width-total)/2,y=height-76*scale;
        for(unsigned i=0;i<visible;++i){const auto id=slots[first+i];const auto item=runtime.inventory().info(id);const float x=start+i*(slot+gap);const bool active=runtime.inventory().equipped()==id;
            DrawRectangleRounded({x,y,slot,60*scale},.14f,6,panel);DrawRectangle(int(x),int(y+57*scale),int(slot),int(3*scale),active?focus:Color{65,65,74,230});
            label(std::to_string(i+1),x+7*scale,y+5*scale,13,active?focus:muted);
            bool iconDrawn=false;if(!item.icon.empty()){const auto path=game::ResolveImagePath(game::AssetsDir(),item.icon);if(!path.empty()){const auto texture=game::AssetTexture(path);if(texture.id){const float side=30*scale;DrawTexturePro(texture,{0,0,float(texture.width),float(texture.height)},{x+(slot-side)/2,y+4*scale,side,side},{0,0},0,WHITE);iconDrawn=true;}}}
            const auto name=fit(item.name,slot-12*scale,17);label(name==item.name?name:name+"...",x+6*scale,y+(iconDrawn?38:29)*scale,17,text);
        }
        if(slots.size()>9)label("PgUp / PgDn · página "+std::to_string(runtime.hotbarPage()+1),start,y-22*scale,13,muted);
    }
    const auto& prompt=runtime.interactions().prompt();if(prompt.object){const float w=std::min(380*scale,float(width)-32),x=(width-w)/2,y=height-(visible?168:112)*scale;
        DrawRectangleRounded({x,y,w,76*scale},.12f,6,panel);label("Segure "+prompt.key+(prompt.gamepad?" / botão X":""),x+16*scale,y+10*scale,14,muted);
        const auto name=fit(prompt.label,w-32*scale,19);label(name==prompt.label?name:name+"...",x+16*scale,y+31*scale,19,text);
        DrawRectangle(int(x+16*scale),int(y+63*scale),int(w-32*scale),int(4*scale),{60,60,68,255});DrawRectangle(int(x+16*scale),int(y+63*scale),int((w-32*scale)*prompt.progress),int(4*scale),focus);
    }
    if(!runtime.itemNotice().empty()){const auto message=fit(runtime.itemNotice(),width-48*scale,15);label(message,24*scale,height-205*scale,15,focus);}
}
}

#include "rendering/ExperienceOverlay.h"
#include "game/Runtime.h"
#include "game/ImageAssets.h"
#include <algorithm>
#include <cstdio>
namespace loblox {
namespace {Font overlayFont{};bool fontAttempted=false;
Font OverlayFont(){if(!fontAttempted){fontAttempted=true;for(const char* file:{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"})if(FileExists(file)){const auto loaded=LoadFontEx(file,32,nullptr,256);if(loaded.texture.id!=GetFontDefault().texture.id)overlayFont=loaded;if(overlayFont.texture.id)SetTextureFilter(overlayFont.texture,TEXTURE_FILTER_BILINEAR);break;}}return overlayFont.texture.id?overlayFont:GetFontDefault();}}
Font ExperienceOverlayFont(){return OverlayFont();}
void ClearExperienceOverlayResources(){if(overlayFont.texture.id){if(IsWindowReady())UnloadFont(overlayFont);else{UnloadFontData(overlayFont.glyphs,overlayFont.glyphCount);MemFree(overlayFont.recs);}}overlayFont={};fontAttempted=false;}

void DrawHealthHud(const game::RuntimeSession& runtime,int width,int height,Font font){
    if(runtime.state()!=game::RuntimeState::Playing||!runtime.healthHud().visible)return;
    if(font.texture.id==GetFontDefault().texture.id)font=OverlayFont();
    const float scale=std::clamp(height/720.f,.75f,1.5f),radius=34*scale;
    const Vector2 center{width-52*scale,58*scale};
    const float fraction=runtime.healthHud().sample(runtime.health(),runtime.settings().maxHealth,GetFrameTime());
    DrawCircleV(center,radius+7*scale,{12,12,16,225});
    DrawRing(center,radius-5*scale,radius,0,360,96,{62,57,63,255});
    // raylib's ring starts at +X; -90 places zero at the top.
    if(fraction>0)DrawRing(center,radius-5*scale,radius,-90,-90+360*fraction,96,{215,38,63,255});
    char value[32];std::snprintf(value,sizeof(value),"%.0f",runtime.health());
    const float size=21*scale;const auto measured=MeasureTextEx(font,value,size,scale);
    DrawTextEx(font,value,{center.x-measured.x/2,center.y-13*scale},size,scale,{246,242,245,255});
    std::snprintf(value,sizeof(value),"/ %.0f",runtime.settings().maxHealth);
    const auto sub=MeasureTextEx(font,value,10*scale,.5f*scale);
    DrawTextEx(font,value,{center.x-sub.x/2,center.y+11*scale},10*scale,.5f*scale,{185,177,184,255});
}

void DrawExperienceOverlay(const game::RuntimeSession& runtime,int width,int height,Font font){
    if(runtime.state()!=game::RuntimeState::Playing)return;
    DrawHealthHud(runtime,width,height,font);
    if(font.texture.id==GetFontDefault().texture.id)font=OverlayFont();
    const float scale=std::clamp(height/720.f,1.f,1.4f);const Color panel{15,15,19,225},red{215,38,63,255},text{242,242,245,255},muted{185,185,195,255};
    auto label=[&](const std::string& value,float x,float y,float size,Color color){DrawTextEx(font,value.c_str(),{x,y},size*scale,scale,color);};
    auto fit=[&](std::string value,float available,float size){while(!value.empty()&&MeasureTextEx(font,(value+"...").c_str(),size*scale,scale).x>available){size_t i=value.size()-1;while(i&&(static_cast<unsigned char>(value[i])&0xc0)==0x80)--i;value.resize(i);}return value;};
    std::vector<InstanceId> slots;for(auto id:runtime.inventory().list())if(runtime.inventory().info(id).equipable)slots.push_back(id);
    const unsigned first=runtime.hotbarPage()*9,visible=first<slots.size()?unsigned(std::min<size_t>(9,slots.size()-first)):0;
    if(visible){const float slot=std::min(104.f*scale,(width-32.f)/visible-6*scale),gap=6*scale,total=visible*slot+(visible-1)*gap;
        const float start=(width-total)/2,y=height-76*scale;
        for(unsigned i=0;i<visible;++i){const auto id=slots[first+i];const auto item=runtime.inventory().info(id);const float x=start+i*(slot+gap);const bool active=runtime.inventory().equipped()==id;
            DrawRectangleRounded({x,y,slot,60*scale},.14f,6,panel);DrawRectangle(int(x),int(y+57*scale),int(slot),int(3*scale),active?red:Color{65,65,74,230});
            label(std::to_string(i+1),x+7*scale,y+5*scale,13,active?red:muted);
            bool iconDrawn=false;if(!item.icon.empty()){const auto path=game::ResolveImagePath(game::AssetsDir(),item.icon);if(!path.empty()){const auto texture=game::AssetTexture(path);if(texture.id){const float side=30*scale;DrawTexturePro(texture,{0,0,float(texture.width),float(texture.height)},{x+(slot-side)/2,y+4*scale,side,side},{0,0},0,WHITE);iconDrawn=true;}}}
            const auto name=fit(item.name,slot-12*scale,17);label(name==item.name?name:name+"...",x+6*scale,y+(iconDrawn?38:29)*scale,17,text);
        }
        if(slots.size()>9)label("PgUp / PgDn · página "+std::to_string(runtime.hotbarPage()+1),start,y-22*scale,13,muted);
    }
    const auto& prompt=runtime.interactions().prompt();if(prompt.object){const float w=std::min(380*scale,float(width)-32),x=(width-w)/2,y=height-(visible?168:112)*scale;
        DrawRectangleRounded({x,y,w,76*scale},.12f,6,panel);label("Segure "+prompt.key+(prompt.gamepad?" / botão X":""),x+16*scale,y+10*scale,14,muted);
        const auto name=fit(prompt.label,w-32*scale,19);label(name==prompt.label?name:name+"...",x+16*scale,y+31*scale,19,text);
        DrawRectangle(int(x+16*scale),int(y+63*scale),int(w-32*scale),int(4*scale),{60,60,68,255});DrawRectangle(int(x+16*scale),int(y+63*scale),int((w-32*scale)*prompt.progress),int(4*scale),red);
    }
    if(!runtime.itemNotice().empty()){const auto message=fit(runtime.itemNotice(),width-48*scale,15);label(message,24*scale,height-205*scale,15,red);}
}
}

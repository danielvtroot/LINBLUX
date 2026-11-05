#include "rendering/UiControls.h"
#include <algorithm>
#include <cmath>
#include <array>
namespace loblox::ui {
namespace {std::array<bool,512> queuedKeys{};}
void BeginInputFrame() {
    queuedKeys.fill(false);
    for(int key=GetKeyPressed();key;key=GetKeyPressed())
        if(key>0&&key<int(queuedKeys.size()))queuedKeys[key]=true;
}
bool KeyPressed(int key) {
    return (key>=0&&key<int(queuedKeys.size())&&queuedKeys[key])||IsKeyPressed(key);
}

void DrawSurface(Rectangle r,Surface surface,bool highlighted) {
    const Color base=surface==Surface::Overlay?theme::overlay:surface==Surface::Raised?theme::raised:theme::panel;
    DrawRectangleRounded(r,.12f,12,base);
    if(highlighted)DrawRectangleRounded(r,.12f,12,theme::selected);
}
bool Button(const char* label,Rectangle r,Font font,float scale,bool enabled,bool focused,bool active) {
    const bool hover=enabled&&CheckCollisionPointRec(GetMousePosition(),r);
    DrawSurface(r,Surface::Interactive,active||hover);
    if(enabled&&focused)DrawRectangleRoundedLines(r,.12f,12,theme::accent);
    const float size=16*scale;
    DrawTextEx(font,label,{r.x+10*scale,r.y+(r.height-size)/2},size,scale,enabled?theme::text:theme::disabled);
    return enabled&&((hover&&IsMouseButtonReleased(MOUSE_BUTTON_LEFT))||(focused&&KeyPressed(KEY_ENTER)));
}
bool Slider(Rectangle r,float& value,float minimum,float maximum,bool enabled,bool focused,bool& dragging,float scale,bool pointerAvailable) {
    const float before=value,range=maximum-minimum;
    if(!enabled||!IsMouseButtonDown(MOUSE_BUTTON_LEFT))dragging=false;
    const Rectangle hit{r.x-8*scale,r.y,r.width+16*scale,r.height};
    if(enabled&&pointerAvailable&&CheckCollisionPointRec(GetMousePosition(),hit)&&
       IsMouseButtonDown(MOUSE_BUTTON_LEFT))dragging=true;
    if(enabled&&dragging)value=minimum+std::clamp((GetMousePosition().x-r.x)/r.width,0.f,1.f)*range;
    if(enabled&&focused){if(KeyPressed(KEY_HOME))value=minimum;if(KeyPressed(KEY_END))value=maximum;
        if(KeyPressed(KEY_LEFT))value-=range/20;if(KeyPressed(KEY_RIGHT))value+=range/20;}
    value=std::clamp(value,minimum,maximum);
    const float fraction=range>0?(value-minimum)/range:0;
    const Rectangle track{r.x,r.y+r.height/2-2*scale,r.width,4*scale};
    DrawRectangleRec(track,theme::border);
    // Keep the thumb distinguishable from the filled track at every volume.
    DrawRectangleRec({track.x,track.y,track.width*fraction,track.height},theme::secondary);
    DrawCircleV({r.x+r.width*fraction,r.y+r.height/2},6*scale,enabled?theme::text:theme::disabled);
    return before!=value;
}
void ProgressBar(Rectangle r,float fraction) {
    DrawRectangleRounded(r,1,12,theme::border);
    fraction=std::clamp(fraction,0.f,1.f);
    if(fraction<=0)return;
    // Preserve the actual proportion even below the usual capsule diameter.
    // No clip-state mutation: also safe inside an existing panel scissor.
    DrawRectangleRounded({r.x,r.y,r.width*fraction,r.height},1,12,theme::text);
}
}

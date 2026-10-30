#pragma once
#include "raylib.h"
#include <string>
namespace loblox::game {class RuntimeSession;}
namespace loblox {
// Standalone Player presentation. The Studio uses the same settings/actions
// through its RmlUi adapter; neither is part of script-created game UI.
class PlayerMenu {
public:
    bool draw(game::RuntimeSession& runtime,int width,int height,Font font,Texture2D logo,const std::string& title);
    float progress()const{return progress_;}
    bool open()const{return open_;}
    void toggle(){open_=!open_;}
private:
    bool open_=false;
    float progress_=0,scroll_=0,lastAudibleVolume_=1.f;
    int page_=0,focus_=0,count_=1,draggingSlider_=-1;
};
}

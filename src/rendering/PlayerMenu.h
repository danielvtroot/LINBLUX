#pragma once
#include "raylib.h"
#include <string>
namespace loblox::game {class RuntimeSession;}
namespace loblox {
// Original Player presentation, hosted by the Studio gameplay viewport.
class BrandLogo3D;
class PlayerMenu {
public:
    bool draw(game::RuntimeSession& runtime,int width,int height,Font font,Texture2D logo,const std::string& title, const BrandLogo3D* brand=nullptr);
    float progress()const{return progress_;}
    bool open()const{return open_;}
    void setOpen(bool value){open_=value;}
    bool takeFullscreenRequest(){const bool value=fullscreenRequested_;fullscreenRequested_=false;return value;}
    void toggle(){open_=!open_;}
private:
    bool open_=false;
    bool fullscreenRequested_=false;
    float progress_=0,scroll_=0,lastAudibleVolume_=1.f;
    int page_=0,focus_=0,count_=1,draggingSlider_=-1;
    bool draggingMenu_=false;
    float dragOffsetX_=0,dragOffsetY_=0,menuLeft_=0;
};
}

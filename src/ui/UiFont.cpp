#include "ui/UiFont.h"
#include <initializer_list>
namespace loblox::ui {
namespace { Font font{}; bool attempted=false; }
Font SystemFont() {
    if(!attempted) {
        attempted=true;
        for(const char* file:{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"}) {
            if(!FileExists(file))continue;
            const auto loaded=LoadFontEx(file,32,nullptr,256);
            if(loaded.texture.id!=GetFontDefault().texture.id)font=loaded;
            if(font.texture.id)SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
            break;
        }
    }
    return font.texture.id?font:GetFontDefault();
}
void ReleaseSystemFont() {
    if(font.texture.id) {
        if(IsWindowReady())UnloadFont(font);
        else {UnloadFontData(font.glyphs,font.glyphCount);MemFree(font.recs);}
    }
    font={};attempted=false;
}
}

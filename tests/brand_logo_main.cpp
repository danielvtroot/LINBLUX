#include "rendering/BrandLogo3D.h"
#include <cstdio>
int main() {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280,720,"LINBLUX brand GLB validation");
    if(!IsWindowReady())return 77;
    int failures=0;
    {
        loblox::BrandLogo3D brand;
        if(!brand.update(0,52))++failures;
        const auto texture=brand.texture();
        Image first=LoadImageFromTexture(texture);
        int ink=0,clear=0;
        if(first.data)for(int y=0;y<first.height;++y)for(int x=0;x<first.width;++x){auto c=GetImageColor(first,x,y);ink+=c.a>200;clear+=c.a==0;}
        if(ink<20||clear<first.width*first.height/4)++failures;
        if(first.data&&GetImageColor(first,0,0).a!=0)++failures;
        for(int i=0;i<30;++i)brand.update(1.f/30,52);
        Image rotated=LoadImageFromTexture(brand.texture());
        int changed=0;
        if(first.data&&rotated.data)for(int y=0;y<first.height;++y)for(int x=0;x<first.width;++x){auto a=GetImageColor(first,x,y),b=GetImageColor(rotated,x,y);changed+=a.r!=b.r||a.g!=b.g||a.b!=b.b||a.a!=b.a;}
        if(changed<10||brand.texture().id!=texture.id)++failures;
        BeginDrawing();ClearBackground({40,50,60,255});brand.draw({16,16,52,52});brand.draw({100,16,104,104});EndDrawing();
        TakeScreenshot("/tmp/linblux-brand-logo.png");
        UnloadImage(first);UnloadImage(rotated);
    }
    CloseWindow();
    std::printf("Brand GLB: ink=%s; %d failures\n",failures?"invalid":"valid",failures);
    return failures?1:0;
}

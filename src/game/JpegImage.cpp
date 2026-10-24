#include "game/ImageAssets.h"
#include "core/ImageInfo.h"
#include <cstdio>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 16384
#define STBI_MALLOC(n) MemAlloc(static_cast<unsigned int>(n))
#define STBI_REALLOC(p,n) MemRealloc(p,static_cast<unsigned int>(n))
#define STBI_FREE(p) MemFree(p)
#include <stb_image.h>

namespace loblox::game {
Image LoadProjectImage(const std::string& path) {
    ImageInfo info;if(!ReadSafeImageInfo(path,info))return {};
    FILE* file=std::fopen(path.c_str(),"rb");if(!file)return {};
    const bool jpeg=std::fgetc(file)==0xff&&std::fgetc(file)==0xd8;std::fclose(file);
    if(!jpeg)return LoadImage(path.c_str()); // one existing PNG decoder
    int width=0,height=0,channels=0;
    auto* pixels=stbi_load(path.c_str(),&width,&height,&channels,4);
    if(!pixels)return {};
    if(width!=int(info.width)||height!=int(info.height)){stbi_image_free(pixels);return {};}
    return Image{pixels,width,height,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
}

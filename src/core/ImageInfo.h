#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace loblox {
struct ImageInfo { unsigned width=0, height=0; };
// Bound compressed bytes AND decoded RGBA allocation before the decoder runs.
// No fixed image size or aspect ratio. Wide 8K images are valid within budget.
inline bool ReadSafeImageInfo(const std::string& path, ImageInfo& result) {
    std::error_code error;
    const auto bytes=std::filesystem::file_size(path,error);
    if(error||bytes<24||bytes>32u*1024u*1024u)return false;
    std::ifstream file(path,std::ios::binary);
    unsigned char h[24]{};file.read(reinterpret_cast<char*>(h),24);
    const auto valid=[&](unsigned w,unsigned height){
        if(!w||!height||w>16384||height>16384||uint64_t(w)*height>16u*1024u*1024u)return false;
        result={w,height};return true;
    };
    const unsigned char signature[]={137,80,78,71,13,10,26,10};
    bool png=true;for(int i=0;i<8;++i)png&=h[i]==signature[i];
    if(png) {
        if(h[12]!='I'||h[13]!='H'||h[14]!='D'||h[15]!='R')return false;
        const auto read=[&](int i){return (uint32_t(h[i])<<24)|(uint32_t(h[i+1])<<16)|(uint32_t(h[i+2])<<8)|h[i+3];};
        return valid(read(16),read(20));
    }
    if(h[0]!=0xff||h[1]!=0xd8)return false;
    file.seekg(2);
    while(file) {
        if(file.get()!=0xff)return false;
        int marker=file.get();while(marker==0xff)marker=file.get();
        if(marker<0||marker==0xd9||marker==0xda)return false;
        if(marker==0x01||(marker>=0xd0&&marker<=0xd8))continue;
        const int hi=file.get(),lo=file.get();if(hi<0||lo<0)return false;
        const int length=(hi<<8)|lo;if(length<2)return false;
        if(marker>=0xc0&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc) {
            if(length<8||file.get()<0)return false;
            const int yh=file.get(),yl=file.get(),xh=file.get(),xl=file.get();
            return yh>=0&&yl>=0&&xh>=0&&xl>=0&&valid((xh<<8)|xl,(yh<<8)|yl);
        }
        file.seekg(length-2,std::ios::cur);
    }
    return false;
}
} // namespace loblox

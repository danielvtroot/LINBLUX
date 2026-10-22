#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace loblox::ui {
// Preflight dimensions before a decoder can allocate an unbounded image.
// PNG IHDR and JPEG SOF are metadata only; decoding remains raylib's job.
inline bool SafeImageFile(const std::string& path) {
    std::error_code error;
    const auto bytes=std::filesystem::file_size(path,error);
    if(error||bytes<24||bytes>32u*1024u*1024u)return false;
    std::ifstream file(path,std::ios::binary);
    unsigned char header[24]{};file.read(reinterpret_cast<char*>(header),24);
    const auto valid=[](unsigned w,unsigned h){return w>0&&h>0&&w<=4096&&h<=4096;};
    const unsigned char signature[]={137,80,78,71,13,10,26,10};
    bool png=true;for(int i=0;i<8;++i)png&=header[i]==signature[i];
    if(png) {
        if(header[12]!='I'||header[13]!='H'||header[14]!='D'||header[15]!='R')return false;
        const auto read=[&](int i){return (uint32_t(header[i])<<24)|(uint32_t(header[i+1])<<16)|(uint32_t(header[i+2])<<8)|header[i+3];};
        return valid(read(16),read(20));
    }
    if(header[0]!=0xff||header[1]!=0xd8)return false;
    file.seekg(2);
    const auto byte=[&](){return file.get();};
    while(file) {
        if(byte()!=0xff)return false;
        int marker=byte();while(marker==0xff)marker=byte();
        if(marker<0||marker==0xd9||marker==0xda)return false;
        if(marker==0x01||(marker>=0xd0&&marker<=0xd8))continue;
        const int hi=byte(),lo=byte();if(hi<0||lo<0)return false;
        const int length=(hi<<8)|lo;if(length<2)return false;
        const bool sof=(marker>=0xc0&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc);
        if(sof) {
            if(length<8||byte()<0)return false;
            const int yh=byte(),yl=byte(),xh=byte(),xl=byte();
            return yh>=0&&yl>=0&&xh>=0&&xl>=0&&valid((xh<<8)|xl,(yh<<8)|yl);
        }
        file.seekg(length-2,std::ios::cur);
    }
    return false;
}
}

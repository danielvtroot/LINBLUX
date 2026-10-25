#include "media/VideoBackground.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cmath>
using namespace loblox::media;
int main(int argc,char** argv) {
    const auto r=VideoBackground::cover(1280,720,1000,1000);
    if(std::abs(r.height-1000)>1||r.width<1000||r.x>=0||std::abs(r.y)>0.01f)return 1;
    VideoBackground video;video.start("/arquivo/inexistente.mp4");
    for(int i=0;i<100&&!video.failed();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if(!video.failed())return 2;video.stop();
    if(video.running())return 3;
    const auto start=std::chrono::steady_clock::now();video.start(argv[1]);
    if(std::chrono::steady_clock::now()-start>std::chrono::milliseconds(50))return 4;
    // No GL context is needed for decoding. Decode more than the complete
    // 24-second clip to prove seek/flush/drain loop and bounded streaming.
    const auto deadline=start+std::chrono::seconds(32);
    while(video.loops()==0&&!video.failed()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(25));
    std::printf("Video: %llu frames, %llu loops, failure=%d\n",(unsigned long long)video.decodedFrames(),(unsigned long long)video.loops(),video.failed());
    const bool valid=!video.failed()&&video.loops()>0&&video.decodedFrames()>=700;
    const auto stop=std::chrono::steady_clock::now();video.stop();
    return valid&&!video.running()&&std::chrono::steady_clock::now()-stop<std::chrono::milliseconds(500)?0:5;
}

#pragma once
#include "raylib.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace loblox::media {
// Local, silent, bounded streaming. All decoder calls run on the worker;
// texture ownership and uploads stay on the graphics thread.
class VideoBackground {
public:
    ~VideoBackground();
    void start(const std::string& path);
    void stop();
    void draw(int width,int height);
    bool running() const {return worker_.joinable();}
    bool failed() const {return failed_;}
    uint64_t decodedFrames() const {return decoded_;}
    uint64_t loops() const {return loops_;}
    static Rectangle cover(int sourceWidth,int sourceHeight,int width,int height);
private:
    void decode(std::string path);
    std::thread worker_;
    std::atomic<bool> stopping_{false},failed_{false};
    std::atomic<uint64_t> decoded_{0},loops_{0};
    std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<unsigned char> pending_;
    bool ready_=false;
    int width_=0,height_=0;
    Texture2D texture_{};
};
}

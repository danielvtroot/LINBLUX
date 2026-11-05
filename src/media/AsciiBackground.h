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

// ASCII/Braille fullscreen animation background (ported from animate.py)
// Renders to a Raylib Texture2D on the graphics thread; animation runs in worker thread.
class AsciiBackground {
public:
    ~AsciiBackground();
    void start(const std::string& asciiPath);
    void stop();
    void draw(int width, int height);
    bool running() const { return worker_.joinable(); }
    bool failed() const { return failed_; }
    uint64_t frames() const { return frames_; }

    static Rectangle cover(int sourceWidth, int sourceHeight, int width, int height);

private:
    void animateLoop(std::string asciiPath);
    std::thread worker_;
    std::atomic<bool> stopping_{false}, failed_{false};
    std::atomic<uint64_t> frames_{0};
    std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<unsigned char> pending_;
    bool ready_ = false;
    int width_ = 0, height_ = 0;
    Texture2D texture_{};
};

} // namespace loblox::media

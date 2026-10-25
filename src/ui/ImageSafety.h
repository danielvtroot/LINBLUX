#pragma once
#include "core/ImageInfo.h"

namespace loblox::ui {
// Preflight dimensions before a decoder can allocate an unbounded image.
// PNG IHDR and JPEG SOF are metadata only; decoding remains raylib's job.
inline bool SafeImageFile(const std::string& path) {ImageInfo info;return ReadSafeImageInfo(path,info);}
}

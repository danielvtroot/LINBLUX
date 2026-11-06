#include "ui/UiFont.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
namespace loblox::ui {
namespace {
// Um atlas por tamanho efetivo. O teto evita crescimento sem fim quando a
// janela muda de tamanho: ao encher, reaproveita o atlas mais proximo em vez de
// carregar outro, porque descarregar uma textura ainda referenciada por um lote
// pendente deixaria o desenho do frame apontando para memoria liberada.
constexpr int kMinPixels = 8, kMaxPixels = 48, kMaxAtlases = 32;
std::map<int, Font>& Atlases() { static std::map<int, Font> cache; return cache; }
const char* SourceFont() {
    for(const char* file:{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                          "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                          "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"})
        if(FileExists(file))return file;
    return nullptr;
}
}  // namespace

int FontPixels(float size) {
    return std::clamp(static_cast<int>(std::lround(size)),kMinPixels,kMaxPixels);
}

Font Text(float size) {
    const int pixels = FontPixels(size);
    auto& cache = Atlases();
    if(const auto found = cache.find(pixels); found != cache.end()) return found->second;
    const char* file = SourceFont();
    if(!file) return GetFontDefault();
    if(static_cast<int>(cache.size()) >= kMaxAtlases) {
        auto best = cache.begin();
        for(auto it = cache.begin(); it != cache.end(); ++it)
            if(std::abs(it->first - pixels) < std::abs(best->first - pixels)) best = it;
        return best->second;
    }
    const Font loaded = LoadFontEx(file, pixels, nullptr, 256);
    if(!loaded.texture.id || loaded.texture.id == GetFontDefault().texture.id) {
        if(loaded.texture.id) UnloadFont(loaded);
        return GetFontDefault();
    }
    // Bilinear at 1:1 keeps glyph coverage; it only softens subpixel positions.
    SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
    return cache.emplace(pixels, loaded).first->second;
}

Font SystemFont() { return Text(16); }

void ReleaseSystemFont() {
    auto& cache = Atlases();
    if(cache.empty()) return;
    for(auto& entry : cache) {
        if(IsWindowReady()) UnloadFont(entry.second);
        else {UnloadFontData(entry.second.glyphs, entry.second.glyphCount); MemFree(entry.second.recs);}
    }
    cache.clear();
}
}

#include "ui/RmlFrontendSupport.h"

#include "game/ImageAssets.h"
#include "ui/ImageSafety.h"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace loblox::ui {
namespace {

// Draws a circle outline with a stroke width. raylib's ImageDrawCircleLines
// is always one pixel wide, so the icons would lose their stroke weight
// when the artwork is rasterised at a higher resolution.
void StrokeCircle(Image& image, float cx, float cy, float radius, float thickness, Color color) {
    const float inner = std::max(0.0f, radius - thickness * 0.5f);
    const float outer = radius + thickness * 0.5f;
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - outer - 1.0f)));
    const int x1 = std::min(image.width - 1, static_cast<int>(std::ceil(cx + outer + 1.0f)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - outer - 1.0f)));
    const int y1 = std::min(image.height - 1, static_cast<int>(std::ceil(cy + outer + 1.0f)));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = x + 0.5f - cx;
            const float dy = y + 0.5f - cy;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d >= inner && d <= outer) ImageDrawPixel(&image, x, y, color);
        }
    }
}

}  // namespace

Image IconImage(const std::string& name, int resolution) {
    const float scale = static_cast<float>(resolution) / static_cast<float>(kIconGrid);
    Image image = GenImageColor(resolution, resolution, BLANK);
    Color ink{231, 236, 245, 255};
    if (name == "workspace" || name == "part" || name == "move" || name == "spawn") ink = {124, 188, 242, 255};
    if (name == "folder" || name == "storage" || name == "lighting" || name == "rotate") ink = {241, 203, 105, 255};
    if (name == "model" || name == "module" || name == "rig") ink = {178, 155, 237, 255};
    if (name == "script" || name == "local-script" || name == "animation" || name == "scale") ink = {115, 210, 173, 255};
    const auto px = [scale](float v) { return v * scale; };
    const auto ix = [scale](float v) { return static_cast<int>(std::lround(v * scale)); };
    const auto line = [&](int x, int y, int a, int b) {
        const int thick = std::max(1, ix(name.rfind("chevron-", 0) == 0 ? 6.0f
                                         : name == "menu" ? 4.5f : 3.0f));
        ImageDrawLineEx(&image, {px(static_cast<float>(x)), px(static_cast<float>(y))},
                        {px(static_cast<float>(a)), px(static_cast<float>(b))}, thick, ink);
    };
    const auto box = [&](int x, int y, int w, int h) {
        line(x, y, x + w, y);
        line(x + w, y, x + w, y + h);
        line(x + w, y + h, x, y + h);
        line(x, y + h, x, y);
    };
    const auto circle = [&](int cx, int cy, int r) {
        StrokeCircle(image, px(static_cast<float>(cx)), px(static_cast<float>(cy)), px(static_cast<float>(r)), std::max(1.0f, scale), ink);
    };
    const auto disc = [&](int cx, int cy, int r, Color color) {
        ImageDrawCircle(&image, ix(static_cast<float>(cx)), ix(static_cast<float>(cy)), std::max(1, ix(static_cast<float>(r))), color);
    };
    const auto triangle = [&](float ax, float ay, float bx, float by, float cx2, float cy2) {
        ImageDrawTriangle(&image, {px(ax), px(ay)}, {px(bx), px(by)}, {px(cx2), px(cy2)}, ink);
    };
    const auto solid = [&](int x, int y, int w, int h) {
        ImageDrawRectangle(&image, ix(static_cast<float>(x)), ix(static_cast<float>(y)), std::max(1, ix(static_cast<float>(w))),
                           std::max(1, ix(static_cast<float>(h))), ink);
    };

    if (name == "player-close") {
        line(10, 10, 38, 38); line(38, 10, 10, 38);
    } else if (name == "player-menu") {
        for (int y : {13, 24, 35}) line(7, y, 41, y);
    } else if (name == "player-left" || name == "player-right") {
        const bool left = name == "player-left";
        line(left ? 31 : 17, 8, left ? 16 : 32, 24);
        line(left ? 16 : 32, 24, left ? 31 : 17, 40);
    } else if (name == "player-settings") {
        // Original cog silhouette, not a downloaded brand/icon asset.
        circle(24, 24, 13);
        circle(24, 24, 5);
        for (int i = 0; i < 8; ++i) {
            const float angle = i * PI / 4.f;
            line(24 + int(14 * std::cos(angle)), 24 + int(14 * std::sin(angle)),
                 24 + int(19 * std::cos(angle)), 24 + int(19 * std::sin(angle)));
        }
    } else if (name == "player-person") {
        circle(24, 14, 8);
        line(17, 27, 31, 27); line(17, 27, 12, 33);
        line(31, 27, 36, 33); line(12, 33, 10, 42);
        line(36, 33, 38, 42); line(10, 42, 38, 42);
    } else if (name == "player-controls") {
        box(5, 12, 38, 27);
        line(12, 25, 24, 25); line(18, 19, 18, 31);
        disc(33, 22, 2, ink); disc(37, 29, 2, ink);
    } else if (name == "player-leave") {
        line(23, 8, 9, 8); line(9, 8, 9, 40); line(9, 40, 23, 40);
        line(20, 24, 41, 24); line(32, 15, 41, 24); line(41, 24, 32, 33);
    } else if (name == "player-heart") {
        line(24, 39, 9, 24); line(9, 24, 9, 15); line(9, 15, 15, 9);
        line(15, 9, 21, 9); line(21, 9, 24, 14); line(24, 14, 28, 9);
        line(28, 9, 34, 9); line(34, 9, 40, 15); line(40, 15, 40, 24);
        line(40, 24, 24, 39);
    } else if (name == "chevron-right") {
        line(18, 11, 31, 24);
        line(31, 24, 18, 37);
    } else if (name == "chevron-down") {
        line(11, 18, 24, 31);
        line(24, 31, 37, 18);
    } else if (name == "workspace") {
        circle(24, 24, 18);
        line(7, 24, 41, 24);
        line(24, 6, 24, 42);
        box(16, 9, 16, 30);
    } else if (name == "window") {
        box(6, 9, 36, 28);
        line(6, 17, 42, 17);
        line(35, 12, 38, 12);
        line(30, 22, 42, 22);
        line(30, 22, 30, 32);
        line(30, 32, 42, 32);
    } else if (name == "lighting") {
        circle(24, 19, 12);
        box(18, 30, 12, 8);
        line(18, 43, 30, 43);
        for (int x : {5, 43}) line(x, 16, x, 22);
        line(24, 0, 24, 3);
    } else if (name == "color") {
        disc(16, 16, 10, {225, 111, 114, 255});
        disc(32, 16, 10, {104, 185, 234, 255});
        disc(24, 32, 10, {109, 210, 157, 255});
    } else if (name == "image") {
        box(5, 7, 38, 34);
        disc(32, 17, 4, ink);
        line(7, 36, 19, 22);
        line(19, 22, 30, 36);
        line(27, 32, 35, 25);
        line(35, 25, 41, 36);
    } else if (name == "anchor") {
        line(24, 6, 24, 39);
        circle(24, 9, 5);
        line(15, 21, 33, 21);
        line(10, 29, 10, 36);
        line(10, 36, 24, 43);
        line(24, 43, 38, 36);
        line(38, 36, 38, 29);
    } else if (name == "delete") {
        box(13, 14, 23, 28);
        line(8, 11, 40, 11);
        line(19, 5, 29, 5);
        line(20, 20, 20, 36);
        line(29, 20, 29, 36);
    } else if (name == "copy" || name == "paste" || name == "duplicate") {
        box(7, 6, 25, 29);
        box(16, 16, 25, 29);
    } else if (name == "focus") {
        box(14, 14, 20, 20);
        line(5, 15, 5, 5);
        line(5, 5, 15, 5);
        line(33, 5, 43, 5);
        line(43, 5, 43, 15);
        line(5, 33, 5, 43);
        line(5, 43, 15, 43);
        line(33, 43, 43, 43);
        line(43, 43, 43, 33);
    } else if (name == "info") {
        circle(24, 24, 18);
        disc(24, 14, 2, ink);
        line(24, 22, 24, 35);
    } else if (name == "play" || name == "select") {
        triangle(15, 8, 15, 40, 39, 24);
    } else if (name == "stop") {
        solid(12, 12, 25, 25);
    } else if (name == "menu") {
        for (int y : {13, 24, 35}) line(3, y, 45, y);
    } else if (name == "move") {
        line(5, 24, 43, 24);
        line(24, 5, 24, 43);
        line(5, 24, 12, 17);
        line(5, 24, 12, 31);
        line(43, 24, 36, 17);
        line(43, 24, 36, 31);
        line(24, 5, 17, 12);
        line(24, 5, 31, 12);
        line(24, 43, 17, 36);
        line(24, 43, 31, 36);
    } else if (name == "scale") {
        box(13, 13, 22, 22);
        for (int x : {8, 35})
            for (int y : {8, 35}) solid(x, y, 6, 6);
    } else if (name == "rotate" || name == "respawn") {
        circle(24, 25, 15);
        triangle(33, 5, 44, 17, 29, 18);
    } else if (name == "folder" || name == "toolbox" || name == "storage") {
        box(6, 15, 36, 25);
        box(8, 8, 14, 7);
        line(6, 24, 42, 24);
    } else if (name == "script" || name == "output" || name == "module" || name == "local-script") {
        box(12, 5, 25, 37);
        for (int y : {15, 23, 31}) line(18, y, 30, y);
        if (name == "module") box(5, 24, 13, 17);
    } else if (name == "model") {
        box(4, 21, 17, 20);
        box(27, 21, 17, 20);
        box(15, 5, 18, 19);
    } else if (name == "explorer" || name == "properties" || name == "settings") {
        for (int y : {12, 24, 36}) {
            line(8, y, 40, y);
            disc(y == 24 ? 30 : 17, y, 4, ink);
        }
    } else if (name == "save") {
        box(8, 6, 32, 36);
        box(14, 6, 18, 13);
        box(15, 27, 19, 15);
    } else if (name == "heart") {
        disc(16, 18, 10, ink);
        disc(32, 18, 10, ink);
        triangle(6, 21, 24, 43, 42, 21);
    } else if (name == "rig") {
        box(17, 3, 14, 12);
        box(14, 18, 20, 15);
        line(10, 19, 7, 32);
        line(38, 19, 41, 32);
        line(19, 34, 16, 45);
        line(29, 34, 32, 45);
    } else if (name == "animation") {
        box(5, 10, 38, 29);
        triangle(20, 16, 20, 33, 33, 24);
    } else if (name == "sphere") {
        circle(24, 24, 18);
        line(6, 24, 42, 24);
        line(24, 6, 24, 42);
    } else if (name == "spawn") {
        box(6, 31, 36, 10);
        line(24, 6, 24, 30);
        line(24, 6, 38, 11);
        line(38, 11, 24, 17);
    } else {
        box(9, 14, 25, 27);
        line(9, 14, 20, 6);
        line(34, 14, 43, 6);
        line(20, 6, 43, 6);
        line(43, 6, 43, 31);
        line(34, 41, 43, 31);
    }
    return image;
}

Image DownscalePremultiplied(const Image& source, int width, int height) {
    if (!source.data || source.width <= 0 || source.height <= 0) return {};
    if (source.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) return {};
    Image target = GenImageColor(std::max(1, width), std::max(1, height), BLANK);
    if (!target.data) return {};

    auto* dst = static_cast<unsigned char*>(target.data);
    const auto* src = static_cast<const unsigned char*>(source.data);
    for (int y = 0; y < target.height; ++y) {
        const int sy0 = static_cast<int>(static_cast<long long>(y) * source.height / target.height);
        const int sy1 = std::max(sy0 + 1, static_cast<int>(static_cast<long long>(y + 1) * source.height / target.height));
        for (int x = 0; x < target.width; ++x) {
            const int sx0 = static_cast<int>(static_cast<long long>(x) * source.width / target.width);
            const int sx1 = std::max(sx0 + 1, static_cast<int>(static_cast<long long>(x + 1) * source.width / target.width));
            unsigned long long sumR = 0, sumG = 0, sumB = 0, sumA = 0, count = 0;
            for (int sy = sy0; sy < sy1 && sy < source.height; ++sy) {
                for (int sx = sx0; sx < sx1 && sx < source.width; ++sx) {
                    const unsigned char* p = src + (static_cast<size_t>(sy) * source.width + sx) * 4;
                    const unsigned long long a = p[3];
                    sumR += static_cast<unsigned long long>(p[0]) * a;
                    sumG += static_cast<unsigned long long>(p[1]) * a;
                    sumB += static_cast<unsigned long long>(p[2]) * a;
                    sumA += a;
                    ++count;
                }
            }
            if (count == 0) continue;
            unsigned char* o = dst + (static_cast<size_t>(y) * target.width + x) * 4;
            // The colour is accumulated premultiplied (r * a), so it has to
            // be divided by 255 as well: the result is the average
            // premultiplied colour, ready for a premultiplied upload.
            const unsigned long long divisor = count * 255ull;
            o[0] = static_cast<unsigned char>(std::min<unsigned long long>(255ull, sumR / divisor));
            o[1] = static_cast<unsigned char>(std::min<unsigned long long>(255ull, sumG / divisor));
            o[2] = static_cast<unsigned char>(std::min<unsigned long long>(255ull, sumB / divisor));
            o[3] = static_cast<unsigned char>(std::min<unsigned long long>(255ull, sumA / count));
        }
    }
    return target;
}

void StudioRenderer::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
    ++drawCalls;
    RenderInterface_GL3::RenderGeometry(geometry, translation, texture);
}

Rml::TextureHandle StudioRenderer::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) {
    const auto name = std::filesystem::path(source).filename().string();
    Image image{};
    if (name.rfind("icon-", 0) == 0) {
        image = IconImage(name.substr(5));
    } else {
        const std::string path = name == "loblox-logo" ? game::BuiltinAssetPath("loblox.png") : source;
        if (path.empty() || !SafeImageFile(path)) return 0;
        image = LoadImage(path.c_str());
    }
    if (!image.data) return 0;
    if (image.width > 4096 || image.height > 4096) {
        UnloadImage(image);
        return 0;
    }

    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        UnloadImage(image);
        return 0;
    }
    // The original logo keeps a wide transparent margin around its artwork.
    // Trim it so the small circular menu button shows the mark at the same
    // visual weight as the Roblox reference instead of shrinking it twice.
    if (name == "loblox-logo") ImageAlphaCrop(&image, 0.02f);
    const int longest = std::max(image.width, image.height);
    if (longest > kUiTextureMaxEdge) {
        // Area-average instead of raylib's bicubic ImageResize: a 1600 px
        // logo reduced to 256 px with a 4x4 kernel drops most of the source
        // and leaves jagged edges before the GPU even minifies it again.
        const float factor = static_cast<float>(kUiTextureMaxEdge) / static_cast<float>(longest);
        const int width = std::max(1, static_cast<int>(std::lround(image.width * factor)));
        const int height = std::max(1, static_cast<int>(std::lround(image.height * factor)));
        Image scaled = DownscalePremultiplied(image, width, height);
        if (!scaled.data) {
            // The source is not RGBA8: keep it as it is and premultiply it.
            ImageAlphaPremultiply(&image);
        } else {
            UnloadImage(image);
            image = scaled;  // already premultiplied
        }
    } else {
        ImageAlphaPremultiply(&image);
    }

    dimensions = {image.width, image.height};
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id == 0) return 0;

    // RmlUi's backend uploads with GL_LINEAR and no mip chain. Interface
    // images are almost always drawn below their native size, so give them
    // a proper chain and trilinear minification.
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    // The line art of the icons is only a few pixels wide. A plain area
    // average spreads those strokes over more than one pixel and dims them,
    // so the mip selection is biased a little towards the sharper level.
    rlTextureParameters(texture.id, RL_TEXTURE_MIPMAP_BIAS_RATIO, -35);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    return static_cast<Rml::TextureHandle>(texture.id);
}

}  // namespace loblox::ui

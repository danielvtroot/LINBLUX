#pragma once

// Shared plumbing between the Studio frontend and the graphics-quality
// fixture: the RmlUi system interface, the Loblox icon artwork and the
// texture policy used by the OpenGL renderer.
//
// The policy is deliberate and per texture class:
//
//   * Interface images (icons, the Loblox logo, imported PNGs) are uploaded
//     with a full mip chain and trilinear filtering. RmlUi's own GL3 backend
//     uploads every texture with GL_TEXTURE_MIN_FILTER = GL_LINEAR and no
//     mipmaps, so anything drawn below its native size is minified with a
//     2x2 tap: that is what made the icons look ragged and the logo shimmer.
//   * The font atlas keeps the upstream behaviour. FreeType rasterises it at
//     the exact CSS pixel size with 8 bit antialiasing, so it must stay
//     unmipped and 1:1 sharp.
//
// Images are uploaded premultiplied because both renderers blend with
// (GL_ONE, GL_ONE_MINUS_SRC_ALPHA). Premultiplication also makes the mip
// chain alpha-correct: source PNGs keep arbitrary colour under alpha = 0
// (the Loblox logo stores magenta there), and filtering that colour would
// bleed fringes into the visible artwork.

#include <RmlUi/Core.h>
#include <RmlUi_Renderer_GL3.h>

#include "raylib.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace loblox::ui {

// The Loblox line-art icons are authored on a 48x48 grid.
inline constexpr int kIconGrid = 48;
// Icons are rasterised at twice the authoring grid so the mip chain has real
// detail to filter when the interface draws them at 9..34 px.
inline constexpr int kIconResolution = 96;
// Interface images are never displayed larger than this. Bigger sources are
// area-averaged down so a 1600x1600 PNG does not become a 10 MB upload.
inline constexpr int kUiTextureMaxEdge = 256;

// RmlUi system interface bound to raylib.
struct RmlPlatform final : Rml::SystemInterface {
    int cursor = MOUSE_CURSOR_DEFAULT;
    double GetElapsedTime() override { return GetTime(); }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if (type <= Rml::Log::LT_WARNING) std::fprintf(stderr, "RmlUi: %s\n", message.c_str());
        return true;
    }
    void SetClipboardText(const Rml::String& s) override { ::SetClipboardText(s.c_str()); }
    void GetClipboardText(Rml::String& s) override {
        const char* text = ::GetClipboardText();
        s = text ? text : "";
    }
    void SetMouseCursor(const Rml::String& name) override {
        cursor = name == "pointer"   ? MOUSE_CURSOR_POINTING_HAND
               : name == "text"     ? MOUSE_CURSOR_IBEAM
               : name == "ew-resize" ? MOUSE_CURSOR_RESIZE_EW
               : name == "ns-resize" ? MOUSE_CURSOR_RESIZE_NS
                                     : MOUSE_CURSOR_DEFAULT;
    }
    void JoinPath(Rml::String& result, const Rml::String& document, const Rml::String& path) override {
        // RmlUi's default treats '/image.png' as a virtual web-root path and
        // strips '/'. Loblox resources use actual Linux filesystem paths.
        if (path.rfind("icon-", 0) == 0 || path == "loblox-logo" || path=="loblox-lock") {
            result = path;
            return;
        }
        const auto resource = std::filesystem::path(path);
        result = (resource.is_absolute() ? resource : std::filesystem::path(document).parent_path() / resource).lexically_normal().string();
    }
};

// Original Loblox line-art icons, rasterised at `resolution` pixels.
Image IconImage(const std::string& name, int resolution = kIconResolution);

// Area-average (box) downscale performed in premultiplied space. raylib's
// ImageResize uses a bicubic kernel, which only samples a 4x4 neighbourhood
// and therefore aliases badly when the source is much larger than the
// target. The result is premultiplied and ready for a premultiplied upload.
Image DownscalePremultiplied(const Image& source, int width, int height);

// RmlUi renderer that owns the interface texture policy.
class StudioRenderer : public RenderInterface_GL3 {
public:
    int drawCalls = 0;

    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
};

}  // namespace loblox::ui

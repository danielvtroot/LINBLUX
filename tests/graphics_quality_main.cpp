// Reproducible graphics-quality fixture for the Loblox frontend.
//
// It renders the two rendering paths with the production code:
//
//   Layer A/B - the RmlUi interface: the Loblox logo at four sizes, the
//   interface icons at the sizes the stylesheet uses, small and medium text,
//   a PNG with transparency, a gradient and rounded corners.
//
//   Layer C - the 3D scene: a Part with an image decal, a Part with a
//   repeating texture on an inclined surface and the R6 rig with its face.
//
// Both are captured to PNG and checked with measurable properties, so a
// regression cannot hide behind "the tests still pass". The same fixture
// also replays the previous pipeline (48 px icons, bicubic logo resize, no
// mip chain) and prints the measured difference.
#include "ui/RmlFrontendSupport.h"
#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "ui/ImageSafety.h"

#include <RmlUi/Core.h>
#include <RmlUi_Renderer_GL3.h>

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace loblox;

namespace {

int checks = 0;
int failures = 0;

void Check(bool condition, const std::string& what) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    }
}

std::string FixtureDocument() {
    // Inline styles keep the fixture self contained: it exercises the same
    // renderer and the same texture policy as the Studio document without
    // adding another stylesheet to the shipped assets.
    return R"(<rml><head><title>Loblox graphics quality</title><style>
body { display:block; margin:0; font-family: DejaVu Sans; font-size: 12px; color:#e7e9ed; background-color:#22252b; }
.row { display:flex; align-items:center; margin: 8px 10px; }
.label { width: 190px; font-size: 11px; color:#aeb4c1; }
.panel { display:flex; align-items:center; padding: 8px; background-color:#2b2f36; border: 1px #454951; }
.clear { background-color: transparent; border-width: 0px; }
.cell { margin-right: 12px; }
</style></head><body>
<div class="row"><span class="label">logo 16 / 24 / 32 / 64</span><div class="panel" id="logo-row">
 <img class="cell" id="logo-16" src="loblox-logo" style="width:16px;height:16px;"/>
 <img class="cell" id="logo-24" src="loblox-logo" style="width:24px;height:24px;"/>
 <img class="cell" id="logo-32" src="loblox-logo" style="width:32px;height:32px;"/>
 <img class="cell" id="logo-64" src="loblox-logo" style="width:64px;height:64px;"/>
</div></div>
<div class="row"><span class="label">icons 9 / 15 / 16 / 25 / 29</span><div class="panel" id="icon-row">
 <img class="cell" id="icon-9" src="icon-part" style="width:9px;height:9px;"/>
 <img class="cell" id="icon-15" src="icon-sphere" style="width:15px;height:15px;"/>
 <img class="cell" id="icon-16" src="icon-spawn" style="width:16px;height:16px;"/>
 <img class="cell" id="icon-25" src="icon-folder" style="width:25px;height:25px;"/>
 <img class="cell" id="icon-29" src="icon-rig" style="width:29px;height:29px;"/>
 <img class="cell" id="icon-model" src="icon-model" style="width:29px;height:29px;"/>
 <img class="cell" id="icon-script" src="icon-script" style="width:29px;height:29px;"/>
</div></div>
<div class="row"><span class="label">icons on transparency</span><div class="panel clear" id="icon-clear">
 <img class="cell" id="icon-clear-16" src="icon-play" style="width:16px;height:16px;"/>
 <img class="cell" id="icon-clear-29" src="icon-explorer" style="width:29px;height:29px;"/>
</div></div>
<div class="row"><span class="label">text 10 / 11 / 12 / 15</span><div class="panel" id="text-row">
 <span class="cell" style="font-size:10px;">Part 0.5</span>
 <span class="cell" style="font-size:11px;">PreciseWall</span>
 <span class="cell" style="font-size:12px;">Explorer</span>
 <b class="cell" style="font-size:15px;">LOBLOX</b>
</div></div>
<div class="row"><span class="label">gradient</span><div class="panel"><div id="gradient" style="width:180px;height:40px;background-color:#4f9dcc;decorator: linear-gradient(to right, #e56e70, #4f9dcc);"></div></div></div>
<div class="row"><span class="label">rounded corners</span><div class="panel"><div id="rounded" style="width:140px;height:56px;background-color:#4f9dcc;border-radius:14px;"></div></div></div>
<div class="row"><span class="label">png with transparency</span><div class="panel" id="logo-panel"><img id="logo-alpha" src="loblox-logo" style="width:48px;height:48px;"/></div></div>
</body></rml>)";
}

struct Bounds {
    int x = 0, y = 0, w = 0, h = 0;
};

Bounds ElementBounds(Rml::ElementDocument* document, const std::string& id) {
    auto* element = document ? document->GetElementById(id) : nullptr;
    if (!element) return {};
    const auto offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
    return {static_cast<int>(std::lround(offset.x)), static_cast<int>(std::lround(offset.y)),
            static_cast<int>(std::lround(size.x)), static_cast<int>(std::lround(size.y))};
}

// Mean absolute difference between the captured region and the reference
// built by area-averaging the very same artwork. A minification that skips
// the mip chain samples a couple of texels instead of the whole footprint,
// so it drifts far away from the reference.
double CompareWithReference(const Image& capture, const Bounds& bounds, const Image& reference) {
    if (bounds.w <= 0 || bounds.h <= 0 || capture.width <= 0) return 1e9;
    double total = 0;
    long long counted = 0;
    for (int y = 0; y < bounds.h; ++y) {
        for (int x = 0; x < bounds.w; ++x) {
            const int cx = bounds.x + x, cy = bounds.y + y;
            if (cx < 0 || cy < 0 || cx >= capture.width || cy >= capture.height) continue;
            const Color actual = GetImageColor(capture, cx, cy);
            const Color expected = GetImageColor(reference, x, y);
            total += std::abs(actual.r - expected.r) + std::abs(actual.g - expected.g) + std::abs(actual.b - expected.b);
            counted += 3;
        }
    }
    return counted ? total / counted : 1e9;
}

// Area-averaged, premultiplied copy of an icon at the requested size,
// composited over the panel colour the fixture uses.
Image ReferenceIcon(const std::string& name, int size, Color background) {
    Image icon = ui::IconImage(name);
    ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageAlphaPremultiply(&icon);
    Image scaled = ui::DownscalePremultiplied(icon, size, size);
    UnloadImage(icon);
    Image out = GenImageColor(size, size, background);
    auto* dst = static_cast<unsigned char*>(out.data);
    const auto* src = static_cast<const unsigned char*>(scaled.data);
    for (int i = 0; i < size * size; ++i) {
        const unsigned alpha = src[i * 4 + 3];
        const unsigned inverse = 255 - alpha;
        const unsigned channels[3] = {background.r, background.g, background.b};
        for (int c = 0; c < 3; ++c) {
            const unsigned value = src[i * 4 + c] + channels[c] * inverse / 255;
            dst[i * 4 + c] = static_cast<unsigned char>(std::min(255u, value));
        }
        dst[i * 4 + 3] = 255;
    }
    UnloadImage(scaled);
    return out;
}

// Fraction of neighbouring pixel pairs inside a region whose luminance
// jumps by more than the given amount. Aliased (unfiltered) minification
// produces many such jumps; a mip chain removes them.
double EdgeEnergy(const Image& capture, const Bounds& bounds, int threshold) {
    if (bounds.w <= 1 || bounds.h <= 1) return 1e9;
    long long jumps = 0, pairs = 0;
    for (int y = bounds.y; y < bounds.y + bounds.h; ++y) {
        if (y < 0 || y >= capture.height) continue;
        for (int x = bounds.x; x < bounds.x + bounds.w; ++x) {
            if (x < 0 || x + 1 >= capture.width) continue;
            const Color a = GetImageColor(capture, x, y);
            const Color b = GetImageColor(capture, x + 1, y);
            const int la = (a.r + a.g + a.b) / 3, lb = (b.r + b.g + b.b) / 3;
            if (std::abs(la - lb) > threshold) ++jumps;
            ++pairs;
        }
    }
    return pairs ? static_cast<double>(jumps) / pairs : 1e9;
}

// Highest magenta excess (min(r,b) - g) inside a region. The Loblox logo
// stores magenta under alpha = 0; a straight alpha mip chain drags that
// colour into the visible artwork.
int MagentaExcess(const Image& capture, const Bounds& bounds) {
    int worst = 0;
    for (int y = bounds.y; y < bounds.y + bounds.h; ++y) {
        if (y < 0 || y >= capture.height) continue;
        for (int x = bounds.x; x < bounds.x + bounds.w; ++x) {
            if (x < 0 || x >= capture.width) continue;
            const Color c = GetImageColor(capture, x, y);
            worst = std::max(worst, std::min(c.r, c.b) - c.g);
        }
    }
    return worst;
}

// The pipeline used before the quality fix: 48x48 hard-edged icons, a
// bicubic logo resize and no mip chain at all. It only lives in the fixture
// so the improvement can be measured instead of assumed.
class LegacyRenderer final : public ui::StudioRenderer {
public:
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override {
        const auto name = std::filesystem::path(source).filename().string();
        Image image{};
        if (name.rfind("icon-", 0) == 0) {
            image = ui::IconImage(name.substr(5), 48);
        } else {
            const std::string path = name == "loblox-logo" ? game::BuiltinAssetPath("loblox.png") : source;
            if (path.empty() || !ui::SafeImageFile(path)) return 0;
            image = LoadImage(path.c_str());
        }
        if (!image.data) return 0;
        if (name == "loblox-logo" && image.width > 256) ImageResize(&image, 256, 256 * image.height / image.width);
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ImageAlphaPremultiply(&image);
        dimensions = {image.width, image.height};
        const auto handle = GenerateTexture({static_cast<const Rml::byte*>(image.data), size_t(image.width) * image.height * 4}, dimensions);
        UnloadImage(image);
        return handle;
    }
};

struct IconCase {
    const char* id;
    const char* name;
    int size;
};

const IconCase kIconCases[] = {
    {"icon-9", "part", 9},
    {"icon-15", "sphere", 15},
    {"icon-16", "spawn", 16},
    {"icon-25", "folder", 25},
    {"icon-29", "rig", 29},
};

}  // namespace

int main(int argc, char** argv) {
    std::string out = "/tmp/loblox-graphics-quality";
    if (argc > 1) out = argv[1];
    std::filesystem::create_directories(out);

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1366, 768, "Loblox graphics quality fixture");
    SetWindowSize(1366, 768);
    SetWindowPosition(0, 0);
    SetExitKey(KEY_NULL);

    // ---------------------------------------------------------------------
    // Layer A/B: the RmlUi interface, old pipeline and current pipeline.
    // ---------------------------------------------------------------------
    Rml::String message;
    if (!RmlGL3::Initialize(&message)) {
        std::fprintf(stderr, "RmlGL3 init failed: %s\n", message.c_str());
        return 77;
    }
    {
        ui::RmlPlatform system;
        Rml::SetSystemInterface(&system);
        if (!Rml::Initialise()) {
            std::fprintf(stderr, "RmlUi init failed\n");
            return 77;
        }
        bool fontLoaded = false;
        for (const char* path : {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                 "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"}) {
            if (std::filesystem::exists(path) &&
                Rml::LoadFontFace(path, "DejaVu Sans", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal)) {
                fontLoaded = true;
                break;
            }
        }
        Check(fontLoaded, "a sans-serif font face is available");
        const char* boldPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
        if (std::filesystem::exists(boldPath))
            Rml::LoadFontFace(boldPath, "DejaVu Sans", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold);

        const auto documentPath = std::filesystem::path(out) / "quality.rml";
        {
            std::ofstream file(documentPath);
            file << FixtureDocument();
        }

        // Renders the fixture document with the given renderer and returns
        // the captured image.
        auto renderFixture = [&](ui::StudioRenderer& renderer, const std::string& contextName, const std::string& captureName) {
            Rml::SetRenderInterface(&renderer);
            Rml::Context* context = Rml::CreateContext(contextName, {1366, 768});
            Check(context != nullptr, std::string(contextName) + ": RmlUi context created");
            if (!context) return Image{};
            Rml::ElementDocument* document = context->LoadDocument(documentPath.string());
            Check(document != nullptr, std::string(contextName) + ": fixture document loaded");
            if (!document) return Image{};
            document->Show();
            context->SetDimensions({1366, 768});
            context->Update();

            const auto draw = [&] {
                BeginDrawing();
                ClearBackground(Color{34, 37, 43, 255});
                rlDrawRenderBatchActive();
                renderer.SetViewport(GetRenderWidth(), GetRenderHeight());
                renderer.BeginFrame();
                context->Render();
                renderer.EndFrame();
                EndDrawing();
            };
            draw();
            // Steady state cost of the interface pass, without the one time
            // texture upload.
            const double started = GetTime();
            for (int i = 0; i < 30; ++i) draw();
            const double average = (GetTime() - started) * 1000.0 / 30.0;
            std::printf("%s interface pass: %.2f ms/frame\n", contextName.c_str(), average);

            const auto capturePath = (std::filesystem::path(out) / captureName).string();
            TakeScreenshot(capturePath.c_str());

            Image capture = LoadImage(capturePath.c_str());
            // The bounds are identical for both pipelines, the document does
            // not change between the two runs.
            if (captureName == "interface-after.png") {
                const Bounds logo = ElementBounds(document, "logo-64");
                Check(logo.w == 64 && logo.h == 64, "logo element keeps its square box");
                if (logo.w > 0) {
                    int minX = logo.w, maxX = -1, minY = logo.h, maxY = -1;
                    for (int y = 0; y < logo.h; ++y)
                        for (int x = 0; x < logo.w; ++x) {
                            const Color c = GetImageColor(capture, logo.x + x, logo.y + y);
                            if (c.r > 90 || c.g > 90 || c.b > 90) {
                                minX = std::min(minX, x);
                                maxX = std::max(maxX, x);
                                minY = std::min(minY, y);
                                maxY = std::max(maxY, y);
                            }
                        }
                    const int width = maxX - minX + 1, height = maxY - minY + 1;
                    Check(width > 0 && height > 0, "logo silhouette is visible");
                    Check(std::abs(width - height) <= 3,
                          "logo silhouette keeps its aspect ratio (" + std::to_string(width) + "x" + std::to_string(height) + ")");
                }
                const Color panel{43, 47, 54, 255};
                for (const auto& icon : kIconCases) {
                    const Bounds bounds = ElementBounds(document, icon.id);
                    Check(bounds.w == icon.size && bounds.h == icon.size,
                          std::string(icon.id) + " is drawn at " + std::to_string(icon.size) + " px");
                    Image reference = ReferenceIcon(icon.name, icon.size, panel);
                    const double error = CompareWithReference(capture, bounds, reference);
                    Check(error < 18.0, std::string(icon.id) + " matches the filtered reference (mean error " + std::to_string(error) + ")");
                    UnloadImage(reference);
                }
                // The Loblox logo keeps magenta under alpha = 0. A straight
                // alpha mip chain drags that colour into the artwork.
                const Bounds alpha = ElementBounds(document, "logo-alpha");
                Check(alpha.w > 0, "transparent logo is laid out");
                if (alpha.w > 0) {
                    const int worst = MagentaExcess(capture, alpha);
                    Check(worst < 40, "the transparent logo has no magenta fringe (" + std::to_string(worst) + ")");
                }
                // Small text must stay legible: the fixture rows are drawn
                // with the production font at 10..15 px.
                const Bounds text = ElementBounds(document, "text-row");
                Check(text.w > 100 && text.h > 10, "text row is laid out");
                if (text.w > 0) {
                    int distinct = 0;
                    bool seen[256] = {false};
                    for (int y = text.y; y < text.y + text.h && y < capture.height; ++y)
                        for (int x = text.x; x < text.x + text.w && x < capture.width; ++x) {
                            const Color c = GetImageColor(capture, x, y);
                            const int luma = (c.r * 30 + c.g * 59 + c.b * 11) / 100;
                            if (!seen[luma]) { seen[luma] = true; ++distinct; }
                        }
                    Check(distinct >= 12, "small text is antialiased (" + std::to_string(distinct) + " grey levels)");
                }
            }
            Rml::RemoveContext(contextName);
            return capture;
        };

        LegacyRenderer legacy;
        const Image before = renderFixture(legacy, "loblox-quality-legacy", "interface-before.png");
        UnloadImage(before);

        ui::StudioRenderer renderer;
        const Image after = renderFixture(renderer, "loblox-quality", "interface-after.png");
        Check(after.width == 1366 && after.height == 768, "interface capture has the native size");

        Rml::Shutdown();
    }
    RmlGL3::Shutdown();

    // ---------------------------------------------------------------------
    // Layer C: the 3D scene.
    // ---------------------------------------------------------------------
    {
        const std::string logoPath = game::BuiltinAssetPath("loblox.png");
        const std::string snakePath = game::BuiltinAssetPath("assets/bd49464336ae6306.png");
        Check(!logoPath.empty(), "the Loblox logo asset is reachable");
        Scene scene;
        scene.lighting()->setSkyEnabled(false);
        scene.lighting()->setBrightness(1.0f);

        auto* wall = scene.createPart(PartShape::Block, {5, 3, 0}, {8, 5, 0.5}, "TexturedWall");
        wall->setAnchored(true);
        wall->setRotationEulerDegrees({0, 28, 14});
        if (!snakePath.empty()) {
            wall->setAttribute("texture_image", snakePath);
            wall->setAttribute("texture_face", std::string("Front"));
            wall->setAttribute("texture_repeat_u", 3.0);
            wall->setAttribute("texture_repeat_v", 2.0);
        }
        auto* decal = scene.createPart(PartShape::Block, {-6, 3, 0}, {5, 5, 0.5}, "DecalPart");
        decal->setAnchored(true);
        if (!logoPath.empty()) {
            decal->setAttribute("decal_image", logoPath);
            decal->setAttribute("decal_face", std::string("Front"));
        }

        Camera3D camera{{0, 3, -18}, {0, 3, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
        SceneRenderer sceneRenderer;
        auto target = LoadRenderTexture(1366, 768);
        auto renderScene = [&](const std::string& name) {
            BeginDrawing();
            BeginTextureMode(target);
            sceneRenderer.drawScene(scene, camera, 1366, 768, kInvalidInstanceId, nullptr, true);
            EndTextureMode();
            EndDrawing();
            Image rendered = LoadImageFromTexture(target.texture);
            ExportImage(rendered, (std::filesystem::path(out) / name).string().c_str());
            return rendered;
        };

        Image after = renderScene("scene-after.png");

        // The previous pipeline minified the surface texture with a plain
        // bilinear filter and no mip chain. Forcing the minification filter
        // back to GL_LINEAR (level 0 only) reproduces that aliasing exactly.
        const Texture2D wallTexture = game::AssetTexture(snakePath);
        const Texture2D decalTexture = game::AssetTexture(logoPath);
        if (wallTexture.id) rlTextureParameters(wallTexture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
        if (decalTexture.id) rlTextureParameters(decalTexture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
        Image before = renderScene("scene-before.png");
        if (wallTexture.id) rlTextureParameters(wallTexture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);
        if (decalTexture.id) rlTextureParameters(decalTexture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);

        // The wall carries a high contrast black and white image. Minified
        // without a mip chain it degenerates into aliased noise, which is
        // what the user reported as a distorted black and white pattern.
        const Bounds wallBounds{520, 230, 420, 380};
        const double beforeEdges = EdgeEnergy(before, wallBounds, 60);
        const double afterEdges = EdgeEnergy(after, wallBounds, 60);
        std::printf("surface texture edge energy: before=%.4f after=%.4f\n", beforeEdges, afterEdges);
        Check(afterEdges < 0.02, "the minified surface texture has no aliased edges (" + std::to_string(afterEdges) + ")");
        Check(afterEdges < beforeEdges, "the minified surface texture is smoother than before (" + std::to_string(afterEdges) + " < " + std::to_string(beforeEdges) + ")");
        const int wallMagenta = MagentaExcess(after, wallBounds);
        Check(wallMagenta < 45, "the minified surface texture has no magenta fringe (" + std::to_string(wallMagenta) + ")");

        const Bounds decalBounds{250, 250, 220, 260};
        const double decalBefore = EdgeEnergy(before, decalBounds, 60);
        const double decalAfter = EdgeEnergy(after, decalBounds, 60);
        std::printf("decal edge energy: before=%.4f after=%.4f\n", decalBefore, decalAfter);
        Check(decalAfter < decalBefore, "the decal is smoother than before (" + std::to_string(decalAfter) + " < " + std::to_string(decalBefore) + ")");
        const int decalMagenta = MagentaExcess(after, decalBounds);
        Check(decalMagenta < 45, "the decal has no magenta fringe (" + std::to_string(decalMagenta) + ")");

        UnloadImage(before);
        UnloadImage(after);
        UnloadRenderTexture(target);
    }

    game::ClearImageTextures();
    CloseWindow();
    std::printf("Graphics quality: %d checks, %d failures; captures: %s\n", checks, failures, out.c_str());
    return failures ? 1 : 0;
}

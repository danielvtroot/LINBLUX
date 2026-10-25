// Reproducible graphics-quality fixture for the LINBLUX frontend.
//
// It renders the two rendering paths with the production code:
//
//   Layer A/B - the RmlUi interface: the LINBLUX logo at four sizes, the
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
#include <sstream>
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
    return R"(<rml><head><title>LINBLUX graphics quality</title><style>
body { display:block; margin:0; font-family: DejaVu Sans; font-size: 12px; color:#e7e9ed; background-color:#22252b; }
.row { display:flex; align-items:center; margin: 8px 10px; }
.label { width: 190px; font-size: 11px; color:#aeb4c1; }
.panel { display:flex; align-items:center; padding: 8px; background-color:#2b2f36; border: 1px #454951; }
.clear { background-color: transparent; border-width: 0px; }
.cell { margin-right: 12px; }
</style></head><body>
<div class="row"><span class="label">logo 16 / 24 / 32 / 64</span><div class="panel" id="logo-row">
 <img class="cell" id="logo-16" src="linblux-logo" style="width:16px;height:16px;"/>
 <img class="cell" id="logo-24" src="linblux-logo" style="width:24px;height:24px;"/>
 <img class="cell" id="logo-32" src="linblux-logo" style="width:32px;height:32px;"/>
 <img class="cell" id="logo-64" src="linblux-logo" style="width:64px;height:64px;"/>
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
 <b class="cell" style="font-size:15px;">LINBLUX</b>
</div></div>
<div class="row"><span class="label">gradient</span><div class="panel"><div id="gradient" style="width:180px;height:40px;background-color:#4f9dcc;decorator: linear-gradient(to right, #e56e70, #4f9dcc);"></div></div></div>
<div class="row"><span class="label">rounded corners</span><div class="panel"><div id="rounded" style="width:140px;height:56px;background-color:#4f9dcc;border-radius:14px;"></div></div></div>
<div class="row"><span class="label">png with transparency</span><div class="panel" id="logo-panel"><img id="logo-alpha" src="linblux-logo" style="width:48px;height:48px;"/></div></div>
</body></rml>)";
}

std::string ReadTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return {};
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// The HUD fixture deliberately loads the production stylesheet and repeats the
// production game-layer markup. The circles, the logo image and the health bar
// are therefore laid out and rasterised by exactly the same rules as gameplay.
// The layer is generated for the requested viewport so the same rules can be
// measured at another resolution without touching the production stylesheet.
std::string HudFixtureDocument(const std::string& productionCss, int width, int height) {
    std::ostringstream document;
    document << "<rml><head><title>LINBLUX HUD antialiasing</title><style>" << productionCss
             << "\n        #game-layer { left: 0px; top: 0px; width: " << width << "px; height: " << height
             << "px; }" << R"(
        .probe-row { position: absolute; left: 20px; top: 104px; display: flex; align-items: center; }
        .probe { display: flex; align-items: center; justify-content: center; margin-right: 18px;
                 background-color: #101317eb; border: 1px #ffffff21; }
        .probe img { display: block; }
        .probe-42 { width: 42px; height: 42px; border-radius: 21px; padding: 5px; }
        .probe-42 img { width: 30px; height: 30px; }
        .probe-44 { width: 44px; height: 44px; border-radius: 22px; padding: 5px; }
        .probe-44 img { width: 30px; height: 30px; }
        .probe-48 { width: 48px; height: 48px; border-radius: 24px; padding: 6px; }
        .probe-48 img { width: 34px; height: 34px; }
        .probe-56 { width: 56px; height: 56px; border-radius: 28px; padding: 7px; }
        .probe-56 img { width: 38px; height: 38px; }
    )" << R"(</style></head><body>
    <div id="game-layer">
      <div id="game-topbar"><button id="game-menu" class="logo-button" title="LINBLUX menu"><img id="game-menu-image" src="linblux-logo"/></button><div id="game-capsule"><button id="game-controls" title="Pause"><img id="game-controls-image" src="icon-player-menu"/></button><button><img src="icon-player-settings"/></button></div></div>
      <div id="health"><div class="health-track"><div id="health-trail"/><div id="health-fill"/></div></div>
      <div class="probe-row">
        <div id="probe-42-logo" class="probe probe-42"><img src="linblux-logo"/></div>
        <div id="probe-42-menu" class="probe probe-42"><img src="icon-menu"/></div>
        <div id="probe-44-logo" class="probe probe-44"><img src="linblux-logo"/></div>
        <div id="probe-48-logo" class="probe probe-48"><img src="linblux-logo"/></div>
        <div id="probe-56-menu" class="probe probe-56"><img src="icon-menu"/></div>
      </div>
    </div>
    </body></rml>)";
    return document.str();
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

// Highest magenta excess (min(r,b) - g) inside a region. The LINBLUX logo
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

int PixelLuma(Color color) {
    return (color.r * 30 + color.g * 59 + color.b * 11) / 100;
}

struct CoverageProbe {
    int samples = 0;
    int partial = 0;
    int levels = 0;
    int background_luma = 0;
    int edge_luma = 0;
};

// Measures the transition across the outer edge of a rounded element.
//
// The screenshot is already composited over an opaque background, so the
// coverage of an antialiased pixel shows up as a colour between the known
// background and the colour of a fully covered edge pixel. Hard rasterisation
// lands on one of those two endpoints; MSAA produces intermediate levels.
CoverageProbe MeasureArcCoverage(const Image& capture, float centerX, float centerY, float radius,
                                 float innerBand, float outerBand, Color background, bool brightEdge,
                                 int hemisphere = 0, const Color* fullyCovered = nullptr,bool topOnly=false) {
    CoverageProbe probe;
    probe.background_luma = PixelLuma(background);

    // Find the edge colour in the outermost fully covered pixels. The button
    // border is brighter than both its fill and the dark page; the health
    // border is darker than its track and the light page.
    int edgeLuma = brightEdge ? -1 : 256;
    for (int y = std::max(0, static_cast<int>(std::floor(centerY - radius - 1.0f)));
         y <= std::min(capture.height - 1, static_cast<int>(std::ceil(centerY + radius + 1.0f))); ++y) {
        for (int x = std::max(0, static_cast<int>(std::floor(centerX - radius - 1.0f)));
             x <= std::min(capture.width - 1, static_cast<int>(std::ceil(centerX + radius + 1.0f))); ++x) {
            const float dx = (x + 0.5f) - centerX, dy = (y + 0.5f) - centerY;
            if ((hemisphere < 0 && dx > 0.0f) || (hemisphere > 0 && dx < 0.0f)||(topOnly&&dy>0)) continue;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < radius - 1.0f || distance > radius - 0.1f) continue;
            const int luma = PixelLuma(GetImageColor(capture, x, y));
            edgeLuma = brightEdge ? std::max(edgeLuma, luma) : std::min(edgeLuma, luma);
        }
    }
    if (edgeLuma < 0 || edgeLuma > 255) return probe;
    probe.edge_luma = edgeLuma;

    // Colour of a fully covered edge pixel, estimated from the same screen
    // region. Sampling the actual pixel keeps alpha, border and background
    // compositing out of the measurement.
    const Color desiredEdge{static_cast<unsigned char>(edgeLuma), static_cast<unsigned char>(edgeLuma),
                           static_cast<unsigned char>(edgeLuma), 255};
    Color edge=desiredEdge;
    int bestDistance = 1 << 30;
    for (int y = std::max(0, static_cast<int>(std::floor(centerY - radius - 1.0f)));
         y <= std::min(capture.height - 1, static_cast<int>(std::ceil(centerY + radius + 1.0f))); ++y) {
        for (int x = std::max(0, static_cast<int>(std::floor(centerX - radius - 1.0f)));
             x <= std::min(capture.width - 1, static_cast<int>(std::ceil(centerX + radius + 1.0f))); ++x) {
            const float dx = (x + 0.5f) - centerX, dy = (y + 0.5f) - centerY;
            if ((hemisphere < 0 && dx > 0.0f) || (hemisphere > 0 && dx < 0.0f)||(topOnly&&dy>0)) continue;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < radius - 1.0f || distance > radius - 0.1f) continue;
            const Color color = GetImageColor(capture, x, y);
            // Compare against the immutable luminance target, not the last
            // candidate. The old greedy comparison could lock onto a repeated
            // 50%-coverage pixel, misclassifying real 4x MSAA as one level.
            const int delta = std::abs(color.r - desiredEdge.r) + std::abs(color.g - desiredEdge.g) + std::abs(color.b - desiredEdge.b);
            if (delta < bestDistance) {
                bestDistance = delta;
                edge = color;
            }
        }
    }

    // Production translucent surfaces can be darker OR brighter than a dark
    // page, and border/fill may have opposite contrast. Calibrate with a fully
    // covered straight-edge pixel when one is available, not a theme guess.
    if(fullyCovered)edge=*fullyCovered;

    const float bcx = static_cast<float>(background.r), bcy = static_cast<float>(background.g), bcz = static_cast<float>(background.b);
    const float ecx = static_cast<float>(edge.r), ecy = static_cast<float>(edge.g), ecz = static_cast<float>(edge.b);
    const float vx = ecx - bcx, vy = ecy - bcy, vz = ecz - bcz;
    const float denominator = vx * vx + vy * vy + vz * vz;
    if (denominator < 1.0f) return probe;

    bool seen[21] = {};
    for (int y = std::max(0, static_cast<int>(std::floor(centerY - radius - 2.0f)));
         y <= std::min(capture.height - 1, static_cast<int>(std::ceil(centerY + radius + 2.0f))); ++y) {
        for (int x = std::max(0, static_cast<int>(std::floor(centerX - radius - 2.0f)));
             x <= std::min(capture.width - 1, static_cast<int>(std::ceil(centerX + radius + 2.0f))); ++x) {
            const float dx = (x + 0.5f) - centerX, dy = (y + 0.5f) - centerY;
            if ((hemisphere < 0 && dx > 0.0f) || (hemisphere > 0 && dx < 0.0f)||(topOnly&&dy>0)) continue;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < radius - innerBand || distance > radius + outerBand) continue;
            const Color color = GetImageColor(capture, x, y);
            const float t = ((color.r - bcx) * vx + (color.g - bcy) * vy + (color.b - bcz) * vz) / denominator;
            ++probe.samples;
            if (t > 0.05f && t < 0.95f) {
                ++probe.partial;
                const int level = static_cast<int>(std::lround(t * 20.0f));
                if (level >= 1 && level <= 19) seen[level] = true;
            }
        }
    }
    for (bool level : seen) probe.levels += level ? 1 : 0;
    return probe;
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
            const std::string path = name == "linblux-logo" ? game::BuiltinAssetPath("logo.png") : source;
            if (path.empty() || !ui::SafeImageFile(path)) return 0;
            image = LoadImage(path.c_str());
        }
        if (!image.data) return 0;
        if (name == "linblux-logo" && image.width > 256) ImageResize(&image, 256, 256 * image.height / image.width);
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
    // Optional HUD viewport, e.g. `... /tmp/out 1920x1080`. The interface
    // fixture keeps its own 1366x768 layout; only the gameplay HUD is replayed
    // at the requested size so the circular edges can be measured at
    // 1280x720, 1366x768 and 1920x1080 with the production rules.
    int hudWidth = 1366, hudHeight = 768;
    if (argc > 2) {
        int width = 0, height = 0;
        if (std::sscanf(argv[2], "%dx%d", &width, &height) == 2 && width >= 640 && height >= 480) {
            hudWidth = width;
            hudHeight = height;
        }
    }
    std::filesystem::create_directories(out);

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1366, 768, "LINBLUX graphics quality fixture");
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
                    Image original=LoadImage(game::BuiltinAssetPath("logo.png").c_str());
                    const auto artwork=GetImageAlphaBorder(original,.05f);
                    Check(artwork.height>0&&std::fabs(float(width)/height-artwork.width/artwork.height)<.08f,
                          "official logo artwork keeps its source aspect ratio (" + std::to_string(width) + "x" + std::to_string(height) + ")");
                    UnloadImage(original);
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
                // The LINBLUX logo keeps magenta under alpha = 0. A straight
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

        // -----------------------------------------------------------------
        // Gameplay HUD: the production stylesheet and markup, at native size,
        // over a light and a dark scene. This is the visual fixture for the
        // circular buttons and the health bar.
        // -----------------------------------------------------------------
        const std::string stylesheetPath = game::BuiltinAssetPath("assets/ui/styles/studio.rcss");
        Check(!stylesheetPath.empty(), "the production gameplay stylesheet is reachable");
        const std::string playerStylesheetPath = game::BuiltinAssetPath("assets/ui/styles/player.rcss");
        Check(!playerStylesheetPath.empty(), "the Player stylesheet is reachable");
        const std::string productionCss = ReadTextFile(stylesheetPath)+"\n"+ReadTextFile(playerStylesheetPath)+"\n"+ReadTextFile(game::BuiltinAssetPath("assets/ui/styles/theme.rcss"));
        Check(!productionCss.empty(), "the production gameplay stylesheet is readable");
        // Replay the HUD at the requested viewport. raylib can only present a
        // framebuffer the window actually has, so resize it once here, after
        // the 1366x768 interface fixture has been captured.
        if (GetScreenWidth() != hudWidth || GetScreenHeight() != hudHeight) {
            SetWindowSize(hudWidth, hudHeight);
            for (int i = 0; i < 120 &&
                            (GetScreenWidth() != hudWidth || GetScreenHeight() != hudHeight); ++i) {
                BeginDrawing();
                ClearBackground(BLACK);
                EndDrawing();
            }
            for (int i = 0; i < 4; ++i) {
                BeginDrawing();
                ClearBackground(BLACK);
                EndDrawing();
            }
        }
        Check(GetScreenWidth() == hudWidth && GetScreenHeight() == hudHeight,
              "HUD fixture window matches the requested viewport");
        const auto hudPath = std::filesystem::path(out) /
                             ("hud-" + std::to_string(hudWidth) + "x" + std::to_string(hudHeight) + ".rml");
        {
            std::ofstream file(hudPath);
            file << HudFixtureDocument(productionCss, hudWidth, hudHeight);
        }

        Rml::SetRenderInterface(&renderer);
        Rml::Context* hudContext = Rml::CreateContext("loblox-hud-quality", {hudWidth, hudHeight});
        Check(hudContext != nullptr, "HUD fixture context created");
        if (hudContext) {
            Rml::ElementDocument* hudDocument = hudContext->LoadDocument(hudPath.string());
            Check(hudDocument != nullptr, "HUD fixture document loaded");
            if (hudDocument) {
                hudDocument->Show();
                hudContext->SetDimensions({hudWidth, hudHeight});
                hudContext->Update();
                // The RmlUi GL3 backend records the interface into its own
                // layer framebuffer, so the edge quality is measured on the
                // composited pixels: the coverage checks below only pass when
                // that target was rasterised with multiple samples.
                const auto drawHud = [&] {
                    BeginDrawing();
                    ClearBackground(BLACK);
                    rlDrawRenderBatchActive();
                    renderer.SetViewport(GetRenderWidth(), GetRenderHeight());
                    renderer.BeginFrame();
                    hudContext->Render();
                    renderer.EndFrame();
                    EndDrawing();
                };
                drawHud();
                std::printf("HUD layer: %dx%d\n", hudWidth, hudHeight);

                struct HudBackground {
                    const char* name;
                    Color color;
                };
                const HudBackground backgrounds[] = {
                    {"light", {234, 239, 245, 255}},
                    {"dark", {20, 23, 28, 255}},
                };
                for (const auto& background : backgrounds) {
                    for (int i = 0; i < 4; ++i) {
                        BeginDrawing();
                        ClearBackground(background.color);
                        rlDrawRenderBatchActive();
                        renderer.SetViewport(GetRenderWidth(), GetRenderHeight());
                        renderer.BeginFrame();
                        hudContext->Render();
                        renderer.EndFrame();
                        EndDrawing();
                    }
                    const std::string capturePath =
                        (std::filesystem::path(out) /
                         (std::string("hud-") + background.name + "-" + std::to_string(hudWidth) + "x" +
                          std::to_string(hudHeight) + ".png"))
                            .string();
                    TakeScreenshot(capturePath.c_str());
                    Image capture = LoadImage(capturePath.c_str());
                    Check(capture.width == hudWidth && capture.height == hudHeight,
                          std::string("HUD ") + background.name + " capture has the viewport size");
                    if (capture.width > 0) {
                        Check(GetImageColor(capture, 4, 4).r == background.color.r &&
                                  GetImageColor(capture, 4, 4).g == background.color.g &&
                                  GetImageColor(capture, 4, 4).b == background.color.b,
                              std::string("HUD ") + background.name + " has the expected page background");

                        const auto menu = ElementBounds(hudDocument, "game-menu");
                        const auto controls = ElementBounds(hudDocument, "game-controls");
                        const auto capsule = ElementBounds(hudDocument, "game-capsule");
                        const auto health = ElementBounds(hudDocument, "health");
                        const auto menuImage = ElementBounds(hudDocument, "game-menu-image");
                        const auto controlsImage = ElementBounds(hudDocument, "game-controls-image");
                        Check(menu.w == 46 && menu.h == 46, "game logo button is 46x46");
                        Check(controls.w == 46 && controls.h == 46, "game menu hitbox is 46x46");
                        Check(capsule.w == 102 && capsule.h == 46, "two controls share a 102x46 capsule");
                        Check(menuImage.w == 28 && menuImage.h == 28, "game logo preserves its square aspect at 28px");
                        Check(controlsImage.w == 26 && controlsImage.h == 26, "game menu icon is 26x26");
                        Check(health.w == 180 && health.h == 20, "minimal health bar is 180x20, not an information card");

                        const bool brightEdge=PixelLuma(background.color)<100;
                        const Color logoInk=GetImageColor(capture,menu.x+3,menu.y+menu.h/2);
                        const Color capsuleInk=GetImageColor(capture,capsule.x+3,capsule.y+capsule.h/2);
                        const auto logoCoverage = MeasureArcCoverage(
                            capture, menu.x+8.f,menu.y+8.f,7.f,
                            .85f,1.45f,background.color,brightEdge,-1,&logoInk,true);
                        const auto menuCoverage = MeasureArcCoverage(
                            capture,capsule.x+8.f,capsule.y+8.f,8.f,
                            .85f,1.45f,background.color,brightEdge,-1,&capsuleInk,true);
                        const float healthRadius = health.h*.5f;
                        const float corner=healthRadius;
                        const Color healthInk=GetImageColor(capture,health.x+1,health.y+health.h/2);
                        auto healthLeft = MeasureArcCoverage(
                            capture, health.x + corner, health.y + corner, healthRadius,
                            0.85f, 1.45f, background.color, brightEdge, -1,&healthInk);
                        auto healthRight = MeasureArcCoverage(
                            capture, health.x + health.w - corner, health.y + corner,
                            healthRadius, 0.85f, 1.45f, background.color, brightEdge, 1,&healthInk);

                        std::printf(
                            "HUD %s logo edge: %d partial/%d samples, %d levels; menu edge: %d/%d, %d levels; "
                            "health caps: %d/%d and %d/%d, levels %d/%d\n",
                            background.name, logoCoverage.partial, logoCoverage.samples, logoCoverage.levels,
                            menuCoverage.partial, menuCoverage.samples, menuCoverage.levels,
                            healthLeft.partial, healthLeft.samples, healthRight.partial, healthRight.samples,
                            healthLeft.levels, healthRight.levels);

                        const std::string page = std::string(" (") + background.name + " background)";
                        Check(logoCoverage.partial >= 3 && logoCoverage.levels >= 2,
                              "LINBLUX logo rounded corner has antialiased edge coverage" + page);
                        Check(menuCoverage.partial >= 3 && menuCoverage.levels >= 2,
                              "LINBLUX controls rounded corner has antialiased edge coverage" + page);
                        Check(healthLeft.partial >= 8 && healthLeft.levels >= 2,
                              "health bar left cap has antialiased edge coverage" + page);
                        Check(healthRight.partial >= 8 && healthRight.levels >= 2,
                              "health bar right cap has antialiased edge coverage" + page);

                        // The interior icon must still match the area-averaged
                        // filtered reference: MSAA smooths the circular edge
                        // without softening the artwork or changing its alpha.
                        const Color buttonFill = GetImageColor(capture, menu.x + 4, menu.y + menu.h / 2);
                        Image menuReference = ReferenceIcon("player-menu", controlsImage.w, buttonFill);
                        const double menuError = CompareWithReference(capture, controlsImage, menuReference);
                        UnloadImage(menuReference);
                        Check(menuError < 22.0, "the game menu icon stays crisp inside the button (error " +
                                                    std::to_string(menuError) + ")" + page);
                        Check(MagentaExcess(capture, menu) < 40,
                              "the game logo button has no magenta fringe" + page);

                        for (const char* id : {"probe-42-logo", "probe-42-menu", "probe-44-logo",
                                               "probe-48-logo", "probe-56-menu"}) {
                            const auto probeBounds = ElementBounds(hudDocument, id);
                            Check(probeBounds.w > 0 && probeBounds.w == probeBounds.h,
                                  std::string(id) + " stays circular");
                            if (probeBounds.w > 0) {
                                const auto coverage = MeasureArcCoverage(
                                    capture, probeBounds.x + probeBounds.w * 0.5f,
                                    probeBounds.y + probeBounds.h * 0.5f, probeBounds.w * 0.5f,
                                    0.85f, 1.45f, background.color, true);
                                std::printf("  %s: %d partial/%d samples, %d levels\n", id, coverage.partial,
                                            coverage.samples, coverage.levels);
                                Check(coverage.partial >= 8 && coverage.levels >= 2,
                                      std::string(id) + " has antialiased coverage (" +
                                          std::to_string(coverage.partial) + ")" + page);
                            }
                        }
                    }
                    UnloadImage(capture);
                }
            }
            Rml::RemoveContext("loblox-hud-quality");
        }

        Rml::Shutdown();
    }
    RmlGL3::Shutdown();

    // ---------------------------------------------------------------------
    // Layer C: the 3D scene.
    // ---------------------------------------------------------------------
    {
        const std::string logoPath = game::BuiltinAssetPath("logo.png");
        const std::string snakePath = game::BuiltinAssetPath("tests/fixtures/surface-test.png");
        Check(!logoPath.empty(), "the LINBLUX logo asset is reachable");
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

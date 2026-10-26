// Graphics proof for script-built UI: the same DrawLuaUi path used by the
// Studio Play texture and the Player, checked by reading the framebuffer and
// by clicking the rendered button.
#include "experiences/ExperienceManager.h"
#include "game/ImageAssets.h"
#include "game/Runtime.h"
#include "rendering/ExperienceOverlay.h"
#include "rendering/LuaUiRenderer.h"
#include "scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

using namespace loblox;
namespace fs = std::filesystem;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
double attributeNumber(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return 0;
    if (const auto* value = std::get_if<double>(&it->second)) return *value;
    return 0;
}
bool Reddish(Color color) { return color.r > 170 && color.g < 90 && color.b < 100; }
Color Pixel(Image image, int x, int y) {
    x = std::clamp(x, 0, image.width - 1);
    y = std::clamp(y, 0, image.height - 1);
    return GetImageColor(image, x, y);
}
} // namespace

int main() {
    SetTraceLogLevel(LOG_ERROR);
    const auto root = fs::temp_directory_path() / ("linblux-ui-render-" + experiences::newId());
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(960, 600, "LINBLUX Lua UI render");
    if (!IsWindowReady()) {
        std::fprintf(stderr, "Lua UI render: window unavailable\n");
        return 1;
    }
    SetTargetFPS(60);
    int failures = 0;
    try {
        fs::create_directories(root / "assets");
        game::SetProjectAssetRoot(root.string());
        Scene scene;
        scene.buildDefaultScene();
        auto* script = scene.createScript(InstanceClass::Script,
            scene.ensureService(InstanceClass::ServerScriptService), "HudLua.lua");
        script->source = R"lua(
local panel = linblux.ui.create("panel", {anchor="center", width=360, height=160, background="#D7263FFF"})
local label = linblux.ui.create("label", {parent=panel, anchor="top", x=0, y=14, width=320, height=30, text="HUD LUA", font_size=22, color="#FFFFFF"})
local button = linblux.ui.create("button", {parent=panel, anchor="bottom", x=0, y=-16, width=160, height=40, text="Acao", background="#101014FF", color="#FFFFFF"})
linblux.scene.root:set_attribute("panel_id", panel)
linblux.scene.root:set_attribute("button_id", button)
linblux.ui.on(button, "click", function()
    linblux.scene.root:set_attribute("clicked", 1)
    linblux.ui.set(panel, "background", "#2ECC71FF")
end)
)lua";
        game::RuntimeSession runtime;
        std::string error;
        require(runtime.start(scene, error), "start: " + error);
        runtime.update(1.f / 60, {});
        require(runtime.scripts().errorCount() == 0, "script error in HUD");
        const auto& workspace = *runtime.scene().workspace();
        const auto panelId = std::uint64_t(attributeNumber(workspace, "panel_id"));
        const auto buttonId = std::uint64_t(attributeNumber(workspace, "button_id"));
        require(panelId && buttonId, "script did not publish element ids");

        auto renderFrames = [&](int count) {
            for (int i = 0; i < count; ++i) {
                runtime.update(1.f / 60, {});
                BeginDrawing();
                ClearBackground(BLACK);
                DrawLuaUi(runtime, GetScreenWidth(), GetScreenHeight());
                EndDrawing();
            }
        };
        auto checkLayout = [&](const char* phase, bool expect_red) {
            const auto* panel = runtime.ui().find(panelId);
            const auto* button = runtime.ui().find(buttonId);
            require(panel && button, std::string(phase) + ": elements missing");
            const auto panelRect = runtime.ui().resolve(*panel, float(GetScreenWidth()), float(GetScreenHeight()));
            require(panelRect.width > 100 && panelRect.height > 60, std::string(phase) + ": panel not resolved");
            const Rectangle viewport{0, 0, float(GetScreenWidth()), float(GetScreenHeight())};
            const bool centered = std::fabs((panelRect.x + panelRect.width * .5f) - viewport.width * .5f) < 2 &&
                                  std::fabs((panelRect.y + panelRect.height * .5f) - viewport.height * .5f) < 2;
            require(centered, std::string(phase) + ": center anchor lost");
            renderFrames(2);
            const auto image = LoadImageFromScreen();
            const auto pixel = Pixel(image, int(panelRect.x + panelRect.width * .5f),
                                     int(panelRect.y + panelRect.height * .2f));
            UnloadImage(image);
            // The panel is injected with a solid LINBLUX red and turns green
            // after the click; either way it must be painted, not background.
            const bool painted = expect_red ? Reddish(pixel)
                : (pixel.g > 140 && pixel.r < 120 && pixel.a > 200);
            require(painted, std::string(phase) + ": panel was not drawn (pixel " +
                std::to_string(pixel.r) + "," + std::to_string(pixel.g) + "," + std::to_string(pixel.b) + ")");
            return panelRect;
        };
        checkLayout("960x600", true);
        require(runtime.scene().workspace()->attributes().count("clicked") == 0, "clicked before the click");
        {
            const auto* button = runtime.ui().find(buttonId);
            const auto rect = runtime.ui().resolve(*button, float(GetScreenWidth()), float(GetScreenHeight()));
            require(runtime.uiClickAt({rect.x + rect.width * .5f, rect.y + rect.height * .5f},
                                      float(GetScreenWidth()), float(GetScreenHeight())), "rendered button not hittable");
        }
        renderFrames(2);
        require(attributeNumber(*runtime.scene().workspace(), "clicked") == 1, "click callback did not run");
        renderFrames(2);
        {
            const auto image = LoadImageFromScreen();
            const auto* panel = runtime.ui().find(panelId);
            const auto rect = runtime.ui().resolve(*panel, float(GetScreenWidth()), float(GetScreenHeight()));
            const auto pixel = Pixel(image, int(rect.x + rect.width * .5f), int(rect.y + rect.height * .2f));
            UnloadImage(image);
            require(pixel.g > 140 && pixel.r < 120, "callback did not repaint the panel");
        }
        // A real resize keeps the anchored layout and the drawing visible.
        SetWindowSize(1280, 720);
        for (int i = 0; i < 30 && (GetScreenWidth() != 1280 || GetScreenHeight() != 720); ++i) renderFrames(1);
        checkLayout("1280x720", false);
        runtime.stop(false);
        require(runtime.ui().count() == 0, "UI survived stop");
        std::printf("Lua UI render: anchored panel drawn and clicked at 960x600 and 1280x720\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Lua UI render failed: %s\n", e.what());
        ++failures;
    }
    ClearExperienceOverlayResources();
    CloseWindow();
    return failures ? 1 : 0;
}

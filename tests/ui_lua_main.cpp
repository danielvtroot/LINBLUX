// Headless contract tests for the script-built UI (linblux.ui). They exercise
// the same RuntimeSession the Studio and the Player use, without a window.
#include "core/Diagnostics.h"
#include "experiences/ExperienceManager.h"
#include "game/ImageAssets.h"
#include "game/Runtime.h"
#include "objects/Part.h"
#include "scene/Scene.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace loblox;
namespace fs = std::filesystem;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
const game::UiElement* findKind(const game::UiModel& ui, game::UiKind kind, const std::string& text = {}) {
    for (const auto* element : ui.drawOrder())
        if (element->kind == kind && (text.empty() || element->text == text)) return element;
    return nullptr;
}
double attributeNumber(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return 0;
    if (const auto* value = std::get_if<double>(&it->second)) return *value;
    return 0;
}
bool attributeBool(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return false;
    if (const auto* value = std::get_if<bool>(&it->second)) return *value;
    return false;
}
std::string attributeString(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return {};
    if (const auto* value = std::get_if<std::string>(&it->second)) return *value;
    return {};
}
struct Fixture {
    Scene scene;
    InstanceId script = 0;
    explicit Fixture(const std::string& source) {
        scene.buildDefaultScene();
        auto* node = scene.createScript(InstanceClass::Script,
            scene.ensureService(InstanceClass::ServerScriptService), "Ui.lua");
        node->source = source;
        script = node->id();
    }
};
void expectError(const std::string& source, const std::string& needle) {
    Fixture fixture(source);
    game::RuntimeSession runtime;
    std::string error;
    require(runtime.start(fixture.scene, error), "runtime start failed: " + error);
    runtime.update(1.f / 60, {});
    bool found = false;
    for (const auto& message : runtime.scripts().messages())
        if (message.error && message.text.find(needle) != std::string::npos) found = true;
    std::string seen;
    for (const auto& message : runtime.scripts().messages())
        if (message.error) seen += message.text + "\n";
    runtime.stop(false);
    require(found, "expected error containing '" + needle + "', got:\n" + seen);
}
} // namespace

int main() {
    SetTraceLogLevel(LOG_ERROR);
    const auto root = fs::temp_directory_path() / ("linblux-ui-lua-" + experiences::newId());
    try {
        diag::configure((root / "ui-lua.jsonl").string());
        fs::create_directories(root / "assets");
        game::SetProjectAssetRoot(root.string());

        // ---- API, layout and events -------------------------------------
        Fixture fixture(R"(
local panel = linblux.ui.create("panel", {anchor="center", width=400, height=200, background="#101014FF"})
local label = linblux.ui.create("label", {parent=panel, x=10, y=10, width=200, height=24, text="Vida"})
local bar = linblux.ui.create("bar", {parent=panel, x=10, y=40, width=300, height=16, value=25, max=100, color="#D7263F"})
local button = linblux.ui.create("button", {parent=panel, anchor="bottom-right", x=-10, y=-10, width=120, height=32, text="Curar"})
linblux.ui.set(label, "text", "100 / 100")
linblux.ui.set(panel, "opacity", 0.5)
linblux.ui.set(button, "color", "#D7263F")
linblux.ui.set(button, "z", 5)
local root = linblux.scene.root
root:set_attribute("ui_count", linblux.ui.count())
root:set_attribute("ui_label", linblux.ui.get(label, "text"))
root:set_attribute("ui_anchor", linblux.ui.get(button, "anchor"))
root:set_attribute("ui_kind", linblux.ui.get(panel, "kind"))
linblux.ui.on(button, "click", function(info)
    root:set_attribute("ui_clicked", (root:get_attribute("ui_clicked") or 0) + 1)
    root:set_attribute("ui_event_kind", info.kind)
    linblux.ui.set(label, "text", "curado")
end)
linblux.ui.on(button, "hover", function(info)
    root:set_attribute("ui_hovered", info.hovered)
end)
)");
        game::RuntimeSession runtime;
        std::string error;
        require(runtime.start(fixture.scene, error), "start: " + error);
        runtime.update(1.f / 60, {});
        require(runtime.scripts().errorCount() == 0, "script errors on valid API use");
        const auto& ui = runtime.ui();
        require(ui.count() == 4, "expected 4 elements");
        require(attributeNumber(*runtime.scene().workspace(), "ui_count") == 4, "ui.count() mismatch");
        const auto* label = findKind(ui, game::UiKind::Label);
        const auto* button = findKind(ui, game::UiKind::Button);
        const auto* panel = findKind(ui, game::UiKind::Panel);
        const auto* bar = findKind(ui, game::UiKind::Bar);
        require(label && button && panel && bar, "missing element kind");
        require(label->text == "100 / 100", "ui.get(text) or set(text) failed");
        require(attributeNumber(*runtime.scene().workspace(), "ui_count") == 4, "count");
        require(panel->opacity > .49f && panel->opacity < .51f, "opacity not applied");
        require(button->z == 5, "z order not applied");
        require(bar->value == 25 && bar->maxValue == 100, "bar values");
        // Design units: 1280x720 is scale 1, so the centered 400x200 panel is
        // at (440,260) and the bottom-right 120x32 button at (710,418).
        const auto panelRect = ui.resolve(*panel, 1280, 720);
        require(std::fabs(panelRect.x - 440) < .01f && std::fabs(panelRect.y - 260) < .01f, "panel anchor");
        const auto buttonRect = ui.resolve(*button, 1280, 720);
        require(std::fabs(buttonRect.x - 710) < .01f && std::fabs(buttonRect.y - 418) < .01f, "button anchor");
        // Half resolution halves the scale, keeping the anchored layout valid.
        const auto half = ui.resolve(*button, 640, 360);
        require(std::fabs(half.width - 60) < .01f && std::fabs(half.x - 355) < .01f, "resolution scaling");
        const Vector2 center{buttonRect.x + buttonRect.width * .5f, buttonRect.y + buttonRect.height * .5f};
        require(runtime.uiClickAt({10, 10}, 1280, 720) == false, "empty area must not be clickable");
        require(runtime.uiClickAt(center, 1280, 720), "button was not hit");
        runtime.update(1.f / 60, {});
        require(attributeNumber(*runtime.scene().workspace(), "ui_clicked") == 1, "click callback did not run");
        require(attributeString(*runtime.scene().workspace(), "ui_event_kind") == "button", "click event payload");
        require(findKind(ui, game::UiKind::Label)->text == "curado", "callback could not update text");
        runtime.uiPointer(center, 1280, 720);
        runtime.update(1.f / 60, {});
        require(attributeBool(*runtime.scene().workspace(), "ui_hovered"), "hover enter not delivered");
        runtime.uiPointer({10, 10}, 1280, 720);
        runtime.update(1.f / 60, {});
        require(!attributeBool(*runtime.scene().workspace(), "ui_hovered"), "hover leave not delivered");
        // Disabling the script removes its nodes on the next update.
        auto* script = runtime.mutableScene().find(fixture.script);
        require(script, "script copy missing");
        static_cast<Script*>(script)->enabled = false;
        runtime.update(1.f / 60, {});
        require(ui.count() == 0, "script UI survived disabling the script");
        runtime.stop(false);
        require(ui.count() == 0, "UI survived RuntimeSession::stop");

        // ---- Validation, budgets and ownership --------------------------
        expectError("linblux.ui.set(9999, \"text\", \"x\")", "não existe");
        expectError("linblux.ui.create(\"spaceship\", {})", "tipo de UI desconhecido");
        expectError(R"(local e = linblux.ui.create("label", {})
linblux.ui.set(e, "text", string.rep("a", 600)))", "512 bytes");
        expectError(R"(local e = linblux.ui.create("image", {image = "assets/missing.png"}))", "não existe nos recursos");
        expectError(R"lua(local a = linblux.ui.create("panel", {})
local b = linblux.ui.create("panel", {parent = a})
local c = linblux.ui.create("panel", {parent = b})
local d = linblux.ui.create("panel", {parent = c})
linblux.ui.create("panel", {parent = d})
)lua", "Profundidade máxima");
        expectError(R"(for i = 1, 100 do linblux.ui.create("label", {text = "x"}) end)", "Limite de alterações");
        expectError(R"(local e = linblux.ui.create("button", {text = "x"})
linblux.ui.on(e, "teleport", function() end))", "evento de UI desconhecido");
        // One script cannot write another script's element.
        {
            Scene scene;
            scene.buildDefaultScene();
            auto* service = scene.ensureService(InstanceClass::ServerScriptService);
            scene.createScript(InstanceClass::Script, service, "Owner.lua")->source =
                "local e = linblux.ui.create('label', {text = 'dono'})\n"
                "linblux.scene.root:set_attribute('owned_id', e)";
            scene.createScript(InstanceClass::Script, service, "Thief.lua")->source =
                "local id = linblux.scene.root:get_attribute('owned_id')\n"
                "linblux.ui.set(id, 'text', 'invasor')";
            game::RuntimeSession other;
            require(other.start(scene, error), "start: " + error);
            other.update(1.f / 60, {});
            bool refused = false;
            for (const auto& message : other.scripts().messages())
                if (message.error && message.text.find("outro script") != std::string::npos) refused = true;
            require(refused, "a script wrote another script's element");
            const auto* ownedLabel = findKind(other.ui(), game::UiKind::Label);
            require(ownedLabel && ownedLabel->text == "dono", "foreign write changed the element");
            other.stop(false);
        }

        // ---- Shipped examples use only the public API --------------------
        const auto examples = fs::path(__FILE__).parent_path().parent_path() / "assets/examples/lua";
        // 1. Ammo counter: fire with F, reload with R.
        {
            Fixture ammo(read(examples / "ui_municao.lua"));
            game::RuntimeSession session;
            require(session.start(ammo.scene, error), "ammo example: " + error);
            session.update(1.f / 60, {});
            require(session.scripts().errorCount() == 0, "ammo example script error");
            require(session.ui().count() == 5, "ammo example must build 5 nodes");
            const auto* counter = findKind(session.ui(), game::UiKind::Label, "30 / 30");
            require(counter, "ammo counter initial text");
            game::PlayerInput press;
            press.script.keys.set(KEY_F, true);
            session.update(1.f / 60, press);
            session.update(1.f / 60, {});
            require(findKind(session.ui(), game::UiKind::Label, "29 / 30"), "ammo did not decrease");
            const auto* meter = findKind(session.ui(), game::UiKind::Bar);
            require(meter && std::fabs(meter->value - 29) < .01, "ammo bar did not follow");
            const auto* reload = findKind(session.ui(), game::UiKind::Button);
            require(reload, "reload button missing");
            const auto rect = session.ui().resolve(*reload, 1280, 720);
            require(session.uiClickAt({rect.x + rect.width * .5f, rect.y + rect.height * .5f}, 1280, 720),
                    "reload button not clickable");
            session.update(1.f / 60, {});
            require(findKind(session.ui(), game::UiKind::Label, "30 / 30"), "reload did not restore ammo");
            session.stop(false);
        }
        // 2. Interactive notification: click "Aceitar" and check the result.
        {
            Fixture notice(read(examples / "ui_notificacao.lua"));
            game::RuntimeSession session;
            require(session.start(notice.scene, error), "notification example: " + error);
            session.update(1.f / 60, {});
            require(session.scripts().errorCount() == 0, "notification example script error");
            const auto* accept = findKind(session.ui(), game::UiKind::Button, "Aceitar");
            require(accept, "accept button missing");
            const auto rect = session.ui().resolve(*accept, 1280, 720);
            require(session.uiClickAt({rect.x + rect.width * .5f, rect.y + rect.height * .5f}, 1280, 720),
                    "accept button not clickable");
            session.update(1.f / 60, {});
            require(findKind(session.ui(), game::UiKind::Label, "Missão aceita"), "notification not resolved");
            session.stop(false);
        }
        // 3. Health HUD follows the character through health_changed.
        {
            Fixture hud(read(examples / "ui_hud_vida.lua"));
            game::RuntimeSession session;
            require(session.start(hud.scene, error), "health example: " + error);
            session.update(1.f / 60, {});
            require(session.scripts().errorCount() == 0, "health example script error");
            require(findKind(session.ui(), game::UiKind::Label, "100 / 100"), "initial health text");
            require(session.takeDamage(80, error), "damage: " + error);
            session.update(1.f / 60, {});
            const auto* bar = findKind(session.ui(), game::UiKind::Bar);
            require(bar && std::fabs(bar->value - 20) < .01, "health bar did not follow damage");
            require(findKind(session.ui(), game::UiKind::Label, "20 / 100"), "health text did not follow");
            require(findKind(session.ui(), game::UiKind::Label, "Estado crítico"), "critical status missing");
            session.stop(false);
        }

        std::printf("UI Lua: model, layout, events, budgets, cleanup and 3 examples OK\n");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "UI Lua test failed: %s\n", e.what());
        return 1;
    }
}

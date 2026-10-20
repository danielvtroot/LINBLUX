// Real X11 events -> GLFW/raylib -> rlImGui -> EditorUI -> camera/gizmo/Bullet.
// Run in a private Xvfb display, never against the user's desktop.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "editor/EditorUI.h"
#include "editor/GizmoLayout.h"
#include "physics/PhysicsWorld.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "core/Diagnostics.h"

namespace loblox {
struct EditorUITestAccess {
    static bool dragging(EditorUI& ui) { return ui.transform_.active(); }
    static void tool(EditorUI& ui, ToolMode mode) { ui.setTool(mode); }
    static void modal(EditorUI& ui) { ui.requestOpenPopup_ = true; }
};
}
using namespace loblox;
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); } } while (0)

int main() {
    char temporary[] = "/tmp/loblox-input-XXXXXX";
    const char* directory = mkdtemp(temporary);
    if (!directory) return 1;
    diag::configure(std::string(directory) + "/input.jsonl");
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 800, "Loblox input regression");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetStyle().WindowPadding = ImVec2(8, 7);
    {
        Scene scene;
        scene.buildDefaultScene();
        Part* part = scene.createPart(PartShape::Block, V3(0, 8, 0), V3(8, 2, 4), "InputFixture");
        part->setAnchored(true);
        scene.select(part);
        PhysicsWorld physics;
        physics.attach(scene);
        EditorCamera camera;
        SceneRenderer renderer;
        AppConfig config;
        config.defaultProjectPath = std::string(directory) + "/scene.json";
        EditorUI ui(scene, physics, camera, renderer, config);
        const auto frames = [&](int count = 3) {
            for (int i = 0; i < count; ++i) {
                physics.simulate(scene, 1.0f / 60);
                BeginDrawing();
                ClearBackground(RAYWHITE);
                rlImGuiBegin();
                ui.draw();
                rlImGuiEnd();
                EndDrawing();
            }
        };
        const auto command = [&](const std::string& args) {
            CHECK(std::system(("xdotool " + args).c_str()) == 0);
            frames();
        };
        frames(5);
        command("search --name '^Loblox input regression$' windowfocus --sync");
        camera.setOrbit(part->position(), 22, 0.75f, 0.4f);
        frames();
        const auto viewport = [&]() {
            ImGuiWindow* window = ImGui::FindWindowByName("Viewport");
            return Rectangle{window->InnerRect.Min.x, window->InnerRect.Min.y,
                window->InnerRect.GetWidth(), window->InnerRect.GetHeight()};
        };
        const auto mouse = [&](Vector2 point) {
            command("mousemove --sync " + std::to_string(static_cast<int>(point.x)) + " " +
                    std::to_string(static_cast<int>(point.y)));
        };
        const auto handle = [&]() {
            const Rectangle rect = viewport();
            const auto layout = gizmolayout::Build(*part, ui.tool(), camera, rect, false);
            Vector2 point = layout.axisButton[0];
            return Vector2{rect.x + point.x, rect.y + point.y};
        };
        EditorUITestAccess::tool(ui, ToolMode::Move);
        frames();
        mouse(handle());
        command("mousedown 3");
        CHECK(camera.cursorCaptured()); // Hover must not deny legitimate RMB.
        command("mouseup 3");

        Rectangle rect = viewport();
        mouse(Vector2{rect.x + 35, rect.y + 90});
        command("mousedown 3");
        CHECK(camera.cursorCaptured());
        command("mousedown 1");
        CHECK(scene.selection() == part);
        CHECK(!EditorUITestAccess::dragging(ui));
        command("mouseup 1");
        command("key 4");
        CHECK(ui.tool() == ToolMode::Move); // Navigation owns keys until release.
        command("mouseup 3");

        // Ctrl+S is not a backwards camera movement.
        mouse(Vector2{rect.x + 35, rect.y + 90});
        command("click 3");
        const Vector3 target = camera.target();
        command("keydown ctrl keydown s");
        frames(5);
        command("keyup s keyup ctrl");
        CHECK(Vector3Distance(target, camera.target()) < 0.001f);

        // A docked Properties window is not the focused viewport.
        ImGui::FocusWindow(ImGui::FindWindowByName("Properties"));
        frames();
        command("key 3");
        CHECK(ui.tool() == ToolMode::Move);

        std::string error;
        CHECK(scene.validate(error));
        CHECK(physics.validate(scene, error));
    }
    rlImGuiShutdown();
    CloseWindow();
    std::printf("Editor input: %d checks, %d failures; artifacts: %s\n", checks, failures, directory);
    return failures ? 1 : 0;
}

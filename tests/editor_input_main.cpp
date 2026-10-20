// Real X11 events -> GLFW/raylib -> rlImGui -> EditorUI -> camera/gizmo/Bullet.
// Run in a private Xvfb display, never against the user's desktop.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <chrono>
#include <vector>
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
#include "serialization/ProjectIO.h"

namespace loblox {
struct EditorUITestAccess {
    static bool dragging(EditorUI& ui) { return ui.transform_.active(); }
    static void tool(EditorUI& ui, ToolMode mode) { ui.setTool(mode); }
    static void modal(EditorUI& ui) { ui.requestOpenPopup_ = true; }
};
}
using namespace loblox;
unsigned long InputTestRootWindow();
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); } } while (0)

bool Equivalent(const nlohmann::json& a, const nlohmann::json& b) {
    if (a.is_number() && b.is_number()) return std::fabs(a.get<double>() - b.get<double>()) < 0.0001;
    if (a.type() != b.type() || a.size() != b.size()) return false;
    if (a.is_object()) {
        for (auto it = a.begin(); it != a.end(); ++it)
            if (!b.contains(it.key()) || !Equivalent(it.value(), b[it.key()])) return false;
        return true;
    }
    if (a.is_array()) {
        for (std::size_t i = 0; i < a.size(); ++i) if (!Equivalent(a[i], b[i])) return false;
        return true;
    }
    return a == b;
}

int main() {
    char temporary[] = "/tmp/loblox-input-XXXXXX";
    const char* directory = mkdtemp(temporary);
    if (!directory) return 1;
    diag::configure(std::string(directory) + "/input.jsonl");
    setenv("LOBLOX_TRACE_INPUT", "1", 1);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 800, "Loblox input regression");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::GetStyle().WindowPadding = ImVec2(8, 7);
    {
        Scene scene;
        scene.buildDefaultScene();
        Part* part = scene.createPart(PartShape::Block, V3(0, 8, 0), V3(8, 2, 4), "InputFixture");
        part->setAnchored(false);
        scene.select(part);
        PhysicsWorld physics;
        physics.attach(scene);
        physics.setGravity(V3(0, 0, 0));
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
        std::vector<double> frameTimes;
        for (int i = 0; i < 60; ++i) {
            const auto started = std::chrono::steady_clock::now();
            frames(1);
            frameTimes.push_back(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count());
        }
        std::sort(frameTimes.begin(), frameTimes.end());
        std::printf("Idle fixture (1280x800, llvmpipe, cap 60): median %.2fms; p95 %.2fms\n",
                    frameTimes[30], frameTimes[57]);
        command("search --name '^Loblox input regression$' windowfocus --sync");
        camera.setOrbit(part->position(), 22, 0.75f, 0.4f);
        frames();
        const auto viewport = [&]() {
            ImGuiWindow* window = ImGui::FindWindowByName("Viewport");
            return Rectangle{window->InnerRect.Min.x, window->InnerRect.Min.y,
                window->InnerRect.GetWidth(), window->InnerRect.GetHeight()};
        };
        const auto mouse = [&](Vector2 point) {
            command("mousemove " + std::to_string(static_cast<int>(point.x)) + " " +
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
        const Vector3 beforeCapture = camera.position();
        command("mousedown 3");
        CHECK(camera.cursorCaptured()); // Hover must not deny legitimate RMB.
        CHECK(NearlyEqual(camera.position(), beforeCapture));
        command("mousemove_relative -- 12 7");
        CHECK(Vector3Distance(camera.position(), beforeCapture) > 0.01f);
        command("mouseup 3");

        Rectangle rect = viewport();
        mouse(Vector2{rect.x + 35, rect.y + 90});
        command("mousedown 3");
        CHECK(camera.cursorCaptured());
        command("mousedown 1");
        CHECK(scene.selection() == part);
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(camera.cursorCaptured());
        command("mouseup 1");
        command("keydown 4");
        command("keyup 4");
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
        command("keydown 3");
        command("keyup 3");
        CHECK(ui.tool() == ToolMode::Move);

        const auto key = [&](const char* name) {
            command(std::string("keydown ") + name);
            command(std::string("keyup ") + name);
        };
        const auto toolbar = [&](ToolMode mode, int index) {
            ImGuiWindow* window = ImGui::FindWindowByName("##Toolbar");
            // Fixture: 1280x800, default 13px ImGui font, Home ribbon.
            const float x[] = {40, 104, 165, 229};
            mouse({window->Pos.x + x[index], window->Pos.y + 66});
            command("mousedown 1");
            command("mouseup 1");
            CHECK(ui.tool() == mode);
            CHECK(scene.selection() == part);
        };
        const auto layout = [&]() {
            return gizmolayout::Build(*part, ui.tool(), camera, viewport(), false);
        };
        const auto screen = [&](Vector2 point) {
            const Rectangle v = viewport();
            return Vector2{v.x + point.x, v.y + point.y};
        };
        const auto beginMove = [&]() {
            mouse(screen(layout().axisButton[0]));
            command("mousedown 1");
            CHECK(EditorUITestAccess::dragging(ui));
            CHECK(physics.isPartEditing(part->id()));
        };
        const loblox::Transform start = part->transform();
        toolbar(ToolMode::Move, 1);
        const Vector3 eye = camera.position();
        const Vector2 moveBegin = layout().axisButton[0];
        const Vector2 direction = layout().axisDirection[0];
        beginMove();
        mouse(screen(Vector2Add(moveBegin, Vector2Scale(direction, 45))));
        CHECK(Vector3Distance(part->position(), start.position) > 0.1f);
        CHECK(NearlyEqual(part->size(), start.size));
        CHECK(NearlyEqual(camera.position(), eye));
        command("mouseup 1");
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!physics.isPartEditing(part->id()));
        CHECK(NearlyEqual(physics.bodyPosition(part->id()), part->position()));
        const loblox::Transform moved = part->transform();

        toolbar(ToolMode::Scale, 2);
        const auto scaleLayout = layout();
        mouse(screen(scaleLayout.scaleHandle[1]));
        command("mousedown 1");
        CHECK(EditorUITestAccess::dragging(ui));
        mouse(screen(Vector2Add(scaleLayout.scaleHandle[1],
                                 Vector2Scale(scaleLayout.axisDirection[0], 40))));
        CHECK(std::fabs(part->size().x - moved.size.x) > 0.1f);
        CHECK(std::fabs(part->size().y - moved.size.y) < 0.001f);
        CHECK(std::fabs((part->position().x - part->size().x / 2) -
                         (moved.position.x - moved.size.x / 2)) < 0.001f);
        command("mouseup 1");
        const loblox::Transform scaled = part->transform();

        toolbar(ToolMode::Rotate, 3);
        const auto rotateLayout = layout();
        mouse(screen(rotateLayout.rotateMarker[1]));
        command("mousedown 1");
        CHECK(EditorUITestAccess::dragging(ui));
        mouse(screen(Vector2Add(rotateLayout.rotateMarker[1], Vector2{32, 28})));
        command("mouseup 1");
        CHECK(std::fabs(Vector4DotProduct(part->rotation(), scaled.rotation)) < 0.9999f);
        CHECK(NearlyEqual(part->position(), scaled.position));
        CHECK(NearlyEqual(part->size(), scaled.size));
        CHECK(NearlyEqual(camera.position(), eye));

        toolbar(ToolMode::Select, 0);
        toolbar(ToolMode::Move, 1);
        const loblox::Transform beforeCancel = part->transform();
        beginMove();
        mouse(screen(Vector2Add(layout().axisButton[0], Vector2{40, 15})));
        key("Escape");
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(NearlyEqual(part->position(), beforeCancel.position));
        command("mouseup 1");
        beginMove();
        command("mouseup 1");
        CHECK(!EditorUITestAccess::dragging(ui));

        // Holding the current tool key past ImGui's repeat delay cannot cancel.
        command("keydown 2");
        beginMove();
        frames(35);
        CHECK(EditorUITestAccess::dragging(ui));
        command("mouseup 1");
        command("keyup 2");

        // A different tool cancels the existing session and restores its transform.
        beginMove();
        key("4");
        CHECK(ui.tool() == ToolMode::Rotate);
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!physics.isPartEditing(part->id()));
        command("mouseup 1");
        key("2");

        // Focus loss must cancel even if release happens on another window.
        beginMove();
        const unsigned long rootWindow = InputTestRootWindow();
        CHECK(rootWindow != 0);
        command("windowfocus " + std::to_string(rootWindow));
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!physics.isPartEditing(part->id()));
        command("mouseup 1");
        command("search --name '^Loblox input regression$' windowfocus --sync");
        beginMove();
        command("mouseup 1");

        // An actual shortcut must work after a viewport click (no stale focus).
        key("3");
        CHECK(ui.tool() == ToolMode::Scale);
        key("2");
        CHECK(ui.tool() == ToolMode::Move);

        // Opening a modal during drag cancels, and its keys cannot change tools.
        beginMove();
        command("keydown ctrl keydown o");
        command("keyup o keyup ctrl");
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!camera.cursorCaptured());
        CHECK(ImGui::IsPopupOpen("Abrir projeto", ImGuiPopupFlags_AnyPopupId));
        key("4");
        CHECK(ui.tool() == ToolMode::Move);
        command("mouseup 1");
        // Close through the real Cancel button in the modal.
        ImGuiWindow* modal = ImGui::FindWindowByName("Abrir projeto");
        CHECK(modal != nullptr);
        if (modal) {
            mouse({modal->Pos.x + 195, modal->Pos.y + modal->Size.y - 18});
            command("mousedown 1");
            command("mouseup 1");
        }
        CHECK(!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup));

        // Properties Name really receives typing; editor shortcuts stay blocked.
        ImGuiWindow* properties = ImGui::FindWindowByName("Properties");
        mouse({properties->InnerRect.Min.x + 45, properties->InnerRect.Min.y + 35});
        command("mousedown 1");
        command("mouseup 1");
        CHECK(ImGui::GetIO().WantTextInput);
        key("2");
        key("4");
        CHECK(ui.tool() == ToolMode::Move);
        CHECK(part->name().find("24") != std::string::npos);
        key("Return");

        // Save using the editor, then load into a fresh scene with fresh Bullet.
        mouse({rect.x + 35, rect.y + 90});
        command("mousedown 3");
        command("mouseup 3");
        const std::string expected = projectio::toJson(scene);
        command("keydown ctrl keydown s");
        command("keyup s keyup ctrl");
        Scene reopened;
        std::string loadError;
        CHECK(projectio::load(reopened, config.defaultProjectPath, loadError));
        CHECK(Equivalent(nlohmann::json::parse(expected),
                         nlohmann::json::parse(projectio::toJson(reopened))));
        PhysicsWorld reopenedPhysics;
        reopenedPhysics.attach(reopened);
        CHECK(reopenedPhysics.validate(reopened, loadError));

        // Delete from a live drag must remove both Part and collider safely.
        const InstanceId removedId = part->id();
        beginMove();
        key("Delete");
        command("mouseup 1");
        CHECK(scene.findPart(removedId) == nullptr);
        CHECK(!physics.hasBody(removedId));
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(scene.selection() == nullptr);
        CHECK(physics.validate(scene, loadError));
        TakeScreenshot((std::string(directory) + "/editor.png").c_str());

        std::string error;
        CHECK(scene.validate(error));
        CHECK(physics.validate(scene, error));
    }
    rlImGuiShutdown();
    CloseWindow();
    std::printf("Editor input: %d checks, %d failures; artifacts: %s\n", checks, failures, directory);
    return failures ? 1 : 0;
}

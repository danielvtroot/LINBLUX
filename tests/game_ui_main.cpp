// Real events exercise the production UI/router/renderer/runtime. Asset setup
// and inspections use a friend fixture, not a second gameplay implementation.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "editor/EditorUI.h"
#include "editor/GizmoLayout.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "core/Diagnostics.h"
#include "serialization/ProjectIO.h"

namespace loblox {
struct EditorUITestAccess {
    static void rigPanel(EditorUI& ui) { ui.showRigEditor_ = true; }
    static void pose(EditorUI& ui) {
        ui.showRigEditor_ = false;
        ui.rigContext_ = EditorUI::RigContext::Pose;
        ui.selectedJoint_ = game::RightShoulder; ui.selectedRigBody_ = game::RightArm;
        ui.setTool(ToolMode::Rotate, "test_joint_selection");
    }
    static const Part& proxy(EditorUI& ui) { return ui.jointProxy_; }
    static bool dragging(EditorUI& ui) { return ui.rigGizmo_.dragging(); }
    static game::Pose preview(EditorUI& ui) { return ui.previewPose_; }
    static game::RuntimeSession& runtime(EditorUI& ui) { return ui.runtime_; }
    static bool captured(EditorUI& ui) { return ui.gameplayCapture_; }
    static void draft(EditorUI& ui) {
        ui.openAnimation("r6-walk");
        ui.animationDraft_.setKey(game::RightShoulder, {.25f, {{}, QuatFromEulerDegrees({55, 0, 0})}});
        ui.animationDirty_ = true; ui.showAnimationEditor_ = false; ui.showRigEditor_ = false;
    }
};
}
using namespace loblox;
int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::fprintf(stderr,"FAIL %d: %s\n", __LINE__, #x); } } while (0)
int main() {
    char temporary[] = "/tmp/loblox-game-ui-XXXXXX";
    const char* directory = mkdtemp(temporary); if (!directory) return 1;
    diag::configure(std::string(directory)+"/game.jsonl");
    SetTraceLogLevel(LOG_WARNING); InitWindow(1280, 800, "Loblox game regression");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL); SetTargetFPS(60); rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::GetIO().IniFilename = nullptr;
    {
        Scene scene; scene.buildDefaultScene();
        auto* wall = scene.createPart(PartShape::Block, {0,5,-18}, {20,10,1}, "GameWall"); wall->setAnchored(true);
        PhysicsWorld physics; physics.attach(scene);
        EditorCamera camera; SceneRenderer renderer; AppConfig config;
        config.defaultProjectPath = std::string(directory)+"/game.json";
        EditorUI ui(scene,physics,camera,renderer,config);
        auto frames = [&](int n = 3) {
            for (int i = 0; i < n; ++i) {
                if (!ui.playing()) physics.simulate(scene, 1.f/60);
                BeginDrawing(); ClearBackground(RAYWHITE); rlImGuiBegin(); ui.draw(); rlImGuiEnd(); EndDrawing();
            }
        };
        auto command = [&](const std::string& arguments) {
            CHECK(std::system(("xdotool "+arguments).c_str()) == 0); frames();
        };
        auto mouse = [&](Vector2 p) { command("mousemove "+std::to_string(int(p.x))+" "+std::to_string(int(p.y))); };
        auto key = [&](const char* k) { command(std::string("keydown ")+k); command(std::string("keyup ")+k); };
        auto click = [&](Vector2 p) { mouse(p); command("mousedown 1"); command("mouseup 1"); };
        auto viewport = [&]() {
            auto* w = ImGui::FindWindowByName("Viewport");
            return Rectangle{w->InnerRect.Min.x,w->InnerRect.Min.y,w->InnerRect.GetWidth(),w->InnerRect.GetHeight()};
        };
        frames(5); command("search --name '^Loblox game regression$' windowfocus --sync");
        EditorUITestAccess::rigPanel(ui); frames();
        ImGui::SetWindowPos("Rig Editor", {80,180}); frames();
        auto* panel = ImGui::FindWindowByName("Rig Editor");
        click({panel->DC.CursorStartPos.x+35, panel->DC.CursorStartPos.y+9});
        CHECK(scene.gameAssets().rigs.size() == 1);
        if (scene.gameAssets().rigs.empty()) { TakeScreenshot((std::string(directory)+"/insert-failed.png").c_str()); return 1; }
        CHECK(scene.gameAssets().animations.size() == 2);
        scene.gameAssets().rigs[0].bodies[game::LeftArm].color = {210,50,140,255}; // asset fixture
        EditorUITestAccess::pose(ui); frames();
        camera.focusOn(scene.gameAssets().rigs[0].root.position, 5); frames();
        const auto initialRest = scene.gameAssets().rigs[0].restPose;
        const auto initialCamera = camera.position();
        auto v = viewport();
        auto layout = gizmolayout::Build(EditorUITestAccess::proxy(ui), ToolMode::Rotate, camera, v, false);
        const auto marker = Vector2Add(layout.rotateMarker[0], {v.x,v.y});
        mouse(marker); command("mousedown 1");
        CHECK(EditorUITestAccess::dragging(ui));
        mouse(Vector2Add(marker, {35,24})); command("mouseup 1");
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(std::fabs(Vector4DotProduct(EditorUITestAccess::preview(ui)[game::RightShoulder].rotation, QuaternionIdentity())) < .9999f);
        CHECK(NearlyEqual(scene.gameAssets().rigs[0].restPose[game::RightShoulder].rotation, initialRest[game::RightShoulder].rotation));
        CHECK(NearlyEqual(camera.position(), initialCamera));
        // Preview and key editing feed the same saved asset loaded by Play.
        EditorUITestAccess::draft(ui); frames();
        v = viewport(); click({v.x+v.width*.5f,v.y+v.height*.75f});
        command("keydown ctrl"); key("s"); command("keyup ctrl");
        CHECK(std::filesystem::exists(config.defaultProjectPath));
        Scene saved; std::string error; CHECK(projectio::load(saved, config.defaultProjectPath, error));
        const auto* clip = saved.gameAssets().findAnimation("r6-walk");
        CHECK(clip && NearlyEqual(EulerDegreesFromQuat(clip->sample(.25f)[game::RightShoulder].rotation).x,55,.01f));
        const auto authoring = projectio::toJson(scene);
        const auto editorEye = camera.position();
        key("F5"); CHECK(ui.playing());
        auto& runtime = EditorUITestAccess::runtime(ui);
        frames(30); CHECK(runtime.grounded());
        auto start = runtime.position(); command("keydown w"); frames(12); command("keyup w");
        CHECK(runtime.position().z < start.z-1);
        CHECK(NearlyEqual(camera.position(),editorEye));
        key("Escape"); CHECK(runtime.state() == game::RuntimeState::Paused);
        CHECK(!EditorUITestAccess::captured(ui));
        auto paused = runtime.position(); command("keydown w"); frames(10); command("keyup w");
        CHECK(NearlyEqual(runtime.position(),paused));
        TakeScreenshot((std::string(directory)+"/pause.png").c_str());
        // Reset is a real menu click; it must retain exactly one capsule.
        auto* menu = ImGui::FindWindowByName("Play paused");
        click({menu->DC.CursorStartPos.x+100, menu->DC.CursorStartPos.y+109});
        CHECK(runtime.bodyCount() == scene.parts().size()+1);
        key("Escape"); CHECK(runtime.state() == game::RuntimeState::Playing);
        frames(25);
        key("space"); CHECK(runtime.velocity().y > 0 || runtime.humanoidState() == game::HumanoidState::Jumping);
        frames(70); CHECK(runtime.grounded());
        v = viewport(); mouse({v.x+v.width*.5f,v.y+v.height*.6f}); command("mousedown 3");
        CHECK(EditorUITestAccess::captured(ui));
        const float yaw = runtime.playerCamera().yaw(); command("mousemove_relative -- 28 10");
        CHECK(std::fabs(runtime.playerCamera().yaw()-yaw) > .01f);
        command("mouseup 3"); CHECK(!EditorUITestAccess::captured(ui));
        mouse({v.x+v.width*.5f,v.y+v.height*.6f});
        command("click --repeat 12 --delay 20 4"); frames();
        CHECK(runtime.playerCamera().firstPerson());
        key("Escape"); CHECK(!EditorUITestAccess::captured(ui));
        CHECK(runtime.state() == game::RuntimeState::Paused);
        key("Escape"); frames(); CHECK(runtime.state() == game::RuntimeState::Playing);
        TakeScreenshot((std::string(directory)+"/play.png").c_str());
        command("keydown shift"); key("F5"); command("keyup shift");
        CHECK(!ui.playing()); CHECK(!EditorUITestAccess::captured(ui)); CHECK(runtime.validate(error));
        CHECK(projectio::toJson(scene) == authoring);
        CHECK(NearlyEqual(camera.position(),editorEye));
        for (int i = 0; i < 3; ++i) {
            key("F5"); CHECK(ui.playing());
            command("keydown shift"); key("F5"); command("keyup shift"); CHECK(!ui.playing());
        }
        CHECK(projectio::toJson(scene) == authoring);
        TakeScreenshot((std::string(directory)+"/editor.png").c_str());
    }
    rlImGuiShutdown(); CloseWindow();
    std::printf("Game UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);
    return failures ? 1 : 0;
}

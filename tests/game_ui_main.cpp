// Real events exercise the production UI/router/renderer/runtime. Asset setup
// and inspections use a friend fixture, not a second gameplay implementation.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <chrono>
#include <vector>
#include <sys/resource.h>
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
    static void sharedGizmoSettings(EditorUI& ui, bool local, float snap) {
        ui.gizmoLocalSpace_ = local; ui.rotateSnapDegrees_ = snap;
    }
    static bool local(EditorUI& ui) { return ui.rigGizmo_.localSpace(); }
    static float rotateSnap(EditorUI& ui) { return ui.rigGizmo_.rotateSnapDegrees(); }
    static int selectedBody(EditorUI& ui) { return ui.selectedRigBody_; }
    static int selectedJoint(EditorUI& ui) { return ui.selectedJoint_; }
    static float time(EditorUI& ui) { return ui.previewTime_; }
    static const game::AnimationClip& draft(EditorUI& ui) { return ui.animationDraft_; }
    static void animation(EditorUI& ui) {
        ui.openAnimation("r6-walk");
        ui.showRigEditor_ = false;
    }
    static void hidePanels(EditorUI& ui) { ui.showAnimationEditor_ = false; ui.showRigEditor_ = false; }
    static ImVec2 rectCenter(const float (&r)[4]) { return {(r[0]+r[2])*.5f, (r[1]+r[3])*.5f}; }
    static void dumpPauseRects(EditorUI& ui) {
        std::printf("DBG rects: tab %g %g %g %g | slider %g %g %g %g | reset %g %g %g %g\n",
            ui.pauseTabRect_[0],ui.pauseTabRect_[1],ui.pauseTabRect_[2],ui.pauseTabRect_[3],
            ui.pauseSensitivityRect_[0],ui.pauseSensitivityRect_[1],ui.pauseSensitivityRect_[2],ui.pauseSensitivityRect_[3],
            ui.pauseResetRect_[0],ui.pauseResetRect_[1],ui.pauseResetRect_[2],ui.pauseResetRect_[3]);
    }
    static ImVec2 pauseTabSettings(EditorUI& ui) { return rectCenter(ui.pauseTabRect_); }
    static ImVec2 pauseSlider(EditorUI& ui) {
        const float (&r)[4] = ui.pauseSensitivityRect_;
        // Clica no campo do DragFloat, longe do rótulo à direita.
        return {r[0] + std::min(60.0f, (r[2]-r[0])*.35f), (r[1]+r[3])*.5f};
    }
    static ImVec2 pauseReset(EditorUI& ui) { return rectCenter(ui.pauseResetRect_); }
    static void saveAsDialog(EditorUI& ui, const std::string& path) {
        ui.requestSavePopup_ = true;
        std::snprintf(ui.pathBuffer_,sizeof(ui.pathBuffer_),"%s",path.c_str());
    }
};
}
using namespace loblox;
unsigned long InputTestRootWindow();
int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::fprintf(stderr,"FAIL %d: %s\n", __LINE__, #x); } } while (0)
int main() {
    std::setvbuf(stdout,nullptr,_IOLBF,0); // preserve results before a possible driver LSan exit
    CHECK(SceneRenderer::MaterialColor(WHITE,PartMaterial::Metal).b == 255);
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
        auto number = [&](Vector2 p, const char* text) {
            command("keydown ctrl"); click(p); command("keyup ctrl");
            if (!ui.playing()) { key("F5"); CHECK(!ui.playing()); }
            command("keydown ctrl"); key("a"); command("keyup ctrl");
            command(std::string("type --delay 0 ")+text); key("Return");
        };
        auto viewport = [&]() {
            auto* w = ImGui::FindWindowByName("Viewport");
            return Rectangle{w->InnerRect.Min.x,w->InnerRect.Min.y,w->InnerRect.GetWidth(),w->InnerRect.GetHeight()};
        };
        frames(5); command("search --name '^Loblox game regression$' windowfocus --sync");
        EditorUITestAccess::rigPanel(ui); frames();
        ImGui::SetWindowDock(ImGui::FindWindowByName("Rig Editor"),0,ImGuiCond_Always);
        ImGui::SetWindowSize("Rig Editor",{390,550});
        ImGui::SetWindowPos("Rig Editor", {80,180}); frames();
        auto* panel = ImGui::FindWindowByName("Rig Editor");
        click({panel->DC.CursorStartPos.x+35, panel->DC.CursorStartPos.y+9});
        CHECK(scene.gameAssets().rigs.size() == 1);
        if (scene.gameAssets().rigs.empty()) { TakeScreenshot((std::string(directory)+"/insert-failed.png").c_str()); return 1; }
        CHECK(scene.gameAssets().animations.size() == 3);
        // Fixed-font/window fixture; body list and first color component.
        const Vector2 rigOrigin{panel->DC.CursorStartPos.x,panel->DC.CursorStartPos.y};
        click(Vector2Add(rigOrigin,{40,285}));
        CHECK(EditorUITestAccess::selectedBody(ui) == game::LeftArm);
        number(Vector2Add(rigOrigin,{40,355}),"210");
        CHECK(scene.gameAssets().rigs[0].bodies[game::LeftArm].color.r == 210);
        EditorUITestAccess::pose(ui); frames();
        EditorUITestAccess::sharedGizmoSettings(ui,true,7.5f); frames();
        CHECK(EditorUITestAccess::local(ui));
        CHECK(NearlyEqual(EditorUITestAccess::rotateSnap(ui),7.5f));
        EditorUITestAccess::sharedGizmoSettings(ui,false,15); frames();
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
        // Real numeric editing, track selection and Insert/Update button.
        EditorUITestAccess::animation(ui); frames();
        TakeScreenshot((std::string(directory)+"/animation-docked.png").c_str());
        ImGui::SetWindowDock(ImGui::FindWindowByName("Animation Editor"),0,ImGuiCond_Always);
        ImGui::SetWindowPos("Animation Editor",{20,160});
        ImGui::SetWindowSize("Animation Editor",{960,620}); frames();
        auto* animation = ImGui::FindWindowByName("Animation Editor");
        const Vector2 animationOrigin{animation->DC.CursorStartPos.x,animation->DC.CursorStartPos.y};
        TakeScreenshot((std::string(directory)+"/animation-before.png").c_str());
        number(Vector2Add(animationOrigin,{60,216}),"0.25");
        CHECK(NearlyEqual(EditorUITestAccess::time(ui),.25f,.001f));
        click(Vector2Add(animationOrigin,{60,325}));
        CHECK(EditorUITestAccess::selectedJoint(ui) == game::RightShoulder);
        number(Vector2Add(animationOrigin,{35,458}),"55");
        CHECK(NearlyEqual(EulerDegreesFromQuat(EditorUITestAccess::preview(ui)[game::RightShoulder].rotation).x,55,.01f));
        click(Vector2Add(animationOrigin,{60,504}));
        CHECK(NearlyEqual(EulerDegreesFromQuat(EditorUITestAccess::draft(ui).sample(.25f)[game::RightShoulder].rotation).x,55,.01f));
        // Retiming owns mouse/keyboard exclusively and Escape restores the key.
        const float timelineWidth = animation->ContentRegionRect.GetWidth()-153;
        const Vector2 diamond = Vector2Add(animationOrigin,{153+timelineWidth*.25f/.8f,325});
        mouse(diamond); command("mousedown 1"); mouse(Vector2Add(diamond,{timelineWidth*.05f/.8f,0}));
        CHECK(NearlyEqual(EditorUITestAccess::time(ui),.3f,.001f));
        key("Escape"); command("mouseup 1");
        CHECK(NearlyEqual(EulerDegreesFromQuat(EditorUITestAccess::draft(ui).sample(.25f)[game::RightShoulder].rotation).x,55,.01f));
        TakeScreenshot((std::string(directory)+"/animation-edited.png").c_str());
        EditorUITestAccess::hidePanels(ui); frames();
        const std::string copyPath = std::string(directory)+"/authored-copy.json";
        EditorUITestAccess::saveAsDialog(ui,copyPath); frames();
        auto* saveDialog = ImGui::FindWindowByName("Salvar projeto");
        click({saveDialog->DC.CursorStartPos.x+40,saveDialog->DC.CursorStartPos.y+53});
        Scene copy; std::string copyError;
        CHECK(projectio::load(copy,copyPath,copyError));
        const auto* copiedClip = copy.gameAssets().findAnimation("r6-walk");
        CHECK(copiedClip && NearlyEqual(EulerDegreesFromQuat(copiedClip->sample(.25f)[game::RightShoulder].rotation).x,55,.01f));
        ui.setProjectPath(config.defaultProjectPath);
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
        std::vector<double> frameTimes;
        for (int i = 0; i < 30; ++i) {
            const auto began = std::chrono::steady_clock::now(); frames(1);
            frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
        }
        std::sort(frameTimes.begin(),frameTimes.end());
        struct rusage usage{}; getrusage(RUSAGE_SELF,&usage);
        std::printf("Play fixture: median %.2f ms, p95 %.2f ms; peak RSS %.1f MiB (software, capped 60)\n",
            frameTimes[15],frameTimes[28],usage.ru_maxrss/1024.);
        CHECK(NearlyEqual(EulerDegreesFromQuat(runtime.scene().gameAssets().findAnimation("r6-walk")->sample(.25f)[game::RightShoulder].rotation).x,55,.01f));
        auto start = runtime.position(); command("keydown w"); frames(12); command("keyup w");
        CHECK(runtime.position().z < start.z-1);
        CHECK(NearlyEqual(camera.position(),editorEye));
        key("Escape"); CHECK(runtime.state() == game::RuntimeState::Paused);
        CHECK(!EditorUITestAccess::captured(ui));
        auto paused = runtime.position(); command("keydown w"); frames(10); command("keyup w");
        CHECK(NearlyEqual(runtime.position(),paused));
        TakeScreenshot((std::string(directory)+"/pause.png").c_str());
    auto* menu = ImGui::FindWindowByName("Play paused");
    const float pausedZoom = runtime.playerCamera().distance();
    command("click 4"); CHECK(NearlyEqual(runtime.playerCamera().distance(), pausedZoom));
    // O próprio menu captura os rects reais (aba/slider/botão) durante o
    // desenho; os cliques miram o centro real do widget.
    EditorUITestAccess::dumpPauseRects(ui);
    const ImVec2 ts = EditorUITestAccess::pauseTabSettings(ui);
    click({ts.x, ts.y}); // aba Settings
    frames(1);
    const ImVec2 sl = EditorUITestAccess::pauseSlider(ui);
    number({sl.x, sl.y}, "0.006"); // slider Sensibilidade
    CHECK(NearlyEqual(runtime.settings().sensitivity, .006f, .00001f));
    // Reset é um clique real no botão do rodapé; mantém exatamente uma capsula.
    const ImVec2 rs = EditorUITestAccess::pauseReset(ui);
    click({rs.x, rs.y});
    CHECK(std::hypot(runtime.position().x, runtime.position().z) < .01f);
    CHECK(runtime.bodyCount() == scene.parts().size() + 1);
    key("Escape"); CHECK(runtime.state() == game::RuntimeState::Playing); // ESC fecha em qualquer aba
        frames(25);
        command("keydown space"); CHECK(runtime.velocity().y > 0 || runtime.humanoidState() == game::HumanoidState::Jumping);
        command("keyup space");
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
        command("windowfocus "+std::to_string(InputTestRootWindow()));
        CHECK(runtime.state() == game::RuntimeState::Paused);
        CHECK(!EditorUITestAccess::captured(ui));
        command("search --name '^Loblox game regression$' windowfocus --sync");
        key("Escape"); CHECK(runtime.state() == game::RuntimeState::Playing);
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

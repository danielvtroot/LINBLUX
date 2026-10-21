// Production UI/renderer, actual window events, measurable overlay geometry.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <filesystem>
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "editor/EditorUI.h"
#include "editor/StudioTheme.h"
#include "rendering/SceneRenderer.h"
#include "rendering/EditorCamera.h"
#include "core/Diagnostics.h"
#include "serialization/ProjectIO.h"

namespace loblox {
struct EditorUITestAccess {
    static void play(EditorUI& ui) { ui.startPlay(); }
    static void stop(EditorUI& ui) { ui.stopPlay(); }
    static game::RuntimeSession& runtime(EditorUI& ui) { return ui.runtime_; }
    static Rectangle viewport(EditorUI& ui) { return {ui.gameViewportX_,ui.gameViewportY_,ui.gameViewportW_,ui.gameViewportH_}; }
    static ImVec2 settings(EditorUI& ui) { auto* r=ui.pauseTabRect_;return {(r[0]+r[2])/2,(r[1]+r[3])/2}; }
    static ImVec2 respawn(EditorUI& ui) { auto* r=ui.pauseResetRect_;return {(r[0]+r[2])/2,(r[1]+r[3])/2}; }
    static int page(EditorUI& ui) { return ui.pausePage_; }
    static bool captured(EditorUI& ui) { return ui.gameplayCapture_; }
    static void script(EditorUI& ui, InstanceId id) { ui.openScript(id); }
};
}
using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do { ++checks; if(!(x)){++failures;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);} } while(0)
int main() {
    char temp[]="/tmp/loblox-visual-studio-XXXXXX";
    const char* directory=mkdtemp(temp);if(!directory)return 1;
    diag::configure(std::string(directory)+"/events.jsonl");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);SetTraceLogLevel(LOG_WARNING);
    InitWindow(1366,768,"Loblox visual acceptance");SetWindowPosition(0,0);SetExitKey(KEY_NULL);SetTargetFPS(60);
    rlImGuiSetup(true);auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly=true;io.IniFilename=nullptr;
    const char* font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if(FileExists(font)) io.FontDefault=io.Fonts->AddFontFromFileTTF(font,14);
    studio::Apply();
    {
        Scene scene;scene.buildDefaultScene();scene.installDefaultScripts();
        auto* folder=scene.createContainer(InstanceClass::Folder,nullptr,"Buildings");
        auto* model=scene.createContainer(InstanceClass::Model,folder,"House");
        auto* part=scene.createPart(PartShape::Block,{9,3,0},{7,6,1},"Wall");part->setAnchored(true);
        std::string error;CHECK(scene.reparent(part,model,error));scene.select(part);
        PhysicsWorld physics;physics.attach(scene);EditorCamera camera;SceneRenderer renderer;AppConfig config;
        config.defaultProjectPath=std::string(directory)+"/visual.json";
        EditorUI ui(scene,physics,camera,renderer,config);
        auto frames=[&](int count=3){for(int i=0;i<count;++i){BeginDrawing();ClearBackground(BLACK);rlImGuiBegin();ui.draw();rlImGuiEnd();EndDrawing();}};
        auto event=[&](const std::string& args){CHECK(std::system(("xdotool "+args).c_str())==0);frames();};
        auto key=[&](const char* key){event(std::string("keydown ")+key);event(std::string("keyup ")+key);};
        auto click=[&](ImVec2 at){event("mousemove "+std::to_string(int(at.x))+" "+std::to_string(int(at.y)));event("mousedown 1");event("mouseup 1");};
        auto capture=[&](const std::string& name){frames();TakeScreenshot((std::string(directory)+"/"+name+".png").c_str());};
        frames(5);event("search --name '^Loblox visual acceptance$' windowfocus --sync");
        capture("studio-1366");
        CHECK(projectio::save(scene,config.defaultProjectPath,error));
        const auto original=projectio::toJson(scene);
        for(auto size:{ImVec2{1280,720},ImVec2{1366,768},ImVec2{1600,900},ImVec2{1920,1080}}) {
            SetWindowSize(int(size.x),int(size.y));frames(5);
            EditorUITestAccess::play(ui);frames(10);
            auto& runtime=EditorUITestAccess::runtime(ui);
            CHECK(runtime.state()==game::RuntimeState::Playing);
            capture("hud-"+std::to_string(int(size.x)));
            key("Escape");CHECK(runtime.state()==game::RuntimeState::Paused);
            const auto vp=EditorUITestAccess::viewport(ui);auto* menu=ImGui::FindWindowByName("Play paused");
            CHECK(menu!=nullptr);CHECK(vp.x>0 && vp.y>0);
            if(menu) {
                CHECK(menu->Pos.x>=vp.x && menu->Pos.y>=vp.y);
                CHECK(menu->Pos.x+menu->Size.x<=vp.x+vp.width+1);
                CHECK(menu->Pos.y+menu->Size.y<=vp.y+vp.height+1);
                CHECK(std::fabs(menu->Pos.x+menu->Size.x/2-(vp.x+vp.width/2))<=1);
                CHECK(std::fabs(menu->Pos.y+menu->Size.y/2-(vp.y+vp.height/2))<=1);
                auto button=EditorUITestAccess::respawn(ui);
                CHECK(button.x>menu->Pos.x && button.y>menu->Pos.y+menu->Size.y*.75f);
                CHECK(button.y<menu->Pos.y+menu->Size.y);
            }
            CHECK(!EditorUITestAccess::captured(ui));
            capture("pause-game-"+std::to_string(int(size.x)));
            click(EditorUITestAccess::settings(ui));CHECK(EditorUITestAccess::page(ui)==1);
            capture("pause-settings-"+std::to_string(int(size.x)));
            const auto position=runtime.position();event("keydown w");frames(5);event("keyup w");CHECK(NearlyEqual(position,runtime.position()));
            const auto generation=runtime.characterGeneration();click(EditorUITestAccess::respawn(ui));CHECK(runtime.characterGeneration()==generation+1);
            key("Escape");CHECK(runtime.state()==game::RuntimeState::Playing);
            EditorUITestAccess::stop(ui);frames();CHECK(!EditorUITestAccess::captured(ui));
            CHECK(projectio::toJson(scene)==original);
        }
        CHECK(scene.validate(error));
    }
    rlImGuiShutdown();CloseWindow();
    std::printf("Visual Studio: %d checks, %d failures; captures: %s\n",checks,failures,directory);
    return failures?1:0;
}

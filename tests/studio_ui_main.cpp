#include <cstdio>
#include <cstdlib>
#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "editor/EditorUI.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "serialization/ProjectIO.h"
using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL %d: %s\n",__LINE__,#x); } } while(false)
int main() {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    char temporary[]="/tmp/loblox-studio-ui-XXXXXX"; const char* directory=mkdtemp(temporary); if(!directory)return 1;
    SetTraceLogLevel(LOG_WARNING); InitWindow(1280,800,"Loblox studio regression");
    if(!IsWindowReady())return 1; SetExitKey(KEY_NULL); SetTargetFPS(60); rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable; ImGui::GetIO().IniFilename=nullptr;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly=true;
    {
        Scene scene; scene.buildDefaultScene();
        auto* part=scene.createPart(PartShape::Block,{20,4,0},{4,4,4},"Source"); part->setAnchored(true);
        const auto id=part->id(); PhysicsWorld physics; physics.attach(scene);
        EditorCamera camera; SceneRenderer renderer; AppConfig config; config.defaultProjectPath=std::string(directory)+"/studio.json";
        EditorUI ui(scene,physics,camera,renderer,config);
        auto frames=[&](int count=3){for(int i=0;i<count;++i){physics.syncScene(scene); BeginDrawing(); ClearBackground(RAYWHITE);rlImGuiBegin();ui.draw();rlImGuiEnd();EndDrawing();}};
        auto command=[&](std::string command){CHECK(std::system(("xdotool "+command).c_str())==0);frames();};
        auto mouse=[&](Vector2 p){command("mousemove "+std::to_string(int(p.x))+" "+std::to_string(int(p.y)));};
        auto click=[&](Vector2 p){mouse(p);command("mousedown 1");command("mouseup 1");};
        auto key=[&](const char* name){command(std::string("keydown ")+name);command(std::string("keyup ")+name);};
        frames(); command("search --name '^Loblox studio regression$' windowfocus --sync");
        auto* window=ImGui::FindWindowByName("Explorer"); CHECK(window);
        if (!window) return 1;
        ImGui::SetWindowDock(window,0,ImGuiCond_Always);
        ImGui::SetWindowPos("Explorer",{10,175}); ImGui::SetWindowSize("Explorer",{390,590}); frames();
        auto* explorer=ImGui::FindWindowByName("Explorer");
        const Vector2 origin{explorer->DC.CursorStartPos.x,explorer->DC.CursorStartPos.y};
        click(Vector2Add(origin,{30,8})); auto* folder=scene.selection();
        CHECK(folder && folder->instanceClass()==InstanceClass::Folder);
        click(Vector2Add(origin,{30,8})); auto* child=scene.selection();
        CHECK(child && child->parent()==folder);
        key("F2"); CHECK(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup));
        command("keydown ctrl");key("a");command("keyup ctrl"); command("type --delay 0 Nested"); key("Return");
        CHECK(child && child->name()=="Nested");
        // Fixed 13px-font fixture: buttons + search + Workspace + three Part rows.
        const float treeTop=origin.y+46;
        mouse({origin.x+65,treeTop+17*3+7}); command("mousedown 1");
        mouse({origin.x+85,treeTop+17*5+7}); frames(6); command("mouseup 1");
        CHECK(part->parent()==child); CHECK(scene.find(id)==part); CHECK(physics.hasBody(id));
        std::string error; CHECK(scene.validate(error)); CHECK(physics.validate(scene,error));
        Part* hit=nullptr; CHECK(physics.raycast({20,20,0},{0,-1,0},25,&hit)); CHECK(hit==part);
        CHECK(projectio::save(scene,config.defaultProjectPath,error)); Scene reopened;
        CHECK(projectio::load(reopened,config.defaultProjectPath,error));
        CHECK(reopened.find(id)->parent()->id()==child->id());
        TakeScreenshot((std::string(directory)+"/explorer.png").c_str());
    }
    rlImGuiShutdown();CloseWindow();
    std::printf("Studio UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory); return failures?1:0;
}

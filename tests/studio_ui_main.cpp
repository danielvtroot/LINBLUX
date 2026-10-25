#include "ui/Localization.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>
#include <sys/resource.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "editor/EditorUI.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "serialization/ProjectIO.h"
#include "core/Diagnostics.h"
namespace loblox {
struct EditorUITestAccess {
    static game::RuntimeSession& runtime(EditorUI& ui){return ui.runtime_;}
    static Vector2 sourceField(EditorUI& ui){const auto r=ui.scriptSourceControl_;return {r.x+50,r.y+12};}
    static void lightingFilter(EditorUI& ui){ui.showScriptEditor_=false;ui.showOutput_=false;std::snprintf(ui.explorerFilter_,sizeof(ui.explorerFilter_),"Lighting");}
};
}
using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL %d: %s\n",__LINE__,#x); } } while(false)
int main() {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    char temporary[]="/tmp/loblox-studio-ui-XXXXXX"; const char* directory=mkdtemp(temporary); if(!directory)return 1;
    diag::configure(std::string(directory)+"/studio.jsonl");
    SetTraceLogLevel(LOG_WARNING); InitWindow(1280,800,"LINBLUX studio regression");
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
        // The test renders synchronously, so drain raylib's bounded character
        // queue between batches; a real application pumps frames while typing.
        auto type=[&](const std::string& text){for(std::size_t at=0;at<text.size();at+=8)
            command("type --clearmodifiers --delay 0 -- '"+text.substr(at,8)+"'");};
        frames(); command("search --name '^LINBLUX studio regression$' windowfocus --sync");
        auto* window=ImGui::FindWindowByName(loblox::i18n::Label("Explorer")); CHECK(window);
        if (!window) return 1;
        ImGui::SetWindowDock(window,0,ImGuiCond_Always);
        ImGui::SetWindowPos(loblox::i18n::Label("Explorer"),{10,175}); ImGui::SetWindowSize(loblox::i18n::Label("Explorer"),{390,590}); frames();
        auto* explorer=ImGui::FindWindowByName(loblox::i18n::Label("Explorer"));
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
        scene.select(nullptr);frames();
        auto* toolbar=ImGui::FindWindowByName(loblox::i18n::Label("##Toolbar"));
        auto* tabs=GImGui->TabBars.GetByKey(toolbar->GetID("##RibbonTabs"));CHECK(tabs);
        if(tabs)for(auto& tab:tabs->Tabs)if(std::string(ImGui::TabBarGetTabName(tabs,&tab))==loblox::i18n::Label("Script")) {
            click({tabs->BarRect.Min.x+tab.Offset+tab.Width*.5f,tabs->BarRect.Min.y+ImGui::GetTextLineHeight()*.5f});break;
        }
        const float createWidth=std::max(ImGui::CalcTextSize(loblox::i18n::Label("New Script")).x+16.f,54.f);
        click({toolbar->DC.CursorStartPos.x+createWidth*.5f,toolbar->Pos.y+66});
        auto* scriptWindow=ImGui::FindWindowByName(loblox::i18n::Label("Script Editor"));CHECK(scriptWindow);
        if (scriptWindow) {
            auto* script=scene.selection();CHECK(script && script->instanceClass()==InstanceClass::Script);
            ImGui::SetWindowDock(scriptWindow,0,ImGuiCond_Always);ImGui::SetWindowPos(loblox::i18n::Label("Script Editor"),{410,175});
            ImGui::SetWindowSize(loblox::i18n::Label("Script Editor"),{850,590});frames();
            const Vector2 editorOrigin{scriptWindow->DC.CursorStartPos.x,scriptWindow->DC.CursorStartPos.y};
            click(EditorUITestAccess::sourceField(ui)); CHECK(ImGui::GetIO().WantTextInput);
            command("keydown ctrl");key("a");command("keyup ctrl");
            // Source was nested: find it through the real hierarchy, not flat name lookup.
            type("workspace.Folder.Nested.Source.Position = Vector3.new(33, 4, 0); game:GetService(\"Players\").CharacterAdded:Connect(function(c) c.Humanoid:TakeDamage(25) end)");
            key("F5");CHECK(!ui.playing()); // focused source consumes editing input
            command("keydown ctrl");key("s");command("keyup ctrl");
            CHECK(script && static_cast<Script*>(script)->source.find("Vector3.new(33")!=std::string::npos);
            TakeScreenshot((std::string(directory)+"/script.png").c_str());
            float playX=editorOrigin.x;
            for(const char* caption:{"New Script","New Module","Save All + Project"})playX+=ImGui::CalcTextSize(loblox::i18n::Label(caption)).x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x;
            click({playX+ImGui::CalcTextSize(loblox::i18n::Label("Play")).x*.5f+ImGui::GetStyle().FramePadding.x,editorOrigin.y+ImGui::GetFrameHeight()*.5f});
            CHECK(ui.playing());
            if(ui.playing()) {
                CHECK(EditorUITestAccess::runtime(ui).scene().findPart(id)->position().x==33);
                CHECK(EditorUITestAccess::runtime(ui).health()==75);frames(10);
                TakeScreenshot((std::string(directory)+"/health.png").c_str());
                CHECK(part->position().x==20);
                command("keydown shift");key("F5");command("keyup shift");CHECK(!ui.playing());
            }
            CHECK(projectio::load(reopened,config.defaultProjectPath,error));
            CHECK(script && static_cast<Script*>(reopened.find(script->id()))->source.find("Vector3.new(33")!=std::string::npos);
        }
        // The real Lighting row must reach its own Properties, not the generic
        // root-service inspector. Its right-click owns exactly one popup ID.
        EditorUITestAccess::lightingFilter(ui);frames();
        auto* lightExplorer=ImGui::FindWindowByName(loblox::i18n::Label("Explorer"));
        const Vector2 lightAt{lightExplorer->DC.CursorStartPos.x+45,
            lightExplorer->DC.CursorStartPos.y+2*ImGui::GetFrameHeightWithSpacing()+ImGui::GetTextLineHeight()*.5f};
        click(lightAt);CHECK(scene.selection()==scene.lighting());
        mouse(lightAt);command("mousedown 3");command("mouseup 3");
        CHECK(GImGui->OpenPopupStack.Size==1);
        if(GImGui->OpenPopupStack.Size==1) {
            const std::string popup="##explorer_context_"+std::to_string(scene.lighting()->id());
            CHECK(GImGui->OpenPopupStack[0].PopupId==ImHashStr(popup.c_str(),0,lightExplorer->ID));
        }
        key("Escape");
        TakeScreenshot((std::string(directory)+"/lighting-properties.png").c_str());
        // Measure the real Explorer traversal with 1,000 additional visible rows.
        // Clear the filter through normal text input before measuring.
        click({lightExplorer->DC.CursorStartPos.x+60,lightExplorer->DC.CursorStartPos.y+ImGui::GetFrameHeightWithSpacing()+ImGui::GetFrameHeight()*.5f});
        command("keydown ctrl");key("a");command("keyup ctrl");key("BackSpace");key("Return");
        const auto previousCount=scene.objects().size();
        for(int i=0;i<1000;++i)scene.createContainer(InstanceClass::Folder,scene.workspace(),"PerfNode"+std::to_string(i));
        CHECK(scene.objects().size()==previousCount+1000);CHECK(scene.validate(error));
        const auto started=std::chrono::steady_clock::now();frames(10);
        struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
        std::printf("Explorer 1000 added nodes: mean frame %.2f ms; peak RSS %.1f MiB (software fixture)\n",
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/10,
            usage.ru_maxrss/1024.);
    }
    rlImGuiShutdown();CloseWindow();
    std::printf("Studio UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory); return failures?1:0;
}

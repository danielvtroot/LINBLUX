#include "ui/Localization.h"
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
    static void hideScript(EditorUI& ui) { ui.showScriptEditor_=false; }
    static void animation(EditorUI& ui) {ui.openAnimation(ui.scene_.gameAssets().player.walkAnimation);ui.showRigEditor_=false;}
    static Rectangle healthHud(EditorUI& ui) {return ui.healthHud_;}
    static void open(EditorUI& ui,const std::string& id) {ui.openAnimation(id);}
    static std::string clip(EditorUI& ui) {return ui.animationSource_;}
    static void hideAnimation(EditorUI& ui) {ui.showAnimationEditor_=false;}
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
    InitWindow(1366,768,"LINBLUX visual acceptance");SetWindowPosition(0,0);SetExitKey(KEY_NULL);SetTargetFPS(60);
    rlImGuiSetup(true);auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly=true;io.IniFilename=nullptr;
    const char* font="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if(FileExists(font)) io.FontDefault=io.Fonts->AddFontFromFileTTF(font,14);
    studio::Apply();
    {
        Scene scene;scene.buildDefaultScene();scene.installDefaultScripts();scene.gameAssets().player.enableShiftLock=true;
        // Script-built UI must reach the Studio Play texture, not only the Player.
        auto* luaHud=scene.createScript(InstanceClass::Script,scene.ensureService(InstanceClass::ServerScriptService),"LuaHud.lua");
        luaHud->source=R"lua(
linblux.ui.create("panel", {anchor="center", width=220, height=120, background="#FF00FFFF"})
)lua";
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
        frames(5);event("search --name '^LINBLUX visual acceptance$' windowfocus --sync");
        capture("studio-1366");
        for(const char* label:{"Avatar","Script","Home"}) {
            auto* toolbar=ImGui::FindWindowByName(loblox::i18n::Label("##Toolbar"));
            auto* bar=GImGui->TabBars.GetByKey(toolbar->GetID("##RibbonTabs"));CHECK(bar);
            if(bar)for(auto& tab:bar->Tabs)if(std::string(ImGui::TabBarGetTabName(bar,&tab))==label) {
                const ImVec2 at{bar->BarRect.Min.x+tab.Offset+tab.Width*.5f,bar->BarRect.Min.y+ImGui::GetTextLineHeight()*.5f};
                click(at);capture(std::string("ribbon-")+label);break;
            }
        }
        const auto& rig=scene.gameAssets().rigs.front();
        part->setPosition({25,3,0});
        camera.setOrbit(Vector3Add(rig.root.position,{0,1,0}),12,PI,0);
        scene.select(nullptr);capture("r6-front");
        camera.setOrbit(Vector3Add(rig.root.position,{0,1,0}),12,PI+.45f,.12f);
        capture("r6-perspective");camera.reset();part->setPosition({9,3,0});scene.select(part);
        CHECK(projectio::save(scene,config.defaultProjectPath,error));
        const auto original=projectio::toJson(scene);
        for(auto size:{ImVec2{1280,720},ImVec2{1366,768},ImVec2{1600,900},ImVec2{1920,1080}}) {
            SetWindowSize(int(size.x),int(size.y));frames(5);
            EditorUITestAccess::play(ui);frames(10);
            auto& runtime=EditorUITestAccess::runtime(ui);
            CHECK(runtime.state()==game::RuntimeState::Playing);
            const auto hud=EditorUITestAccess::healthHud(ui),area=EditorUITestAccess::viewport(ui);
            CHECK(hud.width>50 && hud.x>=area.x && hud.y>=area.y);
            CHECK(hud.x+hud.width<=area.x+area.width && hud.y+hud.height<=area.y+area.height);
            const float health=runtime.health();frames(5);CHECK(runtime.health()==health);
            capture("hud-"+std::to_string(int(size.x)));
            if(size.x==1280) {
                // The panel is drawn into the Play texture by DrawLuaUi.
                {const auto frameImage=LoadImageFromScreen();int painted=0;
                 for(int y=0;y<frameImage.height;y+=2)for(int x=0;x<frameImage.width;x+=2){const auto pixel=GetImageColor(frameImage,x,y);if(pixel.r>200&&pixel.g<70&&pixel.b>200)++painted;}
                 UnloadImage(frameImage);CHECK(painted>200);}
                CHECK(runtime.takeDamage(25,error));frames(12);CHECK(runtime.health()==75);capture("health-75");
                CHECK(runtime.takeDamage(50,error));frames(20);CHECK(runtime.health()==25);capture("health-25");
                CHECK(runtime.heal(75,error));frames(12);CHECK(runtime.health()==100);
                const auto image=EditorUITestAccess::viewport(ui);
                click({image.x+40,image.y+30});CHECK(runtime.state()==game::RuntimeState::Paused);
                key("Escape");CHECK(runtime.state()==game::RuntimeState::Playing);
                key("Shift_L");CHECK(runtime.shiftLockActive());CHECK(EditorUITestAccess::captured(ui));
                capture("shift-lock");
            }
            key("Escape");CHECK(runtime.state()==game::RuntimeState::Paused);
            CHECK(EditorUITestAccess::healthHud(ui).width==0);
            const auto vp=EditorUITestAccess::viewport(ui);auto* menu=ImGui::FindWindowByName(loblox::i18n::Label("Play paused"));
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
            CHECK(EditorUITestAccess::healthHud(ui).width==0);
            CHECK(projectio::toJson(scene)==original);
        }
        CHECK(scene.validate(error));
        SetWindowSize(1366,768);frames();
        for(const auto& node:scene.objects()) if(IsScriptClass(node->instanceClass())) {
            EditorUITestAccess::script(ui,node->id());frames();capture("script");
            EditorUITestAccess::hideScript(ui);break;
        }
        EditorUITestAccess::animation(ui);frames();
        if(auto* panel=ImGui::FindWindowByName(loblox::i18n::Label("Animation Editor"))) {
            ImGui::SetWindowDock(panel,0,ImGuiCond_Always);
            ImGui::SetWindowPos(loblox::i18n::Label("Animation Editor"),{270,410});ImGui::SetWindowSize(loblox::i18n::Label("Animation Editor"),{715,345});
            capture("animation-editor");
        }
        // Actual Animation Editor New -> rename -> save -> Delete, followed by
        // confirmation for an assigned clip. Only private fixture files are used.
        ImGui::SetWindowSize(loblox::i18n::Label("Animation Editor"),{715,530});ImGui::SetWindowPos(loblox::i18n::Label("Animation Editor"),{270,220});frames();
        const auto button=[&](const char* target) {
            auto* panel=ImGui::FindWindowByName(loblox::i18n::Label("Animation Editor"));float x=panel->DC.CursorStartPos.x;
            for(const char* name:{"New","Duplicate","Save Animation + Project","Revert","Use as Walk","Delete"}) {
                const float width=ImGui::CalcTextSize(loblox::i18n::Label(name)).x+ImGui::GetStyle().FramePadding.x*2;
                if(std::string(name)==target) {click({x+width/2,panel->DC.CursorStartPos.y+ImGui::GetFrameHeightWithSpacing()+ImGui::GetFrameHeight()/2});break;}
                x+=width+ImGui::GetStyle().ItemSpacing.x;
            }
        };
        const auto count=scene.gameAssets().animations.size();button("New");
        CHECK(scene.gameAssets().animations.size()==count+1);const auto created=EditorUITestAccess::clip(ui);
        auto* panel=ImGui::FindWindowByName(loblox::i18n::Label("Animation Editor"));
        click({panel->DC.CursorStartPos.x+40,panel->DC.CursorStartPos.y+2*ImGui::GetFrameHeightWithSpacing()+ImGui::GetFrameHeight()/2});
        event("keydown ctrl");key("a");event("keyup ctrl");event("type --delay 0 RenamedClip");key("Return");
        button("Save Animation + Project");CHECK(scene.gameAssets().findAnimation(created)->name=="RenamedClip");
        button("Delete");CHECK(!scene.gameAssets().findAnimation(created));
        EditorUITestAccess::open(ui,scene.gameAssets().player.walkAnimation);frames();button("Delete");
        auto* confirm=ImGui::FindWindowByName(loblox::i18n::Label("Delete assigned animation?"));CHECK(confirm && confirm->Active);
        if(confirm)click({confirm->DC.CursorStartPos.x+40,confirm->DC.CursorStartPos.y+2*ImGui::GetTextLineHeightWithSpacing()+ImGui::GetFrameHeight()/2});
        CHECK(scene.gameAssets().player.walkAnimation.empty());
        CHECK(scene.gameAssets().player.jumpAnimation!=scene.gameAssets().player.fallAnimation);
        EditorUITestAccess::hideAnimation(ui);frames();
        Part* base=nullptr;for(auto* p:scene.parts())if(p->instanceClass()==InstanceClass::Baseplate)base=p;
        CHECK(base);if(base) {const auto id=base->id();scene.select(base);if(auto* view=ImGui::FindWindowByName(loblox::i18n::Label("Viewport")))ImGui::FocusWindow(view);key("Delete");CHECK(!scene.find(id));CHECK(!physics.hasBody(id));}
        event("keydown ctrl");key("s");event("keyup ctrl");
        Scene reloaded;CHECK(projectio::load(reloaded,config.defaultProjectPath,error));
        CHECK(!reloaded.gameAssets().findAnimation(created));CHECK(reloaded.gameAssets().player.walkAnimation.empty());
        for(auto* p:reloaded.parts())CHECK(p->instanceClass()!=InstanceClass::Baseplate);
    }
    rlImGuiShutdown();CloseWindow();
    std::printf("Visual Studio: %d checks, %d failures; captures: %s\n",checks,failures,directory);
    return failures?1:0;
}

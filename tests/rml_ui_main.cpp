#include "ui/Localization.h"
// Actual RML document, GL3 renderer, engine actions and X11 input, not mock UI.
#include "editor/EditorUI.h"
#include "editor/StudioTheme.h"
#include "editor/GizmoLayout.h"
#include "ManipulationFixture.h"
#include "ui/StudioFrontend.h"
#include "ui/EditorActions.h"
#include "rendering/SceneRenderer.h"
#include "rendering/EditorCamera.h"
#include "serialization/ProjectIO.h"
#include "game/ImageAssets.h"
#include "core/Diagnostics.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
unsigned long InputTestRootWindow();

namespace loblox {
struct EditorUITestAccess {
    static game::RuntimeSession& runtime(EditorUI& ui){return ui.runtime_;}
    static bool snap(EditorUI& ui){return ui.snapEnabled_;}
    static bool captured(EditorUI& ui){return ui.gameplayCapture_;}
    static ToolMode tool(EditorUI& ui){return ui.tool_;}
    static Rectangle source(EditorUI& ui){return ui.scriptSourceControl_;}
    static InstanceId activeDocument(EditorUI& ui){return ui.activeDocument_;}
    static size_t documentCount(EditorUI& ui){return ui.openDocuments_.size();}
    static const std::string& scriptText(EditorUI& ui,InstanceId id){return ui.scriptDrafts_[id].text;}
    static int scriptCursor(EditorUI& ui,InstanceId id){return ui.scriptDrafts_[id].cursor;}
    static int scriptLine(EditorUI& ui,InstanceId id){return ui.scriptDrafts_[id].line;}
    static Rectangle outputMessage(EditorUI& ui,size_t index){return index<ui.outputMessageRects_.size()?ui.outputMessageRects_[index]:Rectangle{};}
    static int pendingLine(EditorUI& ui,InstanceId id){return ui.scriptDrafts_[id].pendingLine;}
    static bool scriptDirty(EditorUI& ui,InstanceId id){return ui.scriptDrafts_[id].dirty;}
    static void openAtLine(EditorUI& ui,InstanceId id,int line){ui.openScriptAtLine(id,line);}
    static bool maximized(EditorUI& ui){return ui.gameMaximized_;}
    static bool fullscreen(EditorUI& ui){return ui.gameFullscreen_;}
    static void rigArm(EditorUI& ui,const std::string& id){ui.selectRig(id,game::RightArm);ui.rigContext_=EditorUI::RigContext::Edit;ui.selectedJoint_=game::RightShoulder;ui.showJointPivots_=true;}
    static const Part& proxy(EditorUI& ui){return ui.jointProxy_;}
    static bool dragging(EditorUI& ui){return ui.rigGizmo_.dragging();}
    static Gizmo& partGizmo(EditorUI& ui){return ui.gizmo_;}
    static const Part* modelProxy(EditorUI& ui){return ui.modelTransform_.proxy();}
    static size_t undoCount(EditorUI& ui){return ui.history_.undoCount();}
    static bool partDragging(EditorUI& ui){return ui.transform_.active();}
    static bool editorEdges(EditorUI& ui){return ui.editorGeometry_.edges;}
};
}
using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do {++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}}while(0)
// Projects store Euler angles; quaternion -> Euler roundtrip can change the
// last few float bits. IDs/strings/hierarchy stay exact, floats allow 1e-4.
bool SameProjectValue(const nlohmann::json& a,const nlohmann::json& b) {
    if(a.is_number_integer()&&b.is_number_integer())return a==b;
    if(a.is_number()&&b.is_number())return std::fabs(a.get<double>()-b.get<double>())<.0001;
    if(a.type()!=b.type()||a.size()!=b.size())return false;
    if(a.is_object()) {for(auto it=a.begin();it!=a.end();++it)if(!b.contains(it.key())||!SameProjectValue(it.value(),b[it.key()]))return false;return true;}
    if(a.is_array()) {for(size_t i=0;i<a.size();++i)if(!SameProjectValue(a[i],b[i]))return false;return true;}
    return a==b;
}
int main() {
    char pattern[]="/tmp/loblox-rml-ui-XXXXXX";const char* directory=mkdtemp(pattern);if(!directory)return 1;
    setenv("XDG_CONFIG_HOME",directory,1);diag::configure(std::string(directory)+"/events.jsonl");
    setenv("XDG_STATE_HOME",(std::string(directory)+"/state").c_str(),1);
    std::filesystem::current_path(directory);
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1366,768,"LINBLUX RML integration");SetWindowPosition(0,0);SetExitKey(KEY_NULL);SetTargetFPS(60);
    rlImGuiSetup(true);ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;ImGui::GetIO().IniFilename=nullptr;studio::Apply();
    {
        Scene scene;scene.buildDefaultScene();scene.installDefaultScripts();scene.gameAssets().player.enableShiftLock=true;
        auto* folder=scene.createContainer(InstanceClass::Folder,nullptr,"Buildings");
        auto* part=scene.createPart(PartShape::Block,{14,3,0},{4,6,1},"Wall");part->setAnchored(true);scene.select(part);
        PhysicsWorld physics;physics.attach(scene);EditorCamera camera;SceneRenderer renderer;AppConfig config;
        config.defaultProjectPath=std::string(directory)+"/map.json";
        const auto untouched=projectio::toJson(scene);
        EditorUI ui(scene,physics,camera,renderer,config);std::string error;
        CHECK(ui.enableModernFrontend(error));std::fprintf(stderr,"RML init: %s\n",error.c_str());
        if(!ui.frontend())return 1;
        std::function<void()> observeFrame;
        auto frames=[&](int count=2){for(int i=0;i<count;++i){BeginDrawing();ClearBackground(BLACK);rlImGuiBegin();ui.draw();rlImGuiEnd();ui.renderModernFrontend();EndDrawing();if(observeFrame)observeFrame();}};
        auto event=[&](std::string args){CHECK(std::system(("xdotool "+args).c_str())==0);frames((std::getenv("LINBLUX_BODY_PROPERTIES_ONLY")||std::getenv("LINBLUX_PLAYER_MENU_ONLY"))?1:2);};
        auto key=[&](const std::string& name){event("keydown "+name);event("keyup "+name);};
        auto clickAt=[&](Vector2 p){event("mousemove "+std::to_string(int(p.x))+" "+std::to_string(int(p.y)));event("mousedown 1");event("mouseup 1");};
        auto click=[&](const std::string& id){
            auto b=ui.frontend()->elementBounds(id);
            // RmlUi may need another layout after a hidden parent is revealed.
            // Wait for a real hitbox, never synthesize a phantom click at 0,0.
            for(int i=0;i<8&&(b.width<=0||b.height<=0);++i){frames(1);b=ui.frontend()->elementBounds(id);}
            if(b.width<=0||b.height<=0)std::fprintf(stderr,"MISSING element: %s\n",id.c_str());
            CHECK(b.width>0&&b.height>0);if(b.width<=0||b.height<=0)return;
            clickAt({b.x+b.width/2,b.y+b.height/2});
            if(std::getenv("LINBLUX_PLAYER_MENU_ONLY"))frames(2);
        };
        auto capture=[&](const std::string& name){frames();TakeScreenshot((std::string(directory)+"/"+name+".png").c_str());};
        // Painéis longos rolam; a automação rola até o controle antes de clicar.
        auto clickVisible=[&](const std::string& id){ui.frontend()->scrollIntoView(id);frames(2);click(id);};
        frames(6);event("search --name '^LINBLUX RML integration$' windowfocus --sync");
        std::string windowId;
        {FILE* pipe=popen("xdotool search --name '^LINBLUX RML integration$' | head -1","r");char buffer[64]{};if(pipe&&std::fgets(buffer,sizeof(buffer),pipe))windowId=buffer;if(pipe)pclose(pipe);while(!windowId.empty()&&(windowId.back()=='\n'||windowId.back()=='\r'))windowId.pop_back();}
        // Resize like a user/window manager would instead of calling raylib's
        // SetWindowSize between GL frames; llvmpipe can otherwise expose a
        // half-resized back buffer to the immediate screenshot.
        auto resizeWindow=[&](int width,int height){
            CHECK(!windowId.empty());
            CHECK(std::system(("xdotool windowsize "+windowId+" "+std::to_string(width)+" "+std::to_string(height)).c_str())==0);
            for(int i=0;i<120&&(GetScreenWidth()!=width||GetScreenHeight()!=height);++i)frames(1);
            frames(12);
        };
        // Optional fast iteration path; the registered CTest always exercises
        // the full editor + player sequence without this environment variable.
        capture("studio-1366");
        if(std::getenv("LINBLUX_PLAYER_MENU_ONLY")) {
            scene.createScript(InstanceClass::Script,nullptr,"MenuContinua.lua")->source=R"(
local npc=linblux.npc.create{name='NPC do menu',position=linblux.vec3(30,2.6,30),size=linblux.vec3(2,5,2),speed=2}
linblux.npc.move_to(npc,linblux.vec3(65,2.6,30))
linblux.scene.root:set_attribute('menu_npc',npc.id)
linblux.events.on('update',function() linblux.scene.root:set_attribute('menu_clock',linblux.time.now()) end)
)";
            for(auto size:{Vector2{1280,720},Vector2{1920,1080}}){
                resizeWindow(int(size.x),int(size.y));click("play");frames(6);auto& rt=EditorUITestAccess::runtime(ui);
                CHECK(rt.takeDamage(40,error));key("Escape");frames(16);CHECK(rt.state()==game::RuntimeState::Playing&&ui.gameMenuOpen());CHECK(!EditorUITestAccess::captured(ui));
                const auto clock=std::get<double>(rt.scene().workspace()->attributes().at("menu_clock"));
                const auto npcId=InstanceId(std::get<double>(rt.scene().workspace()->attributes().at("menu_npc")));
                const auto npcBefore=rt.scene().findPart(npcId)->position();frames(12);
                CHECK(std::get<double>(rt.scene().workspace()->attributes().at("menu_clock"))>clock);
                CHECK(Vector3Distance(rt.scene().findPart(npcId)->position(),npcBefore)>.02f);
                const auto vp=ui.frontend()->viewport(),panel=ui.frontend()->elementBounds("pause-panel");
                CHECK(std::fabs(panel.x-vp.x)<2);CHECK(panel.width<=vp.width*.94f);CHECK(panel.height>=vp.height-2);
                for(const char* id:{"resume","page-game","page-settings","page-controls","page-audio","page-video","page-about","leave","pause-close"}){
                    const auto r=ui.frontend()->elementBounds(id);CHECK(r.width>20&&r.height>20);CHECK(r.x>=vp.x&&r.y>=vp.y&&r.x+r.width<=vp.x+vp.width+2&&r.y+r.height<=vp.y+vp.height+2);
                }
                capture("side-menu-"+std::to_string(int(size.x)));
                if(size.x==1920){key("Escape");frames(14);CHECK(rt.state()==game::RuntimeState::Playing);key("shift+F5");CHECK(!ui.playing());continue;}
                const auto position=rt.position();event("keydown w");frames(5);event("keyup w");CHECK(Vector3Distance(position,rt.position())<.001f);
                click("page-settings");const auto mode=rt.settings().cameraMode;click("camera-next");CHECK(rt.settings().cameraMode!=mode);
                const float sensitivity=rt.settings().sensitivity;const auto slider=ui.frontend()->elementBounds("sensitivity-slider");clickAt({slider.x+slider.width*.7f,slider.y+slider.height/2});CHECK(rt.settings().sensitivity!=sensitivity);
                const bool shift=rt.settings().enableShiftLock;click("shift-lock");CHECK(rt.settings().enableShiftLock!=shift);
                click("page-audio");CHECK(ui.frontend()->elementBounds("master-volume").width>80);
                click("page-video");CHECK(ui.frontend()->elementBounds("fullscreen-value").width>40);
                click("page-controls");CHECK(ui.frontend()->elementText("controls-page").find("W A S D")!=std::string::npos);
                click("page-about");capture("side-help-"+std::to_string(int(size.x)));
                click("page-game");click("respawn");frames(4);CHECK(rt.health()==rt.settings().maxHealth);
                if(!ui.gameMenuOpen())key("Escape");frames(14);click("pause-close");frames(14);CHECK(rt.state()==game::RuntimeState::Playing&&!ui.gameMenuOpen());
                key("Escape");frames(14);click("leave");CHECK(!ui.playing());
            }
            std::printf("Player side menu: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);rlImGuiShutdown();CloseWindow();return failures?1:0;
        }
        if(std::getenv("LINBLUX_ITEM_CONTROLS_ONLY")) {
            resizeWindow(1280,720);
            scene.createScript(InstanceClass::Script,nullptr,"Coleta.lua")->source=R"(
local v=linblux.vec3;local p=linblux.player.position
local m=linblux.scene.create('model',{name='Coletável'})
linblux.scene.create('part',{parent=m,name='Peça',position=v(p.x+3,p.y,p.z),size=v(1,1,1)})
linblux.items.register('coleta',m);local item=linblux.items.spawn('coleta');item:set_attribute('carga',37)
)";
            click("play");frames(6);auto& runtime=EditorUITestAccess::runtime(ui);CHECK(ui.playing());CHECK(runtime.scripts().errorCount()==0);
            const auto world=runtime.inventory().worldItems();CHECK(world.size()==1);if(world.empty()){capture("item-start-failure");rlImGuiShutdown();CloseWindow();return 1;}
            const auto id=world.front();const auto start=runtime.position();auto view=ui.frontend()->viewport();
            event("mousemove "+std::to_string(int(view.x+view.width/2))+" "+std::to_string(int(view.y+view.height/2)));frames(2);
            CHECK(runtime.interactions().prompt().object==id);capture("interaction-ready");
            event("keydown e");frames(1);CHECK(runtime.interactions().prompt().progress>0&&runtime.interactions().prompt().progress<1);
            capture("interaction-progress");event("keyup e");CHECK(runtime.interactions().prompt().progress==0);
            std::string error;CHECK(runtime.teleportPlayer(Vector3Add(start,{20,0,0}),error));frames(2);CHECK(!runtime.interactions().prompt().object);
            CHECK(runtime.teleportPlayer(start,error));frames(2);event("keydown e");frames(60);event("keyup e");
            CHECK(runtime.inventory().list().size()==1);key("1");CHECK(runtime.inventory().equipped()==id);capture("hotbar-720");
            key("Delete");CHECK(runtime.inventory().list().empty());CHECK(runtime.inventory().info(id).state=="world");
            event("keydown e");frames(60);event("keyup e");CHECK(runtime.inventory().list().size()==1);CHECK(std::get<double>(runtime.scene().find(id)->attributes().at("carga"))==37);
            for(int i=0;i<9;++i){const auto extra=runtime.inventory().spawn("coleta");runtime.inventory().collect(extra);}frames(2);key("Next");CHECK(runtime.hotbarPage()==1);
            const auto slots=runtime.inventory().list();key("1");CHECK(runtime.inventory().equipped()==slots[9]);
            resizeWindow(1920,1080);if(runtime.state()==game::RuntimeState::Paused)key("Escape");frames(2);capture("hotbar-1080-page2");
            for(const auto item:runtime.inventory().list())runtime.inventory().remove(item);frames(2);CHECK(runtime.inventory().list().empty());capture("hotbar-empty");
            key("shift+F5");CHECK(!ui.playing());std::printf("Item controls UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);
            rlImGuiShutdown();CloseWindow();return failures?1:0;
        }
        if(std::getenv("LINBLUX_AUDIO_PANEL_ONLY")) {
            game::SetProjectAssetRoot(directory);std::filesystem::create_directories(std::string(directory)+"/assets");
            EditorActions actions(ui);const auto fixture=std::filesystem::path(__FILE__).parent_path()/"fixtures/audio/tone.wav";
            CHECK(actions.dispatch({"import-sound",fixture.string()}));CHECK(!scene.gameAssets().sounds.empty());
            const auto id=scene.gameAssets().sounds.front().id;
            click("cmd-show:audio-");frames(3);CHECK(ui.frontend()->elementBounds("audio-import").width>80);
            CHECK(ui.frontend()->elementBounds("music-background").width>80);
            clickVisible("audio-background-"+id);CHECK(scene.gameAssets().music.background==id);
            clickVisible("audio-playlist-"+id);CHECK(scene.gameAssets().music.playlist==std::vector<std::string>{id});
            ui.frontend()->scrollIntoView("audio-delete-"+id);frames(3);capture("audio-library");
            ui.frontend()->scrollIntoView("audio-import");frames(3);capture("audio-music-settings");
            clickVisible("music-playlist-remove-0");CHECK(scene.gameAssets().music.playlist.empty());
            CHECK(actions.dispatch({"music-background",""}));
            CHECK(projectio::save(scene,config.defaultProjectPath,error));Scene reopened;
            CHECK(projectio::load(reopened,config.defaultProjectPath,error));CHECK(reopened.gameAssets().sounds.front().id==id);
            CHECK(!actions.dispatch({"script-example","../../segredo.lua"}));
            CHECK(actions.dispatch({"script-example","musica_de_fundo.lua"}));
            CHECK(scene.selection()&&IsScriptClass(scene.selection()->instanceClass()));
            if(scene.selection()&&IsScriptClass(scene.selection()->instanceClass()))CHECK(!static_cast<Script*>(scene.selection())->source.empty());
            std::printf("Audio panel UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);
            rlImGuiShutdown();CloseWindow();return failures?1:0;
        }
        if(std::getenv("LINBLUX_EDITOR_PRECISION_ONLY")) {
            part->setTransform({{14,5,0},QuaternionIdentity(),{2,2,2}});
            auto* behind=scene.createPart(PartShape::Block,{14,5,-5},{2,2,2},"Peça atrás");behind->setAnchored(true);
            camera.setOrbit({14,5,0},30,0,0);click("cmd-tool-0");frames(3);
            auto view=ui.frontend()->viewport();const auto projected=camera.project(part->position(),{view.width,view.height});
            const Vector2 point{view.x+projected.x,view.y+projected.y};clickAt(point);CHECK(scene.selection()==part);
            event("keydown alt");clickAt(point);event("keyup alt");CHECK(scene.selection()==behind);
            event("keydown alt");clickAt(point);event("keyup alt");CHECK(scene.selection()==part);
            const bool edges=EditorUITestAccess::editorEdges(ui);key("F8");CHECK(EditorUITestAccess::editorEdges(ui)!=edges);
            key("F8");CHECK(EditorUITestAccess::editorEdges(ui)==edges);
            click("cmd-tool-1");frames(2);auto& gizmo=EditorUITestAccess::partGizmo(ui);
            const auto layout=gizmolayout::Build(*part,ToolMode::Move,camera,view,false,gizmo.visualSettings());
            const auto h=layout.moveHandle[1],d=layout.axisDirection[0];const auto original=part->position();
            const auto drag=[&](){event("mousemove "+std::to_string(int(view.x+h.x))+" "+std::to_string(int(view.y+h.y)));event("mousedown 1");
                CHECK(gizmo.dragging());event("mousemove "+std::to_string(int(view.x+h.x+d.x*30))+" "+std::to_string(int(view.y+h.y+d.y*30)));};
            drag();const auto raw=part->position();event("mouseup 1");key("ctrl+z");CHECK(NearlyEqual(part->position(),original));
            auto* target=scene.createPart(PartShape::Block,{raw.x-2.12f,5,0},{2,2,2},"Alvo de encaixe");target->setAnchored(true);frames(2);
            event("keydown ctrl");drag();CHECK(gizmo.surfaceContact().snapped);capture("precision-contact");
            CHECK(std::fabs(gizmomath::BoxSeparation({part->position(),part->rotation(),part->size()},
                {target->position(),target->rotation(),target->size()}))<.0001f);
            event("mouseup 1");event("keyup ctrl");key("ctrl+z");CHECK(NearlyEqual(part->position(),original));
            click("cmd-tool-0");auto* model=scene.group({part->id(),behind->id()},"Modelo selecionável",error);CHECK(model);scene.select(nullptr);frames(2);
            event("keydown shift");clickAt(point);event("keyup shift");CHECK(scene.selection()==model);capture("precision-model-selection");
            std::printf("Editor precision UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);
            rlImGuiShutdown();CloseWindow();return failures?1:0;
        }
        if(std::getenv("LINBLUX_MODELS_ONLY")) {
            auto* second=scene.createPart(PartShape::Block,{22,3,0},{4,6,1},"Segunda peça");second->setAnchored(true);frames(2);
            click("node-"+std::to_string(part->id()));event("keydown ctrl");click("node-"+std::to_string(second->id()));event("keyup ctrl");
            CHECK(scene.selectedIds().size()==2);key("ctrl+g");frames(2);
            auto* model=scene.selection();CHECK(model&&model->instanceClass()==InstanceClass::Model);
            if(!model||model->instanceClass()!=InstanceClass::Model){capture("model-group-failure");rlImGuiShutdown();CloseWindow();return 1;}
            const auto id=model->id();CHECK(part->parent()==model&&second->parent()==model);
            // Same visible gizmo, two real parts, one compound undo entry.
            click("cmd-tool-1");frames(2);const auto* proxy=EditorUITestAccess::modelProxy(ui);CHECK(proxy);
            const auto beforeDrag=projectio::toJson(scene);const auto undoBefore=EditorUITestAccess::undoCount(ui);
            if(proxy){const auto viewport=ui.frontend()->viewport();auto& gizmo=EditorUITestAccess::partGizmo(ui);
                const auto layout=gizmolayout::Build(*proxy,ToolMode::Move,camera,viewport,false,gizmo.visualSettings());
                const auto point=layout.moveHandle[1],direction=layout.axisDirection[0];
                event("mousemove "+std::to_string(int(point.x+viewport.x))+" "+std::to_string(int(point.y+viewport.y)));event("mousedown 1");
                CHECK(gizmo.dragging());event("mousemove "+std::to_string(int(point.x+viewport.x+direction.x*26))+" "+std::to_string(int(point.y+viewport.y+direction.y*26)));event("mouseup 1");
                CHECK(EditorUITestAccess::undoCount(ui)==undoBefore+1);CHECK(part->parent()==model);key("ctrl+z");
                CHECK(SameProjectValue(nlohmann::json::parse(projectio::toJson(scene)),nlohmann::json::parse(beforeDrag)));
            }

            clickVisible("prop-Model.Position.0");key("ctrl+a");event("type --clearmodifiers 20");key("Return");
            CHECK(NearlyEqual(part->position().x,16));CHECK(NearlyEqual(second->position().x,24));
            // The text field owns Ctrl+Z until focus returns to the editor.
            clickVisible("node-"+std::to_string(id));CHECK(!ui.frontend()->textFocused());
            key("ctrl+z");CHECK(NearlyEqual(part->position().x,14));key("ctrl+y");CHECK(NearlyEqual(part->position().x,16));
            clickVisible("prop-Model.Scale");key("ctrl+a");event("type --clearmodifiers 2");key("Return");CHECK(NearlyEqual(part->size().x,8));
            clickVisible("prop-Model.Rotation.1");key("ctrl+a");event("type --clearmodifiers 90");key("Return");CHECK(NearlyEqual(part->position().z,8,.01f));
            const auto original=part->position();clickVisible("prop-Pivot.Position.0");key("ctrl+a");event("type --clearmodifiers 30");key("Return");CHECK(NearlyEqual(part->position(),original));
            clickVisible("node-"+std::to_string(part->id()));clickVisible("property-attachment-");
            auto* anchor=scene.selection();CHECK(anchor&&anchor->instanceClass()==InstanceClass::Attachment);
            if(!anchor||anchor->instanceClass()!=InstanceClass::Attachment){capture("attachment-create-failure");rlImGuiShutdown();CloseWindow();return 1;}
            const auto anchorId=anchor->id();CHECK(anchor->parent()==part);
            clickVisible("prop-Attachment.Position.2");key("ctrl+a");event("type --clearmodifiers -- -2");key("Return");
            CHECK(NearlyEqual(static_cast<Attachment*>(anchor)->localFrame().position.z,-2));
            ModelFrame anchorWorld;CHECK(scene.attachmentFrame(anchorId,anchorWorld,error));
            const auto anchorPosition=anchorWorld.position;
            auto* partner=scene.createAttachment(second,"Encaixe");CHECK(partner);frames(2);
            clickVisible("prop-Attachment.Link");key("Down");key("Return");frames(2);
            CHECK(static_cast<Attachment*>(anchor)->jointTarget()==partner->id());
            ModelFrame partnerWorld;CHECK(scene.attachmentFrame(partner->id(),partnerWorld,error));CHECK(NearlyEqual(partnerWorld.position,anchorPosition));
            capture("attachment-properties");
            CHECK(projectio::save(scene,config.defaultProjectPath,error));Scene loaded;CHECK(projectio::load(loaded,config.defaultProjectPath,error));
            CHECK(loaded.attachmentFrame(anchorId,anchorWorld,error));CHECK(NearlyEqual(anchorWorld.position,anchorPosition));
            clickVisible("node-"+std::to_string(id));
            ModelFrame frame;CHECK(loaded.modelFrame(id,frame,error));CHECK(NearlyEqual(frame.position.x,30));CHECK(frame.scale==2);
            clickVisible("prop-Item.Prototype");frames(2);CHECK(std::get<bool>(model->attributes().at("_linblux_item_prototype")));
            clickVisible("prop-Item.Type");key("ctrl+a");event("type --clearmodifiers visual_item");key("Return");
            CHECK(std::get<std::string>(model->attributes().at("item_type"))=="visual_item");
            CHECK(projectio::save(scene,config.defaultProjectPath,error));
            capture("model-properties");
            scene.createScript(InstanceClass::Script,nullptr,"ModeloPlay.lua")->source="local m=linblux.scene.find("+std::to_string(id)+");assert(m.scale==2);local item=linblux.items.spawn('visual_item');linblux.items.collect(item);linblux.items.equip(item);assert(linblux.items.equipped().id==item.id);linblux.scene.root:set_attribute('model_play',true)";
            click("play");frames(8);CHECK(ui.playing());CHECK(EditorUITestAccess::runtime(ui).scripts().errorCount()==0);
            CHECK(EditorUITestAccess::runtime(ui).scene().workspace()->attributes().count("model_play"));
            CHECK(EditorUITestAccess::runtime(ui).inventory().prototype("visual_item")==id);
            key("shift+F5");frames(2);CHECK(!ui.playing());CHECK(NearlyEqual(part->position(),original));
            clickVisible("node-"+std::to_string(id));key("ctrl+shift+g");frames(2);
            CHECK(scene.find(id)==nullptr);CHECK(part->parent()==scene.workspace());CHECK(NearlyEqual(part->position(),original));
            CHECK(scene.attachmentFrame(anchorId,anchorWorld,error));CHECK(NearlyEqual(anchorWorld.position,anchorPosition));
            CHECK(scene.validate(error));std::printf("Model UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);
            rlImGuiShutdown();CloseWindow();return failures?1:0;
        }
        if(std::getenv("LINBLUX_BODY_PROPERTIES_ONLY")) {
            auto editBody=[&](const std::string& field,const std::string& value){
                click("property-search");key("ctrl+a");event("type --clearmodifiers "+field);key("Return");
                click("prop-"+field);key("ctrl+a");event("type --clearmodifiers -- "+value);key("Return");frames();
            };
            editBody("Density","2.5");CHECK(NearlyEqual(part->bodyProperties().density,2.5f));
            editBody("Friction","0.25");CHECK(NearlyEqual(part->bodyProperties().friction,.25f));
            editBody("Restitution","0.75");CHECK(NearlyEqual(part->bodyProperties().restitution,.75f));
            editBody("LinearDamping","0.4");CHECK(NearlyEqual(part->bodyProperties().linearDamping,.4f));
            editBody("AngularDamping","0.6");CHECK(NearlyEqual(part->bodyProperties().angularDamping,.6f));
            editBody("Density","-1");CHECK(NearlyEqual(part->bodyProperties().density,2.5f));
            click("property-search");key("ctrl+a");key("BackSpace");key("Return");capture("body-properties");
            const auto authored=projectio::toJson(scene);CHECK(projectio::save(scene,config.defaultProjectPath,error));
            Scene reloaded;CHECK(projectio::load(reloaded,config.defaultProjectPath,error));
            CHECK(SameProjectValue(nlohmann::json::parse(projectio::toJson(reloaded)),nlohmann::json::parse(authored)));
            scene.createScript(InstanceClass::Script,nullptr,"CorpoEditor.lua")->source=R"(
local part=linblux.scene.find("Wall")
assert(math.abs(part.body.density-2.5)<0.001 and math.abs(part.body.restitution-0.75)<0.001)
part.anchored=false
assert(math.abs(part.mass-60)<0.001)
part:apply_impulse(linblux.vec3(60,0,0))
assert(math.abs(part.velocity.x-1)<0.001)
)";
            click("play");frames(8);auto& runtime=EditorUITestAccess::runtime(ui);CHECK(ui.playing());CHECK(runtime.scripts().errorCount()==0);
            click("stop");frames();CHECK(!ui.playing());CHECK(part->anchored());
            CHECK(NearlyEqual(part->bodyProperties().density,2.5f));
            std::printf("Body Properties UI: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);
            rlImGuiShutdown();CloseWindow();return failures?1:0;
        }
        // A separate short path exercises real Explorer/Properties controls
        // with X11 events, including dropdowns and independent child deletion.
        if(std::getenv("LINBLUX_SCRIPT_INPUT_ONLY")) {
            scene.createScript(InstanceClass::Script,nullptr,"EntradaReal.lua")->source=R"(
assert(linblux.storage.status().persistent)
linblux.scene.root:set_attribute("stored_presses",linblux.storage.get("tecla_e",0))
linblux.camera.configure({locked=true, fov=85})
linblux.camera.set_pose({position=linblux.vec3(15,10,15),target=linblux.vec3(0,2,0),fov=95})
linblux.events.on("key_pressed", function(key)
    if key == "e" then
        local root = linblux.scene.root
        root:set_attribute("real_presses", (root:get_attribute("real_presses") or 0)+1)
        linblux.storage.set("tecla_e",root:get_attribute("real_presses"))
        assert(linblux.input.key_down("e"))
        if root:get_attribute("real_presses") == 2 then linblux.camera.release() end
    end
end)
linblux.events.on("mouse_pressed", function(button)
    if button == "left" then
        local root, p = linblux.scene.root, linblux.input.pointer()
        assert(p.x >= 0 and p.y >= 0 and p.x < 1366 and p.y < 768)
        root:set_attribute("real_clicks", (root:get_attribute("real_clicks") or 0)+1)
    end
end)
)";
            const auto original=projectio::toJson(scene);CHECK(projectio::save(scene,config.defaultProjectPath,error));click("play");frames(6);
            auto& rt=EditorUITestAccess::runtime(ui);CHECK(rt.state()==game::RuntimeState::Playing);
            CHECK(rt.playerCamera().scripted());CHECK(rt.playerCamera().camera().fovy==95);
            CHECK(!EditorUITestAccess::captured(ui));CHECK(!IsCursorHidden());
            const auto viewport=ui.frontend()->viewport();clickAt({viewport.x+viewport.width*.5f,viewport.y+viewport.height*.5f});
            event("mousedown 3");CHECK(!EditorUITestAccess::captured(ui));event("mouseup 3");
            CHECK(rt.scene().workspace()->attributes().count("real_clicks"));
            event("keydown e");frames(3);CHECK(rt.scriptInput().keys[KEY_E]);
            auto presses=[&]{const auto& attrs=rt.scene().workspace()->attributes();return attrs.count("real_presses")?std::get<double>(attrs.at("real_presses")):0.;};
            CHECK(presses()==1);event("keyup e");CHECK(!rt.scriptInput().keys[KEY_E]);
            key("Escape");CHECK(rt.state()==game::RuntimeState::Paused);
            key("e");CHECK(presses()==1);key("Escape");CHECK(rt.state()==game::RuntimeState::Playing);
            key("e");CHECK(presses()==2);
            CHECK(!rt.playerCamera().scripted());CHECK(rt.playerCamera().camera().fovy==85);
            for(const auto& message:rt.scripts().messages()){if(message.error)std::fprintf(stderr,"Input Lua: %s\n",message.text.c_str());CHECK(!message.error);}
            key("shift+F5");CHECK(!rt.active());CHECK(projectio::toJson(scene)==original);
            click("play");frames(4);CHECK(rt.active());
            CHECK(std::get<double>(rt.scene().workspace()->attributes().at("stored_presses"))==2);
            key("shift+F5");CHECK(!rt.active());CHECK(projectio::toJson(scene)==original);
        } else if(std::getenv("LOBLOX_MANIPULATION_ONLY")) {
            BuildManipulationFixture(scene);physics.syncScene(scene);scene.select(part);
            for(auto* node:scene.parts())if(node->instanceClass()==InstanceClass::SpawnLocation) {
                node->setPosition({-20,.5f,20});physics.applyChanges(node);
            }
            part->setTransform({{0,5,0},QuatFromEulerDegrees({0,37,0}),{4,2,6}});physics.applyChanges(part);
            EditorActions actions(ui);CHECK(actions.dispatch({"snap","false"}));
            camera.setOrbit(part->position(),19,.75f,.48f);frames(5);
            const auto baseline=part->transform();
            click("tab-View");click("gizmo-size");key("Down");key("Return");
            CHECK(NearlyEqual(EditorUITestAccess::partGizmo(ui).pixelScale(),1.25f));
            click("tab-Home");click("cmd-tool-1");
            click("transform-space");key("Down");key("Return");
            CHECK(EditorUITestAccess::partGizmo(ui).localSpace());
            for(int axis=0;axis<3;++axis)for(int side:{-1,1}) {
                auto& gizmo=EditorUITestAccess::partGizmo(ui);const auto viewport=ui.frontend()->viewport();
                const auto layout=gizmolayout::Build(*part,ToolMode::Move,camera,viewport,true,gizmo.visualSettings());
                const auto point=layout.moveHandle[axis*2+(side>0)];const auto direction=Vector2Scale(layout.axisDirection[axis],float(side));
                const auto eye=camera.position();const auto count=EditorUITestAccess::undoCount(ui);
                event("mousemove "+std::to_string(int(point.x+viewport.x))+" "+std::to_string(int(point.y+viewport.y)));
                event("mousedown 1");CHECK(gizmo.activeHandle()==gizmomath::MoveHandleId(axis,side));
                event("mousemove "+std::to_string(int(point.x+viewport.x+direction.x*23))+" "+std::to_string(int(point.y+viewport.y+direction.y*23)));
                CHECK(NearlyEqual(camera.position(),eye));CHECK(Vector3Distance(part->position(),baseline.position)>.01f);
                const auto delta=Vector3Subtract(part->position(),baseline.position),basis=gizmomath::Axis(axis,baseline.rotation,true);
                CHECK(Vector3Length(Vector3CrossProduct(delta,basis))<.001f);
                CHECK(gizmo.activeHandle()==gizmomath::MoveHandleId(axis,side));event("mouseup 1");
                CHECK(EditorUITestAccess::undoCount(ui)==count+1);key("ctrl+z");
                CHECK(NearlyEqual(part->position(),baseline.position,.001f));
            }
            CHECK(actions.dispatch({"transform-space","world"}));frames();
            for(int mode=1;mode<=3;++mode) {
                click("tab-Home");click("cmd-tool-"+std::to_string(mode));frames();
                auto& gizmo=EditorUITestAccess::partGizmo(ui);
                const auto viewport=ui.frontend()->viewport();
                const auto layout=gizmolayout::Build(*part,static_cast<ToolMode>(mode),camera,viewport,gizmo.localSpace(),gizmo.visualSettings());
                const Vector2 point=mode==1?layout.moveHandle[1]:mode==2?layout.scaleHandle[1]:layout.rotateMarker[1];
                const auto eye=camera.position();const auto count=EditorUITestAccess::undoCount(ui);
                event("mousemove "+std::to_string(int(point.x+viewport.x))+" "+std::to_string(int(point.y+viewport.y)));
                capture(mode==1?"move-six-arrows":mode==2?"scale-local-face":"rotate-rings");
                event("mousedown 1");CHECK(EditorUITestAccess::partDragging(ui));
                const auto direction=layout.axisDirection[0];
                for(int step=1;step<=4;++step)event("mousemove "+std::to_string(int(point.x+viewport.x+direction.x*step*9))+" "+std::to_string(int(point.y+viewport.y+direction.y*step*9)));
                CHECK(NearlyEqual(camera.position(),eye));CHECK(scene.selection()==part);
                if(mode==2) {
                    const auto axis=gizmomath::Axis(0,baseline.rotation,true);
                    const auto before=Vector3Subtract(baseline.position,Vector3Scale(axis,baseline.size.x*.5f));
                    const auto after=Vector3Subtract(part->position(),Vector3Scale(axis,part->size().x*.5f));
                    CHECK(Vector3Distance(before,after)<.001f);
                }
                capture("drag-"+std::to_string(mode));event("mouseup 1");
                CHECK(!EditorUITestAccess::partDragging(ui));CHECK(EditorUITestAccess::undoCount(ui)==count+1);
                key("ctrl+z");CHECK(NearlyEqual(part->position(),baseline.position,.001f));CHECK(NearlyEqual(part->size(),baseline.size,.001f));
                CHECK(NearlyEqual(part->rotation(),baseline.rotation,.001f));
            }
            click("tab-View");click("show-collision-shapes");event("mousemove 235 650");capture("collision-shapes");
            click("tab-Home");click("cmd-tool-1");
            for(const Vector2 size:{Vector2{1280,720},Vector2{1366,768},Vector2{1600,900}}) {
                resizeWindow(int(size.x),int(size.y));capture("move-"+std::to_string(int(size.x)));
            }
            CHECK(projectio::save(scene,config.defaultProjectPath,error));Scene loaded;CHECK(projectio::load(loaded,config.defaultProjectPath,error));
            const auto expectedJson=nlohmann::json::parse(projectio::toJson(scene)),loadedJson=nlohmann::json::parse(projectio::toJson(loaded));
            CHECK(SameProjectValue(expectedJson,loadedJson));
            CHECK(loaded.parts().size()==scene.parts().size());CHECK(loaded.objects().size()==scene.objects().size());
            for(const auto* node:scene.parts()) {
                const auto* restored=loaded.findPart(node->id());CHECK(restored!=nullptr);if(!restored)continue;
                CHECK(NearlyEqual(restored->position(),node->position(),.0001f));CHECK(NearlyEqual(restored->size(),node->size(),.0001f));
                CHECK(NearlyEqual(restored->rotation(),node->rotation(),.0001f));CHECK(restored->parent()->id()==node->parent()->id());
            }
            const auto original=projectio::toJson(scene);click("play");frames(12);CHECK(EditorUITestAccess::runtime(ui).active());
            key("shift+F5");CHECK(!EditorUITestAccess::runtime(ui).active());CHECK(projectio::toJson(scene)==original);
            CHECK(NearlyEqual(part->position(),baseline.position,.001f));
        } else if(std::getenv("LOBLOX_MOUSE_LOCK_ONLY")) {
            const auto editorEye=camera.position();const auto original=projectio::toJson(scene);
            const bool originalCursorFlag=(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_NoMouseCursorChange)!=0;
            click("play");auto& rt=EditorUITestAccess::runtime(ui);frames(12);
            event("keydown Shift_L");CHECK(rt.shiftLockActive());CHECK(EditorUITestAccess::captured(ui));event("keyup Shift_L");
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);frames(3);
            CHECK(EditorUITestAccess::captured(ui));CHECK(IsCursorHidden());
            CHECK(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_NoMouseCursorChange);CHECK(!ImGui::GetIO().MouseDrawCursor);
            auto cursorCheck=[&] {
                const auto state=ui.gameplayCursor();const auto viewport=ui.frontend()->viewport();
                const auto cursor=ui.frontend()->elementBounds("mouse-lock-cursor");
                CHECK(state.captured&&state.visual==GameCursorVisual::MouseLock&&IsCursorHidden());
                CHECK(cursor.width==28&&cursor.height==28);
                CHECK(std::fabs(cursor.x+14-state.position.x)<1.5f&&std::fabs(cursor.y+14-state.position.y)<1.5f);
                CHECK(std::fabs(state.position.x-(viewport.x+viewport.width*.5f))<1.5f);
                CHECK(std::fabs(state.position.y-(viewport.y+viewport.height*.5f))<1.5f);
            };
            cursorCheck();
            const auto yaw=rt.playerCamera().yaw(),pitch=rt.playerCamera().pitch();
            event("mousemove_relative -- 40 15");
            CHECK(std::fabs(std::remainder(rt.playerCamera().yaw()-yaw,2*PI))>.02f);
            CHECK(std::fabs(rt.playerCamera().pitch()-pitch)>.01f);
            // Capture every actual rendered frame and state, rather than one
            // still that says nothing about temporal/body-camera jitter.
            FILE* csv=std::fopen((std::string(directory)+"/mouse-lock.csv").c_str(),"w");CHECK(csv);
            if(csv)std::fprintf(csv,"frame,time,physical_y,render_y,camera_x,camera_y,camera_z,pivot_y,yaw,pitch,body_yaw,cursor_x,cursor_y,captured\n");
            int frameIndex=0;const double started=GetTime();const auto startRoot=rt.position();bool sawJump=false,sawFall=false,sawMovement=false;
            observeFrame=[&] {
                cursorCheck();const auto root=rt.renderCharacterRoot();const auto view=rt.playerCamera().camera();const auto pointer=ui.gameplayCursor();
                const auto front=Vector3RotateByQuaternion({0,0,-1},root.rotation);
                CHECK(Vector3Distance(front,rt.playerCamera().moveDirection({0,1}))<.0001f);
                CHECK(std::fabs(front.y)<.0001f);
                sawJump|=rt.humanoidState()==game::HumanoidState::Jumping;sawFall|=rt.humanoidState()==game::HumanoidState::Freefall;
                sawMovement|=std::hypot(root.position.x-startRoot.x,root.position.z-startRoot.z)>1;
                if(csv)std::fprintf(csv,"%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%d\n",frameIndex,GetTime()-started,
                    rt.position().y,root.position.y,view.position.x,view.position.y,view.position.z,rt.playerCamera().pivot().y,
                    rt.playerCamera().yaw(),rt.playerCamera().pitch(),std::atan2(-front.x,-front.z),pointer.position.x,pointer.position.y,pointer.captured);
                char name[64];std::snprintf(name,sizeof(name),"/mouse-lock-%03d.png",frameIndex++);TakeScreenshot((std::string(directory)+name).c_str());
            };
            event("keydown w");const char* directions[]={"w","a","s","d"};
            for(int i=0;i<24;++i) {
                if(i>0&&i%6==0){event(std::string("keyup ")+directions[i/6-1]);event(std::string("keydown ")+directions[i/6]);}
                if(i==4||i==16)key("space");
                event(std::string("mousemove_relative -- 90 ")+(i%2?"-8":"8"));
            }
            event("keyup d");observeFrame={};if(csv)std::fclose(csv);CHECK(sawJump&&sawFall&&sawMovement);
            key("Escape");CHECK(rt.state()==game::RuntimeState::Paused);CHECK(!ui.gameplayCursor().captured);
            CHECK(!IsCursorHidden());CHECK(ui.frontend()->elementBounds("mouse-lock-cursor").width==0);CHECK(rt.shiftLockRequested());
            click("respawn");key("Escape");frames(3);cursorCheck();
            const auto beforeFullscreen=rt.playerCamera().yaw();key("F11");frames(8);cursorCheck();
            CHECK(std::fabs(std::remainder(rt.playerCamera().yaw()-beforeFullscreen,2*PI))<.001f);capture("mouse-lock-fullscreen");
            key("F11");frames(8);cursorCheck();
            rt.settings().cameraMode=game::CameraMode::FirstPerson;frames(3);CHECK(rt.playerCamera().firstPerson());CHECK(rt.playerCamera().shoulderOffset()==0);cursorCheck();
            rt.settings().cameraMode=game::CameraMode::Classic;frames(3);cursorCheck();
            event("windowfocus "+std::to_string(InputTestRootWindow()));CHECK(rt.state()==game::RuntimeState::Paused);
            CHECK(!ui.gameplayCursor().captured);CHECK(!IsCursorHidden());CHECK(ui.frontend()->elementBounds("mouse-lock-cursor").width==0);
            event("windowfocus --sync "+windowId);key("Escape");frames(3);cursorCheck();
            event("keydown Shift_L");CHECK(!rt.shiftLockActive());CHECK(!EditorUITestAccess::captured(ui));event("keyup Shift_L");
            CHECK(!IsCursorHidden());CHECK(ui.frontend()->elementBounds("mouse-lock-cursor").width==0);
            key("Shift_L");cursorCheck();key("shift+F5");CHECK(!rt.active());CHECK(!ui.gameplayCursor().captured);CHECK(!IsCursorHidden());
            CHECK(((ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_NoMouseCursorChange)!=0)==originalCursorFlag);
            CHECK(ui.frontend()->elementBounds("mouse-lock-cursor").width==0);CHECK(NearlyEqual(camera.position(),editorEye));CHECK(projectio::toJson(scene)==original);
        } else if(std::getenv("LOBLOX_RIG_DECALS_ONLY")) {
            std::filesystem::create_directories("assets");
            Image image=GenImageColor(32,32,MAGENTA);CHECK(ExportImage(image,"assets/front.png"));UnloadImage(image);
            image=GenImageColor(32,32,SKYBLUE);CHECK(ExportImage(image,"assets/back.png"));UnloadImage(image);
            const auto rigId=scene.gameAssets().createR6().id;
            auto rig=[&](){return scene.gameAssets().findRig(rigId);};
            click("tree-search");event("type --clearmodifiers R6");key("Return");
            click("rig-"+rigId);click("rig-body-"+rigId+"-0");
            click("cmd-rig-decal-add-");CHECK(rig()->bodies[game::Torso].decals.size()==1);
            if(rig()->bodies[game::Torso].decals.empty()){capture("rig-add-failure");rlImGuiShutdown();CloseWindow();return 1;}
            const auto first=rig()->bodies[game::Torso].decals.front().id;
            click("rig-prop-Name");key("ctrl+a");event("type --clearmodifiers DecalFront");key("Return");
            CHECK(rig()->findDecal(first)->name=="DecalFront");
            // O seletor visual aplica pela miniatura/nome; o antigo select de
            // IDs técnicos não existe mais.
            clickVisible("image-query-front.png");frames(2);capture("rig-image-open");
            CHECK(rig()->findDecal(first)->surface.image=="assets/front.png");
            clickVisible("cmd-rig-decal-add-");CHECK(rig()->bodies[game::Torso].decals.size()==2);
            const auto second=rig()->bodies[game::Torso].decals.back().id;
            clickVisible("rig-prop-Name");key("ctrl+a");event("type --clearmodifiers DecalBack");key("Return");
            clickVisible("rig-prop-Face");key("Down");key("Return");
            CHECK(rig()->findDecal(second)->surface.face==1);
            clickVisible("image-query-back.png");frames(2);
            CHECK(rig()->findDecal(second)->surface.image=="assets/back.png");
            clickVisible("rig-prop-Transparency");key("ctrl+a");event("type --clearmodifiers 0.25");key("Return");
            CHECK(NearlyEqual(rig()->findDecal(second)->surface.transparency,.25f));
            capture("rig-decals-properties");
            // Import through the existing dialog; choose the imported image
            // for this child without replacing its sibling or face.
            image=GenImageColor(32,32,ORANGE);CHECK(ExportImage(image,"external.png"));UnloadImage(image);
            clickVisible("cmd-import-dialog-");
            click("dialog-value");SetClipboardText((std::string(directory)+"/external.png").c_str());key("ctrl+a");key("ctrl+v");key("Return");
            auto imported=game::ListImages(game::AssetsDir());
            CHECK(imported.size()==3);
            std::string importedImage;for(const auto& item:imported)if(item.filename!="front.png"&&item.filename!="back.png")importedImage=item.filename;
            CHECK(!importedImage.empty());
            if(importedImage.empty()) {std::fprintf(stderr,"Import dialog error: %s\n",ui.frontend()->elementText("dialog-error").c_str());capture("rig-import-error");key("Escape");}
            else {
            clickVisible("image-query-"+importedImage);frames(2);
            CHECK(rig()->findDecal(second)->surface.image==game::ImageAssetReference(importedImage));
            // Nome amigável é editável e persiste em assets/image-names.json.
            clickVisible("cmd-show:assets-");frames(3);
            clickVisible("cmd-rename-image-"+importedImage);
            CHECK(ui.frontend()->elementText("dialog-title")=="Renomear imagem");
            click("dialog-value");key("ctrl+a");event("type --clearmodifiers Texturaimportada");
            click("dialog-confirm");frames(2);
            CHECK(game::ImageDisplayName(game::AssetsDir(),importedImage)=="Texturaimportada");
            CHECK(ui.frontend()->elementBounds("image-query-"+importedImage).width>0);
            // Excluir imagem em uso é recusado com relatório de dependências.
            clickVisible("cmd-delete-image-"+importedImage);
            CHECK(ui.frontend()->elementText("dialog-description").find("em uso")!=std::string::npos);
            key("Escape");frames(2);
            CHECK(std::filesystem::exists(std::filesystem::path("assets")/importedImage));
            }
            CHECK(rig()->findDecal(first)->surface.image=="assets/front.png");CHECK(rig()->findDecal(second)->surface.face==1);
            CHECK(projectio::save(scene,config.defaultProjectPath,error));
            Scene roundTrip;CHECK(projectio::load(roundTrip,config.defaultProjectPath,error));
            const auto* saved=roundTrip.gameAssets().findRig(rigId);CHECK(saved&&saved->bodies[game::Torso].decals.size()==2);
            click("rig-decal-"+rigId+"-"+first);key("F2");key("ctrl+a");event("type --clearmodifiers FrontRenamed");key("Return");
            CHECK(rig()->findDecal(first)->name=="FrontRenamed");
            click("rig-decal-"+rigId+"-"+first);key("Delete");
            CHECK(rig()->findDecal(first)==nullptr);CHECK(rig()->findDecal(second)!=nullptr);
            CHECK(ui.frontend()->elementBounds("rig-decal-"+rigId+"-"+first).width==0);
            CHECK(ui.frontend()->elementBounds("rig-decal-"+rigId+"-"+second).width>0);
            CHECK(projectio::save(scene,config.defaultProjectPath,error));
            CHECK(projectio::load(roundTrip,config.defaultProjectPath,error));
            CHECK(roundTrip.gameAssets().findRig(rigId)->bodies[game::Torso].decals.size()==1);
            // Remover a imagem aplicada mantém o adesivo; excluir o arquivo é
            // uma operação separada e só acontece sem referências.
            click("rig-decal-"+rigId+"-"+second);frames(2);
            clickVisible("cmd-clear-image-rig");frames(2);
            CHECK(rig()->findDecal(second)!=nullptr);
            CHECK(rig()->findDecal(second)->surface.image.empty());
            CHECK(std::filesystem::exists(std::filesystem::path("assets")/importedImage));
            // A antiga imagem direta do membro aparece no registro e pode ser
            // localizada e removida sem tocar no rig ou no arquivo importado.
            rig()->bodies[game::Head].image=game::ImageAssetReference(importedImage);frames(2);
            clickVisible("cmd-delete-image-"+importedImage);
            CHECK(ui.frontend()->elementText("appearance-controls").find("Cabeça")!=std::string::npos);
            capture("image-references");click("usage-image-use-0");frames(2);
            CHECK(ui.frontend()->elementBounds("rig-image-"+rigId+"-1").width>0);
            clickVisible("cmd-clear-image-rig-body");CHECK(rig()->bodies[game::Head].image.empty());
            CHECK(rig()->findDecal(second)!=nullptr);
            clickVisible("cmd-delete-image-"+importedImage);
            CHECK(ui.frontend()->elementText("dialog-description").find("Nenhuma referência")!=std::string::npos);
            click("dialog-confirm");frames(3);
            CHECK(!std::filesystem::exists(std::filesystem::path("assets")/importedImage));
            CHECK(rig()->findDecal(second)!=nullptr);
            // Componente Surface independente: selecionar pelo Explorador,
            // aplicar imagem, editar propriedades, salvar e limpar só o vínculo.
            click("tree-search");key("ctrl+a");key("BackSpace");key("Return");
            auto* surface=scene.createSurface(InstanceClass::Decal,part,"Placa original");CHECK(surface);frames(2);
            clickVisible("node-"+std::to_string(surface->id()));
            clickVisible("image-query-surface-front.png");CHECK(surface->properties().image=="assets/front.png");
            clickVisible("prop-Surface.Transparency");key("ctrl+a");event("type --clearmodifiers 0.5");key("Return");
            CHECK(NearlyEqual(surface->properties().transparency,.5f));
            CHECK(projectio::save(scene,config.defaultProjectPath,error));CHECK(projectio::load(roundTrip,config.defaultProjectPath,error));
            auto* loadedSurface=roundTrip.find(surface->id());CHECK(loadedSurface&&IsSurfaceClass(loadedSurface->instanceClass()));
            if(!loadedSurface){std::fprintf(stderr,"Surface persistence: %s\n",error.c_str());capture("surface-save-failure");rlImGuiShutdown();CloseWindow();return 1;}
            CHECK(static_cast<Surface*>(loadedSurface)->properties().image=="assets/front.png");
            capture("surface-properties");clickVisible("cmd-clear-image-surface");CHECK(surface->properties().image.empty());
            CHECK(scene.find(surface->id())==surface);
            CHECK(scene.validate(error));capture("rig-decals-deleted");
        } else {
        const auto cameraStart=camera.camera();
        CHECK(camera.navigationMode()==EditorCamera::NavigationMode::FPS);
        auto checkStudioLook=[&](bool selected){
            scene.select(selected?part:nullptr);frames();
            const auto view=ui.frontend()->viewport();
            const auto eye=camera.position(),target=camera.target();
            event("mousemove "+std::to_string(int(view.x+view.width*.5f))+" "+std::to_string(int(view.y+view.height*.4f)));
            event("mousedown 3");event("mousemove_relative -- 23 -13");event("mouseup 3");
            CHECK(NearlyEqual(camera.position(),eye,.0001f));
            CHECK(Vector3Distance(camera.target(),target)>.01f);
        };
        checkStudioLook(true);checkStudioLook(false);scene.select(part);
        const auto fpsEye=camera.position();click("editor-camera-focus");
        CHECK(camera.navigationMode()==EditorCamera::NavigationMode::Focus);
        CHECK(NearlyEqual(camera.position(),fpsEye,.0001f));
        const auto pivot=camera.target(),orbitEye=camera.position();
        const auto orbitView=ui.frontend()->viewport();
        event("mousemove "+std::to_string(int(orbitView.x+50))+" "+std::to_string(int(orbitView.y+90)));
        event("mousedown 3");event("mousemove_relative -- 23 13");event("mouseup 3");
        CHECK(Vector3Distance(camera.position(),orbitEye)>.01f);CHECK(NearlyEqual(camera.target(),pivot));
        const auto focusedEye=camera.position();click("editor-camera-fps");CHECK(NearlyEqual(camera.position(),focusedEye,.0001f));
        camera.setOrbit(cameraStart.target,Vector3Distance(cameraStart.position,cameraStart.target),.75f,.48f);frames();
        if(!std::getenv("LOBLOX_PLAYER_UI_ONLY")) {
        CHECK(projectio::toJson(scene)==untouched);
        CHECK(ui.frontend()->viewport().x>0);CHECK(scene.selection()==part);
        // RML owns the pointer in the editor: hovering a panel must not leave
        // stale ImGui viewport focus driving WASD into the 3D camera.
        auto* viewportWindow=ImGui::FindWindowByName(loblox::i18n::Label("Viewport"));
        CHECK(viewportWindow!=nullptr);
        if(viewportWindow)ImGui::FocusWindow(viewportWindow);
        frames(3);
        const auto savedTarget=camera.target();
        const auto savedDistance=camera.distance();
        const auto savedPitch=camera.pitch();
        const auto view=ui.frontend()->viewport();
        event("mousemove "+std::to_string(int(view.x+view.width*.5f))+" "+std::to_string(int(view.y+view.height*.5f)));
        const auto beforeViewportW=camera.target();
        event("keydown w");frames(5);event("keyup w");frames(2);
        CHECK(Vector3Distance(camera.target(),beforeViewportW)>0.01f);
        const auto title=ui.frontend()->elementBounds("property-title");
        CHECK(title.width>0);
        event("mousemove "+std::to_string(int(title.x+title.width*.5f))+" "+std::to_string(int(title.y+title.height*.5f)));
        const auto blockedTarget=camera.target();
        event("keydown w");frames(5);event("keyup w");frames(2);
        CHECK(NearlyEqual(camera.target(),blockedTarget));
        camera.setOrbit(savedTarget,savedDistance,0.75f,savedPitch);
        frames(2);
        EditorActions actions(ui);
        click("cmd-tool-1");CHECK(EditorUITestAccess::tool(ui)==ToolMode::Move);
        const bool snap=EditorUITestAccess::snap(ui);click("snap");CHECK(EditorUITestAccess::snap(ui)!=snap);click("snap");CHECK(EditorUITestAccess::snap(ui)==snap);
        // Real field editing must not route M/R/S/Delete to scene tools.
        click("prop-Name");key("ctrl+a");event("type --clearmodifiers --delay 20 PreciseWall");key("Return");CHECK(part->name()=="PreciseWall");
        click("cmd-tool-2");CHECK(EditorUITestAccess::tool(ui)==ToolMode::Scale);
        click("cmd-tool-3");CHECK(EditorUITestAccess::tool(ui)==ToolMode::Rotate);
        // Hierarchy drag moves the real Parent, preserving ID and collision.
        const auto partId=part->id();auto from=ui.frontend()->elementBounds("node-"+std::to_string(partId));auto to=ui.frontend()->elementBounds("node-"+std::to_string(folder->id()));
        event("mousemove "+std::to_string(int(from.x+from.width/2))+" "+std::to_string(int(from.y+from.height/2)));event("mousedown 1");
        event("mousemove "+std::to_string(int(to.x+to.width/2))+" "+std::to_string(int(to.y+to.height/2)));event("mouseup 1");
        CHECK(part->parent()==folder);CHECK(scene.find(partId)==part);capture("explorer-properties");
        // Common panels are RML; code/timeline/rig keep their proven native
        // editing controls inside the single RML split region.
        click("node-"+std::to_string(scene.lighting()->id()));capture("properties-lighting");
        click("prop-Brightness");key("ctrl+a");event("type --clearmodifiers 1.25");key("Return");CHECK(NearlyEqual(scene.lighting()->brightness(),1.25f));
        click("property-search");event("type --clearmodifiers SunColor");key("Return");click("color-SunColor");capture("lighting-color-dialog");
        SetClipboardText("#88aadd");key("ctrl+a");key("ctrl+v");key("Return");CHECK(scene.lighting()->sunColor().r==136&&scene.lighting()->sunColor().b==221);
        click("property-search");key("ctrl+a");key("BackSpace");key("Return");
        click("node-"+std::to_string(partId));auto row=ui.frontend()->elementBounds("node-"+std::to_string(partId));
        event("mousemove "+std::to_string(int(row.x+row.width/2))+" "+std::to_string(int(row.y+row.height/2)));event("mousedown 3");event("mouseup 3");
        CHECK(ui.frontend()->elementBounds("context-menu").height>100);click("context-rename-");key("Escape");CHECK(part->name()=="PreciseWall");
        // Any Part can opt into spawning from the real properties panel. The
        // SPAWN group is below the fold at 1366x768, so use the property filter
        // like a user would instead of clicking clipped coordinates.
        click("property-search");event("type --clearmodifiers Spawn");key("Return");frames();
        CHECK(ui.frontend()->elementBounds("prop-Spawn").width>0);capture("spawn-properties");
        click("prop-Spawn");frames();CHECK(part->attributes().count("spawn"));
        click("property-title");frames();
        CHECK(ui.frontend()->elementBounds("prop-SpawnPriority").width>0);
        click("prop-Spawn");frames();CHECK(!part->attributes().count("spawn"));
        click("property-search");key("ctrl+a");key("BackSpace");key("Return");frames();
        click("tab-Script");capture("ribbon-script");click("cmd-script-");frames();
        CHECK(scene.selection()&&IsScriptClass(scene.selection()->instanceClass()));
        const InstanceId scriptId=scene.selection()?scene.selection()->id():0;
        CHECK(EditorUITestAccess::activeDocument(ui)==scriptId);
        CHECK(EditorUITestAccess::documentCount(ui)==1);
        CHECK(ui.frontend()->elementBounds("document-"+std::to_string(scriptId)).width>45);
        CHECK(ui.frontend()->elementBounds("close-document-"+std::to_string(scriptId)).width>8);
        auto source=EditorUITestAccess::source(ui);CHECK(source.width>100&&source.height>40);
        clickAt({source.x+25,source.y+18});key("ctrl+a");event("type --clearmodifiers 'print(123)'");key("ctrl+s");
        CHECK(scene.find(scriptId)&&static_cast<Script*>(scene.find(scriptId))->source=="print(123)");capture("script-editor");
        CHECK(ui.frontend()->elementText("document-"+std::to_string(scriptId)).find("Script")!=std::string::npos);
        CHECK(!EditorUITestAccess::scriptDirty(ui,scriptId));
        // One tab per script; opening the same source does not duplicate.
        click("cmd-script-");frames();
        const InstanceId script2=scene.selection()?scene.selection()->id():0;
        CHECK(script2!=0&&script2!=scriptId);CHECK(EditorUITestAccess::documentCount(ui)==2);
        CHECK(ui.frontend()->elementBounds("document-"+std::to_string(script2)).width>45);
        clickAt({source.x+25,source.y+18});key("ctrl+a");event("type --clearmodifiers 'print(456)'");frames();
        CHECK(EditorUITestAccess::scriptText(ui,script2)=="print(456)");
        CHECK(actions.dispatch({"open-script","",scriptId}));frames();
        CHECK(EditorUITestAccess::documentCount(ui)==2);CHECK(EditorUITestAccess::activeDocument(ui)==scriptId);
        CHECK(EditorUITestAccess::scriptText(ui,scriptId)=="print(123)");
        const int savedCursor=EditorUITestAccess::scriptCursor(ui,scriptId);
        click("document-"+std::to_string(script2));frames();CHECK(EditorUITestAccess::activeDocument(ui)==script2);
        CHECK(EditorUITestAccess::scriptText(ui,script2)=="print(456)");
        click("document-"+std::to_string(scriptId));frames();CHECK(EditorUITestAccess::activeDocument(ui)==scriptId);
        CHECK(EditorUITestAccess::scriptText(ui,scriptId)=="print(123)");
        CHECK(EditorUITestAccess::scriptCursor(ui,scriptId)==savedCursor);
        key("ctrl+z");frames();CHECK(EditorUITestAccess::scriptText(ui,scriptId)!="print(123)");
        key("ctrl+y");frames();CHECK(EditorUITestAccess::scriptText(ui,scriptId)=="print(123)");
        // Rename updates the tab title without losing the draft.
        click("prop-Name");key("ctrl+a");event("type --clearmodifiers --delay 20 ScriptOne");key("Return");frames();
        CHECK(scene.find(scriptId)&&scene.find(scriptId)->name()=="ScriptOne");
        CHECK(ui.frontend()->elementText("document-"+std::to_string(scriptId)).find("ScriptOne")!=std::string::npos);
        CHECK(EditorUITestAccess::scriptText(ui,scriptId)=="print(123)");
        // Place restores the 3D viewport; script tabs restore the central editor.
        click("document-0");frames();CHECK(EditorUITestAccess::activeDocument(ui)==0);CHECK(!ui.frontend()->textFocused());
        click("document-"+std::to_string(script2));frames();CHECK(EditorUITestAccess::activeDocument(ui)==script2);
        click("close-document-"+std::to_string(script2));frames();
        CHECK(EditorUITestAccess::documentCount(ui)==1);CHECK(EditorUITestAccess::activeDocument(ui)==scriptId);
        click("tab-Game");click("cmd-show:settings-");frames();
        const auto beforeConfig=projectio::toJson(scene);frames(5);CHECK(projectio::toJson(scene)==beforeConfig);
        click("game-WalkSpeed");key("ctrl+a");event("type --clearmodifiers 18");key("Return");CHECK(scene.gameAssets().player.walkSpeed==18);capture("game-config");
        click("document-0");frames();CHECK(EditorUITestAccess::activeDocument(ui)==0);
        click("tab-Avatar");capture("ribbon-avatar");click("cmd-show:rig-");
        const auto rigId=scene.gameAssets().rigs.front().id;EditorUITestAccess::rigArm(ui,rigId);frames();
        auto& rig=*scene.gameAssets().findRig(rigId);camera.setOrbit(Vector3Add(rig.root.position,{0,1,0}),12,.7f,.23f);
        click("tab-Home");if(EditorUITestAccess::snap(ui))click("snap");
        for(int mode:{1,3,2}) {
            click("cmd-tool-"+std::to_string(mode));frames();const auto vp=ui.frontend()->viewport();
            const auto layout=gizmolayout::Build(EditorUITestAccess::proxy(ui),static_cast<ToolMode>(mode),camera,vp,false);CHECK(layout.valid);
            const auto local=mode==3?layout.rotateMarker[0]:mode==2?layout.scaleHandle[1]:layout.axisButton[0];const auto at=Vector2Add(local,{vp.x,vp.y});
            const auto before=projectio::toJson(scene);const auto cam=camera.position();
            event("mousemove "+std::to_string(int(at.x))+" "+std::to_string(int(at.y)));event("mousedown 1");CHECK(EditorUITestAccess::dragging(ui));
            const auto delta=mode==3?Vector2{28,16}:Vector2Scale(layout.axisDirection[0],28);
            event("mousemove "+std::to_string(int(at.x+delta.x))+" "+std::to_string(int(at.y+delta.y)));
            CHECK(projectio::toJson(scene)!=before);CHECK(NearlyEqual(camera.position(),cam));capture("rig-"+std::to_string(mode));
            key("Escape");event("mouseup 1");CHECK(!EditorUITestAccess::dragging(ui));CHECK(projectio::toJson(scene)==before);
        }
        click("advanced-animation");capture("animation-editor");
        if(auto* window=ImGui::FindWindowByName(loblox::i18n::Label("Animation Editor"))) {
            const auto start=window->DC.CursorStartPos;float x=start.x;
            for(const char* label:{"New","Duplicate","Save Animation + Project","Revert","Use as Walk"})x+=ImGui::CalcTextSize(loblox::i18n::Label(label)).x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x;
            const float buttonY=start.y+ImGui::GetFrameHeightWithSpacing()+ImGui::GetFrameHeight()/2;
            clickAt({x+ImGui::CalcTextSize(loblox::i18n::Label("Delete")).x/2+ImGui::GetStyle().FramePadding.x,buttonY});
            auto* popup=ImGui::FindWindowByName(loblox::i18n::Label("Delete assigned animation?"));CHECK(popup&&popup->Active);
            capture("animation-delete-confirmation");
            if(popup&&popup->Active) {
                const auto cursor=popup->DC.CursorStartPos;
                clickAt({cursor.x+ImGui::CalcTextSize(loblox::i18n::Label("Delete and clear")).x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x+ImGui::CalcTextSize(loblox::i18n::Label("Cancel")).x/2+ImGui::GetStyle().FramePadding.x,
                    cursor.y+2*ImGui::GetTextLineHeightWithSpacing()+ImGui::GetFrameHeight()/2});
            }
            CHECK(scene.gameAssets().findAnimation(scene.gameAssets().player.walkAnimation));
        }
        click("advanced-output");capture("output");
        // The assets page owns its own scope so its controls never collide
        // with the rig panel's import button.
        click("node-"+std::to_string(partId));click("advanced-assets");
        // The local sounds section is built by the same panel: it must exist
        // even without an audio device (preview only, import stays available).
        CHECK(ui.frontend()->elementBounds("cmd-import-sound-").width>0);
        click("assets-import-dialog-");
        Image image=GenImageChecked(64,64,8,8,{110,174,218,255},{232,195,106,255});
        const auto imagePath=std::string(directory)+"/source.png";CHECK(ExportImage(image,imagePath.c_str()));UnloadImage(image);
        // Paste as a user would: a blocking xdotool type of a long path can
        // overflow raylib's bounded char queue while no frame is being pumped.
        SetClipboardText(imagePath.c_str());key("ctrl+v");key("Return");frames(4);
        const auto imported=game::ListImages(game::AssetsDir());CHECK(imported.size()==1);
        if(!imported.empty()) {
            const auto assetPath=std::filesystem::absolute(std::filesystem::path(game::AssetsDir())/imported.front().filename).string();
            click("cmd-apply-image-"+assetPath);
            CHECK(part->attributes().count("decal_image"));
            capture("image-library-decal");
        }
        // Close the embedded panel through its RML close button.
        CHECK(actions.dispatch({"select","",partId}));
        // A native scene action does not implicitly change panel layout.
        auto close=ui.frontend()->elementBounds("advanced-tabs");clickAt({close.x+close.width-15,close.y+13});frames();camera.reset();
        const auto wallPosition=part->position();part->setPosition({40,3,0});CHECK(actions.dispatch({"select","",0}));
        const auto world=rig.evaluate({},rig.root);camera.setOrbit(world[game::Head].position,4,PI,0);capture("r6-front");
        part->setPosition(wallPosition);camera.reset();CHECK(actions.dispatch({"select","",partId}));
        }
        CHECK(projectio::save(scene,config.defaultProjectPath,error));const auto original=projectio::toJson(scene);
        for(auto size:{Vector2{1280,720},Vector2{1366,768},Vector2{1600,900},Vector2{1920,1080}}) {
            if(const char* requested=std::getenv("LOBLOX_PLAYER_UI_SIZE");requested&&int(size.x)!=std::atoi(requested))continue;
            resizeWindow(int(size.x),int(size.y));capture("studio-"+std::to_string(int(size.x)));
            if(size.x==1366)click("editor-camera-focus");
            const auto editorMode=camera.navigationMode();const auto editorEye=camera.position();
            click("play");auto& rt=EditorUITestAccess::runtime(ui);CHECK(rt.state()==game::RuntimeState::Playing);
            if(size.x==1366) {
                click("game-settings");CHECK(rt.state()==game::RuntimeState::Paused);
                CHECK(ui.frontend()->elementBounds("sensitivity-slider").width>40);
                CHECK(!EditorUITestAccess::captured(ui));
                key("Escape");CHECK(rt.state()==game::RuntimeState::Playing);
                click("game-controls");CHECK(rt.state()==game::RuntimeState::Paused);
                click("game-controls");CHECK(rt.state()==game::RuntimeState::Playing);
                click("game-controls");CHECK(rt.state()==game::RuntimeState::Paused);
                click("respawn");key("Tab");key("Return");CHECK(rt.state()==game::RuntimeState::Playing);
            }
            frames(12);auto vp=ui.frontend()->viewport();auto hud=ui.frontend()->elementBounds("health");
            CHECK(hud.x>=vp.x && hud.y>=vp.y);CHECK(hud.x+hud.width<=vp.x+vp.width+1);CHECK(hud.y+hud.height<=vp.y+vp.height+1);
            // Regression: keep the slim bar, never restore the rejected card.
            CHECK(hud.width==180);CHECK(hud.height==20);
            CHECK(ui.frontend()->elementText("health").find("health-heading")==std::string::npos);
            CHECK(hud.y-vp.y>=8 && hud.y-vp.y<=20);
            CHECK(vp.x+vp.width-(hud.x+hud.width)>=8 && vp.x+vp.width-(hud.x+hud.width)<=20);
            if(size.x==1366) {
                const auto logo=ui.frontend()->elementBounds("game-menu");
                const auto menu=ui.frontend()->elementBounds("game-controls");
                CHECK(std::fabs(logo.width-46)<1 && std::fabs(logo.height-46)<1);
                CHECK(std::fabs(menu.width-46)<1 && std::fabs(menu.height-46)<1);
                CHECK(logo.x-vp.x>=10 && logo.y-vp.y>=10);
                CHECK(menu.x-(logo.x+logo.width)>=2);
                const auto capsule=ui.frontend()->elementBounds("game-capsule");
                const auto settings=ui.frontend()->elementBounds("game-settings");
                CHECK(capsule.width>menu.width*2&&capsule.height==menu.height);
                CHECK(settings.x>=menu.x+menu.width-1);
            }
            const auto fill100=ui.frontend()->elementBounds("health-fill");
            const auto track=ui.frontend()->elementBounds("health-track");
            CHECK(fill100.width>=track.width-1);
            capture("health-100-"+std::to_string(int(size.x)));
            // The captured Mouse Lock cursor is screen space: jumping, falling, walking and
            // resizing the window must never move it away from the viewport centre.
            key("Shift_L");frames(2);CHECK(rt.shiftLockActive());
            auto checkReticle=[&](const char* phase){
                auto viewport=ui.frontend()->viewport();auto reticle=ui.frontend()->elementBounds("mouse-lock-cursor");
                CHECK(reticle.width==28&&reticle.height==28);
                CHECK(ui.frontend()->elementText("mouse-lock-cursor").find("icon-focus")!=std::string::npos);
                CHECK(std::fabs((reticle.x+reticle.width*.5f)-(viewport.x+viewport.width*.5f))<1.5f);
                CHECK(std::fabs((reticle.y+reticle.height*.5f)-(viewport.y+viewport.height*.5f))<1.5f);
                if(std::string(phase)=="jump")capture("reticle-jump-"+std::to_string(int(size.x)));
            };
            checkReticle("idle");
            key("space");frames(8);checkReticle("jump");frames(42);checkReticle("fall");
            event("keydown d");frames(8);checkReticle("walk");event("keyup d");frames(2);
            CHECK(rt.takeDamage(75,error));frames(20);CHECK(rt.health()==25);capture("health-25-"+std::to_string(int(size.x)));
            const auto fill25=ui.frontend()->elementBounds("health-fill");
            CHECK(fill25.width>25 && fill25.width<hud.width*.45f);
            if(size.x==1366) {
                CHECK(rt.heal(25,error));frames(18);CHECK(rt.health()==50);capture("health-50-1366");
                CHECK(std::fabs(ui.frontend()->elementBounds("health-fill").width/track.width-.5f)<.02f);
                CHECK(rt.takeDamage(30,error));frames(18);CHECK(rt.health()==20);capture("health-20-1366");
                CHECK(std::fabs(ui.frontend()->elementBounds("health-fill").width/track.width-.2f)<.02f);
                CHECK(rt.heal(5,error));frames(8);
            }
            if(size.x==1280) {
                CHECK(rt.takeDamage(25,error));frames(1);CHECK(rt.health()==0);capture("health-0");
                CHECK(ui.frontend()->elementBounds("health-fill").width<=1);
                CHECK(rt.respawn(error));CHECK(rt.takeDamage(30,error));frames(10);CHECK(rt.health()==70);capture("health-70");
                CHECK(rt.heal(30,error));CHECK(rt.health()==100);capture("health-respawn");
                CHECK(ui.frontend()->elementBounds("health-fill").width>=track.width-1);
            }
            key("Escape");CHECK(rt.state()==game::RuntimeState::Paused);CHECK(!EditorUITestAccess::captured(ui));
            CHECK(ui.frontend()->elementBounds("health").width==0);
            const auto panel=ui.frontend()->elementBounds("pause-panel");
            CHECK(panel.width>=vp.width*.8f&&panel.width<=vp.width*.96f);
            CHECK(panel.x>=vp.x&&panel.x+panel.width<=vp.x+vp.width);
            CHECK(panel.y>=vp.y+62&&panel.y+panel.height<=vp.y+vp.height);
            auto overlay=ui.frontend()->elementBounds("pause");CHECK(std::fabs(overlay.x-vp.x)<1 && std::fabs(overlay.y-vp.y)<1);
            for(const char* id:{"page-game","leave","respawn","resume","game-maximize","game-fullscreen"}) {auto b=ui.frontend()->elementBounds(id);CHECK(b.x>=vp.x && b.y>=vp.y && b.x+b.width<=vp.x+vp.width+1 && b.y+b.height<=vp.y+vp.height+1);}
            capture("pause-game-"+std::to_string(int(size.x)));
            if(size.x==1366) {
                // Maximize from the real pause overlay, then restore with F10.
                const auto normalView=ui.frontend()->viewport();
                click("game-maximize");frames(4);CHECK(EditorUITestAccess::maximized(ui));
                auto maximizedView=ui.frontend()->viewport();
                CHECK(maximizedView.x==0&&maximizedView.y==0);
                CHECK(maximizedView.width==GetScreenWidth()&&maximizedView.height==GetScreenHeight());
                CHECK(ui.frontend()->elementBounds("header").height==0);
                CHECK(ui.frontend()->elementBounds("viewport-label").height==0);
                auto pauseMax=ui.frontend()->elementBounds("pause");
                CHECK(pauseMax.width>=maximizedView.width-1&&pauseMax.height>=maximizedView.height-1);
                capture("pause-maximized-1366");
                click("resume");CHECK(rt.state()==game::RuntimeState::Playing);
                key("F10");frames(4);CHECK(!EditorUITestAccess::maximized(ui));
                auto restoredView=ui.frontend()->viewport();
                CHECK(std::fabs(restoredView.x-normalView.x)<1&&std::fabs(restoredView.y-normalView.y)<1);
                CHECK(std::fabs(restoredView.width-normalView.width)<1&&std::fabs(restoredView.height-normalView.height)<1);
                CHECK(ui.frontend()->elementBounds("header").height>0);
                // Real borderless fullscreen via F11, not a resized ImGui panel.
                key("F11");frames(8);CHECK(EditorUITestAccess::fullscreen(ui));
                CHECK(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE));
                auto fullView=ui.frontend()->viewport();
                CHECK(fullView.x==0&&fullView.y==0);
                CHECK(fullView.width==GetScreenWidth()&&fullView.height==GetScreenHeight());
                CHECK(ui.frontend()->elementBounds("health").width>150);
                if(rt.shiftLockActive())checkReticle("fullscreen");
                capture("game-fullscreen-1366");
                key("Escape");CHECK(rt.state()==game::RuntimeState::Paused);
                auto fullPause=ui.frontend()->elementBounds("pause");
                CHECK(fullPause.width>=fullView.width-1&&fullPause.height>=fullView.height-1);
                capture("pause-fullscreen-1366");
                click("page-settings");capture("pause-settings-fullscreen");
                click("page-controls");capture("pause-controls-fullscreen");
                click("page-about");capture("pause-about-fullscreen");
                click("page-game");
                // The overlay button restores the previous Studio window and layout.
                click("game-fullscreen");frames(8);CHECK(!EditorUITestAccess::fullscreen(ui));
                CHECK(!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE));
                CHECK(std::fabs(ui.frontend()->viewport().width-normalView.width)<1);
                CHECK(rt.state()==game::RuntimeState::Paused);
            }
            click("page-settings");capture("pause-settings-"+std::to_string(int(size.x)));
            if(size.x==1366) {
                const auto position=rt.position();const auto originalCamera=rt.settings().cameraMode;
                click("camera-next");CHECK(rt.settings().cameraMode!=originalCamera);
                click("camera-previous");CHECK(rt.settings().cameraMode==originalCamera);
                const bool firstPerson=rt.settings().allowFirstPerson;
                click("first-person");CHECK(rt.settings().allowFirstPerson!=firstPerson);
                click("first-person");CHECK(rt.settings().allowFirstPerson==firstPerson);
                const bool shift=rt.settings().enableShiftLock;
                click("shift-lock");CHECK(rt.settings().enableShiftLock!=shift);
                click("shift-lock");CHECK(rt.settings().enableShiftLock==shift);
                click("sensitivity");key("ctrl+a");event("type --clearmodifiers 0.005");key("Return");CHECK(NearlyEqual(rt.settings().sensitivity,.005f));
                click("sensitivity");key("ctrl+a");event("type --clearmodifiers nan");key("Return");CHECK(NearlyEqual(rt.settings().sensitivity,.005f));
                CHECK(!ui.frontend()->elementText("player-setting-error").empty());
                click("sensitivity");key("ctrl+a");event("type --clearmodifiers 0.003");key("Return");CHECK(NearlyEqual(rt.settings().sensitivity,.003f));
                const auto range=ui.frontend()->elementBounds("sensitivity-slider");
                clickAt({range.x+range.width*.7f,range.y+range.height*.5f});frames(3);CHECK(rt.settings().sensitivity>.005f);
                click("camera-distance");key("ctrl+a");event("type --clearmodifiers 15");key("Return");CHECK(NearlyEqual(rt.settings().cameraDistance,15));
                CHECK(NearlyEqual(rt.position(),position));CHECK(rt.health()==70||rt.health()==25);
                capture("settings-interactive-1366");
            }
            click("page-controls");capture("pause-controls-"+std::to_string(int(size.x)));
            click("page-about");capture("pause-about-"+std::to_string(int(size.x)));
            click("page-game");
            const auto generation=rt.characterGeneration();click("respawn");CHECK(rt.characterGeneration()==generation+1);CHECK(rt.health()==100);
            if(size.x==1366) {
                // Focus after Respawn stays on that footer action: Tab reaches
                // Resume; Space activates it once without also jumping.
                key("Tab");CHECK(std::system("xdotool keydown space")==0);
                float resumePeak=-100.f;
                for(int i=0;i<5;++i){frames(1);resumePeak=std::max(resumePeak,rt.velocity().y);}
                event("keyup space");CHECK(rt.state()==game::RuntimeState::Playing);
                CHECK(resumePeak<1.f); // observe every frame, not after a possible jump apex
            }else {click("resume");CHECK(rt.state()==game::RuntimeState::Playing);}
            CHECK(rt.shiftLockActive());key("Escape");CHECK(rt.state()==game::RuntimeState::Paused);
            click("leave");CHECK(!rt.active());CHECK(!EditorUITestAccess::captured(ui));CHECK(projectio::toJson(scene)==original);
            CHECK(camera.navigationMode()==editorMode);CHECK(NearlyEqual(camera.position(),editorEye,.0001f));
            if(editorMode==EditorCamera::NavigationMode::Focus)click("editor-camera-fps");
        }
        // Output rows are real navigation targets: click the runtime error and
        // land on its script/line; deleting that script closes its tab safely.
        click("tab-Script");click("cmd-script-");frames();
        auto* errorNode=scene.selection();CHECK(errorNode&&IsScriptClass(errorNode->instanceClass()));
        const InstanceId errorId=errorNode?errorNode->id():0;
        const auto errorSource=EditorUITestAccess::source(ui);
        clickAt({errorSource.x+25,errorSource.y+18});key("ctrl+a");
        event("type --clearmodifiers 'print(1)'");key("Return");
        event("type --clearmodifiers 'error(\"boom\")'");frames(2);
        CHECK(EditorUITestAccess::scriptText(ui,errorId).find("boom")!=std::string::npos);
        const size_t documentsBefore=EditorUITestAccess::documentCount(ui);
        click("document-0");frames();
        click("play");auto& errorRuntime=EditorUITestAccess::runtime(ui);frames(20);
        CHECK(errorRuntime.state()==game::RuntimeState::Playing);
        const auto& messages=errorRuntime.scripts().messages();
        int errorIndex=-1;
        for(size_t i=0;i<messages.size();++i)
            if(messages[i].error&&messages[i].scriptId==errorId)errorIndex=static_cast<int>(i);
        CHECK(errorIndex>=0);
        click("stop");frames();CHECK(!errorRuntime.active());
        click("tab-Game");click("cmd-show:settings-");frames();click("advanced-output");frames(2);capture("output-error");
        const auto errorRow=EditorUITestAccess::outputMessage(ui,static_cast<size_t>(std::max(0,errorIndex)));
        CHECK(errorRow.width>10&&errorRow.height>5);
        clickAt({errorRow.x+10,errorRow.y+errorRow.height*.5f});frames(3);
        CHECK(EditorUITestAccess::activeDocument(ui)==errorId);
        CHECK(EditorUITestAccess::scriptLine(ui,errorId)==2);
        capture("script-error-navigation");
        CHECK(scene.removeById(errorId));frames();
        CHECK(EditorUITestAccess::documentCount(ui)==documentsBefore-1);
        CHECK(EditorUITestAccess::activeDocument(ui)==0);
        CHECK(ui.frontend()->elementBounds("document-"+std::to_string(errorId)).width==0);
        CHECK(scene.validate(error));
        Scene restored;CHECK(projectio::load(restored,config.defaultProjectPath,error));CHECK(projectio::toJson(restored)==original);
        }
    }
    game::ClearImageTextures();rlImGuiShutdown();CloseWindow();
    std::printf("RML UI: %d checks, %d failures; screenshots: %s\n",checks,failures,directory);return failures?1:0;
}

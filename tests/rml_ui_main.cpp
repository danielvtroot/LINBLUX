// Actual RML document, GL3 renderer, engine actions and X11 input, not mock UI.
#include "editor/EditorUI.h"
#include "editor/StudioTheme.h"
#include "editor/GizmoLayout.h"
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
};
}
using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do {++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}}while(0)
int main() {
    char pattern[]="/tmp/loblox-rml-ui-XXXXXX";const char* directory=mkdtemp(pattern);if(!directory)return 1;
    setenv("XDG_CONFIG_HOME",directory,1);diag::configure(std::string(directory)+"/events.jsonl");
    std::filesystem::current_path(directory);
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1366,768,"Loblox RML integration");SetWindowPosition(0,0);SetExitKey(KEY_NULL);SetTargetFPS(60);
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
        auto frames=[&](int count=2){for(int i=0;i<count;++i){BeginDrawing();ClearBackground(BLACK);rlImGuiBegin();ui.draw();rlImGuiEnd();ui.renderModernFrontend();EndDrawing();}};
        auto event=[&](std::string args){CHECK(std::system(("xdotool "+args).c_str())==0);frames();};
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
        };
        auto capture=[&](const std::string& name){frames();TakeScreenshot((std::string(directory)+"/"+name+".png").c_str());};
        frames(6);event("search --name '^Loblox RML integration$' windowfocus --sync");
        std::string windowId;
        {FILE* pipe=popen("xdotool search --name '^Loblox RML integration$' | head -1","r");char buffer[64]{};if(pipe&&std::fgets(buffer,sizeof(buffer),pipe))windowId=buffer;if(pipe)pclose(pipe);while(!windowId.empty()&&(windowId.back()=='\n'||windowId.back()=='\r'))windowId.pop_back();}
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
        auto* viewportWindow=ImGui::FindWindowByName("Viewport");
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
        if(auto* window=ImGui::FindWindowByName("Animation Editor")) {
            const auto start=window->DC.CursorStartPos;float x=start.x;
            for(const char* label:{"New","Duplicate","Save Animation + Project","Revert","Use as Walk"})x+=ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x;
            const float buttonY=start.y+ImGui::GetFrameHeightWithSpacing()+ImGui::GetFrameHeight()/2;
            clickAt({x+ImGui::CalcTextSize("Delete").x/2+ImGui::GetStyle().FramePadding.x,buttonY});
            auto* popup=ImGui::FindWindowByName("Delete assigned animation?");CHECK(popup&&popup->Active);
            capture("animation-delete-confirmation");
            if(popup&&popup->Active) {
                const auto cursor=popup->DC.CursorStartPos;
                clickAt({cursor.x+ImGui::CalcTextSize("Delete and clear").x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x+ImGui::CalcTextSize("Cancel").x/2+ImGui::GetStyle().FramePadding.x,
                    cursor.y+2*ImGui::GetTextLineHeightWithSpacing()+ImGui::GetFrameHeight()/2});
            }
            CHECK(scene.gameAssets().findAnimation(scene.gameAssets().player.walkAnimation));
        }
        click("advanced-output");capture("output");
        click("node-"+std::to_string(partId));click("advanced-assets");click("cmd-import-dialog-");
        Image image=GenImageChecked(64,64,8,8,{110,174,218,255},{232,195,106,255});
        const auto imagePath=std::string(directory)+"/source.png";CHECK(ExportImage(image,imagePath.c_str()));UnloadImage(image);
        // Paste as a user would: a blocking xdotool type of a long path can
        // overflow raylib's bounded char queue while no frame is being pumped.
        SetClipboardText(imagePath.c_str());key("ctrl+v");key("Return");frames(4);
        const auto imported=game::ListImages(game::AssetsDir());CHECK(imported.size()==1);
        if(!imported.empty()) {click("cmd-apply-image-assets/"+imported.front().filename);CHECK(part->attributes().count("decal_image"));capture("image-library-decal");}
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
            // Player health is a labelled, viewport-local component. Neither
            // supplied JPEG contains Health: do not pretend this is a pixel
            // copy of a Roblox CoreGui health widget.
            CHECK(hud.width>=200 && hud.width<=210);
            CHECK(hud.height>=44 && hud.height<=50);
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
            CHECK(ui.frontend()->elementText("health-value")=="100 / 100");
            capture("health-100-"+std::to_string(int(size.x)));
            // Shift Lock reticle is screen space: jumping, falling, walking and
            // resizing the window must never move it away from the viewport centre.
            key("Shift_L");frames(2);CHECK(rt.shiftLockActive());
            auto checkReticle=[&](const char* phase){
                auto viewport=ui.frontend()->viewport();auto reticle=ui.frontend()->elementBounds("reticle");
                CHECK(reticle.width>5&&reticle.height>5);
                CHECK(std::fabs((reticle.x+reticle.width*.5f)-(viewport.x+viewport.width*.5f))<1.5f);
                CHECK(std::fabs((reticle.y+reticle.height*.5f)-(viewport.y+viewport.height*.5f))<1.5f);
                if(std::string(phase)=="jump")capture("reticle-jump-"+std::to_string(int(size.x)));
            };
            checkReticle("idle");
            key("space");frames(8);checkReticle("jump");frames(42);checkReticle("fall");
            event("keydown d");frames(8);checkReticle("walk");event("keyup d");frames(2);
            CHECK(rt.takeDamage(75,error));frames(20);CHECK(rt.health()==25);capture("health-25-"+std::to_string(int(size.x)));
            CHECK(ui.frontend()->elementText("health-value")=="25 / 100");
            const auto fill25=ui.frontend()->elementBounds("health-fill");
            CHECK(fill25.width>25 && fill25.width<hud.width*.45f);
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
    game::ClearImageTextures();rlImGuiShutdown();CloseWindow();
    std::printf("RML UI: %d checks, %d failures; screenshots: %s\n",checks,failures,directory);return failures?1:0;
}

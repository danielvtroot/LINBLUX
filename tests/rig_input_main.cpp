#include <cstdio>
#include <cstdlib>
#include <string>
#include <filesystem>
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "editor/EditorUI.h"
#include "editor/GizmoLayout.h"
#include "editor/StudioTheme.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "serialization/ProjectIO.h"
#include "core/Diagnostics.h"
namespace loblox {
struct EditorUITestAccess {
    static void setup(EditorUI& ui,const std::string& id,ToolMode mode,bool pose=false) {
        ui.selectRig(id,game::RightArm);ui.selectedJoint_=game::RightShoulder;
        ui.rigContext_=pose?EditorUI::RigContext::Pose:EditorUI::RigContext::Edit;
        ui.setTool(mode,"rig_regression");ui.showRigEditor_=false;
        ui.showAnimationEditor_=false;
        ui.snapEnabled_=false;ui.moveSnap_=1;ui.scaleSnap_=.5f;ui.rotateSnapDegrees_=15;
    }
    static const Part& proxy(EditorUI& ui) { return ui.jointProxy_; }
    static bool dragging(EditorUI& ui) { return ui.rigGizmo_.dragging(); }
    static game::Pose pose(EditorUI& ui) { return ui.previewPose_; }
    static bool save(EditorUI& ui,const std::string& path,std::string& error) {return ui.saveProjectAt(path,error);}
    static Rectangle toolControl(EditorUI& ui) {return ui.rigToolControl_;}
    static ToolMode tool(EditorUI& ui) {return ui.tool_;}
    static void showPanel(EditorUI& ui) {ui.showRigEditor_=true;}
    static void rename(EditorUI& ui) {ui.renameSelection();}
    static void duplicate(EditorUI& ui) {ui.duplicateSelection();}
    static void remove(EditorUI& ui) {ui.deleteSelection();}
    static Rectangle snapControl(EditorUI& ui) {return ui.snapControl_;}
    static bool snapped(EditorUI& ui) {return ui.snapEnabled_;}
    static bool freeGizmos(EditorUI& ui) {return ui.gizmo_.moveSnap()==0 && ui.rigGizmo_.moveSnap()==0;}
    static void normal(EditorUI& ui,Part* part,ToolMode mode) {ui.scene_.select(part);ui.setTool(mode,"part_snap_regression");ui.showRigEditor_=false;ui.snapEnabled_=false;}
    static bool normalDragging(EditorUI& ui) {return ui.gizmo_.dragging();}
};
}
using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do {++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);}} while(0)
int main() {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    char temp[]="/tmp/loblox-rig-input-XXXXXX";const char* dir=mkdtemp(temp);if(!dir)return 1;
    diag::configure(std::string(dir)+"/events.jsonl");
    SetTraceLogLevel(LOG_WARNING);InitWindow(1366,768,"Loblox rig regression");SetWindowPosition(0,0);
    SetExitKey(KEY_NULL);SetTargetFPS(60);rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;ImGui::GetIO().IniFilename=nullptr;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly=true;studio::Apply();
    {
        Scene scene;scene.buildDefaultScene();auto& rig=scene.gameAssets().createR6();rig.root.position={0,6,0};
        const auto id=rig.id;
        // Composite rest rotations expose double application and local/world errors.
        rig.restPose[game::RightShoulder].rotation=QuaternionFromEuler(.15f,.2f,-.1f);
        PhysicsWorld physics;physics.attach(scene);EditorCamera camera;SceneRenderer renderer;AppConfig config;
        config.defaultProjectPath=std::string(dir)+"/rig.json";EditorUI ui(scene,physics,camera,renderer,config);
        camera.setOrbit({0,6,0},12,.65f,.32f);
        auto frames=[&](int count=3){for(int i=0;i<count;++i){BeginDrawing();ClearBackground(BLACK);rlImGuiBegin();ui.draw();rlImGuiEnd();EndDrawing();}};
        auto event=[&](const std::string& args){CHECK(std::system(("xdotool "+args).c_str())==0);frames();};
        auto key=[&](const char* key){event(std::string("keydown ")+key);event(std::string("keyup ")+key);};
        // ImGui coordinates belong to this client window, not the X root.
        // A tiling compositor may ignore SetWindowPosition or center a window
        // wider than its output; XTest must translate the client coordinates.
        auto mouse=[&](Vector2 p){event("search --name '^Loblox rig regression$' mousemove --window %1 "+std::to_string(int(p.x))+" "+std::to_string(int(p.y)));};
        auto click=[&](Vector2 p){mouse(p);event("mousedown 1");event("mouseup 1");};
        auto viewport=[&](){auto* w=ImGui::FindWindowByName("Viewport");return Rectangle{w->InnerRect.Min.x,w->InnerRect.Min.y,w->InnerRect.GetWidth(),w->InnerRect.GetHeight()};};
        auto screenshot=[&](const std::string& name){TakeScreenshot((std::string(dir)+"/"+name+".png").c_str());};
        frames(5);event("search --name '^Loblox rig regression$' windowfocus --sync");
        const auto originalClips=scene.gameAssets().animations;
        for(auto mode:{ToolMode::Move,ToolMode::Rotate,ToolMode::Scale}) {
            EditorUITestAccess::setup(ui,id,mode);frames();
            EditorUITestAccess::showPanel(ui);frames();
            ImGui::SetWindowDock(ImGui::FindWindowByName("Rig Editor"),0,ImGuiCond_Always);
            ImGui::SetWindowPos("Rig Editor",{3,145});ImGui::SetWindowSize("Rig Editor",{295,610});frames();
            const auto combo=EditorUITestAccess::toolControl(ui);click({combo.x+35,combo.y+combo.height/2});
            CHECK(!GImGui->OpenPopupStack.empty());
            if(!GImGui->OpenPopupStack.empty()) {
                auto* popup=GImGui->OpenPopupStack.back().Window;
                CHECK(popup);
                const int index=mode==ToolMode::Rotate?0:mode==ToolMode::Move?1:2;
                if(popup)click({popup->DC.CursorStartPos.x+30,popup->DC.CursorStartPos.y+index*ImGui::GetTextLineHeightWithSpacing()+ImGui::GetTextLineHeight()/2});
            }
            CHECK(EditorUITestAccess::tool(ui)==mode);
            auto& current=*scene.gameAssets().findRig(id);
            auto before=projectio::toJson(scene);
            auto world=current.evaluate({},current.root);
            if(mode!=ToolMode::Rotate)CHECK(NearlyEqual(EditorUITestAccess::proxy(ui).position(),world[game::RightArm].position,.001f));
            const auto vp=viewport();auto layout=gizmolayout::Build(EditorUITestAccess::proxy(ui),mode,camera,vp,false);
            screenshot(ToolModeName(mode));
            // All Move axes, a rotation ring, and all scale axes use the exact
            // production layout used for drawing/picking.
            const int count=mode==ToolMode::Rotate?1:3;
            for(int axis=0;axis<count;++axis) {
                layout=gizmolayout::Build(EditorUITestAccess::proxy(ui),mode,camera,vp,false);
                Vector2 at=mode==ToolMode::Rotate?layout.rotateMarker[axis]:layout.axisButton[axis];
                if(mode==ToolMode::Scale) at=layout.scaleHandle[axis*2+1];
                at=Vector2Add(at,{vp.x,vp.y});
                const auto cameraBefore=camera.position();
                const auto torsoBefore=current.evaluate({},current.root)[game::Torso];
                const auto start=current.evaluate({},current.root)[game::RightArm];const auto size=current.bodies[game::RightArm].size;
                auto direction=layout.axisDirection[axis];
                if(mode==ToolMode::Rotate)direction={1,.6f};
                mouse(at);event("mousedown 1");CHECK(EditorUITestAccess::dragging(ui));
                mouse(Vector2Add(at,Vector2Scale(direction,28)));
                const auto moved=current.evaluate({},current.root)[game::RightArm];
                if(mode==ToolMode::Move)CHECK(Vector3Distance(moved.position,start.position)>.02f);
                if(mode==ToolMode::Rotate)CHECK(!NearlyEqual(moved.rotation,start.rotation,.001f));
                if(mode==ToolMode::Scale)CHECK(!NearlyEqual(current.bodies[game::RightArm].size,size,.001f));
                CHECK(NearlyEqual(camera.position(),cameraBefore));
                CHECK(NearlyEqual(current.evaluate({},current.root)[game::Torso].position,torsoBefore.position));
                CHECK(NearlyEqual(current.evaluate({},current.root)[game::Torso].rotation,torsoBefore.rotation));
                key("Escape");event("mouseup 1");CHECK(!EditorUITestAccess::dragging(ui));
                CHECK(projectio::toJson(scene)==before);
                if(axis==count-1) {
                    mouse(at);event("mousedown 1");CHECK(EditorUITestAccess::dragging(ui));
                    mouse(Vector2Add(at,Vector2Scale(direction,24)));event("mouseup 1");
                    CHECK(!EditorUITestAccess::dragging(ui));CHECK(projectio::toJson(scene)!=before);
                    CHECK(scene.selection()==nullptr);
                }
            }
            // Toggle the actual topbar checkbox: nonzero configured increments
            // must be bypassed in OFF, shared by both gizmos in ON.
            CHECK(EditorUITestAccess::freeGizmos(ui));
            const auto snap=EditorUITestAccess::snapControl(ui);
            click({snap.x+8,snap.y+snap.height/2});CHECK(EditorUITestAccess::snapped(ui));
            const auto start=current.evaluate({},current.root)[game::RightArm];
            const auto size=current.bodies[game::RightArm].size;const auto saved=projectio::toJson(scene);
            layout=gizmolayout::Build(EditorUITestAccess::proxy(ui),mode,camera,vp,false);
            auto at=mode==ToolMode::Rotate?layout.rotateMarker[0]:mode==ToolMode::Scale?layout.scaleHandle[1]:layout.axisButton[0];
            at=Vector2Add(at,{vp.x,vp.y});auto direction=mode==ToolMode::Rotate?Vector2{1,.6f}:layout.axisDirection[0];
            mouse(at);event("mousedown 1");CHECK(EditorUITestAccess::dragging(ui));
            mouse(Vector2Add(at,Vector2Scale(direction,32)));
            const auto changed=current.evaluate({},current.root)[game::RightArm];
            float steps=0;
            if(mode==ToolMode::Move)steps=changed.position.x-start.position.x;
            else if(mode==ToolMode::Scale)steps=(current.bodies[game::RightArm].size.x-size.x)/.5f;
            else {Vector3 axis;float angle;QuaternionToAxisAngle(QuaternionMultiply(changed.rotation,QuaternionInvert(start.rotation)),&axis,&angle);steps=angle*RAD2DEG/15;}
            CHECK(std::fabs(steps-std::round(steps))<.005f);
            key("Escape");event("mouseup 1");CHECK(projectio::toJson(scene)==saved);
            click({snap.x+8,snap.y+snap.height/2});CHECK(!EditorUITestAccess::snapped(ui));CHECK(EditorUITestAccess::freeGizmos(ui));
        }
        // Domain limits must not move the opposite face, even with a huge drag.
        EditorUITestAccess::setup(ui,id,ToolMode::Scale);camera.setOrbit({0,6,0},45,.65f,.32f);frames();
        for(float pixels:{500.f,-500.f}) {
            const auto before=projectio::toJson(scene);const auto vp=viewport();
            const auto layout=gizmolayout::Build(EditorUITestAccess::proxy(ui),ToolMode::Scale,camera,vp,false);
            const auto at=Vector2Add(layout.scaleHandle[1],{vp.x,vp.y});
            const auto start=rig.evaluate({},rig.root)[game::RightArm];const auto startSize=rig.bodies[game::RightArm].size;
            const auto direction=gizmomath::Axis(0,start.rotation,true);
            const auto fixed=Vector3Subtract(start.position,Vector3Scale(direction,startSize.x*.5f));
            mouse(at);event("mousedown 1");CHECK(EditorUITestAccess::dragging(ui));
            mouse(Vector2Add(at,Vector2Scale(layout.axisDirection[0],pixels)));
            const auto current=rig.evaluate({},rig.root)[game::RightArm];const auto size=rig.bodies[game::RightArm].size;
            CHECK(size.x>=.2f && size.x<=10);
            CHECK(NearlyEqual(Vector3Subtract(current.position,Vector3Scale(direction,size.x*.5f)),fixed,.01f));
            key("Escape");event("mouseup 1");CHECK(projectio::toJson(scene)==before);
        }
        camera.setOrbit({0,6,0},12,.65f,.32f);
        // Tool changes cancel the same transaction as Esc (including all binds).
        EditorUITestAccess::setup(ui,id,ToolMode::Move);frames();
        const auto beforeSwitch=projectio::toJson(scene);const auto vp=viewport();
        const auto switchLayout=gizmolayout::Build(EditorUITestAccess::proxy(ui),ToolMode::Move,camera,vp,false);
        const auto switchAt=Vector2Add(switchLayout.axisButton[0],{vp.x,vp.y});
        mouse(switchAt);event("mousedown 1");CHECK(EditorUITestAccess::dragging(ui));
        mouse(Vector2Add(switchAt,Vector2Scale(switchLayout.axisDirection[0],25)));key("3");event("mouseup 1");
        CHECK(!EditorUITestAccess::dragging(ui));CHECK(projectio::toJson(scene)==beforeSwitch);
        std::string error;CHECK(scene.validate(error));
        CHECK(EditorUITestAccess::save(ui,config.defaultProjectPath,error));Scene loaded;
        CHECK(projectio::load(loaded,config.defaultProjectPath,error));CHECK(projectio::toJson(loaded)==projectio::toJson(scene));
        game::RuntimeSession runtime;CHECK(runtime.start(loaded,error));
        for(int i=0;i<120;++i)runtime.update(1.f/60,{});
        CHECK(runtime.validate(error));runtime.stop();
        CHECK(scene.gameAssets().animations.size()==originalClips.size());
        EditorUITestAccess::duplicate(ui);CHECK(scene.gameAssets().rigs.size()==2);
        EditorUITestAccess::rename(ui);frames();event("keydown ctrl");key("a");event("keyup ctrl");
        event("type --delay 0 RenamedRig");key("Return");
        CHECK(scene.gameAssets().rigs.back().name=="RenamedRig");
        EditorUITestAccess::remove(ui);CHECK(scene.gameAssets().rigs.size()==1);CHECK(scene.validate(error));
        auto* part=scene.createPart(PartShape::Block,{0,6,0},{2,2,2},"SnapPart");part->setAnchored(true);
        for(auto mode:{ToolMode::Move,ToolMode::Scale,ToolMode::Rotate})for(bool snapped:{false,true}) {
            EditorUITestAccess::normal(ui,part,mode);frames();
            if(snapped) {const auto c=EditorUITestAccess::snapControl(ui);click({c.x+8,c.y+c.height/2});}
            CHECK(EditorUITestAccess::snapped(ui)==snapped);
            const auto saved=projectio::toJson(scene);const auto start=part->transform();const auto vp=viewport();
            const auto layout=gizmolayout::Build(*part,mode,camera,vp,false);
            auto at=mode==ToolMode::Move?layout.axisButton[0]:mode==ToolMode::Scale?layout.scaleHandle[1]:layout.rotateMarker[0];
            at=Vector2Add(at,{vp.x,vp.y});const auto direction=mode==ToolMode::Rotate?Vector2{1,.6f}:layout.axisDirection[0];
            mouse(at);event("mousedown 1");CHECK(EditorUITestAccess::normalDragging(ui));
            mouse(Vector2Add(at,Vector2Scale(direction,snapped?32.f:4.f)));
            float steps;
            if(mode==ToolMode::Move)steps=part->position().x-start.position.x;
            else if(mode==ToolMode::Scale)steps=(part->size().x-start.size.x)/.5f;
            else {Vector3 axis;float angle;QuaternionToAxisAngle(QuaternionMultiply(part->rotation(),QuaternionInvert(start.rotation)),&axis,&angle);steps=angle*RAD2DEG/15;}
            if(snapped)CHECK(std::fabs(steps-std::round(steps))<.005f);
            else CHECK(std::fabs(steps)>.001f && std::fabs(steps-std::round(steps))>.001f);
            key("Escape");event("mouseup 1");CHECK(projectio::toJson(scene)==saved);
        }
    }
    rlImGuiShutdown();CloseWindow();std::printf("Rig input: %d checks, %d failures; captures %s\n",checks,failures,dir);return failures?1:0;
}

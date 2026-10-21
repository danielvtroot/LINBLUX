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
        ui.moveSnap_=ui.scaleSnap_=ui.rotateSnapDegrees_=0;
    }
    static const Part& proxy(EditorUI& ui) { return ui.jointProxy_; }
    static bool dragging(EditorUI& ui) { return ui.rigGizmo_.dragging(); }
    static game::Pose pose(EditorUI& ui) { return ui.previewPose_; }
    static bool save(EditorUI& ui,const std::string& path,std::string& error) {return ui.saveProjectAt(path,error);}
};
}
using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do {++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);}} while(0)
int main() {
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
        auto mouse=[&](Vector2 p){event("mousemove "+std::to_string(int(p.x))+" "+std::to_string(int(p.y)));};
        auto viewport=[&](){auto* w=ImGui::FindWindowByName("Viewport");return Rectangle{w->InnerRect.Min.x,w->InnerRect.Min.y,w->InnerRect.GetWidth(),w->InnerRect.GetHeight()};};
        auto screenshot=[&](const std::string& name){TakeScreenshot((std::string(dir)+"/"+name+".png").c_str());};
        frames(5);event("search --name '^Loblox rig regression$' windowfocus --sync");
        const auto originalClips=scene.gameAssets().animations;
        for(auto mode:{ToolMode::Move,ToolMode::Rotate,ToolMode::Scale}) {
            EditorUITestAccess::setup(ui,id,mode);frames();
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
                key("Escape");event("mouseup 1");CHECK(!EditorUITestAccess::dragging(ui));
                CHECK(projectio::toJson(scene)==before);
                if(axis==count-1) {
                    mouse(at);event("mousedown 1");CHECK(EditorUITestAccess::dragging(ui));
                    mouse(Vector2Add(at,Vector2Scale(direction,24)));event("mouseup 1");
                    CHECK(!EditorUITestAccess::dragging(ui));CHECK(projectio::toJson(scene)!=before);
                    CHECK(scene.selection()==nullptr);
                }
            }
        }
        std::string error;CHECK(scene.validate(error));
        CHECK(EditorUITestAccess::save(ui,config.defaultProjectPath,error));Scene loaded;
        CHECK(projectio::load(loaded,config.defaultProjectPath,error));CHECK(projectio::toJson(loaded)==projectio::toJson(scene));
        game::RuntimeSession runtime;CHECK(runtime.start(loaded,error));
        for(int i=0;i<120;++i)runtime.update(1.f/60,{});
        CHECK(runtime.validate(error));runtime.stop();
        CHECK(scene.gameAssets().animations.size()==originalClips.size());
    }
    rlImGuiShutdown();CloseWindow();std::printf("Rig input: %d checks, %d failures; captures %s\n",checks,failures,dir);return failures?1:0;
}

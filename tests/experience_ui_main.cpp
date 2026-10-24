#include "editor/EditorUI.h"
#include "editor/StudioTheme.h"
#include "ui/StudioFrontend.h"
#include "ui/ExperienceUI.h"
#include "editor/GizmoLayout.h"
#include "experiences/ExperienceManager.h"
#include "rendering/SceneRenderer.h"
#include "rendering/EditorCamera.h"
#include "game/ImageAssets.h"
#include "serialization/ProjectIO.h"
#include "core/Diagnostics.h"
#include "imgui.h"
#include "rlImGui.h"
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace loblox {struct EditorUITestAccess {
    static ExperienceUI* experiences(EditorUI& e){return e.experienceUi_;}
    static game::RuntimeSession& runtime(EditorUI& e){return e.runtime_;}
    static Gizmo& rigGizmo(EditorUI& e){return e.rigGizmo_;}
    static const Part& proxy(EditorUI& e){return e.jointProxy_;}
};}
using namespace loblox;
namespace fs=std::filesystem;
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}}while(0)
std::string Quote(const std::string& text){std::string out="'";for(char c:text)out+=c=='\''?"'\\''":std::string(1,c);return out+"'";}
std::string Window(const char* name){FILE* p=popen((std::string("xdotool search --onlyvisible --name '")+name+"' 2>/dev/null | head -1").c_str(),"r");char text[64]{};if(p)std::fgets(text,sizeof(text),p);if(p)pclose(p);std::string s=text;while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();return s;}
int main() {
    const auto directory=fs::temp_directory_path()/("loblox-experience-ui-"+experiences::newId());fs::create_directories(directory);
    setenv("XDG_DATA_HOME",directory.c_str(),1);setenv("XDG_CONFIG_HOME",directory.c_str(),1);diag::configure((directory/"events.jsonl").string());
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_RESIZABLE);InitWindow(1366,768,"Loblox Experiences UI");SetWindowPosition(0,0);SetExitKey(KEY_NULL);SetTargetFPS(60);
    rlImGuiSetup(true);ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;ImGui::GetIO().IniFilename=nullptr;studio::Apply();
    try {
        experiences::ExperienceManager manager(directory/"data",fs::path(__FILE__).parent_path().parent_path()/"assets/templates");
        Scene scene;PhysicsWorld physics;physics.attach(scene);EditorCamera camera;SceneRenderer renderer;AppConfig config;config.defaultProjectPath=(directory/"legacy.json").string();
        EditorUI editor(scene,physics,camera,renderer,config);std::string error;CHECK(editor.enableModernFrontend(error));
        editor.frontend()->enableExperiences(manager,true);
        std::function<void()> observe;
        auto frames=[&](int n=2){for(int i=0;i<n;++i){BeginDrawing();ClearBackground(BLACK);rlImGuiBegin();editor.draw();rlImGuiEnd();editor.renderModernFrontend();EndDrawing();if(observe)observe();}};
        auto event=[&](const std::string& command){CHECK(std::system(("xdotool "+command).c_str())==0);frames();};
        auto key=[&](const std::string& name){event("keydown "+name);event("keyup "+name);};
        auto click=[&](const std::string& id){frames();const auto r=editor.frontend()->elementBounds(id);std::fprintf(stderr,"click %s: %.0f %.0f %.0f %.0f\n",id.c_str(),r.x,r.y,r.width,r.height);CHECK(r.width>0&&r.height>0);
            event("mousemove "+std::to_string(int(r.x+r.width*.5f))+" "+std::to_string(int(r.y+r.height*.5f)));event("mousedown 1");event("mouseup 1");frames(3);};
        auto fill=[&](const std::string& id,const std::string& value){click(id);key("ctrl+a");for(size_t i=0;i<value.size();i+=8)event("type --clearmodifiers "+Quote(value.substr(i,8)));};
        auto capture=[&](const std::string& name){frames();TakeScreenshot((directory/(name+".png")).c_str());};
        frames(6);event("search --name '^Loblox Experiences UI$' windowfocus --sync");capture("home");
        for(const auto size:std::initializer_list<Vector2>{{1280,720},{1600,900},{1920,1080},{1366,768}}) {
            SetWindowSize(int(size.x),int(size.y));frames(8);
            const auto r=editor.frontend()->elementBounds("exp-new-main");CHECK(r.width>60&&r.x+r.width<=size.x);CHECK(editor.frontend()->elementBounds("exp-nav-library").height>25);
            capture("home-"+std::to_string(int(size.x))+"x"+std::to_string(int(size.y)));
        }
        const auto mainWindow=Window("^Loblox Experiences UI$");CHECK(!mainWindow.empty());
        auto nativeFile=[&](const std::string& title,const fs::path& file,const std::string& shot) {
            std::string popup;for(int i=0;i<100&&popup.empty();++i){frames(1);popup=Window(title.c_str());}
            CHECK(!popup.empty());if(popup.empty())return;
            event("windowfocus --sync "+popup);
            event("mousemove --window "+popup+" 290 140");event("mousedown 1");event("mouseup 1");
            event("key ctrl+l");event("key ctrl+a");event("type --clearmodifiers "+Quote(file.string()));
            if(!shot.empty())CHECK(std::system(("scrot -o "+Quote((directory/(shot+".png")).string())).c_str())==0);
            event("key Return");frames(6);
            if(EditorUITestAccess::experiences(editor)->nativeDialogActive())event("key alt+o");
            for(int i=0;i<100&&EditorUITestAccess::experiences(editor)->nativeDialogActive();++i)frames(1);
            CHECK(!EditorUITestAccess::experiences(editor)->nativeDialogActive());event("windowfocus --sync "+mainWindow);frames(4);
        };
        Image icon=GenImageChecked(96,96,12,12,{35,100,200,255},{90,190,230,255});CHECK(ExportImage(icon,(directory/"icon.png").c_str()));UnloadImage(icon);
        const auto banner=fs::path(__FILE__).parent_path()/"fixtures/image-test.jpg";
        click("exp-new");CHECK(editor.frontend()->elementBounds("exp-name").width>0);capture("new-experience");
        fill("exp-name","Integration Obby");fill("exp-description","A portable local experience");
        click("exp-template");key("Down");key("Return");
        click("exp-choose-icon");nativeFile("^Choose an image$",directory/"icon.png","native-icon");
        click("exp-choose-banner");nativeFile("^Choose an image$",banner,"native-banner");
        click("exp-choose-loading");nativeFile("^Choose an image$",directory/"icon.png","native-loading");
        capture("wizard-with-images");click("exp-create");
        CHECK(manager.active().has_value());if(!manager.active())throw std::runtime_error("Creation failed: "+editor.frontend()->elementText("experience-error"));
        CHECK(manager.active()->manifest.name=="Integration Obby");CHECK(manager.active()->manifest.description=="A portable local experience");
        CHECK(!manager.active()->manifest.icon.empty());CHECK(!manager.active()->manifest.banner.empty());CHECK(!manager.active()->manifest.loadingBackground.empty());
        CHECK(!editor.experienceOverlay());CHECK(scene.parts().size()==2);CHECK(scene.gameAssets().rigs.size()==1);capture("studio");
        const auto rigId=scene.gameAssets().defaultRig;const auto* rig=scene.gameAssets().findRig(rigId);CHECK(rig!=nullptr);
        for(int i=0;i<18;++i){const auto row=editor.frontend()->elementBounds("rig-"+rigId),panel=editor.frontend()->elementBounds("explorer");if(row.y+row.height<panel.y+panel.height)break;event("mousemove "+std::to_string(int(panel.x+80))+" "+std::to_string(int(panel.y+80)));event("click 5");}
        click("rig-"+rigId);frames(5);click("tab-Home");click("cmd-tool-1");frames(4);
        const auto startRig=*scene.gameAssets().findRig(rigId);const auto beforeBodies=startRig.evaluate({},startRig.root);const auto editorEye=camera.position();
        auto moveRig=[&](int axis,float units){
            auto& gizmo=EditorUITestAccess::rigGizmo(editor);const auto& proxy=EditorUITestAccess::proxy(editor);const auto view=editor.frontend()->viewport();
            const auto layout=gizmolayout::Build(proxy,ToolMode::Move,camera,view,gizmo.localSpace(),gizmo.visualSettings());const auto point=layout.moveHandle[axis*2+1];
            const auto basis=gizmomath::Axis(axis,proxy.rotation(),gizmo.localSpace());const auto ray=camera.rayFromScreen(point,{view.width,view.height});
            const auto facing=Vector3Normalize(Vector3Subtract(proxy.position(),camera.position()));const auto normal=Vector3Normalize(Vector3Subtract(facing,Vector3Scale(basis,Vector3DotProduct(facing,basis))));
            const auto origin=Vector3Add(ray.position,Vector3Scale(ray.direction,Vector3DotProduct(Vector3Subtract(proxy.position(),ray.position),normal)/Vector3DotProduct(ray.direction,normal)));Vector2 goal{};
            CHECK(camera.projectChecked(Vector3Add(origin,Vector3Scale(basis,units)),{view.width,view.height},goal));
            event("mousemove "+std::to_string(int(view.x+point.x))+" "+std::to_string(int(view.y+point.y)));event("mousedown 1");CHECK(gizmo.dragging());
            event("mousemove "+std::to_string(int(view.x+goal.x))+" "+std::to_string(int(view.y+goal.y)));event("mouseup 1");CHECK(!gizmo.dragging());
        };
        moveRig(0,5);moveRig(1,3);rig=scene.gameAssets().findRig(rigId);const auto moved=rig->evaluate({},rig->root);
        for(int i=0;i<game::kBodyCount;++i)CHECK(Vector3Distance(moved[i].position,Vector3Add(beforeBodies[i].position,{5,3,0}))<.01f);
        CHECK(NearlyEqual(rig->restPose[game::RootJoint].position,startRig.restPose[game::RootJoint].position));CHECK(NearlyEqual(camera.position(),editorEye));capture("rig-whole-moved");
        click("cmd-tool-3");frames(3);{
            const auto view=editor.frontend()->viewport();auto& gizmo=EditorUITestAccess::rigGizmo(editor);const auto layout=gizmolayout::Build(EditorUITestAccess::proxy(editor),ToolMode::Rotate,camera,view,gizmo.localSpace(),gizmo.visualSettings());const auto point=layout.rotateMarker[1];
            const auto pivot=rig->authoringPivot().position;const auto rotation=rig->root.rotation;
            event("mousemove "+std::to_string(int(view.x+point.x))+" "+std::to_string(int(view.y+point.y)));event("mousedown 1");
            event("mousemove "+std::to_string(int(view.x+point.x+31))+" "+std::to_string(int(view.y+point.y+24)));event("mouseup 1");
            CHECK(!NearlyEqual(rotation,rig->root.rotation,.001f));CHECK(NearlyEqual(pivot,rig->authoringPivot().position,.001f));capture("rig-whole-rotated");
        }
        click("experience-menu");click("exp-default-r6");click("experience-menu");click("exp-default-map");
        bool myDefault=false;for(const auto& item:manager.templates())myDefault|=item.id=="my-default";CHECK(myDefault);
        click("experience-menu");click("exp-details");capture("experience-details");fill("exp-name","Renamed Experience");click("exp-save-details");CHECK(manager.active()->manifest.name=="Renamed Experience");
        const auto authoring=projectio::toJson(scene);bool loadingPainted=false;
        observe=[&]{if(!loadingPainted&&EditorUITestAccess::experiences(editor)->loading()&&editor.frontend()->elementBounds("exp-loading-title").width>0){TakeScreenshot((directory/"loading.png").c_str());loadingPainted=true;}};
        click("play");for(int i=0;i<240&&EditorUITestAccess::experiences(editor)->loading();++i)frames(1);observe={};
        CHECK(loadingPainted);CHECK(EditorUITestAccess::runtime(editor).active());CHECK(!EditorUITestAccess::experiences(editor)->loading());capture("play");
        key("shift+F5");CHECK(!EditorUITestAccess::runtime(editor).active());CHECK(projectio::toJson(scene)==authoring);
        click("experience-menu");click("exp-export");nativeFile("^Export Loblox Experience$",directory/"Shared.lo","native-export");CHECK(fs::exists(directory/"Shared.lo"));
        experiences::ExperienceManager receiver(directory/"receiver",fs::path(__FILE__).parent_path().parent_path()/"assets/templates");auto received=receiver.importPackage(directory/"Shared.lo");
        Scene portable;receiver.open(received,portable);CHECK(portable.gameAssets().findRig(rigId));CHECK(projectio::toJson(portable)==authoring);CHECK(fs::exists(received.root/received.manifest.banner));receiver.close();
        game::SetProjectAssetRoot(manager.active()->root.string());game::SetProjectFaceImage(manager.active()->manifest.faceImage);
        click("experience-menu");click("exp-back-home");CHECK(editor.experienceOverlay());CHECK(!manager.active());capture("recent-experiences");
        click("exp-template-my-default");fill("exp-name","From My Default");capture("default-wizard");click("exp-create");CHECK(manager.active());
        if(!manager.active()){capture("default-create-error");throw std::runtime_error("My Default creation failed: "+editor.frontend()->elementText("experience-error"));}
        CHECK(scene.gameAssets().findRig(rigId));
        click("experience-menu");click("exp-back-home");click("exp-import");nativeFile("^Open / Import Experience$",directory/"Shared.lo","native-import");CHECK(manager.active());CHECK(scene.gameAssets().findRig(rigId));capture("imported-experience");
        click("experience-menu");click("exp-back-home");click("exp-details-0");capture("card-menu");click("exp-card-details");
        EditorUITestAccess::experiences(editor)->enqueue("delete");frames(3);capture("delete-confirmation");click("exp-confirm-ok");capture("archive");CHECK(manager.list(true).size()==1);
        click("exp-details-0");click("exp-card-details");EditorUITestAccess::experiences(editor)->enqueue("restore");frames(3);CHECK(manager.list(true).empty());
        std::fprintf(stderr,"Experience integration finished; native chooser active: %d\n",EditorUITestAccess::experiences(editor)->nativeDialogActive());
    } catch(const std::exception& error){++failures;std::fprintf(stderr,"Experience UI exception: %s\n",error.what());}
    rlImGuiShutdown();CloseWindow();std::printf("Experience UI: %d checks, %d failures; captures: %s\n",checks,failures,directory.c_str());return failures?1:0;
}

#include "editor/EditorUI.h"
#include "editor/StudioTheme.h"
#include "ui/ExperienceUI.h"
#include "ui/StudioFrontend.h"
#include "experiences/ExperienceManager.h"
#include "rendering/SceneRenderer.h"
#include "rendering/EditorCamera.h"
#include "physics/PhysicsWorld.h"
#include "serialization/ProjectIO.h"
#include "imgui.h"
#include "rlImGui.h"
#include <filesystem>
#include <cstdio>
#include <thread>
namespace loblox {struct EditorUITestAccess {static ExperienceUI* experiences(EditorUI& e){return e.experienceUi_;}};}
using namespace loblox;
int main(){
    namespace fs=std::filesystem;
    auto root=fs::temp_directory_path()/("linblux-home-video-"+experiences::newId());fs::create_directories(root);
    setenv("XDG_DATA_HOME",root.c_str(),1);setenv("XDG_CONFIG_HOME",root.c_str(),1);
    SetTraceLogLevel(LOG_WARNING);InitWindow(1280,720,"LINBLUX Home validation");SetTargetFPS(60);SetExitKey(KEY_NULL);
    rlImGuiSetup(true);ImGui::GetIO().IniFilename=nullptr;studio::Apply();
    int result=0;
    try{
        experiences::ExperienceManager manager(root/"data",fs::path(__FILE__).parent_path().parent_path()/"assets/templates");
        Scene scene;PhysicsWorld physics;physics.attach(scene);EditorCamera camera;SceneRenderer renderer;AppConfig config;
        EditorUI editor(scene,physics,camera,renderer,config);std::string error;if(!editor.enableModernFrontend(error))throw std::runtime_error(error);
        editor.frontend()->enableExperiences(manager,true);auto* home=EditorUITestAccess::experiences(editor);
        const auto frame=[&]{BeginDrawing();ClearBackground(BLACK);rlImGuiBegin();editor.draw();rlImGuiEnd();editor.renderModernFrontend();EndDrawing();};
        const auto frames=[&](int count){for(int i=0;i<count;++i)frame();};
        frames(100);if(!home->videoRunning()||home->videoDecodedFrames()<5)throw std::runtime_error("Vídeo não iniciou");
        Image a=LoadImageFromScreen();const auto decoded=home->videoDecodedFrames();frames(80);Image b=LoadImageFromScreen();
        int changed=0;for(int y:{5,650,710})for(int x:{5,400,800,1270}){auto p=GetImageColor(a,x,y),q=GetImageColor(b,x,y);changed+=abs(int(p.r)-q.r)+abs(int(p.g)-q.g)+abs(int(p.b)-q.b)>8;}
        UnloadImage(a);UnloadImage(b);if(changed<4||home->videoDecodedFrames()<=decoded)throw std::runtime_error("Fundo não se move nas bordas");
        for(auto size:{Vector2{1280,720},Vector2{1920,1080},Vector2{1000,800}}){SetWindowSize(int(size.x),int(size.y));frames(10);auto r=editor.frontend()->elementBounds("exp-new-main");if(r.width<100||r.x<0||r.x+r.width>size.x)throw std::runtime_error("Botão fora da janela");TakeScreenshot((root/("home-"+std::to_string(int(size.x))+".png")).c_str());}
        home->enqueue("new");frames(4);if(editor.frontend()->elementBounds("exp-choose-loading").width!=0)throw std::runtime_error("Terceira imagem ainda visível");
        home->enqueue("create");frames(10);if(!manager.active()||home->overlayActive()||home->videoRunning())throw std::runtime_error("Editor não encerrou o vídeo: "+editor.frontend()->elementText("experience-error"));
        const auto before=projectio::toJson(scene);home->requestHome();frames(10);if(!home->videoRunning())throw std::runtime_error("Vídeo não reiniciou ao voltar");
        if(projectio::toJson(scene)==before)throw std::runtime_error("Experiência não fechou");
        std::printf("Home: vídeo nas bordas, movimento, resize, duas imagens, criação e liberação no editor passaram. Capturas: %s\n",root.c_str());
    }catch(const std::exception& e){std::fprintf(stderr,"Home: %s\n",e.what());result=1;}
    rlImGuiShutdown();CloseWindow();return result;
}

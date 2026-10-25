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
int main(int argc,char** argv){
    namespace fs=std::filesystem;
    const bool layoutSmoke=argc==3&&(std::string(argv[1])=="--layout-smoke"||std::string(argv[1])=="--actions-smoke");
    const bool capture=layoutSmoke&&std::string(argv[1])=="--layout-smoke";
    auto root=layoutSmoke?fs::absolute(argv[2]):fs::temp_directory_path()/("linblux-home-video-"+experiences::newId());fs::create_directories(root);
    setenv("XDG_DATA_HOME",root.c_str(),1);setenv("XDG_CONFIG_HOME",root.c_str(),1);
    SetTraceLogLevel(LOG_WARNING);InitWindow(1280,720,"LINBLUX Home validation");SetTargetFPS(60);SetExitKey(KEY_NULL);
    if(layoutSmoke)SetWindowPosition(0,0);
    rlImGuiSetup(true);ImGui::GetIO().IniFilename=nullptr;studio::Apply();
    int result=0;
    try{
        experiences::ExperienceManager manager(root/"data",fs::path(__FILE__).parent_path().parent_path()/"assets/templates");
        if(layoutSmoke)for(const char* name:{"Meu mundo de exploração e criação com um nome comprido", "Experiência de teste"}) {
            experiences::CreateOptions options;options.name=name;options.description="Uma experiência local para criar e explorar.";manager.create(options);
        }
        Scene scene;PhysicsWorld physics;physics.attach(scene);EditorCamera camera;SceneRenderer renderer;AppConfig config;
        EditorUI editor(scene,physics,camera,renderer,config);std::string error;if(!editor.enableModernFrontend(error))throw std::runtime_error(error);
        editor.frontend()->enableExperiences(manager,true);auto* home=EditorUITestAccess::experiences(editor);
        const auto frame=[&]{BeginDrawing();ClearBackground(BLACK);rlImGuiBegin();editor.draw();rlImGuiEnd();editor.renderModernFrontend();EndDrawing();};
        const auto frames=[&](int count){for(int i=0;i<count;++i)frame();};
        if(layoutSmoke) {
            const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
            const auto bounds=[&](const char* id){auto r=editor.frontend()->elementBounds(id);require(r.width>0&&r.height>0,id);return r;};
            const auto inside=[](Rectangle a,Rectangle b){return a.x>=b.x-1&&a.y>=b.y-1&&a.x+a.width<=b.x+b.width+1&&a.y+a.height<=b.y+b.height+1;};
            const auto click=[&](const char* id){const auto r=bounds(id);
                const std::string move="xdotool mousemove "+std::to_string(int(r.x+r.width/2))+" "+std::to_string(int(r.y+r.height/2))+" mousedown 1";
                require(std::system(move.c_str())==0,"Falha ao pressionar botão");frames(2);
                require(std::system("xdotool mouseup 1")==0,"Falha ao soltar botão");frames(4);
            };
            frames(25);require(std::system("xdotool search --name '^LINBLUX Home validation$' windowfocus --sync")==0,"Foco da janela");frames(3);
            const auto create=bounds("exp-new-main"),open=bounds("exp-import");
            require(create.width>=160&&open.width>=160&&std::abs(create.y-open.y)<1&&create.x+create.width<open.x,"Ações principais desalinhadas");
            require(inside(bounds("exp-edit-default"),bounds("engine-default-card")),"Editar padrão fora do bloco");
            for(int i=0;i<2;++i){
                auto card=bounds(("exp-card-"+std::to_string(i)).c_str());
                auto primary=bounds(("exp-open-"+std::to_string(i)).c_str());
                auto more=bounds(("exp-details-"+std::to_string(i)).c_str());
                require(inside(primary,card)&&inside(more,card),"Ação fora do cartão");
                require(std::abs(more.width-34)<1&&std::abs(primary.y-more.y)<1&&more.x-primary.x-primary.width>=7,"Abrir e … sobrepostos ou desproporcionais");
            }
            require(home->videoRunning()&&home->videoDecodedFrames()>2,"Vídeo da Home não está reproduzindo");
            if(capture)TakeScreenshot((root/"home.png").c_str());click("exp-details-0");
            const auto menu=bounds("experience-card-menu");require(menu.height>200&&inside(menu,{0,0,1280,720}),"Menu comprimido ou cortado pela janela");
            float bottom=menu.y;
            for(const char* id:{"exp-card-details","exp-card-duplicate","exp-card-export","exp-card-delete","exp-card-close"}) {
                const auto row=bounds(id);require(inside(row,menu)&&row.y>=bottom,"Itens do menu fora da caixa ou sobrepostos");bottom=row.y+row.height;
            }
            if(capture)TakeScreenshot((root/"menu.png").c_str());click("exp-card-close");
            require(editor.frontend()->elementBounds("experience-card-menu").height==0,"Menu não fechou");if(capture)TakeScreenshot((root/"home-sem-menu.png").c_str());
            click("exp-details-0");click("exp-card-duplicate");require(manager.list().size()==3,"Duplicar pelo menu falhou");home->enqueue("cancel-form");frames(4);
            click("exp-details-0");click("exp-card-delete");bounds("exp-confirm-ok");click("exp-confirm-cancel");require(manager.list().size()==3,"Cancelar exclusão removeu arquivos");
            click("exp-details-0");click("exp-card-details");bounds("exp-save-details");home->enqueue("cancel-form");frames(4);
            click("exp-open-0");require(manager.active().has_value()&&!home->homeVisible()&&!home->videoRunning(),"Abrir experiência não liberou o vídeo");home->requestHome();frames(4);
            click("exp-new-main");bounds("exp-create");home->enqueue("cancel-form");frames(4);
            click("exp-edit-default");require(manager.active()&&manager.isEngineDefault(*manager.active())&&!home->homeVisible(),"Editar padrão não abriu o recurso protegido");
            std::printf("Home layout: alinhamento, menu, duplicação, cancelamento da exclusão, detalhes, abrir, criar e editar padrão passaram. Capturas: %s\n",root.c_str());
        } else {
        frames(100);if(editor.frontend()->elementBounds("engine-default-card").height<=0||editor.frontend()->elementBounds("exp-edit-default").width<=0)throw std::runtime_error("Cartão fixo do Mundo Padrão ausente");if(!home->videoRunning()||home->videoDecodedFrames()<5)throw std::runtime_error("Vídeo não iniciou");
        Image a=LoadImageFromScreen();const auto decoded=home->videoDecodedFrames();frames(80);Image b=LoadImageFromScreen();
        int changed=0;for(int y:{5,650,710})for(int x:{5,400,800,1270}){auto p=GetImageColor(a,x,y),q=GetImageColor(b,x,y);changed+=abs(int(p.r)-q.r)+abs(int(p.g)-q.g)+abs(int(p.b)-q.b)>8;}
        UnloadImage(a);UnloadImage(b);if(changed<4||home->videoDecodedFrames()<=decoded)throw std::runtime_error("Fundo não se move nas bordas");
        for(auto size:{Vector2{1280,720},Vector2{1920,1080},Vector2{1000,800}}){SetWindowSize(int(size.x),int(size.y));frames(10);auto r=editor.frontend()->elementBounds("exp-new-main");if(r.width<100||r.x<0||r.x+r.width>size.x)throw std::runtime_error("Botão fora da janela");TakeScreenshot((root/("home-"+std::to_string(int(size.x))+".png")).c_str());}
        home->enqueue("new");frames(4);if(editor.frontend()->elementBounds("exp-choose-loading").width!=0)throw std::runtime_error("Terceira imagem ainda visível");
        home->enqueue("create");frames(10);if(!manager.active()||home->overlayActive()||home->videoRunning())throw std::runtime_error("Editor não encerrou o vídeo: "+editor.frontend()->elementText("experience-error"));
        const auto before=projectio::toJson(scene);home->requestHome();frames(10);if(!home->videoRunning())throw std::runtime_error("Vídeo não reiniciou ao voltar");
        if(projectio::toJson(scene)==before)throw std::runtime_error("Experiência não fechou");
        TakeScreenshot((root/"recentes.png").c_str());
        const auto experience=manager.list().front();home->enqueue("details",experience.manifest.installationId);frames(3);
        home->enqueue("delete");frames(3);
        if(editor.frontend()->elementText("exp-confirm-title").find(experience.manifest.name)==std::string::npos)throw std::runtime_error("Nome ausente na confirmação");
        home->enqueue("cancel-confirm");frames(3);if(!fs::exists(experience.root))throw std::runtime_error("Cancelar excluiu arquivos");
        home->enqueue("delete");frames(3);TakeScreenshot((root/"confirmar-exclusao.png").c_str());home->enqueue("confirm");frames(5);
        if(fs::exists(experience.root)||!manager.list().empty())throw std::runtime_error("Exclusão pela interface falhou");
        home->enqueue("edit-default");frames(5);if(home->homeVisible()||!manager.active())throw std::runtime_error("Edição de mundo padrão não abriu");
        scene.createPart(PartShape::Block,{3,5,2},{2,2,2},"Padrão pela interface");home->enqueue("default-map");frames(3);if(editor.frontend()->elementBounds("exp-confirm-ok").width<=0)throw std::runtime_error("Salvar padrão sem confirmação");home->enqueue("confirm");frames(5);
        home->requestHome();frames(4);home->enqueue("edit-character");frames(5);
        const auto rigId=scene.gameAssets().defaultRig;auto* rig=scene.gameAssets().findRig(rigId);
        if(!rig)throw std::runtime_error("Edição de personagem padrão não abriu");rig->bodies[game::Torso].color={201,31,62,255};
        home->enqueue("save");frames(3);home->enqueue("default-map");frames(3);home->enqueue("confirm");frames(5);home->requestHome();frames(4);
        experiences::CreateOptions inherited;inherited.name="Padrões pela interface";auto inheritedProject=manager.create(inherited);Scene inheritedScene;manager.open(inheritedProject,inheritedScene);
        bool found=false;for(auto* part:inheritedScene.parts())found|=part->name()=="Padrão pela interface";
        if(!found||inheritedScene.gameAssets().findRig(rigId)->bodies[game::Torso].color.r!=201)throw std::runtime_error("Padrões da UI não persistiram");manager.close();
        home->enqueue("restore-factory");frames(3);if(editor.frontend()->elementBounds("exp-confirm-ok").width<=0)throw std::runtime_error("Restauração sem confirmação");
        home->enqueue("cancel-confirm");frames(3);home->enqueue("restore-factory");frames(3);home->enqueue("confirm");frames(3);
        std::printf("Home: vídeo nas bordas, movimento, resize, duas imagens, criação e liberação no editor passaram. Capturas: %s\n",root.c_str());
        }
    }catch(const std::exception& e){std::fprintf(stderr,"Home: %s\n",e.what());result=1;}
    rlImGuiShutdown();CloseWindow();return result;
}

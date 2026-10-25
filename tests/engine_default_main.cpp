#include "experiences/ExperienceManager.h"
#include "serialization/ProjectIO.h"
#include "game/Runtime.h"
#include "core/Diagnostics.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
using namespace loblox;
namespace fs=std::filesystem;
using json=nlohmann::json;
namespace {
std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void require(bool condition,const char* error){if(!condition)throw std::runtime_error(error);}
experiences::ExperienceManager manager(const fs::path& root){return experiences::ExperienceManager(root/"data/linblux",root/"factory",root/"defaults");}
void author(const fs::path& root) {
    diag::configure((root/"engine-tests.jsonl").string());
    fs::create_directories(root/"defaults");experiences::ExperienceManager::writeFactoryResources(root/"factory");
    fs::copy(fs::path(__FILE__).parent_path().parent_path()/"assets/defaults/factory",root/"defaults/factory",fs::copy_options::recursive);
    auto library=manager(root);experiences::CreateOptions options;options.name="Experiência anterior";
    const auto old=library.create(options);const auto oldManifest=read(old.root/"experience.json"),oldPlace=read(old.root/old.manifest.mainPlace);
    auto special=library.editEngineDefault(false);Scene world;library.open(special,world);
    require(library.isEngineDefault(special),"Recurso não identificado como especial");
    require(library.list().size()==1,"Mundo Padrão apareceu nas experiências comuns");
    bool refused=false;try{library.deletePermanently(special);}catch(...){refused=true;}require(refused&&fs::exists(special.root),"Padrão desprotegido contra exclusão");
    refused=false;try{library.setArchived(special,true);}catch(...){refused=true;}require(refused,"Padrão pode ser arquivado");
    const auto oldRig=world.gameAssets().defaultRig;require(world.gameAssets().removeRig(oldRig),"Personagem anterior não removido");
    auto& original=world.gameAssets().createLinbluxCharacter();original.name="Personagem original do autor";
    const auto originalId=original.id;world.gameAssets().defaultRig=originalId;
    original.resizeBody(game::Torso,{2.1f,2.4f,1.2f});original.bodies[game::Torso].color={91,171,133,255};
    original.joints[game::RightHip].parent=game::LeftLeg;original.joints[game::RightHip].name="Articulação original";
    auto walk=game::MakeWalk(43,1.25f);walk.id="passadas-originais";walk.name="Passadas originais";
    world.gameAssets().animations.push_back(walk);auto& player=world.gameAssets().player;
    player.walkAnimation=walk.id;player.walkSpeed=23.5f;player.gravity=62;player.jumpHeight=4.75f;
    player.collisionRadius=.55f;player.collisionHeight=6.7f;player.cameraMode=game::CameraMode::Follow;player.cameraDistance=9.3f;
    Image image=GenImageColor(32,32,{82,190,140,255});require(ExportImage(image,(root/"original.png").c_str()),"Imagem original não criada");UnloadImage(image);
    original.bodies[game::Head].image=library.importImage(root/"original.png");
    world.lighting()->setSkyZenithColor({31,48,69,255});world.lighting()->setFogDensity(.003f);
    auto* marker=world.createPart(PartShape::Block,{4,6,2},{3,2,4},"MarcadorPadrao");marker->setAnchored(true);const auto markerId=marker->id();
    for(auto* p:world.parts())if(p->instanceClass()==InstanceClass::SpawnLocation)p->setPosition({20,.5f,-10});
    auto* script=world.createScript(InstanceClass::Script,world.ensureService(InstanceClass::ServerScriptService),"ComportamentoOriginal");
    script->source="local p = workspace:FindFirstChild(\"MarcadorPadrao\")\np:SetAttribute(\"executado\", true)\n";
    library.save(world);
    const auto package=root/"personagem-original.lo";library.exportPackage(*library.active(),package);
    Scene importProbe;std::string importError;require(projectio::fromJson(importProbe,projectio::toJson(world),importError),importError.c_str());
    importProbe.gameAssets().rigs.clear();importProbe.gameAssets().defaultRig.clear();
    const auto imported=library.importCharacter(package,importProbe);
    require(importProbe.gameAssets().findRig(imported)&&importProbe.gameAssets().defaultRig==imported&&imported!=originalId,"Importação de personagem não criou identidade independente");
    require(importProbe.gameAssets().validate(importError),importError.c_str());
    game::RuntimeSession importedRuntime;require(importedRuntime.start(importProbe,importError),importError.c_str());
    require(importedRuntime.rig().id==imported,"Personagem importado não assumiu o jogador");importedRuntime.stop();
    library.saveEngineDefaults(world,originalId);
    const auto committed=read(root/"defaults/index.json");auto index=json::parse(committed);
    require(index.contains("world")&&index.contains("character"),"Mundo e personagem não publicados juntos");
    original.bodies[game::Head].image="assets/inexistente.png";
    refused=false;try{library.saveEngineDefaults(world,originalId);}catch(...){refused=true;}
    require(refused&&read(root/"defaults/index.json")==committed,"Falha publicou metade do padrão");
    json expected{{"rig",originalId},{"oldRig",oldRig},{"marker",markerId},{"oldRoot",old.root.string()},{"oldPlacePath",(old.root/old.manifest.mainPlace).string()},
      {"oldManifest",oldManifest},{"oldPlace",oldPlace}};
    std::ofstream(root/"expected.json")<<expected.dump(2);
    std::puts("AUTORIA: abriu padrão protegido, removeu personagem, criou rig original, configurou recursos/controlador e publicou a dupla.");
}
void receive(const fs::path& root,bool restore) {
    diag::configure((root/"engine-tests.jsonl").string());
    const auto expected=json::parse(read(root/"expected.json"));auto library=manager(root);
    experiences::CreateOptions options;options.name="Nova experiência após reiniciar";const auto fresh=library.create(options);
    Scene world;library.open(fresh,world);const std::string id=expected.at("rig");const auto marker=expected.at("marker").get<InstanceId>();
    const auto* rig=world.gameAssets().findRig(id);require(rig&&world.gameAssets().defaultRig==id,"Novo personagem não herdado");
    require(!world.gameAssets().findRig(expected.at("oldRig")),"Personagem anterior reapareceu");
    require(rig->joints[game::RightHip].parent==game::LeftLeg&&rig->joints[game::RightHip].name=="Articulação original","Hierarquia não persistiu");
    require(world.findPart(marker)&&world.lighting()->skyZenithColor().r==31,"Configurações do mundo não herdadas");
    const auto& settings=world.gameAssets().player;
    require(settings.walkSpeed==23.5f&&settings.gravity==62&&settings.collisionHeight==6.7f&&settings.cameraMode==game::CameraMode::Follow,"Configurações de física/câmera não herdadas");
    require(fs::is_regular_file(fresh.root/rig->bodies[game::Head].image),"Recurso associado não copiado");
    const auto fileBefore=read(fresh.root/fresh.manifest.mainPlace);
    const auto before=projectio::toJson(world);game::RuntimeSession runtime;std::string error;
    require(runtime.start(world,error),error.c_str());require(runtime.rig().id==id,"Jogador executou o personagem incorreto");
    const auto start=runtime.position();game::PlayerInput input;input.movement={0,1};
    for(int i=0;i<120;++i)runtime.update(1.f/60,input);
    require(runtime.validate(error),error.c_str());require(Vector3Distance(start,runtime.position())>3,"Controlador não moveu o personagem");
    require(runtime.settings().collisionRadius==.55f&&runtime.settings().walkAnimation=="passadas-originais","Controlador/animação não aplicados");
    const auto* executed=runtime.scene().findPart(marker);const auto it=executed->attributes().find("executado");
    require(it!=executed->attributes().end()&&std::get_if<bool>(&it->second)&&std::get<bool>(it->second),"Script original não executou");
    runtime.stop();require(projectio::toJson(world)==before,"Execução alterou a cena autoral");
    require(read(expected.at("oldPlacePath").get<std::string>())==expected.at("oldPlace"),"Cena anterior sobrescrita");
    require(read(fs::path(expected.at("oldRoot").get<std::string>())/"experience.json")==expected.at("oldManifest"),"Manifesto anterior alterado");
    if(restore) {
        const auto current=read(root/"defaults/index.json");library.restoreFactoryDefaults();
        require(fs::is_regular_file(library.lastDefaultsBackup())&&read(library.lastDefaultsBackup())==current,"Restauração sem backup recuperável");
        auto factory=library.create(options);Scene restored;library.open(factory,restored);require(!restored.findPart(marker),"Restauração não aplicou a fábrica");
        require(read(fresh.root/fresh.manifest.mainPlace)==fileBefore,"Restauração alterou experiência existente");
    }
    std::printf("REINÍCIO: mundo, rig, imagens, animação, colisão, câmera, scripts e movimento passaram; experiência: %s\n",fresh.root.c_str());
}
}
int main(int argc,char** argv) {
    SetTraceLogLevel(LOG_WARNING);
    try {
        if(argc==3&&std::string(argv[1])=="--author"){author(fs::absolute(argv[2]));return 0;}
        if(argc==3&&std::string(argv[1])=="--receive"){receive(fs::absolute(argv[2]),true);return 0;}
        const auto root=fs::temp_directory_path()/("linblux-default-lifecycle-"+experiences::newId());const auto executable=fs::absolute(argv[0]);
        for(const char* phase:{"--author","--receive"}) {
            const auto pid=fork();require(pid>=0,"fork falhou");if(pid==0){execl(executable.c_str(),executable.c_str(),phase,root.c_str(),static_cast<char*>(nullptr));_exit(127);}
            int status=0;require(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"Fase em processo separado falhou");
        }
        std::printf("Mundo Padrão: dez contratos do ciclo completo passaram em processos separados. Fixture: %s\n",root.c_str());return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"Padrão: %s\n",e.what());return 1;}
}

#include "experiences/ExperienceManager.h"
#include "serialization/ProjectIO.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
using namespace loblox;
namespace fs=std::filesystem;
int main(){
    const auto root=fs::temp_directory_path()/("linblux-defaults-"+experiences::newId());
    fs::create_directories(root/"factory");
    try{
        experiences::ExperienceManager::writeFactoryResources(root/"factory");
        fs::copy(fs::path(__FILE__).parent_path().parent_path()/"assets/defaults",root/"defaults",fs::copy_options::recursive);
        experiences::ExperienceManager manager(root/"data",root/"factory",root/"defaults");
        auto source=manager.editEngineDefault(false);Scene scene;manager.open(source,scene);
        auto* part=scene.createPart(PartShape::Block,{0,8,0},{2,2,2},"Persistência padrão");const auto id=part->id();
        manager.saveEngineWorld(scene);
        auto charSource=manager.editEngineDefault(true);Scene character;manager.open(charSource,character);
        if(character.gameAssets().rigs.empty())throw std::runtime_error("Personagem de fábrica ausente");
        const auto rig=character.gameAssets().defaultRig;
        character.gameAssets().findRig(rig)->bodies[game::Torso].color={213,42,61,255};
        manager.saveEngineCharacter(character,rig);
        experiences::ExperienceManager reopened(root/"data",root/"factory",root/"defaults");
        experiences::CreateOptions options;options.name="Herdou padrões";auto experience=reopened.create(options);Scene world;reopened.open(experience,world);
        if(!world.findPart(id)||world.gameAssets().defaultRig!=rig||world.gameAssets().findRig(rig)->bodies[game::Torso].color.r!=213)throw std::runtime_error("Padrões não herdados");
        const auto before=projectio::toJson(world);world.findPart(id)->setName("Independente");reopened.save(world);
        auto another=reopened.create(options);Scene second;reopened.open(another,second);
        if(second.findPart(id)->name()!="Persistência padrão")throw std::runtime_error("Projeto alterou o recurso padrão");
        reopened.restoreFactoryDefaults();auto restored=reopened.create(options);Scene factory;reopened.open(restored,factory);
        for(auto* p:factory.parts())if(p->name()=="Persistência padrão")throw std::runtime_error("Restauração de fábrica falhou");
        Scene existing;reopened.open(experience,existing);if(existing.findPart(id)->name()!="Independente")throw std::runtime_error("Restauração sobrescreveu projeto existente");
        std::ofstream(root/"defaults/index.json")<<"{\"format\":\"linblux-engine-defaults\",\"version\":99}";
        bool rejected=false;try{reopened.create(options);}catch(...){rejected=true;}if(!rejected)throw std::runtime_error("Versão futura aceita");
        std::puts("Padrões: criação, edição, persistência, independência, restauração e versão futura passaram.");
        fs::remove_all(root);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s; fixture: %s\n",e.what(),root.c_str());return 1;}
}

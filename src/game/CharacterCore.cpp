#include "game/CharacterCore.h"
namespace loblox::game {
bool CharacterCore::hidden(const Instance& part) {
    for(const Instance* node=&part;node;node=node->parent()) {
        const auto instance=node->attributes().find("CharacterInstance");
        const auto hidden=node->attributes().find("CharacterHidden");
        if(instance==node->attributes().end()||hidden==node->attributes().end())continue;
        const auto* live=std::get_if<bool>(&instance->second);const auto* invisible=std::get_if<bool>(&hidden->second);
        if(live&&*live&&invisible&&*invisible)return true;
    }
    return false;
}

std::string CharacterCore::role(const Instance& part) {
    const auto it=part.attributes().find("CharacterRole");
    if(it!=part.attributes().end())if(auto* value=std::get_if<std::string>(&it->second))return *value;
    return {};
}
bool CharacterCore::setRole(Instance& part,const std::string& value,std::string& error) {
    if(!part.isPart()||value.size()>128){error="Papel requer uma Part e até 128 bytes.";return false;}
    if(!part.setAttribute("CharacterRole",value)){error="Limite de atributos atingido.";return false;}
    error.clear();return true;
}
std::vector<InstanceId> CharacterCore::parts(const Scene& scene,InstanceId id,const std::string& wanted) {
    std::vector<InstanceId> result;const auto* model=scene.find(id);
    if(!model||model->instanceClass()!=InstanceClass::Model)return result;
    std::function<void(const Instance*)> visit=[&](const Instance* node){
        if(node->isPart()&&(wanted.empty()||role(*node)==wanted))result.push_back(node->id());
        for(auto* child:node->children())visit(child);
    };visit(model);return result;
}
bool CharacterCore::validate(const Scene& scene,InstanceId id,std::string& error) {
    const auto* model=scene.find(id);
    if(!model||model->instanceClass()!=InstanceClass::Model){error="Personagem requer um Model válido.";return false;}
    const auto all=parts(scene,id),roots=parts(scene,id,"Root");
    if(all.empty()||all.size()>512||roots.size()!=1){error="Personagem requer 1–512 Parts e exatamente uma com papel Root.";return false;}
    for(auto part:all)if(role(*scene.find(part)).size()>128){error="Papel inválido.";return false;}
    error.clear();return true;
}
bool CharacterCore::move(Scene& scene,InstanceId id,RigidTransform target,std::string& error) {
    if(!Valid(target)||!validate(scene,id,error)){if(error.empty())error="Transformação inválida.";return false;}
    ModelFrame frame;if(!scene.modelFrame(id,frame,error))return false;
    frame.position=target.position;frame.rotation=target.rotation;
    return scene.transformModel(id,frame,error);
}
InstanceId CharacterCore::spawn(Scene& scene,InstanceId source,RigidTransform target,std::string& error) {
    if(!validate(scene,source,error))return 0;
    if(!Valid(target)){error="Transformação inválida.";return 0;}
    unsigned instances=0;
    for(const auto& node:scene.objects()) {
        const auto it=node->attributes().find("CharacterInstance");
        if(it!=node->attributes().end())if(const auto* value=std::get_if<bool>(&it->second);value&&*value)++instances;
    }
    if(instances>=128){error="Limite de 128 personagens vivos atingido.";return 0;}
    auto* copy=scene.duplicateTree(*scene.find(source),scene.workspace());
    if(!copy){error="Não foi possível clonar o personagem.";return 0;}
    if(!copy->setAttribute("CharacterInstance",true)||!copy->setAttribute("PlayerAvatar",false)||!copy->setAttribute("CharacterHidden",false)) {
        scene.removeById(copy->id());error="Limite de atributos do personagem atingido.";return 0;
    }
    const auto root=parts(scene,copy->id(),"Root");const auto* part=scene.findPart(root.front());
    ModelFrame pivot;if(!scene.modelFrame(copy->id(),pivot,error)){scene.removeById(copy->id());return 0;}
    pivot.position=part->position();pivot.rotation=part->rotation();
    if(!scene.setModelPivot(copy->id(),pivot,error)||!move(scene,copy->id(),target,error)){scene.removeById(copy->id());return 0;}
    for(auto id:parts(scene,copy->id())){auto* piece=scene.findPart(id);piece->setAnchored(true);piece->setCanCollide(false);}
    return copy->id();
}
}

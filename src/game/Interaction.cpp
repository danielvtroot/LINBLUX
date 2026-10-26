#include "game/Interaction.h"
#include "game/Inventory.h"
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace loblox::game {
void InteractionSystem::add(InstanceId id,InteractionOptions options){
    const auto* n=scene_.find(id);
    if(!n||(!n->isPart()&&n->instanceClass()!=InstanceClass::Model))throw std::runtime_error("Interação requer uma peça ou Modelo existente.");
    if(options.label.empty())options.label=n->name();bool known=false;for(const auto& key:ScriptKeys)known|=key.code==options.key;
    if(options.label.size()>128||!known||!std::isfinite(options.distance)||options.distance<.25f||options.distance>20||!std::isfinite(options.holdSeconds)||options.holdSeconds<.05f||options.holdSeconds>10)throw std::runtime_error("Interação: distância0,25–20; duração0,05–10; tecla permitida e nome até128 bytes.");
    if(!entries_.count(id)&&entries_.size()>=256)throw std::runtime_error("Limite de 256 interações atingido.");
    entries_[id]=std::move(options);suppressed_.erase(id);if(prompt_.object==id){elapsed_=0;prompt_={};}
}
void InteractionSystem::remove(InstanceId id,bool suppress){if(entries_.erase(id)&&suppress)suppressed_.insert(id);if(prompt_.object==id){prompt_={};elapsed_=0;}}
void InteractionSystem::ensureItem(InstanceId id,const std::string& label){if(!entries_.count(id)&&!suppressed_.count(id))add(id,{label,6,.6f,KEY_E,true});}
void InteractionSystem::pruneItems(const std::set<InstanceId>& world){for(auto it=entries_.begin();it!=entries_.end();)if(!scene_.find(it->first)||(it->second.automaticItem&&!world.count(it->first))){if(prompt_.object==it->first){prompt_={};elapsed_=0;}it=entries_.erase(it);}else ++it;for(auto it=suppressed_.begin();it!=suppressed_.end();)if(!scene_.find(*it))it=suppressed_.erase(it);else ++it;}
void InteractionSystem::cancel(){prompt_={};elapsed_=0;latched_=true;}
InstanceId InteractionSystem::update(Vector3 player,const ScriptInput& input,float dt){
    if(!std::isfinite(dt)||dt<0)return 0;dt=std::min(dt,.1f);
    const bool pad=input.gamepad&&input.gamepadButtons[GAMEPAD_BUTTON_RIGHT_FACE_LEFT];
    if(latched_&&!input.keys[latchKey_]&&!pad)latched_=false;
    struct Candidate {InstanceId id;Vector3 point;float distance;};std::vector<Candidate> candidates;
    for(const auto& entry:entries_){auto* owner=scene_.find(entry.first);if(!owner)continue;float best=std::numeric_limits<float>::infinity();Vector3 closest{};
        for(const auto* part:scene_.parts())if(owner->isAncestorOf(part)&&!InventoryHidden(*part)){
            Vector3 point;const auto bounds=part->bounds();
            if(part->shape()==PartShape::Sphere){const auto delta=Vector3Subtract(player,part->position());point=Vector3Length(delta)>part->sphereRadius()?Vector3Add(part->position(),Vector3Scale(Vector3Normalize(delta),part->sphereRadius())):player;}
            else {auto local=Vector3RotateByQuaternion(Vector3Subtract(player,bounds.center),QuaternionInvert(bounds.rotation));local={std::clamp(local.x,-bounds.half.x,bounds.half.x),std::clamp(local.y,-bounds.half.y,bounds.half.y),std::clamp(local.z,-bounds.half.z,bounds.half.z)};point=Vector3Add(bounds.center,Vector3RotateByQuaternion(local,bounds.rotation));}
            const float distance=Vector3Distance(player,point);if(distance<best){best=distance;closest=point;}}
        if(best<=entry.second.distance)candidates.push_back({entry.first,closest,best});
    }
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.distance==b.distance?a.id<b.id:a.distance<b.distance;});
    InstanceId selected=0;int rays=0;
    for(const auto& candidate:candidates){if(++rays>4)break;Part* hit=nullptr;auto* owner=scene_.find(candidate.id);
        if(candidate.distance>.02f&&physics_.raycast(player,Vector3Subtract(candidate.point,player),candidate.distance+.01f,&hit,nullptr,nullptr,true)&&hit&&!owner->isAncestorOf(hit))continue;
        selected=candidate.id;break;}
    if(!selected){prompt_={};elapsed_=0;return 0;}
    if(prompt_.object!=selected){elapsed_=0;prompt_={};}
    const auto& options=entries_.at(selected);prompt_.object=selected;prompt_.label=options.label;prompt_.key="E";
    for(const auto& key:ScriptKeys)if(key.code==options.key){prompt_.key=key.name;for(auto& c:prompt_.key)if(c>='a'&&c<='z')c=char(c-'a'+'A');break;}
    const bool held=input.keys[options.key]||pad;
    if(!held||latched_)elapsed_=0;else elapsed_+=dt;
    prompt_.progress=std::clamp(elapsed_/options.holdSeconds,0.f,1.f);
    if(prompt_.progress<1)return 0;latched_=true;latchKey_=options.key;elapsed_=0;prompt_.progress=0;return selected;
}
}

#include "game/Inventory.h"
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include "game/ImageAssets.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace loblox::game {
bool InventoryHidden(const Instance& node){const auto it=node.attributes().find("_linblux_item_hidden");if(it==node.attributes().end())return false;const auto* flag=std::get_if<bool>(&it->second);return flag&&*flag;}

namespace {
void Need(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Snapshot {InstanceId id;InstanceClass type;bool anchored=false,collide=false,enabled=false;unsigned char alpha=255;};
std::vector<Instance*> Nodes(Instance* root){std::vector<Instance*> out;std::function<void(Instance*)> visit=[&](Instance* n){out.push_back(n);for(auto* c:n->children())visit(c);};visit(root);return out;}
Snapshot Capture(const Instance* n){Snapshot s{n->id(),n->instanceClass()};if(n->isPart()){const auto* p=static_cast<const Part*>(n);s.anchored=p->anchored();s.collide=p->canCollide();s.alpha=p->color().a;}if(IsScriptClass(n->instanceClass()))s.enabled=static_cast<const Script*>(n)->enabled;return s;}
void Present(Scene& scene,const std::vector<Snapshot>& nodes,bool visible,bool world,bool scripts=false,bool fall=false){
    for(const auto& s:nodes)if(auto* n=scene.find(s.id)){
        if(n->isPart()){auto* p=static_cast<Part*>(n);auto c=p->color();c.a=visible?s.alpha:0;p->setColor(c);p->setAttribute("_linblux_item_hidden",!visible);p->setAnchored(world?(fall?false:s.anchored):true);p->setCanCollide(world?s.collide:false);}
        if(scripts&&IsScriptClass(n->instanceClass()))static_cast<Script*>(n)->enabled=s.enabled;
    }
}
}
struct Inventory::Impl {
    struct Type {InstanceId prototype;ItemOptions options;std::vector<Snapshot> nodes;};
    struct Item {std::string type;ItemOptions options;std::vector<Snapshot> nodes;std::string state="world";};
    Scene& scene;PhysicsWorld& physics;std::map<std::string,Type> types;std::map<InstanceId,Item> items;
    std::vector<InstanceId> bag;InstanceId equipped=0;Vector3 player{};float yaw=0;std::vector<InventoryChange> changes;
    Impl(Scene& s,PhysicsWorld& p):scene(s),physics(p){}
    Instance* model(InstanceId id)const{auto* n=scene.find(id);Need(n&&n->instanceClass()==InstanceClass::Model,"Modelo do item não existe mais.");return n;}
    Item& item(InstanceId id){model(id);auto it=items.find(id);Need(it!=items.end(),"Modelo não é um item registrado.");return it->second;}
    void change(InstanceId id,const char* action){if(changes.size()<256)changes.push_back({id,action});}
    void forget(InstanceId id){bag.erase(std::remove(bag.begin(),bag.end(),id),bag.end());if(equipped==id)equipped=0;items.erase(id);}
    ModelFrame held(InstanceId id,const Item& item)const{
        const auto rotation=QuatFromEulerDegreesZYX({0,yaw*RAD2DEG,0});ModelFrame frame;
        frame.position=Vector3Add(player,Vector3RotateByQuaternion(item.options.grip.position,rotation));
        frame.rotation=QuaternionMultiply(rotation,item.options.grip.rotation);ModelFrame current;std::string error;
        if(!scene.modelFrame(id,current,error))throw std::runtime_error(error);frame.scale=current.scale;return frame;
    }
    void move(InstanceId id,const ModelFrame& pose){std::string error;Need(pose.valid(),"Pose do item inválida.");if(!scene.transformModel(id,pose,error))throw std::runtime_error(error);}
};
Inventory::Inventory(Scene& s,PhysicsWorld& p):impl_(std::make_unique<Impl>(s,p)){}
Inventory::~Inventory()=default;
void Inventory::registerType(const std::string& type,InstanceId prototype,ItemOptions options){
    auto& h=*impl_;Need(!type.empty()&&type.size()<=64&&type.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"Identificador do tipo de item inválido.");
    Need(h.types.size()<64&&!h.types.count(type),"Tipo de item já registrado ou limite de 64 tipos atingido.");
    auto* root=h.model(prototype);Need(!h.items.count(prototype),"Item ativo não pode virar um protótipo.");
    for(const auto& active:h.items){auto* other=h.scene.find(active.first);Need(!other||(!root->isAncestorOf(other)&&!other->isAncestorOf(root)),"Protótipo não pode conter itens ativos nem pertencer a eles.");}
    for(const auto& t:h.types){auto* other=h.scene.find(t.second.prototype);Need(!other||(!root->isAncestorOf(other)&&!other->isAncestorOf(root)),"Protótipos de itens não podem se sobrepor.");}
    Need(root->attributes().size()<=62,"Modelo precisa de espaço para metadados do item.");
    if(options.name.empty())options.name=root->name();Need(options.name.size()<=128&&options.name.find('\0')==std::string::npos&&options.icon.size()<=512&&options.grip.valid()&&options.grip.scale==1,"Configuração do item inválida.");
    if(!options.icon.empty()){options.icon=NormalizeImageReference(options.icon);Need(options.icon.rfind("assets/",0)==0&&options.icon.find("..") == std::string::npos,"Ícone deve ser um recurso gerenciado em assets/.");Need(!ResolveImagePath(AssetsDir(),options.icon).empty(),"Ícone do item não encontrado nos recursos da experiência.");}
    const auto nodes=Nodes(root);Need(nodes.size()<=256,"Protótipo excede 256 componentes.");size_t parts=0;Impl::Type definition{prototype,options,{}};
    for(auto* n:nodes){if(n->isPart()){const auto* p=static_cast<const Part*>(n);Need(!p->hasNpc(),"Protótipo de item não aceita controlador NPC.");Need(p->attributes().size()<64||p->attributes().count("_linblux_item_hidden"),"Peça precisa de espaço para metadados de visibilidade.");}parts+=n->isPart();definition.nodes.push_back(Capture(n));}
    Need(parts>0&&parts<=128,"Item requer de 1 a 128 peças.");
    for(const auto& n:h.scene.objects())if(n->instanceClass()==InstanceClass::Attachment){const auto target=static_cast<const Attachment&>(*n).jointTarget();if(target){auto* other=h.scene.find(target);Need(other&&root->isAncestorOf(n.get())==root->isAncestorOf(other),"Protótipo possui junta ligada a um objeto externo.");}}

    h.types.emplace(type,definition);root->setAttribute("item_icon",options.icon);
    Present(h.scene,definition.nodes,false,false);
    for(auto* n:nodes)if(IsScriptClass(n->instanceClass()))static_cast<Script*>(n)->enabled=false;
    h.physics.syncScene(h.scene);
}
InstanceId Inventory::prototype(const std::string& type)const{
    auto it=impl_->types.find(type);Need(it!=impl_->types.end(),"Tipo de item não registrado.");impl_->model(it->second.prototype);return it->second.prototype;
}
InstanceId Inventory::spawn(const std::string& type,const ModelFrame* pose){
    auto& h=*impl_;Need(h.items.size()<128,"Limite de 128 itens da sessão atingido.");auto* source=h.model(prototype(type));const auto& definition=h.types.at(type);
    const auto original=Nodes(source);Need(original.size()==definition.nodes.size(),"Estrutura do protótipo mudou; registre outro tipo.");
    for(size_t i=0;i<original.size();++i)Need(original[i]->id()==definition.nodes[i].id&&original[i]->instanceClass()==definition.nodes[i].type,"Componentes do protótipo foram substituídos.");
    if(pose)Need(pose->valid(),"Pose de criação do item inválida.");auto* copy=h.scene.duplicateTree(*source);Need(copy,"Não foi possível instanciar o item.");
    const auto id=copy->id();auto nodes=Nodes(copy);Impl::Item item{type,definition.options,{}};
    for(size_t i=0;i<nodes.size();++i){auto snapshot=definition.nodes[i];snapshot.id=nodes[i]->id();item.nodes.push_back(snapshot);}
    try{
        if(pose)h.move(id,*pose);copy->setAttribute("item_icon",item.options.icon);copy->setAttribute("item_type",type);
        Present(h.scene,item.nodes,true,true,true);h.items.emplace(id,std::move(item));h.physics.syncScene(h.scene);h.change(id,"spawn");return id;
    }catch(...){h.scene.remove(copy);throw;}
}
void Inventory::collect(InstanceId id){auto& h=*impl_;auto& item=h.item(id);Need(item.state=="world","Item já está no inventário.");Need(h.bag.size()<64,"Inventário cheio: limite de 64 itens.");h.bag.push_back(id);item.state="stored";Present(h.scene,item.nodes,false,false);h.physics.syncScene(h.scene);h.change(id,"collect");}
void Inventory::equip(InstanceId id){
    auto& h=*impl_;auto& item=h.item(id);Need(item.state=="stored"||item.state=="equipped","Colete o item antes de equipar.");Need(item.options.equipable,"Item não é equipável.");
    if(h.equipped==id)return;h.move(id,h.held(id,item));unequip();item.state="equipped";h.equipped=id;Present(h.scene,item.nodes,true,false);h.physics.syncScene(h.scene);h.change(id,"equip");
}
void Inventory::unequip(){auto& h=*impl_;if(!h.equipped)return;const auto id=h.equipped;auto it=h.items.find(id);h.equipped=0;if(it!=h.items.end()){it->second.state="stored";Present(h.scene,it->second.nodes,false,false);h.change(id,"unequip");}h.physics.syncScene(h.scene);}
void Inventory::drop(InstanceId id){
    auto& h=*impl_;auto& item=h.item(id);Need(item.state!="world","Item já está no mundo.");
    ModelFrame current;std::string error;if(!h.scene.modelFrame(id,current,error))throw std::runtime_error(error);
    Vector3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
    for(const auto& snapshot:item.nodes)if(auto* part=h.scene.findPart(snapshot.id))for(int bits=0;bits<8;++bits){const auto half=part->halfExtents();const auto p=Vector3Add(part->position(),Vector3RotateByQuaternion({(bits&1)?half.x:-half.x,(bits&2)?half.y:-half.y,(bits&4)?half.z:-half.z},part->rotation()));low=Vector3Min(low,p);high=Vector3Max(high,p);}
    const auto center=Vector3Scale(Vector3Add(low,high),.5f),half=Vector3Scale(Vector3Subtract(high,low),.5f);Need(std::isfinite(half.x)&&Vector3Length(half)<32,"Item vazio ou grande demais para soltar com segurança.");
    const float radius=std::sqrt(half.x*half.x+half.z*half.z)+2;bool found=false;ModelFrame target=current;
    h.physics.syncScene(h.scene);
    for(int i=0;i<16&&!found;++i){const float angle=h.yaw+float(i%8)*PI/4;const float distance=radius+(i/8)*3;
        Vector3 candidate{h.player.x-std::sin(angle)*distance,h.player.y,h.player.z-std::cos(angle)*distance},ground;
        if(h.physics.raycast(Vector3Add(candidate,{0,6,0}),{0,-1,0},36,nullptr,&ground,nullptr,true))candidate.y=ground.y+half.y+.15f;
        if(h.physics.isSpaceClear(candidate,half,0)){target.position=Vector3Add(current.position,Vector3Subtract(candidate,center));found=target.valid();}}
    Need(found,"Não há espaço livre para soltar o item próximo ao jogador.");h.move(id,target);
    h.bag.erase(std::remove(h.bag.begin(),h.bag.end(),id),h.bag.end());if(h.equipped==id)h.equipped=0;item.state="world";Present(h.scene,item.nodes,true,true,false,true);h.physics.syncScene(h.scene);h.change(id,"drop");
}
void Inventory::remove(InstanceId id){auto& h=*impl_;h.item(id);Need(h.scene.remove(h.scene.find(id)),"Não foi possível remover o item.");h.forget(id);h.change(id,"remove");}
ItemInfo Inventory::info(InstanceId id)const{auto& h=*impl_;const auto& item=h.item(id);return {id,item.type,item.options.name,item.options.icon,item.state,item.options.equipable};}
std::vector<InstanceId> Inventory::list()const{std::vector<InstanceId> result;for(auto id:impl_->bag)if(impl_->scene.find(id))result.push_back(id);return result;}
InstanceId Inventory::equipped()const{return impl_->scene.find(impl_->equipped)?impl_->equipped:0;}
void Inventory::update(Vector3 player,float yaw){
    auto& h=*impl_;h.player=player;h.yaw=yaw;
    for(auto it=h.items.begin();it!=h.items.end();)if(!h.scene.find(it->first)){const auto id=it->first;++it;h.forget(id);h.change(id,"remove");}else ++it;
    if(h.equipped){auto& item=h.item(h.equipped);try{h.move(h.equipped,h.held(h.equipped,item));}catch(...){unequip();throw;}}
}
std::vector<InventoryChange> Inventory::takeChanges(){std::vector<InventoryChange> result;result.swap(impl_->changes);return result;}
}

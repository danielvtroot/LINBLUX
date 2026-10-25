#include "scripting/LuaRuntime.h"
#include "game/Runtime.h"
#include "scene/Scene.h"
#include "core/Diagnostics.h"
#include <lua.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <stdexcept>

namespace loblox::scripting {
namespace {
enum class Kind { Instance,Game,Players,Player,RunService,AnimationService,Character,Humanoid,Animator,Signal,Connection,Task };
enum Signal { Spawn=1,Heartbeat,HealthChanged,Died,EngineStart,EngineStop,EngineDestroy,PhysicsUpdate,CollisionEnter,CollisionExit,
    KeyPressed,KeyReleased,MousePressed,MouseReleased,GamepadPressed,GamepadReleased };
struct Handle { Kind kind; InstanceId id; unsigned generation; unsigned avatar=0; bool modern=false; };
constexpr std::size_t kNativeContentLimit=8*1024*1024, kNativeNodes=10000;
bool Eq(const char* a,const char* b) { return std::strcmp(a,b)==0; }
void Require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
int LuaErrorLine(const char* text) {
    if(!text)return 0;
    for(const char* p=text;*p;++p) {
        if(*p!=':'||!std::isdigit(static_cast<unsigned char>(p[1])))continue;
        char* end=nullptr;const long line=std::strtol(p+1,&end,10);
        if(end&&*end==':'&&line>0&&line<=10000000)return static_cast<int>(line);
    }
    return 0;
}
double Number(lua_State* L,int index) {
    Require(lua_type(L,index)==LUA_TNUMBER,"expected finite number");
    const auto n=lua_tonumber(L,index); Require(std::isfinite(n),"non-finite number rejected"); return n;
}
const char* Text(lua_State* L,int index) {
    Require(lua_type(L,index)==LUA_TSTRING,"expected string"); return lua_tostring(L,index);
}
void Vector(lua_State* L,Vector3 v) {
    lua_createtable(L,0,3); lua_pushnumber(L,v.x);lua_setfield(L,-2,"X");
    lua_pushnumber(L,v.y);lua_setfield(L,-2,"Y");lua_pushnumber(L,v.z);lua_setfield(L,-2,"Z");
}
void ModernVector(lua_State* L,Vector3 v) {
    lua_createtable(L,0,3);
    const char* names[]{"x","y","z"};const float values[]{v.x,v.y,v.z};
    for(int i=0;i<3;++i){lua_pushnumber(L,values[i]);lua_setfield(L,-2,names[i]);}
}
Vector3 ReadVector(lua_State* L,int index) {
    Require(lua_istable(L,index),"expected Vector3 table with X/Y/Z");
    Vector3 v; const char* names[]{"X","Y","Z"}; float* fields[]{&v.x,&v.y,&v.z};
    for(int i=0;i<3;++i) { lua_getfield(L,index,names[i]);
        if(lua_isnil(L,-1)){lua_pop(L,1);const char lower[]{char('x'+i),0};lua_getfield(L,index,lower);}
        const auto n=Number(L,-1);
        Require(std::fabs(n)<=1000000,"vector component exceeds supported range"); *fields[i]=float(n);lua_pop(L,1); }
    return v;
}
}
struct LuaRuntime::Impl {
    lua_State* state=nullptr;
    game::RuntimeSession* runtime=nullptr;
    std::size_t memory=0;
    std::size_t errors=0;
    double time=0;
    unsigned generation=0;
    int instructions=0, frameBudget=0, moduleDepth=0;
    InstanceId executing=0;
    Limits limits;
    bool stopping=false,closing=false,contactOverflowReported=false;
    struct Job { lua_State* thread; int ref; int arguments; InstanceId script; double wake; bool done=false; InstanceId connection=0; InstanceId token=0; };
    struct Listener { int signal; int ref; InstanceId script; unsigned avatar=0; InstanceId owner=0; std::string topic; bool once=false; };
    std::deque<Job> jobs;
    std::map<InstanceId,Listener> listeners;
    InstanceId nextConnection=0;
    InstanceId nextTask=0;
    std::vector<Message> output;
    std::map<InstanceId,int> modules;
    std::set<InstanceId> loading;
    std::set<InstanceId> startedScripts;

    static Impl& Host(lua_State* L) { return **static_cast<Impl**>(lua_getextraspace(L)); }
    static void* Allocate(void* userdata,void* pointer,std::size_t old,std::size_t size) {
        auto& self=*static_cast<Impl*>(userdata); if(!pointer) old=0;
        if(size==0) { std::free(pointer); self.memory-=std::min(self.memory,old); return nullptr; }
        if(size>old && size-old>self.limits.memoryBytes-std::min(self.memory,self.limits.memoryBytes)) return nullptr;
        void* next=std::realloc(pointer,size);
        if(next) self.memory=self.memory-old+size;
        return next;
    }
    template<int(*Function)(lua_State*)> static int Safe(lua_State* L) {
        try { Require(!Host(L).closing,"A VM Lua está encerrando");return Function(L); }
        catch(const std::exception& e) { lua_pushstring(L,e.what()); }
        catch(...) { lua_pushliteral(L,"Falha na API nativa LINBLUX"); }
        return lua_error(L);
    }
    static void Hook(lua_State* L,lua_Debug*) {
        auto& self=Host(L); self.instructions-=1000; self.frameBudget-=1000;
        if(self.instructions<=0 || self.frameBudget<=0) luaL_error(L,"Lua instruction quota exceeded; task terminated");
    }
    Handle& handle(lua_State* L,int index) {
        auto* h=static_cast<Handle*>(luaL_testudata(L,index,"Loblox.Handle"));
        Require(h && h->generation==generation,"Referência LINBLUX inválida ou expirada");
        if(h->kind==Kind::Character || h->kind==Kind::Humanoid || h->kind==Kind::Animator)
            Require(h->id==runtime->characterGeneration(),"character was respawned; use the new CharacterAdded handle");
        if(h->kind==Kind::Signal && h->avatar)Require(h->avatar==runtime->characterGeneration(),"expired character signal");
        return *h;
    }
    Instance& instance(lua_State* L,int index) {
        const auto& h=handle(L,index); Require(h.kind==Kind::Instance,"expected Instance");
        auto* object=runtime->mutableScene().find(h.id); Require(object,"Instance was destroyed"); return *object;
    }
    void push(lua_State* L,Kind kind,InstanceId id=0,bool modern=false) {
        if(kind==Kind::Character || kind==Kind::Humanoid || kind==Kind::Animator)id=runtime->characterGeneration();
        auto* h=static_cast<Handle*>(lua_newuserdatauv(L,sizeof(Handle),0));
        *h={kind,id,generation,(kind==Kind::Signal && (id==HealthChanged || id==Died))?runtime->characterGeneration():0,modern}; luaL_setmetatable(L,"Loblox.Handle");
    }
    void pushInstance(lua_State* L,const Instance* object,bool modern=false) {
        if(object) push(L,Kind::Instance,object->id(),modern); else lua_pushnil(L);
    }
    static std::size_t contentBytes(const Instance& object) {
        std::size_t bytes=object.name().size();
        if(IsScriptClass(object.instanceClass()))bytes+=static_cast<const Script&>(object).source.size();
        for(const auto& pair:object.attributes()) {
            bytes+=pair.first.size()+sizeof(Instance::Attribute);
            if(const auto* value=std::get_if<std::string>(&pair.second))bytes+=value->size();
        }
        return bytes;
    }
    void nativeBudget(std::size_t extraNodes,std::size_t extraBytes) const {
        const auto& objects=runtime->scene().objects();
        Require(extraNodes<=kNativeNodes && objects.size()<=kNativeNodes-extraNodes,"native Instance budget exceeded");
        std::size_t bytes=0;for(const auto& object:objects)bytes+=contentBytes(*object);
        Require(extraBytes<=kNativeContentLimit && bytes<=kNativeContentLimit-extraBytes,"native content budget exceeded");
    }
    void message(bool error,InstanceId id,const char* text) {
        if(error)++errors;
        const auto* object=runtime?runtime->scene().find(id):nullptr;
        const std::string name=object?object->name():"Runtime";
        if(output.size()>=256) output.erase(output.begin());
        const std::string detail=std::string(text?text:"").substr(0,2048);
        output.push_back({error,name,detail,id,error?LuaErrorLine(text):0});
        diag::Event event; event.subsystem="script"; event.name=error?"error":"output";
        event.entityId=id; event.level=error?diag::Level::Error:diag::Level::Info;
        event.error=error?output.back().text:""; event.context={{"script",name},{"message",output.back().text}}; diag::log(event);
    }
    void environment(lua_State* L,InstanceId id) {
        lua_newtable(L); pushInstance(L,runtime->scene().find(id));lua_setfield(L,-2,"script");
        // Each script gets its own context; the shared API contains no authoring pointers.
        lua_newtable(L);
        const auto* script=runtime->scene().find(id);
        pushInstance(L,script?script->parent():nullptr,true);lua_setfield(L,-2,"self");
        pushInstance(L,script,true);lua_setfield(L,-2,"script");
        lua_newtable(L);lua_getglobal(L,"linblux");lua_setfield(L,-2,"__index");lua_setmetatable(L,-2);
        lua_setfield(L,-2,"linblux");
        lua_newtable(L);lua_pushglobaltable(L);lua_setfield(L,-2,"__index");lua_setmetatable(L,-2);
        lua_setupvalue(L,-2,1);
    }
    static int LoadAsset(lua_State* L) {
        auto& self=Host(L); const auto id=static_cast<InstanceId>(lua_tointeger(L,1));
        auto* object=self.runtime->scene().find(id); Require(object && IsScriptClass(object->instanceClass()),"Script asset missing");
        const auto& script=static_cast<const Script&>(*object);
        Require(self.jobs.size()<self.limits.tasks,"Lua task limit reached");
        lua_State* thread=lua_newthread(L); const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
        char name[200];std::snprintf(name,sizeof(name),"@%s#%llu",script.name().c_str(),static_cast<unsigned long long>(id));
        if(luaL_loadbufferx(thread,script.source.data(),script.source.size(),name,"t")!=LUA_OK) {
            self.message(true,id,lua_tostring(thread,-1));luaL_unref(L,LUA_REGISTRYINDEX,ref);return 0;
        }
        self.environment(thread,id);self.jobs.push_back({thread,ref,0,id,self.time});return 0;
    }
    static int SpawnTask(lua_State* L) {
        auto& self=Host(L); Require(lua_isfunction(L,1),"task.spawn requires function");
        Require(self.jobs.size()<self.limits.tasks,"Lua task limit reached");
        const int count=lua_gettop(L);lua_State* thread=lua_newthread(L);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
        for(int i=1;i<=count;++i)lua_pushvalue(L,i);
        lua_xmove(L,thread,count);self.jobs.push_back({thread,ref,count-1,self.executing,self.time,false,0,++self.nextTask});return 0;
    }
    static int ModernSpawn(lua_State* L) {
        SpawnTask(L);auto& self=Host(L);self.push(L,Kind::Task,self.jobs.back().token,true);return 1;
    }
    static int CancelTask(lua_State* L) {
        auto& self=Host(L);const auto task=self.handle(L,1);Require(task.kind==Kind::Task,"cancel requer uma tarefa LINBLUX");
        for(auto& job:self.jobs)if(job.token==task.id){const bool changed=!job.done;job.done=true;lua_pushboolean(L,changed);return 1;}
        lua_pushboolean(L,false);return 1;
    }
    static int Wait(lua_State* L) {
        auto& self=Host(L); Require(self.moduleDepth==0,"ModuleScript cannot yield during require");
        Require(!self.stopping,"Callbacks de stop não podem aguardar");
        double seconds=lua_gettop(L)?Number(L,1):0;
        Require(seconds>=0 && seconds<=3600,"task.wait seconds must be 0..3600");
        lua_pushnumber(L,seconds);return lua_yield(L,1);
    }
    static int Print(lua_State* L) {
        char line[2049]{}; std::size_t used=0;
        for(int i=1;i<=lua_gettop(L) && used<2048;++i) {
            const char* value=lua_tostring(L,i);
            if(!value) value=lua_isboolean(L,i)?(lua_toboolean(L,i)?"true":"false"):luaL_typename(L,i);
            const auto remaining=sizeof(line)-used;
            const int n=std::snprintf(line+used,remaining,"%s%s",i>1?"\t":"",value);
            used+=std::min<std::size_t>(n<0?0:n,remaining-1);
        }
        Host(L).message(false,Host(L).executing,line);return 0;
    }
    static int NewVector(lua_State* L) { Vector3 v{float(Number(L,1)),float(Number(L,2)),float(Number(L,3))}; Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"Vector3 overflow");Vector(L,v);return 1; }
    static int NewColor(lua_State* L) { Vector3 v{float(Number(L,1)),float(Number(L,2)),float(Number(L,3))}; Require(v.x>=0&&v.y>=0&&v.z>=0&&v.x<=255&&v.y<=255&&v.z<=255,"Color3.fromRGB channels must be 0..255");Vector(L,v);return 1; }
    static int Equal(lua_State* L) { auto& self=Host(L);const auto a=self.handle(L,1),b=self.handle(L,2);lua_pushboolean(L,a.kind==b.kind && a.id==b.id);return 1; }
    static int GetService(lua_State* L) {
        auto& self=Host(L);const char* name=Text(L,2);
        if(Eq(name,"Players")) self.push(L,Kind::Players);
        else if(Eq(name,"RunService")) self.push(L,Kind::RunService);
        else if(Eq(name,"AnimationService")) self.push(L,Kind::AnimationService);
        else {
            InstanceClass type; Require(InstanceClassFromName(name,type)&&IsRootServiceClass(type),"unsupported service; no networking services exist");
            auto* object=self.runtime->mutableScene().ensureService(type); Require(object,"service unavailable");self.pushInstance(L,object);
        }return 1;
    }
    static int Children(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);lua_createtable(L,int(object.children().size()),0);int i=0;
        for(auto* child:object.children()){self.pushInstance(L,child,self.handle(L,1).modern);lua_rawseti(L,-2,++i);}return 1;
    }
    static int Descendants(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);lua_newtable(L);int i=0;
        for(const auto& child:self.runtime->scene().objects()) if(child.get()!=&object && object.isAncestorOf(child.get())) {
            self.pushInstance(L,child.get(),self.handle(L,1).modern);lua_rawseti(L,-2,++i);
        }
        return 1;
    }
    static int Find(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);const char* name=Text(L,2);
        for(auto* child:object.children()) if(child->name()==name){self.pushInstance(L,child,self.handle(L,1).modern);return 1;}
        lua_pushnil(L);return 1;
    }
    static int Destroy(lua_State* L) { auto& self=Host(L);auto& object=self.instance(L,1);Require(self.runtime->mutableScene().remove(&object),"cannot destroy protected instance/subtree");return 0; }
    static int Clone(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);std::size_t nodes=0,bytes=0;
        for(const auto& node:self.runtime->scene().objects())if(object.isAncestorOf(node.get())){++nodes;bytes+=contentBytes(*node);}
        self.nativeBudget(nodes,bytes+nodes*128); // allow generated unique names too
        auto* copy=self.runtime->mutableScene().duplicateTree(object);Require(copy,"cannot clone service or unsupported instance");self.pushInstance(L,copy,self.handle(L,1).modern);return 1;
    }
    static int GetAttribute(lua_State* L) {
        auto& object=Host(L).instance(L,1);const char* key=Text(L,2);const auto it=object.attributes().find(key);
        if(it==object.attributes().end())lua_pushnil(L);
        else std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<T,bool>)lua_pushboolean(L,value);
            else if constexpr(std::is_same_v<T,double>)lua_pushnumber(L,value);
            else lua_pushlstring(L,value.data(),value.size());},it->second);return 1;
    }
    static int SetAttribute(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);const char* key=Text(L,2);bool ok=false;
        if(lua_isnil(L,3)){object.removeAttribute(key);return 0;}
        std::size_t before=0,after=std::strlen(key)+sizeof(Instance::Attribute);
        if(auto it=object.attributes().find(key);it!=object.attributes().end()) {
            before=it->first.size()+sizeof(Instance::Attribute);
            if(const auto* value=std::get_if<std::string>(&it->second))before+=value->size();
        }
        if(lua_type(L,3)==LUA_TSTRING)after+=lua_rawlen(L,3);
        self.nativeBudget(0,after>before?after-before:0);
        if(lua_isboolean(L,3))ok=object.setAttribute(key,bool(lua_toboolean(L,3)));
        else if(lua_type(L,3)==LUA_TNUMBER)ok=object.setAttribute(key,Number(L,3));
        else if(lua_type(L,3)==LUA_TSTRING)ok=object.setAttribute(key,std::string(Text(L,3)));
        Require(ok,"invalid attribute or attribute budget exceeded");return 0;
    }
    static int NewInstance(lua_State* L) {
        auto& self=Host(L);const char* name=Text(L,1);InstanceClass type;
        Require(InstanceClassFromName(name,type),"unknown Instance class");
        auto* parent=lua_gettop(L)>1?&self.instance(L,2):self.runtime->mutableScene().workspace();
        auto& scene=self.runtime->mutableScene();self.nativeBudget(1,128);
        Instance* object=nullptr;
        if(type==InstanceClass::Part)object=scene.createPart(PartShape::Block,{});
        else if(type==InstanceClass::Folder || type==InstanceClass::Model)object=scene.createContainer(type,parent);
        else throw std::runtime_error("Instance.new supports Part, Folder and Model");
        Require(object,"invalid parent for new Instance");
        std::string error;const bool ok=scene.reparent(object,parent,error);
        if(!ok){scene.remove(object);throw std::runtime_error(error);} self.pushInstance(L,object);return 1;
    }
    static void Field(lua_State* L,int index,const char* name) {
        index=lua_absindex(L,index);lua_pushstring(L,name);lua_rawget(L,index);
    }
    static int SetMetatable(lua_State* L) {
        Require(lua_istable(L,1)&&(lua_isnil(L,2)||lua_istable(L,2)),"setmetatable requer tabela e metatabela ou nil");
        if(lua_getmetatable(L,1)) {
            Field(L,lua_gettop(L),"__metatable");const bool locked=!lua_isnil(L,-1);lua_pop(L,2);
            Require(!locked,"Metatabela protegida");
        }
        if(lua_istable(L,2)) {
            // Lua disables instruction hooks in __gc: even a false value would
            // mark the table for finalization and could later be replaced by Lua.
            Field(L,2,"__gc");Require(lua_isnil(L,-1),"Finalizadores __gc não são permitidos; use o evento stop");lua_pop(L,1);
        }
        lua_settop(L,2);lua_setmetatable(L,1);lua_pushvalue(L,1);return 1;
    }
    static PartMaterial Material(lua_State* L,int index) {
        const char* name=Text(L,index);
        const char* modern[]{"smooth","plastic","wood","metal","concrete","emissive"};
        for(int i=0;i<PartMaterialCount();++i)if(Eq(name,modern[i]))return PartMaterialFromIndex(i);
        PartMaterial material;Require(PartMaterialFromName(name,material),"Material desconhecido");return material;
    }
    static int ModernNew(lua_State* L) {
        auto& self=Host(L);const char* kind=Text(L,1);
        const bool part=Eq(kind,"part"),group=Eq(kind,"group"),model=Eq(kind,"model");
        Require(part||group||model,"scene.create aceita part, group ou model");
        Require(lua_gettop(L)==1||lua_istable(L,2),"As opções devem ser uma tabela");
        Vector3 position{},rotation{},size{4,4,4},color{163,162,165};
        bool anchored=true,collidable=true;double transparency=0;
        PartMaterial material=PartMaterial::SmoothPlastic;PartShape shape=PartShape::Block;
        std::string name=part?"Objeto":group?"Grupo":"Modelo";
        Instance* parent=self.runtime->mutableScene().workspace();
        if(lua_istable(L,2)) {
            // Reject typos before creating anything. Read raw fields, without invoking user metamethods.
            lua_pushnil(L);while(lua_next(L,2)) {
                const char* key=Text(L,-2);bool known=false;
                for(const char* allowed:{"name","parent","position","rotation","size","color","anchored","collidable","transparency","material","shape"})known|=Eq(key,allowed);
                Require(known,"Opção de criação desconhecida");
                Require(part||Eq(key,"name")||Eq(key,"parent"),"Grupos não possuem transformação ou aparência de peça");lua_pop(L,1);
            }
            Field(L,2,"name");if(!lua_isnil(L,-1)){name=Text(L,-1);Require(!name.empty()&&name.size()<=128&&name.size()==lua_rawlen(L,-1),"Nome deve ter de 1 a 128 bytes, sem NUL");}lua_pop(L,1);
            Field(L,2,"parent");if(!lua_isnil(L,-1))parent=&self.instance(L,-1);lua_pop(L,1);
            Vector3* vectors[]{&position,&rotation,&size,&color};const char* keys[]{"position","rotation","size","color"};
            for(int i=0;i<4;++i){Field(L,2,keys[i]);if(!lua_isnil(L,-1))*vectors[i]=ReadVector(L,lua_gettop(L));lua_pop(L,1);}
            Field(L,2,"anchored");if(!lua_isnil(L,-1)){Require(lua_isboolean(L,-1),"anchored requer booleano");anchored=lua_toboolean(L,-1);}lua_pop(L,1);
            Field(L,2,"collidable");if(!lua_isnil(L,-1)){Require(lua_isboolean(L,-1),"collidable requer booleano");collidable=lua_toboolean(L,-1);}lua_pop(L,1);
            Field(L,2,"transparency");if(!lua_isnil(L,-1))transparency=Number(L,-1);lua_pop(L,1);
            Field(L,2,"material");if(!lua_isnil(L,-1))material=Material(L,-1);lua_pop(L,1);
            Field(L,2,"shape");if(!lua_isnil(L,-1)){const char* value=Text(L,-1);Require(Eq(value,"box")||Eq(value,"sphere"),"shape aceita box ou sphere");shape=Eq(value,"sphere")?PartShape::Sphere:PartShape::Block;}lua_pop(L,1);
        }
        Require(size.x>=kMinPartSize&&size.y>=kMinPartSize&&size.z>=kMinPartSize,"Tamanho mínimo é 0,2");
        Require(color.x>=0&&color.y>=0&&color.z>=0&&color.x<=255&&color.y<=255&&color.z<=255,"Cor requer canais de 0 a 255");
        Require(transparency>=0&&transparency<=1,"Transparência deve estar entre 0 e 1");
        self.nativeBudget(1,name.size()+128);auto& scene=self.runtime->mutableScene();
        Instance* created=part?static_cast<Instance*>(scene.createPart(shape,position,size,name)):scene.createContainer(group?InstanceClass::Folder:InstanceClass::Model,parent,name);
        Require(created,"Não foi possível criar o objeto");std::string error;
        if(!scene.reparent(created,parent,error)){scene.remove(created);throw std::runtime_error(error);}
        if(part){auto& object=static_cast<Part&>(*created);object.setRotationEulerDegrees(rotation);object.setColor({static_cast<unsigned char>(color.x),static_cast<unsigned char>(color.y),static_cast<unsigned char>(color.z),static_cast<unsigned char>((1-transparency)*255)});object.setAnchored(anchored);object.setCanCollide(collidable);object.setMaterial(material);}
        self.pushInstance(L,created,true);return 1;
    }
    static int ModernFind(lua_State* L) {
        auto& self=Host(L);
        if(lua_isinteger(L,1)){const auto id=lua_tointeger(L,1);Require(id>0,"ID deve ser positivo");self.pushInstance(L,self.runtime->scene().find(InstanceId(id)),true);}
        else {const char* name=Text(L,1);const Instance* found=nullptr;for(const auto& object:self.runtime->scene().objects())if(object->name()==name){found=object.get();break;}self.pushInstance(L,found,true);}
        return 1;
    }
    template<game::PartMotion Operation> static int Motion(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);Require(object.isPart(),"Movimento físico requer uma peça");
        const auto value=ReadVector(L,2);std::string error;
        if(!self.runtime->partMotion(object.id(),value,Operation,error))throw std::runtime_error(error);
        return 0;
    }
    static int Raycast(lua_State* L) {
        auto& self=Host(L);const auto origin=ReadVector(L,1),direction=ReadVector(L,2);const double distance=Number(L,3);
        Require(distance>0&&distance<=1000000&&Vector3LengthSqr(direction)>1e-12f,"Raio requer direção não nula e distância de 0 a 1000000");
        const auto ignored=lua_gettop(L)>3&&!lua_isnil(L,4)?self.instance(L,4).id():0;
        Part* object=nullptr;Vector3 point{},normal{};
        if(!self.runtime->raycastParts(origin,direction,float(distance),object,point,normal,ignored)){lua_pushnil(L);return 1;}
        lua_createtable(L,0,4);self.pushInstance(L,object,true);lua_setfield(L,-2,"object");ModernVector(L,point);lua_setfield(L,-2,"point");ModernVector(L,normal);lua_setfield(L,-2,"normal");lua_pushnumber(L,Vector3Distance(origin,point));lua_setfield(L,-2,"distance");return 1;
    }
    static int Nearby(lua_State* L) {
        auto& self=Host(L);const auto center=ReadVector(L,1);const double radius=Number(L,2);
        Require(radius>=0&&radius<=1000000,"Raio de consulta fora do intervalo");lua_newtable(L);int index=0;
        for(const auto* part:self.runtime->scene().parts())if(Vector3DistanceSqr(center,part->position())<=radius*radius){self.pushInstance(L,part,true);lua_rawseti(L,-2,++index);}
        return 1;
    }
    static int Gravity(lua_State* L){lua_pushnumber(L,Host(L).runtime->settings().gravity);return 1;}
    static int SetGravity(lua_State* L) {
        auto& self=Host(L);const auto value=Number(L,1);std::string error;
        if(!self.runtime->setGravity(float(value),error))throw std::runtime_error(error);
        return 0;
    }
    static int ModernVec(lua_State* L) {
        const Vector3 value{float(Number(L,1)),float(Number(L,2)),float(Number(L,3))};
        Require(std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z)&&std::fabs(value.x)<=1000000&&std::fabs(value.y)<=1000000&&std::fabs(value.z)<=1000000,"Vetor fora do intervalo suportado");ModernVector(L,value);return 1;
    }
    static int Now(lua_State* L){lua_pushnumber(L,Host(L).time);return 1;}
    template<int Device,int Phase> static int InputButton(lua_State* L) {
        const char* name=Text(L,1);int code=-1;
        if constexpr(Device==0){for(const auto& button:game::ScriptKeys)if(Eq(name,button.name))code=button.code;}
        if constexpr(Device==1){for(const auto& button:game::ScriptMouseButtons)if(Eq(name,button.name))code=button.code;}
        if constexpr(Device==2){for(const auto& button:game::ScriptGamepadButtons)if(Eq(name,button.name))code=button.code;}
        Require(code>=0,"Nome de tecla/botão não suportado; consulte a API de entrada");
        auto& self=Host(L);const auto& current=self.runtime->scriptInput();const auto& previous=self.runtime->previousScriptInput();
        bool now=false,before=false;
        if constexpr(Device==0){now=current.keys[code];before=previous.keys[code];}
        if constexpr(Device==1){now=current.mouse[code];before=previous.mouse[code];}
        if constexpr(Device==2){now=current.gamepad&&current.gamepadButtons[code];before=previous.gamepad&&previous.gamepadButtons[code];}
        lua_pushboolean(L,Phase==0?now:Phase==1?now&&!before:!now&&before);return 1;
    }
    static int InputPointer(lua_State* L) {
        const auto& input=Host(L).runtime->scriptInput();lua_createtable(L,0,5);
        const char* names[]{"x","y","dx","dy","wheel"};const float values[]{input.pointer.x,input.pointer.y,input.delta.x,input.delta.y,input.wheel};
        for(int i=0;i<5;++i){lua_pushnumber(L,values[i]);lua_setfield(L,-2,names[i]);}return 1;
    }
    static int GamepadConnected(lua_State* L){lua_pushboolean(L,Host(L).runtime->scriptInput().gamepad);return 1;}
    static int GamepadAxis(lua_State* L) {
        const char* name=Text(L,1);int code=-1;for(const auto& axis:game::ScriptGamepadAxes)if(Eq(name,axis.name))code=axis.code;
        Require(code>=0,"Eixo de controle desconhecido");const auto& input=Host(L).runtime->scriptInput();
        lua_pushnumber(L,input.gamepad?input.gamepadAxes[code]:0);return 1;
    }
    static int After(lua_State* L) {
        auto& self=Host(L);const double delay=Number(L,1);Require(delay>=0&&delay<=3600,"Atraso deve estar entre 0 e 3600 segundos");
        Require(lua_isfunction(L,2),"after requer uma função");lua_remove(L,1);SpawnTask(L);self.jobs.back().wake=self.time+delay;self.push(L,Kind::Task,self.jobs.back().token,true);return 1;
    }
    static int Every(lua_State* L) {
        const auto seconds=Number(L,1);Require(seconds>0&&seconds<=3600,"every requer intervalo maior que zero e até 3600 segundos");
        Require(lua_isfunction(L,2),"every requer uma função");
        // Trusted scheduler wrapper; imported scripts still cannot load source.
        static constexpr char wrapper[]="return function(wait, seconds, callback, ...) while true do wait(seconds); callback(...) end end";
        const int loaded=luaL_loadbufferx(L,wrapper,sizeof(wrapper)-1,"@linblux/interval","t");
        Require(loaded==LUA_OK,"Não foi possível preparar o intervalo Lua");
        const int called=lua_pcall(L,0,1,0);Require(called==LUA_OK,"Não foi possível iniciar o intervalo Lua");
        lua_insert(L,1);lua_pushcfunction(L,Safe<Wait>);lua_insert(L,2);
        return ModernSpawn(L);
    }
    static int NativeSignal(const char* topic) {
        return Eq(topic,"start")?EngineStart:Eq(topic,"update")?Heartbeat:Eq(topic,"stop")?EngineStop:
            Eq(topic,"spawn")?Spawn:Eq(topic,"health_changed")?HealthChanged:Eq(topic,"death")?Died:Eq(topic,"destroy")?EngineDestroy:
            Eq(topic,"physics_update")?PhysicsUpdate:Eq(topic,"collision_enter")?CollisionEnter:Eq(topic,"collision_exit")?CollisionExit:
            Eq(topic,"key_pressed")?KeyPressed:Eq(topic,"key_released")?KeyReleased:
            Eq(topic,"mouse_pressed")?MousePressed:Eq(topic,"mouse_released")?MouseReleased:
            Eq(topic,"gamepad_pressed")?GamepadPressed:Eq(topic,"gamepad_released")?GamepadReleased:0;
    }
    static const char* Topic(lua_State* L) {
        const char* topic=Text(L,1);const auto length=lua_rawlen(L,1);
        Require(length>0&&length<=96&&std::strlen(topic)==length,"Evento requer nome de 1 a 96 bytes, sem NUL");return topic;
    }
    static int On(lua_State* L) {
        auto& self=Host(L);const char* topic=Topic(L);Require(lua_isfunction(L,2),"on requer callback");
        self.pruneListeners();
        Require(self.listeners.size()<self.limits.listeners,"event connection limit reached");
        const InstanceId owner=lua_gettop(L)>2&&!lua_isnil(L,3)?self.instance(L,3).id():0;
        Require(!Eq(topic,"destroy")||owner!=0,"O evento destroy requer um objeto de escopo");
        lua_pushvalue(L,2);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);const auto id=++self.nextConnection;
        self.listeners.emplace(id,Listener{NativeSignal(topic),ref,self.executing,0,owner,topic});self.push(L,Kind::Connection,id,true);return 1;
    }
    static int Publish(lua_State* L) {
        auto& self=Host(L);const char* topic=Topic(L);Require(!NativeSignal(topic),"Eventos de ciclo de vida são emitidos somente pela engine");
        if(lua_gettop(L)<2)lua_pushnil(L);
        for(const auto& pair:self.listeners) {
            const auto& listener=pair.second;if(listener.signal||listener.topic!=topic||!self.listenerAlive(listener))continue;
            Require(self.jobs.size()<self.limits.tasks,"Lua task limit reached");
            lua_State* thread=lua_newthread(L);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
            lua_rawgeti(thread,LUA_REGISTRYINDEX,listener.ref);lua_pushvalue(L,2);lua_xmove(L,thread,1);
            self.jobs.push_back({thread,ref,1,listener.script,self.time,false,pair.first});
        }
        return 0;
    }
    static int Connect(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);Require(h.kind==Kind::Signal,"expected signal");Require(lua_isfunction(L,2),"Connect requires callback");
        Require(self.listeners.size()<self.limits.listeners,"event connection limit reached");
        lua_pushvalue(L,2);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
        const auto id=++self.nextConnection;
        self.listeners.emplace(id,Listener{int(h.id),ref,self.executing,h.avatar,0,{}}); self.push(L,Kind::Connection,id);return 1;
    }
    static int Disconnect(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);Require(h.kind==Kind::Connection,"invalid connection");
        auto it=self.listeners.find(h.id);if(it!=self.listeners.end()){luaL_unref(L,LUA_REGISTRYINDEX,it->second.ref);self.listeners.erase(it);}return 0;
    }
    static int RequireModule(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);
        Require(object.instanceClass()==InstanceClass::ModuleScript,"require accepts a project ModuleScript handle, never a filesystem path");
        if(auto it=self.modules.find(object.id());it!=self.modules.end()){lua_rawgeti(L,LUA_REGISTRYINDEX,it->second);return 1;}
        Require(self.modules.size()<64 && self.moduleDepth<16,"module budget exceeded");
        Require(!self.loading.count(object.id()),"cyclic require");
        const auto& script=static_cast<const Script&>(object); char name[200];std::snprintf(name,sizeof(name),"@%s#%llu",script.name().c_str(),static_cast<unsigned long long>(script.id()));
        if(luaL_loadbufferx(L,script.source.data(),script.source.size(),name,"t")!=LUA_OK) throw std::runtime_error(lua_tostring(L,-1));
        self.environment(L,object.id()); self.loading.insert(object.id()); ++self.moduleDepth;
        const int status=lua_pcall(L,0,1,0);--self.moduleDepth;self.loading.erase(object.id());
        if(status!=LUA_OK)throw std::runtime_error(lua_tostring(L,-1));
        lua_pushvalue(L,-1);self.modules[object.id()]=luaL_ref(L,LUA_REGISTRYINDEX);return 1;
    }
    static int SetAnimation(lua_State* L) {
        auto& self=Host(L);Require(self.handle(L,1).kind==Kind::Animator,"expected Animator");
        const char* state=Text(L,2);const char* id=Text(L,3);std::string error;
        if (!self.runtime->setAnimation(state,id,error)) throw std::runtime_error(error);
        return 0;
    }
    static int Damage(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);Require(h.kind==Kind::Humanoid||(h.kind==Kind::Player&&h.modern),"expected character");
        const auto amount=Number(L,2);Require(amount>=0 && amount<=100000,"damage amount must be 0..100000");
        std::string error;if(!self.runtime->takeDamage(float(amount),error))throw std::runtime_error(error);return 0;
    }
    static int Heal(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);Require(h.kind==Kind::Humanoid||(h.kind==Kind::Player&&h.modern),"expected character");
        const auto amount=Number(L,2);Require(amount>=0 && amount<=100000,"heal amount must be 0..100000");
        std::string error;if(!self.runtime->heal(float(amount),error))throw std::runtime_error(error);return 0;
    }
    static int Respawn(lua_State* L) {
        auto& self=Host(L);Require(self.handle(L,1).kind==Kind::Player,"expected LocalPlayer");
        std::string error;if(!self.runtime->resetCharacter(error))throw std::runtime_error(error);return 0;
    }
    static int GetAnimation(lua_State* L) {
        auto& self=Host(L);const char* id=Text(L,2);const auto* clip=self.runtime->scene().gameAssets().findAnimation(id);
        Require(clip,"animation ID not present in this project");
        lua_createtable(L,0,7);const auto uri="anim://"+clip->id;
        lua_pushlstring(L,uri.data(),uri.size());lua_setfield(L,-2,"Id");lua_pushstring(L,clip->name.c_str());lua_setfield(L,-2,"Name");
        lua_pushstring(L,clip->rigType.c_str());lua_setfield(L,-2,"RigType");lua_pushnumber(L,clip->duration);lua_setfield(L,-2,"Duration");
        lua_pushboolean(L,clip->loop);lua_setfield(L,-2,"Loop");lua_pushinteger(L,clip->version);lua_setfield(L,-2,"Version");return 1;
    }
    static int Teleport(lua_State* L) {
        auto& self=Host(L);Require(self.handle(L,1).kind==Kind::Player,"Jogador inválido");const auto position=ReadVector(L,2);std::string error;
        if(!self.runtime->teleportPlayer(position,error))throw std::runtime_error(error);
        return 0;
    }
    static int Checkpoint(lua_State* L) {
        auto& self=Host(L);Require(self.handle(L,1).kind==Kind::Player,"Jogador inválido");
        const auto position=lua_gettop(L)>1?ReadVector(L,2):self.runtime->position();std::string error;
        if(!self.runtime->setCheckpoint(position,error))throw std::runtime_error(error);
        return 0;
    }
    static int PlayerAnimation(lua_State* L) {
        auto& self=Host(L);Require(self.handle(L,1).kind==Kind::Player,"Jogador inválido");const char* state=Text(L,2);
        const char* lower[]{"idle","walk","jump","fall"};const char* native[]{"Idle","Walk","Jump","Fall"};const char* selected=nullptr;
        for(int i=0;i<4;++i)if(Eq(state,lower[i]))selected=native[i];
        Require(selected,"Estado de animação desconhecido");const char* id=Text(L,3);std::string error;
        if(!self.runtime->setAnimation(selected,id,error))throw std::runtime_error(error);
        return 0;
    }
    static int Animations(lua_State* L) {
        auto& self=Host(L);lua_newtable(L);int index=0;
        for(const auto& clip:self.runtime->scene().gameAssets().animations) {
            lua_createtable(L,0,4);lua_pushstring(L,clip.id.c_str());lua_setfield(L,-2,"id");lua_pushstring(L,clip.name.c_str());lua_setfield(L,-2,"name");
            lua_pushnumber(L,clip.duration);lua_setfield(L,-2,"duration");lua_pushboolean(L,clip.loop);lua_setfield(L,-2,"loop");lua_rawseti(L,-2,++index);
        }
        return 1;
    }
    static int CameraState(lua_State* L) {
        auto& self=Host(L);const auto camera=self.runtime->playerCamera().camera();lua_createtable(L,0,5);
        ModernVector(L,camera.position);lua_setfield(L,-2,"position");ModernVector(L,camera.target);lua_setfield(L,-2,"target");
        ModernVector(L,camera.up);lua_setfield(L,-2,"up");ModernVector(L,Vector3Scale(QuaternionToEuler(self.runtime->playerCamera().orientation()),RAD2DEG));lua_setfield(L,-2,"rotation");
        lua_pushnumber(L,camera.fovy);lua_setfield(L,-2,"fov");lua_pushboolean(L,self.runtime->playerCamera().scripted());lua_setfield(L,-2,"scripted");lua_pushboolean(L,self.runtime->playerCamera().transitioning());lua_setfield(L,-2,"transitioning");
        lua_pushnumber(L,self.runtime->playerCamera().distance());lua_setfield(L,-2,"distance");lua_pushboolean(L,self.runtime->playerCamera().firstPerson());lua_setfield(L,-2,"first_person");
        lua_pushboolean(L,!self.runtime->playerCamera().scripted()&&(self.runtime->settings().cameraLock||self.runtime->shiftLockActive()));lua_setfield(L,-2,"locked");return 1;
    }
    static int CameraPose(lua_State* L) {
        auto& self=Host(L);Require(lua_istable(L,1),"set_pose requer tabela");
        auto pose=self.runtime->playerCamera().camera();if(!self.runtime->playerCamera().scripted())pose.fovy=self.runtime->settings().cameraFov;
        double seconds=0;bool position=false,target=false,rotation=false,up=false;Vector3 angles{};
        lua_pushnil(L);while(lua_next(L,1)){
            const char* key=Text(L,-2);
            if(Eq(key,"position")){pose.position=ReadVector(L,lua_gettop(L));position=true;}
            else if(Eq(key,"target")){pose.target=ReadVector(L,lua_gettop(L));target=true;}
            else if(Eq(key,"rotation")){angles=ReadVector(L,lua_gettop(L));rotation=true;}
            else if(Eq(key,"up")){pose.up=ReadVector(L,lua_gettop(L));up=true;}
            else if(Eq(key,"fov"))pose.fovy=float(Number(L,-1));
            else if(Eq(key,"duration"))seconds=Number(L,-1);
            else throw std::runtime_error("Opção de pose de câmera desconhecida");
            lua_pop(L,1);
        }
        Require(position&&(target!=rotation)&&!(rotation&&up),"Informe position e target ou rotation; up só acompanha target");
        if(rotation){const auto q=QuaternionFromEuler(angles.x*DEG2RAD,angles.y*DEG2RAD,angles.z*DEG2RAD);pose.target=Vector3Add(pose.position,Vector3RotateByQuaternion({0,0,-1},q));pose.up=Vector3RotateByQuaternion({0,1,0},q);}
        std::string error;if(!self.runtime->playerCamera().setPose(pose,float(seconds),error))throw std::runtime_error(error);return 0;
    }
    static int CameraRelease(lua_State* L){auto& self=Host(L);self.runtime->playerCamera().release(self.runtime->characterRoot().position,self.runtime->settings());return 0;}
    static int CameraConfigure(lua_State* L) {
        auto& self=Host(L);Require(lua_istable(L,1),"Configuração da câmera requer tabela");auto settings=self.runtime->settings();
        lua_pushnil(L);while(lua_next(L,1)) {
            const char* key=Text(L,-2);
            if(Eq(key,"distance"))settings.cameraDistance=float(Number(L,-1));
            else if(Eq(key,"sensitivity"))settings.sensitivity=float(Number(L,-1));
            else if(Eq(key,"fov"))settings.cameraFov=float(Number(L,-1));
            else if(Eq(key,"min_distance"))settings.minZoom=float(Number(L,-1));
            else if(Eq(key,"max_distance"))settings.maxZoom=float(Number(L,-1));
            else if(Eq(key,"mode")){const char* mode=Text(L,-1);Require(Eq(mode,"orbit")||Eq(mode,"follow")||Eq(mode,"first_person"),"Modo de câmera desconhecido");settings.cameraMode=Eq(mode,"orbit")?game::CameraMode::Classic:Eq(mode,"follow")?game::CameraMode::Follow:game::CameraMode::FirstPerson;}
            else if(Eq(key,"allow_first_person")||Eq(key,"locked")||Eq(key,"allow_shift_lock")) {
                Require(lua_isboolean(L,-1),"Opção de câmera requer booleano");const bool value=lua_toboolean(L,-1);
                if(Eq(key,"locked"))settings.cameraLock=value;else if(Eq(key,"allow_shift_lock"))settings.enableShiftLock=value;else settings.allowFirstPerson=value;
            }else throw std::runtime_error("Opção de câmera desconhecida");
            lua_pop(L,1);
        }
        std::string error;if(!settings.validate(error))throw std::runtime_error(error);
        self.runtime->settings()=settings;self.runtime->playerCamera().setDistance(settings.cameraDistance);return 0;
    }
    static const char* PlayerKey(const char* key) {
        const std::pair<const char*,const char*> aliases[]{{"health","Health"},{"max_health","MaxHealth"},{"walk_speed","WalkSpeed"},{"jump_height","JumpHeight"},{"auto_rotate","AutoRotate"}};
        for(const auto& pair:aliases)if(Eq(key,pair.first))return pair.second;
        return key;
    }
    static const char* ModernKey(const char* key) {
        const std::pair<const char*,const char*> aliases[]{
            {"name","Name"},{"parent","Parent"},{"position","Position"},{"rotation","Rotation"},{"size","Size"},
            {"color","Color"},{"transparency","Transparency"},{"anchored","Anchored"},{"collidable","CanCollide"},{"material","Material"},{"enabled","Enabled"},
            {"children","GetChildren"},{"descendants","GetDescendants"},{"find_child","FindFirstChild"},{"clone","Clone"},
            {"destroy","Destroy"},{"get_attribute","GetAttribute"},{"set_attribute","SetAttribute"}};
        for(const auto& pair:aliases)if(Eq(key,pair.first))return pair.second;
        return key;
    }
    static int Index(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);const char* key=Text(L,2);
        if(h.modern&&h.kind==Kind::Instance) {
            if(Eq(key,"alive")){lua_pushboolean(L,self.runtime->scene().find(h.id)!=nullptr);return 1;}
            if(Eq(key,"id")){self.instance(L,1);lua_pushinteger(L,lua_Integer(h.id));return 1;}
            if(Eq(key,"type")){auto& object=self.instance(L,1);lua_pushstring(L,object.isPart()?"part":object.instanceClass()==InstanceClass::Folder?"group":object.instanceClass()==InstanceClass::Model?"model":IsScriptClass(object.instanceClass())?"script":"service");return 1;}
            if(Eq(key,"velocity")){auto& object=self.instance(L,1);Require(object.isPart(),"Velocidade requer uma peça");ModernVector(L,self.runtime->partVelocity(object.id()));return 1;}
            if(Eq(key,"apply_force")){lua_pushcfunction(L,Safe<Motion<game::PartMotion::Force>>);return 1;}
            if(Eq(key,"apply_impulse")){lua_pushcfunction(L,Safe<Motion<game::PartMotion::Impulse>>);return 1;}
            key=ModernKey(key);
        }
#define METHOD(name,fn) if(Eq(key,name)){lua_pushcfunction(L,Safe<fn>);return 1;}
        if(h.kind==Kind::Task){
            METHOD("cancel",CancelTask)
            if(Eq(key,"active")){
                bool active=false;for(const auto& job:self.jobs)if(job.token==h.id)active=!job.done&&self.scriptAlive(job.script);
                lua_pushboolean(L,active);return 1;
            }
        }
        if(h.kind==Kind::Player&&h.modern) {
            METHOD("damage",Damage) METHOD("heal",Heal) METHOD("respawn",Respawn) METHOD("teleport",Teleport) METHOD("set_checkpoint",Checkpoint) METHOD("set_animation",PlayerAnimation)
            if(Eq(key,"position")){ModernVector(L,self.runtime->position());return 1;}
            if(Eq(key,"velocity")){ModernVector(L,self.runtime->velocity());return 1;}
            if(Eq(key,"grounded")){lua_pushboolean(L,self.runtime->grounded());return 1;}
            if(Eq(key,"name")){lua_pushstring(L,self.runtime->rig().name.c_str());return 1;}
            if(Eq(key,"generation")){lua_pushinteger(L,self.runtime->characterGeneration());return 1;}
            if(Eq(key,"state")){const char* states[]{"idle","walking","jumping","falling","landing","dead"};lua_pushstring(L,states[int(self.runtime->humanoidState())]);return 1;}
            key=PlayerKey(key);
        }
        if(h.kind==Kind::Game){METHOD("GetService",GetService)}
        if(h.kind==Kind::Signal){METHOD("Connect",Connect)}
        if(h.kind==Kind::Connection){METHOD("Disconnect",Disconnect) METHOD("disconnect",Disconnect)
            if(Eq(key,"connected")){const auto it=self.listeners.find(h.id);lua_pushboolean(L,it!=self.listeners.end()&&self.listenerAlive(it->second));return 1;}}
        if(h.kind==Kind::Animator){METHOD("SetAnimation",SetAnimation)}
        if(h.kind==Kind::AnimationService){METHOD("GetAnimation",GetAnimation)}
        if(h.kind==Kind::Players || h.kind==Kind::Player) {
            if(h.kind==Kind::Player){METHOD("Respawn",Respawn)}
            if(Eq(key,"CharacterAdded")){self.push(L,Kind::Signal,Spawn);return 1;}
            if(Eq(key,"LocalPlayer")){self.push(L,Kind::Player);return 1;}
            if(Eq(key,"Character")){self.push(L,Kind::Character);return 1;}
        }
        if(h.kind==Kind::RunService && Eq(key,"Heartbeat")){self.push(L,Kind::Signal,Heartbeat);return 1;}
        if(h.kind==Kind::Character) {
            if(Eq(key,"Humanoid")){self.push(L,Kind::Humanoid);return 1;}
            if(Eq(key,"Animator")){self.push(L,Kind::Animator);return 1;}
            if(Eq(key,"Name")){lua_pushstring(L,self.runtime->rig().name.c_str());return 1;}
        }
        if(h.kind==Kind::Humanoid||(h.kind==Kind::Player&&h.modern)) {
            METHOD("TakeDamage",Damage) METHOD("Heal",Heal)
            if(Eq(key,"Health")){lua_pushnumber(L,self.runtime->health());return 1;}
            if(Eq(key,"MaxHealth")){lua_pushnumber(L,self.runtime->settings().maxHealth);return 1;}
            if(Eq(key,"HealthDisplayType")){const char* values[]{"AlwaysOn","DisplayWhenDamaged","AlwaysOff"};lua_pushstring(L,values[self.runtime->settings().healthDisplayType]);return 1;}
            if(Eq(key,"HealthDisplayDistance")){lua_pushnumber(L,self.runtime->settings().healthDisplayDistance);return 1;}
            if(Eq(key,"NameDisplayDistance")){lua_pushnumber(L,self.runtime->settings().nameDisplayDistance);return 1;}
            if(Eq(key,"HealthChanged")){self.push(L,Kind::Signal,HealthChanged);return 1;}
            if(Eq(key,"Died")){self.push(L,Kind::Signal,Died);return 1;}
            if(Eq(key,"WalkSpeed")){lua_pushnumber(L,self.runtime->settings().walkSpeed);return 1;}
            if(Eq(key,"JumpHeight")){lua_pushnumber(L,self.runtime->settings().jumpHeight);return 1;}
            if(Eq(key,"AutoRotate")){lua_pushboolean(L,self.runtime->settings().autoRotate);return 1;}
            if(Eq(key,"State")){lua_pushstring(L,game::StateName(self.runtime->humanoidState()));return 1;}
        }
        if(h.kind==Kind::Instance) {
            METHOD("FindFirstChild",Find) METHOD("GetChildren",Children) METHOD("GetDescendants",Descendants)
            METHOD("Destroy",Destroy) METHOD("Clone",Clone) METHOD("GetAttribute",GetAttribute) METHOD("SetAttribute",SetAttribute)
            auto& object=self.instance(L,1);
            if(Eq(key,"Name")){lua_pushlstring(L,object.name().data(),object.name().size());return 1;}
            if(Eq(key,"ClassName")){lua_pushstring(L,object.className());return 1;}
            if(Eq(key,"Parent")){self.pushInstance(L,object.parent(),h.modern);return 1;}
            if(h.modern&&IsScriptClass(object.instanceClass())&&Eq(key,"Enabled")){lua_pushboolean(L,static_cast<const Script&>(object).enabled);return 1;}
            if(object.instanceClass()==InstanceClass::Workspace && Eq(key,"FallenPartsDestroyHeight")) {
                lua_pushnumber(L,self.runtime->scene().workspace()->fallenPartsDestroyHeight());return 1;
            }
            if(object.isPart()) {
                const auto& part=static_cast<const Part&>(object);
                if(Eq(key,"Position")){if(h.modern)ModernVector(L,part.position());else Vector(L,part.position());return 1;}
                if(Eq(key,"Size")){if(h.modern)ModernVector(L,part.size());else Vector(L,part.size());return 1;}
                if(Eq(key,"Rotation")){if(h.modern)ModernVector(L,part.rotationEulerDegrees());else Vector(L,part.rotationEulerDegrees());return 1;}
                if(Eq(key,"Anchored")){lua_pushboolean(L,part.anchored());return 1;}
                if(Eq(key,"CanCollide")){lua_pushboolean(L,part.canCollide());return 1;}
                if(Eq(key,"Transparency")){lua_pushnumber(L,1-part.color().a/255.f);return 1;}
                if(Eq(key,"Color")){const auto c=part.color();const Vector3 value{float(c.r),float(c.g),float(c.b)};if(h.modern)ModernVector(L,value);else Vector(L,value);return 1;}
                if(h.modern&&Eq(key,"Material")){const char* names[]{"smooth","plastic","wood","metal","concrete","emissive"};lua_pushstring(L,names[int(part.material())]);return 1;}
            }
            for(auto* child:object.children()) if(child->name()==key){self.pushInstance(L,child,h.modern);return 1;}
        }
#undef METHOD
        lua_pushnil(L);return 1;
    }
    static int NewIndex(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);const char* key=Text(L,2);
        if(h.modern&&h.kind==Kind::Instance&&Eq(key,"velocity")) {
            auto& object=self.instance(L,1);Require(object.isPart(),"Velocidade requer uma peça");const auto value=ReadVector(L,3);std::string error;
            if(!self.runtime->partMotion(object.id(),value,game::PartMotion::Velocity,error))throw std::runtime_error(error);
            return 0;
        }
        if(h.modern&&h.kind==Kind::Instance)key=ModernKey(key);
        if(h.kind==Kind::Player&&h.modern)key=PlayerKey(key);
        if(h.kind==Kind::Humanoid||(h.kind==Kind::Player&&h.modern)) {
            if(Eq(key,"Health")) {const auto value=Number(L,3);Require(std::fabs(value)<=100000,"Health outside range");std::string error;if(!self.runtime->setHealth(float(value),error))throw std::runtime_error(error);return 0;}
            auto settings=self.runtime->settings();
            if(Eq(key,"MaxHealth")){settings.maxHealth=float(Number(L,3));settings.health=std::min(settings.health,settings.maxHealth);}
            else if(Eq(key,"HealthDisplayDistance"))settings.healthDisplayDistance=float(Number(L,3));
            else if(Eq(key,"NameDisplayDistance"))settings.nameDisplayDistance=float(Number(L,3));
            else if(Eq(key,"HealthDisplayType")) {
                const char* type=Text(L,3);Require(Eq(type,"AlwaysOn")||Eq(type,"DisplayWhenDamaged")||Eq(type,"AlwaysOff"),"invalid HealthDisplayType");
                settings.healthDisplayType=Eq(type,"AlwaysOn")?0:Eq(type,"DisplayWhenDamaged")?1:2;
            }
            else if(Eq(key,"WalkSpeed"))settings.walkSpeed=float(Number(L,3));
            else if(Eq(key,"JumpHeight"))settings.jumpHeight=float(Number(L,3));
            else if(Eq(key,"AutoRotate")){Require(lua_isboolean(L,3),"AutoRotate expects boolean");settings.autoRotate=lua_toboolean(L,3);}
            else throw std::runtime_error("unsupported or readonly Humanoid property");
            std::string error;Require(settings.validate(error),"Humanoid property outside valid limits");self.runtime->settings()=settings;
            if(self.runtime->health()>settings.maxHealth)self.runtime->setHealth(settings.maxHealth,error);
            return 0;
        }
        Require(h.kind==Kind::Instance,"readonly service property");auto& object=self.instance(L,1);
        if(Eq(key,"Name")){const char* name=Text(L,3);const auto size=std::strlen(name);Require(*name && size<=128,"Name length must be 1..128");self.nativeBudget(0,size>object.name().size()?size-object.name().size():0);object.setName(name);return 0;}
        if(Eq(key,"Parent")){auto& parent=self.instance(L,3);std::string error;const bool ok=self.runtime->mutableScene().reparent(&object,&parent,error);if(!ok)throw std::runtime_error(error);return 0;}
        if(h.modern&&IsScriptClass(object.instanceClass())&&Eq(key,"Enabled")){
            Require(lua_isboolean(L,3),"enabled requer booleano");static_cast<Script&>(object).enabled=lua_toboolean(L,3);
            if(!static_cast<Script&>(object).enabled){self.startedScripts.erase(object.id());self.cancelScript(object.id());}
            return 0;
        }
        if(object.instanceClass()==InstanceClass::Workspace && Eq(key,"FallenPartsDestroyHeight")) {
            const auto height=Number(L,3);Require(std::fabs(height)<=100000,"void height outside supported range");
            self.runtime->mutableScene().workspace()->setFallenPartsDestroyHeight(float(height));return 0;
        }
        Require(object.isPart(),"unknown or readonly Instance property");auto& part=static_cast<Part&>(object);
        if(Eq(key,"Position"))part.setPosition(ReadVector(L,3));
        else if(Eq(key,"Size")){auto size=ReadVector(L,3);Require(size.x>=kMinPartSize&&size.y>=kMinPartSize&&size.z>=kMinPartSize,"Size below 0.2");part.setSize(size);}
        else if(Eq(key,"Rotation"))part.setRotationEulerDegrees(ReadVector(L,3));
        else if(Eq(key,"Color")){auto c=ReadVector(L,3);Require(c.x>=0&&c.y>=0&&c.z>=0&&c.x<=255&&c.y<=255&&c.z<=255,"Color channels must be 0..255");part.setColor({static_cast<unsigned char>(c.x),static_cast<unsigned char>(c.y),static_cast<unsigned char>(c.z),part.color().a});}
        else if(Eq(key,"Transparency")){const auto n=Number(L,3);Require(n>=0&&n<=1,"Transparency must be 0..1");auto c=part.color();c.a=static_cast<unsigned char>((1-n)*255);part.setColor(c);}
        else if(h.modern&&Eq(key,"Material"))part.setMaterial(Material(L,3));
        else if(Eq(key,"Anchored")||Eq(key,"CanCollide")){Require(lua_isboolean(L,3),"expected boolean");if(Eq(key,"Anchored"))part.setAnchored(lua_toboolean(L,3));else part.setCanCollide(lua_toboolean(L,3));}
        else throw std::runtime_error("unknown or readonly Part property");
        return 0;
    }
    static int Setup(lua_State* L) {
        auto& self=Host(L);
        luaL_requiref(L,"_G",luaopen_base,1);lua_pop(L,1);
        luaL_requiref(L,"math",luaopen_math,1);lua_pop(L,1);
        luaL_requiref(L,"string",luaopen_string,1);lua_pop(L,1);
        luaL_requiref(L,"table",luaopen_table,1);lua_pop(L,1);
        luaL_requiref(L,"utf8",luaopen_utf8,1);lua_pop(L,1);
        for(const char* forbidden:{"dofile","loadfile","load","collectgarbage","pcall","xpcall"}){lua_pushnil(L);lua_setglobal(L,forbidden);}
        lua_getglobal(L,"string");lua_pushnil(L);lua_setfield(L,-2,"dump");lua_pop(L,1);
        luaL_newmetatable(L,"Loblox.Handle");lua_pushcfunction(L,Safe<Index>);lua_setfield(L,-2,"__index");
        lua_pushcfunction(L,Safe<NewIndex>);lua_setfield(L,-2,"__newindex");lua_pushcfunction(L,Safe<Equal>);lua_setfield(L,-2,"__eq");
        lua_pushliteral(L,"locked");lua_setfield(L,-2,"__metatable");lua_pop(L,1);
        lua_pushcfunction(L,Safe<Print>);lua_setglobal(L,"print");lua_pushcfunction(L,Safe<Print>);lua_setglobal(L,"warn");
        lua_pushcfunction(L,Safe<SetMetatable>);lua_setglobal(L,"setmetatable");
        lua_pushcfunction(L,Safe<RequireModule>);lua_setglobal(L,"require");
        lua_newtable(L);lua_pushcfunction(L,Safe<SpawnTask>);lua_setfield(L,-2,"spawn");lua_pushcfunction(L,Safe<Wait>);lua_setfield(L,-2,"wait");lua_setglobal(L,"task");
        lua_newtable(L);lua_pushcfunction(L,Safe<NewVector>);lua_setfield(L,-2,"new");lua_setglobal(L,"Vector3");
        lua_newtable(L);lua_pushcfunction(L,Safe<NewColor>);lua_setfield(L,-2,"fromRGB");lua_setglobal(L,"Color3");
        lua_newtable(L);lua_pushcfunction(L,Safe<NewInstance>);lua_setfield(L,-2,"new");lua_setglobal(L,"Instance");
        self.push(L,Kind::Game);lua_setglobal(L,"game");self.pushInstance(L,self.runtime->scene().workspace());lua_setglobal(L,"workspace");
        lua_newtable(L);lua_pushinteger(L,1);lua_setfield(L,-2,"api_version");lua_pushcfunction(L,Safe<ModernVec>);lua_setfield(L,-2,"vec3");
        self.push(L,Kind::Player,0,true);lua_setfield(L,-2,"player");
        lua_newtable(L);lua_pushcfunction(L,Safe<CameraState>);lua_setfield(L,-2,"state");lua_pushcfunction(L,Safe<CameraConfigure>);lua_setfield(L,-2,"configure");lua_pushcfunction(L,Safe<CameraPose>);lua_setfield(L,-2,"set_pose");lua_pushcfunction(L,Safe<CameraRelease>);lua_setfield(L,-2,"release");lua_setfield(L,-2,"camera");
        lua_newtable(L);lua_pushcfunction(L,Safe<Animations>);lua_setfield(L,-2,"list");lua_setfield(L,-2,"animations");
        lua_newtable(L);lua_pushcfunction(L,Safe<ModernNew>);lua_setfield(L,-2,"create");lua_pushcfunction(L,Safe<ModernFind>);lua_setfield(L,-2,"find");self.pushInstance(L,self.runtime->scene().workspace(),true);lua_setfield(L,-2,"root");lua_setfield(L,-2,"scene");
        lua_newtable(L);lua_pushcfunction(L,Safe<On>);lua_setfield(L,-2,"on");lua_pushcfunction(L,Safe<Publish>);lua_setfield(L,-2,"emit");lua_setfield(L,-2,"events");
        lua_newtable(L);lua_pushcfunction(L,Safe<Raycast>);lua_setfield(L,-2,"raycast");lua_pushcfunction(L,Safe<Nearby>);lua_setfield(L,-2,"nearby");lua_pushcfunction(L,Safe<Gravity>);lua_setfield(L,-2,"gravity");lua_pushcfunction(L,Safe<SetGravity>);lua_setfield(L,-2,"set_gravity");lua_setfield(L,-2,"physics");
        lua_newtable(L);lua_pushcfunction(L,(Safe<InputButton<0,0>>));lua_setfield(L,-2,"key_down");lua_pushcfunction(L,(Safe<InputButton<0,1>>));lua_setfield(L,-2,"key_pressed");lua_pushcfunction(L,(Safe<InputButton<0,2>>));lua_setfield(L,-2,"key_released");
        lua_pushcfunction(L,(Safe<InputButton<1,0>>));lua_setfield(L,-2,"mouse_down");lua_pushcfunction(L,(Safe<InputButton<1,1>>));lua_setfield(L,-2,"mouse_pressed");lua_pushcfunction(L,(Safe<InputButton<1,2>>));lua_setfield(L,-2,"mouse_released");
        lua_pushcfunction(L,(Safe<InputButton<2,0>>));lua_setfield(L,-2,"gamepad_down");lua_pushcfunction(L,(Safe<InputButton<2,1>>));lua_setfield(L,-2,"gamepad_pressed");lua_pushcfunction(L,(Safe<InputButton<2,2>>));lua_setfield(L,-2,"gamepad_released");
        lua_pushcfunction(L,Safe<InputPointer>);lua_setfield(L,-2,"pointer");lua_pushcfunction(L,Safe<GamepadConnected>);lua_setfield(L,-2,"gamepad_connected");lua_pushcfunction(L,Safe<GamepadAxis>);lua_setfield(L,-2,"gamepad_axis");lua_setfield(L,-2,"input");
        lua_newtable(L);lua_pushcfunction(L,Safe<Now>);lua_setfield(L,-2,"now");lua_pushcfunction(L,Safe<Wait>);lua_setfield(L,-2,"wait");lua_pushcfunction(L,Safe<After>);lua_setfield(L,-2,"after");lua_pushcfunction(L,Safe<ModernSpawn>);lua_setfield(L,-2,"spawn");lua_pushcfunction(L,Safe<Every>);lua_setfield(L,-2,"every");lua_pushcfunction(L,Safe<CancelTask>);lua_setfield(L,-2,"cancel");lua_setfield(L,-2,"time");lua_setglobal(L,"linblux");return 0;
    }
    static int Emit(lua_State* L) {
        auto& self=Host(L);const int signal=int(lua_tointeger(L,1)),ref=int(lua_tointeger(L,2));
        const auto value=lua_tonumber(L,3);const auto connection=InstanceId(lua_tointeger(L,4));lua_settop(L,0);lua_rawgeti(L,LUA_REGISTRYINDEX,ref);
        if(signal==Spawn){if(connection)self.push(L,Kind::Player,0,true);else self.push(L,Kind::Character);}else lua_pushnumber(L,value);
        SpawnTask(L);self.jobs.back().connection=connection;return 0;
    }
    static int Trace(lua_State* L) {
        auto* thread=static_cast<lua_State*>(lua_touserdata(L,1));
        luaL_traceback(L,thread,lua_tostring(thread,-1),1);return 1;
    }
    static int EmitDestroyed(lua_State* L) {
        const int ref=int(lua_tointeger(L,1));const auto id=lua_tointeger(L,2);const auto connection=InstanceId(lua_tointeger(L,3));lua_settop(L,0);
        lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_pushinteger(L,id);SpawnTask(L);Host(L).jobs.back().connection=connection;return 0;
    }
    static int EmitContact(lua_State* L) {
        auto& self=Host(L);const int ref=int(lua_tointeger(L,1));const auto first=InstanceId(lua_tointeger(L,2)),second=InstanceId(lua_tointeger(L,3)),connection=InstanceId(lua_tointeger(L,4));lua_settop(L,0);
        lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_createtable(L,0,4);
        const InstanceId ids[]{first,second};const char* names[]{"first","second"};const char* keys[]{"first_id","second_id"};
        for(int i=0;i<2;++i){
            if(ids[i])self.pushInstance(L,self.runtime->scene().find(ids[i]),true);else self.push(L,Kind::Player,0,true);
            lua_setfield(L,-2,names[i]);lua_pushinteger(L,lua_Integer(ids[i]));lua_setfield(L,-2,keys[i]);
        }
        SpawnTask(L);self.jobs.back().connection=connection;return 0;
    }
    static int EmitInput(lua_State* L) {
        auto& self=Host(L);const int ref=int(lua_tointeger(L,1));const auto connection=InstanceId(lua_tointeger(L,3));
        const auto* name=static_cast<const char*>(lua_touserdata(L,2));lua_settop(L,0);
        lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_pushstring(L,name);
        SpawnTask(L);self.jobs.back().connection=connection;return 0;
    }
    bool scriptAlive(InstanceId id) const {
        if(!id)return true;
        const auto* object=runtime->scene().find(id);
        if(!object||!IsScriptClass(object->instanceClass())||!static_cast<const Script*>(object)->enabled)return false;
        const auto* service=runtime->scene().service(InstanceClass::ServerScriptService);
        return runtime->scene().workspace()->isAncestorOf(object)||(service&&service->isAncestorOf(object));
    }
    bool listenerAlive(const Listener& listener) const {
        return scriptAlive(listener.script)&&(!listener.owner||runtime->scene().find(listener.owner))&&
            (!listener.avatar||listener.avatar==runtime->characterGeneration());
    }
    void pruneListeners() {
        for(auto it=listeners.begin();it!=listeners.end();) {
            auto& listener=it->second;
            if(!listenerAlive(listener)) {
                if(listener.signal==EngineDestroy&&scriptAlive(listener.script)&&listener.owner&&!runtime->scene().find(listener.owner)) {
                    const auto previous=executing;executing=listener.script;
                    lua_pushcfunction(state,Safe<EmitDestroyed>);lua_pushinteger(state,listener.ref);lua_pushinteger(state,lua_Integer(listener.owner));lua_pushinteger(state,lua_Integer(it->first));
                    const bool queued=lua_pcall(state,3,0,0)==LUA_OK;
                    if(!queued){message(true,executing,lua_tostring(state,-1));lua_pop(state,1);}
                    executing=previous;
                    if(queued){listener.owner=0;listener.once=true;++it;continue;}
                }
                luaL_unref(state,LUA_REGISTRYINDEX,listener.ref);it=listeners.erase(it);
            }else ++it;
        }
    }
    void event(int signal,float value=0,InstanceId onlyScript=0) {
        if(!state)return;
        pruneListeners();
        for(const auto& pair:listeners) {
            const auto& listener=pair.second;
            if(onlyScript&&listener.script!=onlyScript)continue;
            if(listener.signal!=signal || (listener.avatar && listener.avatar!=runtime->characterGeneration()))continue;
            executing=listener.script; lua_pushcfunction(state,Safe<Emit>);
            // Legacy signals already emitted before respawn retain their original
            // delivery semantics. New subscriptions support cancellation of queued work.
            lua_pushinteger(state,signal);lua_pushinteger(state,listener.ref);lua_pushnumber(state,value);lua_pushinteger(state,lua_Integer(listener.topic.empty()?0:pair.first));
            if(lua_pcall(state,4,0,0)!=LUA_OK){message(true,executing,lua_tostring(state,-1));lua_pop(state,1);}
        }
    }
    bool hasSignal(int signal) const {
        for(const auto& entry:listeners)if(entry.second.signal==signal&&listenerAlive(entry.second))return true;
        return false;
    }
    void inputEvent(int signal,const char* name) {
        for(const auto& entry:listeners){
            const auto& listener=entry.second;if(listener.signal!=signal||!listenerAlive(listener))continue;
            executing=listener.script;lua_pushcfunction(state,Safe<EmitInput>);lua_pushinteger(state,listener.ref);lua_pushlightuserdata(state,const_cast<char*>(name));lua_pushinteger(state,lua_Integer(entry.first));
            if(lua_pcall(state,3,0,0)!=LUA_OK){message(true,executing,lua_tostring(state,-1));lua_pop(state,1);}
        }
        executing=0;
    }
    void inputEvents() {
        const auto& current=runtime->scriptInput();const auto& previous=runtime->previousScriptInput();
        for(const auto& key:game::ScriptKeys)if(current.keys[key.code]!=previous.keys[key.code])inputEvent(current.keys[key.code]?KeyPressed:KeyReleased,key.name);
        for(const auto& button:game::ScriptMouseButtons)if(current.mouse[button.code]!=previous.mouse[button.code])inputEvent(current.mouse[button.code]?MousePressed:MouseReleased,button.name);
        for(const auto& button:game::ScriptGamepadButtons){
            const bool now=current.gamepad&&current.gamepadButtons[button.code],before=previous.gamepad&&previous.gamepadButtons[button.code];
            if(now!=before)inputEvent(now?GamepadPressed:GamepadReleased,button.name);
        }
    }
    void contact(InstanceId first,InstanceId second,bool entering) {
        if(!state)return;
        pruneListeners();const int signal=entering?CollisionEnter:CollisionExit;
        for(const auto& entry:listeners) {
            const auto& listener=entry.second;if(listener.signal!=signal||!listenerAlive(listener))continue;
            if(listener.owner){
                const auto* owner=runtime->scene().find(listener.owner);const auto* a=runtime->scene().find(first);const auto* b=runtime->scene().find(second);
                if(owner!=a&&owner!=b&&!(a&&owner->isAncestorOf(a))&&!(b&&owner->isAncestorOf(b)))continue;
            }
            if(jobs.size()>=limits.tasks){
                if(!contactOverflowReported){message(true,listener.script,"Fila de contatos Lua cheia; notificações excedentes descartadas");contactOverflowReported=true;}
                break; // bounded queue, never execute Lua during Bullet inspection
            }
            const auto previous=executing;executing=listener.script;
            lua_pushcfunction(state,Safe<EmitContact>);lua_pushinteger(state,listener.ref);lua_pushinteger(state,lua_Integer(first));lua_pushinteger(state,lua_Integer(second));lua_pushinteger(state,lua_Integer(entry.first));
            if(lua_pcall(state,4,0,0)!=LUA_OK){message(true,executing,lua_tostring(state,-1));lua_pop(state,1);}
            executing=previous;
        }
    }
    void tick(float dt,bool resetBudget=true) {
        if(!state)return;
        pruneListeners();
        time+=dt;if(resetBudget)frameBudget=limits.instructionsPerFrame;
        const auto count=jobs.size();
        for(std::size_t i=0;i<count;++i) {
            auto& job=jobs[i];
            const auto listener=job.connection?listeners.find(job.connection):listeners.end();
            if(!scriptAlive(job.script)||(job.connection&&(listener==listeners.end()||!listenerAlive(listener->second))))job.done=true;
            if(job.done || job.wake>time)continue;
            if(frameBudget<=0)break;
            executing=job.script;instructions=limits.instructionsPerTask;lua_sethook(job.thread,Hook,LUA_MASKCOUNT,1000);
            int results=0;const int status=lua_resume(job.thread,nullptr,job.arguments,&results);job.arguments=0;
            if(status==LUA_YIELD) {
                const double wait=results>0 && lua_isnumber(job.thread,-1)?lua_tonumber(job.thread,-1):0;
                job.wake=time+std::max(.0001,wait);lua_pop(job.thread,results);
            } else {
                job.done=true;
                if(status!=LUA_OK) {
                    lua_pushcfunction(state,Safe<Trace>);lua_pushlightuserdata(state,job.thread);
                    if(lua_pcall(state,1,1,0)==LUA_OK)message(true,job.script,lua_tostring(state,-1));
                    else message(true,job.script,lua_tostring(job.thread,-1));
                    lua_pop(state,1);
                }
            }
        }
        for(auto it=jobs.begin();it!=jobs.end();) {
            if(it->done){
                if(it->connection){const auto listener=listeners.find(it->connection);if(listener!=listeners.end()&&listener->second.once){luaL_unref(state,LUA_REGISTRYINDEX,listener->second.ref);listeners.erase(listener);}}
                luaL_unref(state,LUA_REGISTRYINDEX,it->ref);it=jobs.erase(it);
            }else ++it;
        }
        executing=0;
    }
    void close() {
        jobs.clear();listeners.clear();modules.clear();loading.clear();startedScripts.clear();
        if(state){closing=true;instructions=limits.instructionsPerTask;frameBudget=limits.instructionsPerFrame;lua_sethook(state,Hook,LUA_MASKCOUNT,1000);lua_close(state);}
        jobs.clear();listeners.clear();modules.clear();loading.clear();startedScripts.clear();
        state=nullptr;memory=0;runtime=nullptr;stopping=closing=false;
    }
    void stop() {
        if(state&&runtime) {
            // Stop cancels pending work, then runs one bounded, non-yielding cleanup pass.
            for(const auto& job:jobs)luaL_unref(state,LUA_REGISTRYINDEX,job.ref);
            jobs.clear();stopping=true;event(EngineStop);tick(0);
        }
        close();
    }
    void characterAdded() {
        if(!state)return;
        for(auto it=listeners.begin();it!=listeners.end();) {
            if(it->second.avatar && it->second.avatar!=runtime->characterGeneration()) {
                luaL_unref(state,LUA_REGISTRYINDEX,it->second.ref);it=listeners.erase(it);
            } else ++it;
        }
        event(Spawn);
    }
    void cancelScript(InstanceId id) {
        for(auto& job:jobs)if(job.script==id)job.done=true;
        for(auto it=listeners.begin();it!=listeners.end();) {
            if(it->second.script==id){luaL_unref(state,LUA_REGISTRYINDEX,it->second.ref);it=listeners.erase(it);}else ++it;
        }
    }
    std::vector<InstanceId> discoverScripts() {
        for(auto it=startedScripts.begin();it!=startedScripts.end();) {
            if(!scriptAlive(*it))it=startedScripts.erase(it);else ++it;
        }
        std::vector<InstanceId> candidates,added;
        const auto* service=runtime->scene().service(InstanceClass::ServerScriptService);
        for(const auto& object:runtime->scene().objects())if(IsScriptClass(object->instanceClass())&&
            object->instanceClass()!=InstanceClass::ModuleScript&&static_cast<const Script&>(*object).enabled&&!startedScripts.count(object->id())&&
            (runtime->scene().workspace()->isAncestorOf(object.get())||(service&&service->isAncestorOf(object.get()))))candidates.push_back(object->id());
        std::sort(candidates.begin(),candidates.end());
        for(const auto id:candidates) {
            if(jobs.size()>=limits.tasks)break; // defer the next batch, never silently drop scripts
            // A function already running when disabled may finish its current call.
            // Purge anything it registered afterwards before starting the new context.
            cancelScript(id);
            startedScripts.insert(id);added.push_back(id);
            lua_pushcfunction(state,Safe<LoadAsset>);lua_pushinteger(state,lua_Integer(id));
            if(lua_pcall(state,1,0,0)!=LUA_OK){message(true,id,lua_tostring(state,-1));lua_pop(state,1);}
        }
        return added;
    }
};
LuaRuntime::LuaRuntime():impl_(std::make_unique<Impl>()){}
LuaRuntime::~LuaRuntime(){stop();}
bool LuaRuntime::configure(const Limits& limits,std::string& error) {
    if(active()){error="As cotas Lua só podem ser alteradas com a sessão parada.";return false;}
    if(limits.memoryBytes<256*1024||limits.memoryBytes>256*1024*1024||limits.instructionsPerTask<1000||limits.instructionsPerTask>10000000||limits.instructionsPerFrame<limits.instructionsPerTask||limits.instructionsPerFrame>20000000||!limits.tasks||limits.tasks>1024||!limits.listeners||limits.listeners>1024){error="Cotas Lua fora dos intervalos suportados.";return false;}
    impl_->limits=limits;error.clear();return true;
}
const Limits& LuaRuntime::limits()const{return impl_->limits;}
bool LuaRuntime::start(game::RuntimeSession& runtime) {
    stop();auto& self=*impl_;self.runtime=&runtime;self.output.clear();self.errors=0;self.time=0;self.moduleDepth=0;self.contactOverflowReported=false;++self.generation;
    self.state=lua_newstate(Impl::Allocate,&self);
    if(!self.state){self.message(true,0,"Cannot allocate Lua VM");self.close();return false;}
    *static_cast<Impl**>(lua_getextraspace(self.state))=&self;
    lua_pushcfunction(self.state,Impl::Safe<Impl::Setup>);
    if(lua_pcall(self.state,0,0,0)!=LUA_OK){self.message(true,0,lua_tostring(self.state,-1));self.close();return false;}
    self.discoverScripts();
    self.tick(0);self.event(EngineStart);self.event(Spawn);self.tick(0);return true;
}
void LuaRuntime::stop(){impl_->stop();}
void LuaRuntime::update(float dt){
    if(!impl_->state||!std::isfinite(dt)||dt<0)return;
    const auto added=impl_->discoverScripts();impl_->inputEvents();impl_->event(Heartbeat,dt);impl_->tick(dt);
    for(const auto id:added)impl_->event(EngineStart,0,id);
}
void LuaRuntime::fixedUpdate(float dt){
    if(!impl_->state||!std::isfinite(dt)||dt<=0)return;
    if(impl_->hasSignal(PhysicsUpdate))impl_->event(PhysicsUpdate,dt);
    if(impl_->hasSignal(PhysicsUpdate)||wantsContacts())impl_->tick(0,false);
}
bool LuaRuntime::wantsContacts()const{return impl_->state&&(impl_->hasSignal(CollisionEnter)||impl_->hasSignal(CollisionExit));}
void LuaRuntime::contact(InstanceId first,InstanceId second,bool entering){impl_->contact(first,second,entering);}
void LuaRuntime::characterAdded(){impl_->characterAdded();}
void LuaRuntime::healthChanged(float health,bool died){impl_->event(HealthChanged,health);if(died)impl_->event(Died);}
const std::vector<Message>& LuaRuntime::messages()const{return impl_->output;}
std::size_t LuaRuntime::errorCount()const{return impl_->errors;}
std::size_t LuaRuntime::memoryUsed()const{return impl_->memory;}
std::size_t LuaRuntime::taskCount()const{return impl_->jobs.size();}
bool LuaRuntime::active()const{return impl_->state!=nullptr;}
} // namespace loblox::scripting

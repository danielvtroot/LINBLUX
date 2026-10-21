#include "scripting/LuaRuntime.h"
#include "game/Runtime.h"
#include "scene/Scene.h"
#include "core/Diagnostics.h"
#include <lua.hpp>
#include <algorithm>
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
enum class Kind { Instance,Game,Players,Player,RunService,Character,Humanoid,Animator,Signal,Connection };
enum Signal { Spawn=1,Heartbeat,HealthChanged,Died };
struct Handle { Kind kind; InstanceId id; unsigned generation; };
constexpr std::size_t kMemoryLimit=8*1024*1024, kTasks=128, kListeners=128;
constexpr int kInstructions=100000;
bool Eq(const char* a,const char* b) { return std::strcmp(a,b)==0; }
void Require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
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
Vector3 ReadVector(lua_State* L,int index) {
    Require(lua_istable(L,index),"expected Vector3 table with X/Y/Z");
    Vector3 v; const char* names[]{"X","Y","Z"}; float* fields[]{&v.x,&v.y,&v.z};
    for(int i=0;i<3;++i) { lua_getfield(L,index,names[i]); const auto n=Number(L,-1);
        Require(std::fabs(n)<=1000000,"vector component exceeds supported range"); *fields[i]=float(n);lua_pop(L,1); }
    return v;
}
}
struct LuaRuntime::Impl {
    lua_State* state=nullptr;
    game::RuntimeSession* runtime=nullptr;
    std::size_t memory=0;
    double time=0;
    unsigned generation=0;
    int instructions=0, frameBudget=0, moduleDepth=0;
    InstanceId executing=0;
    struct Job { lua_State* thread; int ref; int arguments; InstanceId script; double wake; bool done=false; };
    struct Listener { int signal; int ref; InstanceId script; bool active=true; };
    std::deque<Job> jobs;
    std::vector<Listener> listeners;
    std::vector<Message> output;
    std::map<InstanceId,int> modules;
    std::set<InstanceId> loading;

    static Impl& Host(lua_State* L) { return **static_cast<Impl**>(lua_getextraspace(L)); }
    static void* Allocate(void* userdata,void* pointer,std::size_t old,std::size_t size) {
        auto& self=*static_cast<Impl*>(userdata); if(!pointer) old=0;
        if(size==0) { std::free(pointer); self.memory-=std::min(self.memory,old); return nullptr; }
        if(size>old && size-old>kMemoryLimit-std::min(self.memory,kMemoryLimit)) return nullptr;
        void* next=std::realloc(pointer,size);
        if(next) self.memory=self.memory-old+size;
        return next;
    }
    template<int(*Function)(lua_State*)> static int Safe(lua_State* L) {
        try { return Function(L); }
        catch(const std::exception& e) { lua_pushstring(L,e.what()); }
        catch(...) { lua_pushliteral(L,"Loblox native API failure"); }
        return lua_error(L);
    }
    static void Hook(lua_State* L,lua_Debug*) {
        auto& self=Host(L); self.instructions-=1000; self.frameBudget-=1000;
        if(self.instructions<=0 || self.frameBudget<=0) luaL_error(L,"Lua instruction quota exceeded; task terminated");
    }
    Handle& handle(lua_State* L,int index) {
        auto* h=static_cast<Handle*>(luaL_testudata(L,index,"Loblox.Handle"));
        Require(h && h->generation==generation,"invalid or expired Loblox handle");
        if(h->kind==Kind::Character || h->kind==Kind::Humanoid || h->kind==Kind::Animator)
            Require(h->id==runtime->characterGeneration(),"character was respawned; use the new CharacterAdded handle");
        return *h;
    }
    Instance& instance(lua_State* L,int index) {
        const auto& h=handle(L,index); Require(h.kind==Kind::Instance,"expected Instance");
        auto* object=runtime->mutableScene().find(h.id); Require(object,"Instance was destroyed"); return *object;
    }
    void push(lua_State* L,Kind kind,InstanceId id=0) {
        if(kind==Kind::Character || kind==Kind::Humanoid || kind==Kind::Animator)id=runtime->characterGeneration();
        auto* h=static_cast<Handle*>(lua_newuserdatauv(L,sizeof(Handle),0));
        *h={kind,id,generation}; luaL_setmetatable(L,"Loblox.Handle");
    }
    void pushInstance(lua_State* L,const Instance* object) {
        if(object) push(L,Kind::Instance,object->id()); else lua_pushnil(L);
    }
    void message(bool error,InstanceId id,const char* text) {
        const auto* object=runtime?runtime->scene().find(id):nullptr;
        const std::string name=object?object->name():"Runtime";
        if(output.size()>=256) output.erase(output.begin());
        output.push_back({error,name,std::string(text?text:"").substr(0,2048)});
        diag::Event event; event.subsystem="script"; event.name=error?"error":"output";
        event.entityId=id; event.level=error?diag::Level::Error:diag::Level::Info;
        event.error=error?output.back().text:""; event.context={{"script",name},{"message",output.back().text}}; diag::log(event);
    }
    void environment(lua_State* L,InstanceId id) {
        lua_newtable(L); pushInstance(L,runtime->scene().find(id));lua_setfield(L,-2,"script");
        lua_newtable(L);lua_pushglobaltable(L);lua_setfield(L,-2,"__index");lua_setmetatable(L,-2);
        lua_setupvalue(L,-2,1);
    }
    static int LoadAsset(lua_State* L) {
        auto& self=Host(L); const auto id=static_cast<InstanceId>(lua_tointeger(L,1));
        auto* object=self.runtime->scene().find(id); Require(object && IsScriptClass(object->instanceClass()),"Script asset missing");
        const auto& script=static_cast<const Script&>(*object);
        Require(self.jobs.size()<kTasks,"Lua task limit reached");
        lua_State* thread=lua_newthread(L); const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
        char name[200];std::snprintf(name,sizeof(name),"@%s#%llu",script.name().c_str(),static_cast<unsigned long long>(id));
        if(luaL_loadbufferx(thread,script.source.data(),script.source.size(),name,"t")!=LUA_OK) {
            self.message(true,id,lua_tostring(thread,-1));luaL_unref(L,LUA_REGISTRYINDEX,ref);return 0;
        }
        self.environment(thread,id);self.jobs.push_back({thread,ref,0,id,self.time});return 0;
    }
    static int SpawnTask(lua_State* L) {
        auto& self=Host(L); Require(lua_isfunction(L,1),"task.spawn requires function");
        Require(self.jobs.size()<kTasks,"Lua task limit reached");
        const int count=lua_gettop(L);lua_State* thread=lua_newthread(L);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
        for(int i=1;i<=count;++i)lua_pushvalue(L,i);
        lua_xmove(L,thread,count);self.jobs.push_back({thread,ref,count-1,self.executing,self.time});return 0;
    }
    static int Wait(lua_State* L) {
        auto& self=Host(L); Require(self.moduleDepth==0,"ModuleScript cannot yield during require");
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
        else {
            InstanceClass type; Require(InstanceClassFromName(name,type)&&IsRootServiceClass(type),"unsupported service; no networking services exist");
            auto* object=self.runtime->mutableScene().ensureService(type); Require(object,"service unavailable");self.pushInstance(L,object);
        }return 1;
    }
    static int Children(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);lua_createtable(L,int(object.children().size()),0);int i=0;
        for(auto* child:object.children()){self.pushInstance(L,child);lua_rawseti(L,-2,++i);}return 1;
    }
    static int Descendants(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);lua_newtable(L);int i=0;
        for(const auto& child:self.runtime->scene().objects()) if(child.get()!=&object && object.isAncestorOf(child.get())) {
            self.pushInstance(L,child.get());lua_rawseti(L,-2,++i);
        }
        return 1;
    }
    static int Find(lua_State* L) {
        auto& self=Host(L);auto& object=self.instance(L,1);const char* name=Text(L,2);
        for(auto* child:object.children()) if(child->name()==name){self.pushInstance(L,child);return 1;}
        lua_pushnil(L);return 1;
    }
    static int Destroy(lua_State* L) { auto& self=Host(L);auto& object=self.instance(L,1);Require(self.runtime->mutableScene().remove(&object),"cannot destroy protected instance/subtree");return 0; }
    static int Clone(lua_State* L) { auto& self=Host(L);auto& object=self.instance(L,1);auto* copy=self.runtime->mutableScene().duplicateTree(object);Require(copy,"cannot clone service or unsupported instance");self.pushInstance(L,copy);return 1; }
    static int GetAttribute(lua_State* L) {
        auto& object=Host(L).instance(L,1);const char* key=Text(L,2);const auto it=object.attributes().find(key);
        if(it==object.attributes().end())lua_pushnil(L);
        else std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<T,bool>)lua_pushboolean(L,value);
            else if constexpr(std::is_same_v<T,double>)lua_pushnumber(L,value);
            else lua_pushlstring(L,value.data(),value.size());},it->second);return 1;
    }
    static int SetAttribute(lua_State* L) {
        auto& object=Host(L).instance(L,1);const char* key=Text(L,2);bool ok=false;
        if(lua_isnil(L,3)){object.removeAttribute(key);return 0;}
        if(lua_isboolean(L,3))ok=object.setAttribute(key,bool(lua_toboolean(L,3)));
        else if(lua_type(L,3)==LUA_TNUMBER)ok=object.setAttribute(key,Number(L,3));
        else if(lua_type(L,3)==LUA_TSTRING)ok=object.setAttribute(key,std::string(Text(L,3)));
        Require(ok,"invalid attribute or attribute budget exceeded");return 0;
    }
    static int NewInstance(lua_State* L) {
        auto& self=Host(L);const char* name=Text(L,1);InstanceClass type;
        Require(InstanceClassFromName(name,type),"unknown Instance class");
        auto* parent=lua_gettop(L)>1?&self.instance(L,2):self.runtime->mutableScene().workspace();
        auto& scene=self.runtime->mutableScene(); Require(scene.objects().size()<10000,"runtime Instance limit reached");
        Instance* object=nullptr;
        if(type==InstanceClass::Part)object=scene.createPart(PartShape::Block,{});
        else if(type==InstanceClass::Folder || type==InstanceClass::Model)object=scene.createContainer(type,parent);
        else throw std::runtime_error("Instance.new supports Part, Folder and Model");
        Require(object,"invalid parent for new Instance");
        std::string error;const bool ok=scene.reparent(object,parent,error);
        if(!ok){scene.remove(object);throw std::runtime_error(error);} self.pushInstance(L,object);return 1;
    }
    static int Connect(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);Require(h.kind==Kind::Signal,"expected signal");Require(lua_isfunction(L,2),"Connect requires callback");
        Require(self.listeners.size()<kListeners,"event connection limit reached");
        lua_pushvalue(L,2);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
        self.listeners.push_back({int(h.id),ref,self.executing}); self.push(L,Kind::Connection,self.listeners.size());return 1;
    }
    static int Disconnect(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);Require(h.kind==Kind::Connection && h.id>0 && h.id<=self.listeners.size(),"invalid connection");
        auto& listener=self.listeners[h.id-1]; if(listener.active){luaL_unref(L,LUA_REGISTRYINDEX,listener.ref);listener.active=false;}return 0;
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
    static int Index(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);const char* key=Text(L,2);
#define METHOD(name,fn) if(Eq(key,name)){lua_pushcfunction(L,Safe<fn>);return 1;}
        if(h.kind==Kind::Game){METHOD("GetService",GetService)}
        if(h.kind==Kind::Signal){METHOD("Connect",Connect)}
        if(h.kind==Kind::Connection){METHOD("Disconnect",Disconnect)}
        if(h.kind==Kind::Players || h.kind==Kind::Player) {
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
        if(h.kind==Kind::Humanoid) {
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
            if(Eq(key,"Parent")){self.pushInstance(L,object.parent());return 1;}
            if(object.isPart()) {
                const auto& part=static_cast<const Part&>(object);
                if(Eq(key,"Position")){Vector(L,part.position());return 1;}
                if(Eq(key,"Size")){Vector(L,part.size());return 1;}
                if(Eq(key,"Rotation")){Vector(L,part.rotationEulerDegrees());return 1;}
                if(Eq(key,"Anchored")){lua_pushboolean(L,part.anchored());return 1;}
                if(Eq(key,"CanCollide")){lua_pushboolean(L,part.canCollide());return 1;}
                if(Eq(key,"Transparency")){lua_pushnumber(L,1-part.color().a/255.f);return 1;}
                if(Eq(key,"Color")){const auto c=part.color();Vector(L,{float(c.r),float(c.g),float(c.b)});return 1;}
            }
            for(auto* child:object.children()) if(child->name()==key){self.pushInstance(L,child);return 1;}
        }
#undef METHOD
        lua_pushnil(L);return 1;
    }
    static int NewIndex(lua_State* L) {
        auto& self=Host(L);const auto h=self.handle(L,1);const char* key=Text(L,2);
        if(h.kind==Kind::Humanoid) {
            auto settings=self.runtime->settings();
            if(Eq(key,"WalkSpeed"))settings.walkSpeed=float(Number(L,3));
            else if(Eq(key,"JumpHeight"))settings.jumpHeight=float(Number(L,3));
            else if(Eq(key,"AutoRotate")){Require(lua_isboolean(L,3),"AutoRotate expects boolean");settings.autoRotate=lua_toboolean(L,3);}
            else throw std::runtime_error("unsupported or readonly Humanoid property");
            std::string error;Require(settings.validate(error),"Humanoid property outside valid limits");self.runtime->settings()=settings;return 0;
        }
        Require(h.kind==Kind::Instance,"readonly service property");auto& object=self.instance(L,1);
        if(Eq(key,"Name")){const char* name=Text(L,3);Require(*name && std::strlen(name)<=128,"Name length must be 1..128");object.setName(name);return 0;}
        if(Eq(key,"Parent")){auto& parent=self.instance(L,3);std::string error;const bool ok=self.runtime->mutableScene().reparent(&object,&parent,error);if(!ok)throw std::runtime_error(error);return 0;}
        Require(object.isPart(),"unknown or readonly Instance property");auto& part=static_cast<Part&>(object);
        if(Eq(key,"Position"))part.setPosition(ReadVector(L,3));
        else if(Eq(key,"Size")){auto size=ReadVector(L,3);Require(size.x>=kMinPartSize&&size.y>=kMinPartSize&&size.z>=kMinPartSize,"Size below 0.2");part.setSize(size);}
        else if(Eq(key,"Rotation"))part.setRotationEulerDegrees(ReadVector(L,3));
        else if(Eq(key,"Color")){auto c=ReadVector(L,3);Require(c.x>=0&&c.y>=0&&c.z>=0&&c.x<=255&&c.y<=255&&c.z<=255,"Color channels must be 0..255");part.setColor({static_cast<unsigned char>(c.x),static_cast<unsigned char>(c.y),static_cast<unsigned char>(c.z),part.color().a});}
        else if(Eq(key,"Transparency")){const auto n=Number(L,3);Require(n>=0&&n<=1,"Transparency must be 0..1");auto c=part.color();c.a=static_cast<unsigned char>((1-n)*255);part.setColor(c);}
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
        lua_pushcfunction(L,Safe<RequireModule>);lua_setglobal(L,"require");
        lua_newtable(L);lua_pushcfunction(L,Safe<SpawnTask>);lua_setfield(L,-2,"spawn");lua_pushcfunction(L,Safe<Wait>);lua_setfield(L,-2,"wait");lua_setglobal(L,"task");
        lua_newtable(L);lua_pushcfunction(L,Safe<NewVector>);lua_setfield(L,-2,"new");lua_setglobal(L,"Vector3");
        lua_newtable(L);lua_pushcfunction(L,Safe<NewColor>);lua_setfield(L,-2,"fromRGB");lua_setglobal(L,"Color3");
        lua_newtable(L);lua_pushcfunction(L,Safe<NewInstance>);lua_setfield(L,-2,"new");lua_setglobal(L,"Instance");
        self.push(L,Kind::Game);lua_setglobal(L,"game");self.pushInstance(L,self.runtime->scene().workspace());lua_setglobal(L,"workspace");return 0;
    }
    static int Emit(lua_State* L) {
        auto& self=Host(L);const int signal=int(lua_tointeger(L,1)),ref=int(lua_tointeger(L,2));
        const auto value=lua_tonumber(L,3);lua_settop(L,0);lua_rawgeti(L,LUA_REGISTRYINDEX,ref);
        if(signal==Spawn)self.push(L,Kind::Character);else lua_pushnumber(L,value);
        return SpawnTask(L);
    }
    static int Trace(lua_State* L) {
        auto* thread=static_cast<lua_State*>(lua_touserdata(L,1));
        luaL_traceback(L,thread,lua_tostring(thread,-1),1);return 1;
    }
    void event(int signal,float value=0) {
        if(!state)return;
        for(const auto& listener:listeners) {
            if(!listener.active || listener.signal!=signal)continue;
            executing=listener.script; lua_pushcfunction(state,Safe<Emit>);
            lua_pushinteger(state,signal);lua_pushinteger(state,listener.ref);lua_pushnumber(state,value);
            if(lua_pcall(state,3,0,0)!=LUA_OK){message(true,executing,lua_tostring(state,-1));lua_pop(state,1);}
        }
    }
    void tick(float dt) {
        if(!state)return;
        time+=dt;frameBudget=250000;
        const auto count=jobs.size();
        for(std::size_t i=0;i<count;++i) {
            auto& job=jobs[i];if(job.done || job.wake>time)continue;
            if(frameBudget<=0)break;
            executing=job.script;instructions=kInstructions;lua_sethook(job.thread,Hook,LUA_MASKCOUNT,1000);
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
            if(it->done){luaL_unref(state,LUA_REGISTRYINDEX,it->ref);it=jobs.erase(it);}else ++it;
        }
        executing=0;
    }
    void close() {
        jobs.clear();listeners.clear();modules.clear();loading.clear();
        if(state)lua_close(state);
        state=nullptr;memory=0;runtime=nullptr;
    }
};
LuaRuntime::LuaRuntime():impl_(std::make_unique<Impl>()){}
LuaRuntime::~LuaRuntime(){stop();}
bool LuaRuntime::start(game::RuntimeSession& runtime) {
    stop();auto& self=*impl_;self.runtime=&runtime;self.output.clear();self.time=0;self.moduleDepth=0;++self.generation;
    self.state=lua_newstate(Impl::Allocate,&self);
    if(!self.state){self.message(true,0,"Cannot allocate Lua VM");self.close();return false;}
    *static_cast<Impl**>(lua_getextraspace(self.state))=&self;
    lua_pushcfunction(self.state,Impl::Safe<Impl::Setup>);
    if(lua_pcall(self.state,0,0,0)!=LUA_OK){self.message(true,0,lua_tostring(self.state,-1));self.close();return false;}
    std::vector<InstanceId> scripts;
    for(const auto& object:runtime.scene().objects()) if(IsScriptClass(object->instanceClass()) &&
        object->instanceClass()!=InstanceClass::ModuleScript && static_cast<const Script&>(*object).enabled) {
        const auto* service=runtime.scene().service(InstanceClass::ServerScriptService);
        if(runtime.scene().workspace()->isAncestorOf(object.get()) || (service && service->isAncestorOf(object.get()))) scripts.push_back(object->id());
    }
    std::sort(scripts.begin(),scripts.end());
    for(auto id:scripts) {
        lua_pushcfunction(self.state,Impl::Safe<Impl::LoadAsset>);lua_pushinteger(self.state,lua_Integer(id));
        if(lua_pcall(self.state,1,0,0)!=LUA_OK){self.message(true,id,lua_tostring(self.state,-1));lua_pop(self.state,1);}
    }
    self.tick(0);self.event(Spawn);self.tick(0);return true;
}
void LuaRuntime::stop(){impl_->close();}
void LuaRuntime::update(float dt){impl_->event(Heartbeat,dt);impl_->tick(dt);}
void LuaRuntime::characterAdded(){impl_->event(Spawn);}
void LuaRuntime::healthChanged(float health,bool died){impl_->event(HealthChanged,health);if(died)impl_->event(Died);}
const std::vector<Message>& LuaRuntime::messages()const{return impl_->output;}
std::size_t LuaRuntime::memoryUsed()const{return impl_->memory;}
std::size_t LuaRuntime::taskCount()const{return impl_->jobs.size();}
bool LuaRuntime::active()const{return impl_->state!=nullptr;}
} // namespace loblox::scripting

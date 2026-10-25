#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include "serialization/ProjectIO.h"
#include "serialization/SceneLua.h"
#include "game/Runtime.h"
#include "core/Diagnostics.h"
#include "core/NumberFormat.h"
#include "editor/EditHistory.h"
#include "editor/TransformMetrics.h"

using namespace loblox;
namespace {
int checks=0, failures=0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL %d: %s\n",__LINE__,#x); } } while(false)
void Hierarchy() {
    Scene scene; scene.buildDefaultScene(); PhysicsWorld physics; physics.attach(scene);
    std::string error;
    auto* folder=scene.createContainer(InstanceClass::Folder,nullptr,"Buildings");
    auto* model=scene.createContainer(InstanceClass::Model,folder,"House");
    auto* detail=scene.createContainer(InstanceClass::Folder,folder,"Details");
    auto* part=scene.createPart(PartShape::Block,{30,4,0}); part->setAnchored(true);
    const auto id=part->id(); const auto transform=part->transform();
    physics.syncScene(scene);
    CHECK(scene.reparent(part,model,error)); CHECK(part->parent()==model);
    CHECK(scene.reparent(part,detail,error)); CHECK(part->parent()==detail);
    CHECK(scene.find(id)==part); CHECK(NearlyEqual(part->position(),transform.position));
    CHECK(scene.parts().size()==3); CHECK(physics.hasBody(id));
    Part* hit=nullptr;
    CHECK(physics.raycast({30,20,0},{0,-1,0},30,&hit)); CHECK(hit==part);
    CHECK(physics.validate(scene,error)); CHECK(scene.validate(error));
    CHECK(!scene.reparent(folder,detail,error)); CHECK(!scene.reparent(folder,folder,error));
    CHECK(!scene.reparent(part,scene.lighting(),error));
    CHECK(!scene.reparent(scene.workspace(),folder,error));
    Scene other; CHECK(!scene.reparent(part,other.workspace(),error));
    auto json=projectio::toJson(scene); CHECK(nlohmann::json::parse(json)["version"]==3);
    Scene loaded; CHECK(projectio::fromJson(loaded,json,error));
    CHECK(loaded.find(id)->parent()->id()==detail->id());
    CHECK(projectio::toJson(loaded)==json);
    PhysicsWorld restored; restored.attach(loaded); CHECK(restored.validate(loaded,error));
    CHECK(restored.raycast({30,20,0},{0,-1,0},30,&hit)); CHECK(hit->id()==id);
    auto invalid=nlohmann::json::parse(json);
    for(auto& object:invalid["objects"]) if(object["id"]==folder->id()) object["parent"]=detail->id();
    CHECK(!projectio::fromJson(loaded,invalid.dump(),error)); CHECK(projectio::toJson(loaded)==json);
    auto movedIds=nlohmann::json::parse(json);
    for(auto& object:movedIds["objects"]) {
        if(object["id"]==1)object["id"]=101;
        if(object["id"]==2)object["id"]=102;
        if(object["parent"]==1)object["parent"]=101;
    }
    Scene changedIds;CHECK(projectio::fromJson(changedIds,movedIds.dump(),error));
    CHECK(changedIds.workspace()->id()==101);CHECK(changedIds.lighting()->id()==102);
    CHECK(changedIds.find(id)->parent()->id()==detail->id());
    bool refused=false; try { scenelua::toLua(scene); } catch(...) { refused=true; } CHECK(refused);
    auto* copy=scene.duplicateTree(*folder); CHECK(copy && copy!=folder);
    CHECK(copy && copy->children().size()==2); CHECK(scene.parts().size()==4);
    CHECK(scene.validate(error)); physics.syncScene(scene); CHECK(physics.validate(scene,error));
    scene.select(part); CHECK(scene.remove(folder)); CHECK(scene.find(id)==nullptr);
    CHECK(scene.selection()==nullptr); CHECK(!physics.hasBody(id)); CHECK(scene.validate(error));
    CHECK(!scene.remove(scene.workspace()));
    auto* guarded=scene.createContainer(InstanceClass::Folder);
    scene.parts()[0]->setDeletable(false);
    CHECK(scene.reparent(scene.parts()[0],guarded,error)); CHECK(!scene.remove(guarded));
    auto* depth=scene.createContainer(InstanceClass::Folder); int levels=0;
    while(auto* next=scene.createContainer(InstanceClass::Folder,depth)) { depth=next; ++levels; }
    CHECK(levels<129); CHECK(scene.validate(error));
    Scene large;
    auto begin=std::chrono::steady_clock::now();
    for(int i=0;i<1000;++i) CHECK(large.createContainer(InstanceClass::Folder,nullptr,"Node"+std::to_string(i)));
    CHECK(large.validate(error)); Scene roundtrip;
    CHECK(projectio::fromJson(roundtrip,projectio::toJson(large),error)); CHECK(roundtrip.objects().size()==1002);
    std::printf("1000-node hierarchy create/validate/round-trip: %.2f ms\n",
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
}
void Scripts() {
    Scene author;author.buildDefaultScene();
    auto* target=author.createPart(PartShape::Block,{20,4,0},{4,4,4},"Target");target->setAnchored(true);
    auto* storage=author.ensureService(InstanceClass::ReplicatedStorage);
    auto* module=author.createScript(InstanceClass::ModuleScript,storage,"Config");module->source="return { x = 31 }";
    auto* logic=author.createScript(InstanceClass::Script,nullptr,"Game.server.lua");
    logic->source=R"(
assert(os == nil and io == nil and debug == nil and package == nil)
assert(load == nil and dofile == nil and loadfile == nil and pcall == nil and coroutine == nil)
local config = require(game:GetService("ReplicatedStorage"):FindFirstChild("Config"))
workspace.Target.Position = Vector3.new(config.x, 4, 0)
workspace.Target.Color = Color3.fromRGB(25, 100, 240)
workspace.Target:SetAttribute("score", 42)
assert(workspace.Target.Parent == workspace)
assert(#workspace:GetDescendants() == 3)
local Players = game:GetService("Players")
Players.CharacterAdded:Connect(function(character)
    workspace:SetAttribute("spawns", (workspace:GetAttribute("spawns") or 0) + 1)
    character.Humanoid.WalkSpeed = 20
end)
local connection
connection = game:GetService("RunService").Heartbeat:Connect(function(dt)
    assert(dt >= 0)
    workspace:SetAttribute("heartbeat", true)
    connection:Disconnect()
end)
task.wait(0.05)
workspace.Target.Size = Vector3.new(3, 5, 2)
print("Game task completed")
)";
    auto* disabled=author.createScript(InstanceClass::Script,nullptr,"Disabled");disabled->enabled=false;disabled->source="error('must not execute')";
    auto* stored=author.createScript(InstanceClass::Script,storage,"Stored");stored->source="error('storage must not autoexecute')";
    const auto baseline=projectio::toJson(author); CHECK(target->position().x==20);CHECK(author.workspace()->attributes().empty());
    Scene reopened;std::string error;CHECK(projectio::fromJson(reopened,baseline,error));CHECK(projectio::toJson(reopened)==baseline);
    game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    CHECK(runtime.scripts().active());CHECK(runtime.scene().findPart(target->id())->position().x==31);
    CHECK(runtime.scene().workspace()->attributes().count("spawns"));CHECK(runtime.settings().walkSpeed==20);
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().findPart(target->id())->size().y==5);
    CHECK(runtime.scene().workspace()->attributes().count("heartbeat"));
    CHECK(runtime.scripts().memoryUsed()>0 && runtime.scripts().memoryUsed()<=8*1024*1024);
    CHECK(runtime.scripts().taskCount()==0);CHECK(runtime.validate(error));
    CHECK(runtime.respawn(error));runtime.update(1.f/60,{});
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("spawns"))==2);
    CHECK(projectio::toJson(author)==baseline);runtime.stop();
    CHECK(!runtime.scripts().active());CHECK(runtime.scripts().taskCount()==0);CHECK(runtime.scripts().memoryUsed()==0);
    for(int i=0;i<3;++i){CHECK(runtime.start(author,error));CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("spawns"))==1);runtime.stop();}
    auto* broken=author.createScript(InstanceClass::Script,nullptr,"Broken.lua");broken->source="local function bad()\n error('specific failure')\nend\nbad()";
    auto* loop=author.createScript(InstanceClass::Script,nullptr,"Loop.lua");loop->source="while true do end";
    auto* memory=author.createScript(InstanceClass::LocalScript,author.workspace(),"Memory.lua");memory->source="local big = string.rep('x', 32*1024*1024)";
    auto* stale=author.createScript(InstanceClass::Script,nullptr,"Stale.lua");stale->source="local p = Instance.new('Part'); p:Destroy(); print(p.Name)";
    auto* syntax=author.createScript(InstanceClass::Script,nullptr,"Syntax.lua");syntax->source="local = )";
    auto* wait=author.createScript(InstanceClass::Script,nullptr,"Waiting.lua");wait->source="task.wait(100);error('must be canceled on Stop')";
    CHECK(runtime.start(author,error));
    bool trace=false,quota=false,oom=false,destroyed=false,parse=false,forbiddenRun=false;
    for(const auto& msg:runtime.scripts().messages()) {
        trace|=msg.error&&msg.text.find("specific failure")!=std::string::npos&&msg.text.find("stack traceback")!=std::string::npos;
        quota|=msg.error&&msg.text.find("quota")!=std::string::npos;
        oom|=msg.error&&msg.text.find("memory")!=std::string::npos;
        destroyed|=msg.error&&msg.text.find("destroyed")!=std::string::npos;
        parse|=msg.error&&msg.script=="Syntax.lua";
        forbiddenRun|=msg.text.find("must not")!=std::string::npos;
    }
    CHECK(trace);CHECK(quota);CHECK(oom);CHECK(destroyed);CHECK(parse);CHECK(!forbiddenRun);
    CHECK(runtime.scripts().taskCount()>0);CHECK(runtime.validate(error));
    runtime.stop();CHECK(runtime.scripts().taskCount()==0);CHECK(runtime.validate(error));
}
void LinbluxScripts() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* platform=author.createPart(PartShape::Block,{20,8,0},{6,1,6},"Plataforma");platform->setAnchored(true);
    auto* bound=author.createScript(InstanceClass::Script,platform,"Plataforma.lua");
    bound->source=R"(
assert(linblux.api_version == 1 and linblux.self.name == "Plataforma")
assert(linblux.script.parent == linblux.self and linblux.script.type == "script")
local part = linblux.self
part.material = "metal"
part.color = linblux.vec3(210, 35, 65)
part.transparency = 0.25
part.size = linblux.vec3(8, 2, 6)
part.rotation = linblux.vec3(0, 45, 0)
linblux.events.on("start", function() part:set_attribute("started", true) end)
linblux.events.on("update", function(dt)
    local p = part.position
    part.position = linblux.vec3(p.x + dt * 3, p.y, p.z)
end)
linblux.time.after(0.03, function() part:set_attribute("timer", true) end)
)";
    auto* global=author.createScript(InstanceClass::Script,nullptr,"Mundo.lua");
    global->source=R"(
local root = linblux.scene.root
assert(linblux.self.type == "service")
local group = linblux.scene.create("group", {name="ObjetosLua"})
local part = linblux.scene.create("part", {name="CriadoLua", parent=group,
    position=linblux.vec3(30,9,0), size=linblux.vec3(2,3,4), material="emissive", shape="sphere"})
assert(part.type == "part" and part.anchored and part.collidable)
assert(part.parent == group and group:find_child("CriadoLua") == part)
assert(linblux.scene.find(part.id) == part and linblux.scene.find("CriadoLua") == part)
assert(#group:children() == 1 and #group:descendants() == 1)
local copy = part:clone()
assert(copy.id ~= part.id)
copy:destroy()
assert(not copy.alive)
local canceled = linblux.events.on("pontuar", function() error("listener cancelado") end)
canceled:disconnect()
assert(not canceled.connected)
local connection
connection = linblux.events.on("pontuar", function(payload)
    root:set_attribute("score", payload.value)
    connection:disconnect()
end)
linblux.events.emit("pontuar", {value=42})
linblux.time.spawn(function()
    linblux.time.wait(0.04)
    assert(linblux.time.now() >= 0.04)
    root:set_attribute("waited", true)
end)
linblux.events.on("stop", function() print("LINBLUX cleanup") end)
)";
    const auto baseline=projectio::toJson(author);Scene reopened;
    CHECK(projectio::fromJson(reopened,baseline,error));CHECK(projectio::toJson(reopened)==baseline);
    game::RuntimeSession runtime;
    scripting::Limits policy;policy.memoryBytes=2*1024*1024;
    CHECK(runtime.scripts().configure(policy,error));CHECK(runtime.start(reopened,error));
    CHECK(!runtime.scripts().configure(policy,error));
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    const auto* moved=runtime.scene().findPart(platform->id());
    CHECK(moved&&moved->position().x>20.4f&&moved->size().x==8);
    CHECK(moved->material()==PartMaterial::Metal&&moved->color().r==210&&moved->color().a==191);
    CHECK(moved->attributes().count("timer")&&moved->attributes().count("started"));
    CHECK(runtime.scene().workspace()->attributes().count("score"));
    CHECK(runtime.scene().workspace()->attributes().count("waited"));
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Lua error: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);
    runtime.stop();bool cleanup=false;for(const auto& message:runtime.scripts().messages())cleanup|=message.text=="LINBLUX cleanup";
    CHECK(cleanup);CHECK(runtime.scripts().memoryUsed()==0);CHECK(runtime.scripts().taskCount()==0);

    // Removing a bound script or an explicit scope cancels callbacks already queued.
    Scene scoped;scoped.buildDefaultScene();
    auto* owner=scoped.createPart(PartShape::Block,{15,5,0},{2,2,2},"Escopo");owner->setAnchored(true);
    auto* listener=scoped.createScript(InstanceClass::Script,owner,"Listener.lua");
    listener->source=R"(linblux.events.on("late", function() error("orphan listener") end)
linblux.time.after(0.1, function() error("orphan timer") end))";
    auto* controller=scoped.createScript(InstanceClass::Script,nullptr,"Controller.lua");
    controller->source=R"(
local scope = linblux.scene.find("Escopo")
local scoped = linblux.events.on("late", function() error("dead scope") end, scope)
linblux.events.emit("late", {})
scope:destroy()
assert(not scoped.connected)
linblux.time.after(0.02, function()
    for i=1,150 do
        local p = linblux.scene.create("part")
        linblux.events.on("late", function() error("scope leaked") end, p)
        p:destroy()
        linblux.time.wait(0)
    end
    linblux.events.emit("late", {})
    print("scopes cleaned")
end)
)";
    CHECK(runtime.start(scoped,error));for(int i=0;i<170;++i)runtime.update(1.f/60,{});
    bool cleaned=false;for(const auto& message:runtime.scripts().messages()){CHECK(!message.error);cleaned|=message.text=="scopes cleaned";}
    CHECK(cleaned);CHECK(runtime.scripts().taskCount()==0);runtime.stop();

    // Invalid creation is transactional; quotas and errors leave native gameplay valid.
    Scene hostile;hostile.buildDefaultScene();
    auto* invalid=hostile.createScript(InstanceClass::Script,nullptr,"Invalid.lua");invalid->source="linblux.scene.create('part', {size=linblux.vec3(-1,2,3)})";
    auto* loop=hostile.createScript(InstanceClass::Script,nullptr,"Quota.lua");loop->source="while true do end";
    auto* memory=hostile.createScript(InstanceClass::Script,nullptr,"Memory.lua");memory->source="string.rep('x', 4*1024*1024)";
    auto* blocked=hostile.createScript(InstanceClass::Script,nullptr,"Sandbox.lua");blocked->source=R"(
assert(os==nil and io==nil and package==nil and debug==nil)
assert(load==nil and loadfile==nil and dofile==nil and string.dump==nil)
assert(pcall==nil and xpcall==nil)
require("/tmp/unsafe.lua")
)";
    CHECK(runtime.start(hostile,error));CHECK(runtime.scene().parts().size()==hostile.parts().size());
    bool quota=false,oom=false,invalidOption=false,denied=false;
    for(const auto& message:runtime.scripts().messages()){
        quota|=message.error&&message.text.find("quota")!=std::string::npos;
        oom|=message.error&&message.text.find("memory")!=std::string::npos;
        invalidOption|=message.error&&message.script=="Invalid.lua"&&message.line==1;
        denied|=message.error&&message.script=="Sandbox.lua";
    }
    CHECK(quota&&oom&&invalidOption&&denied);CHECK(runtime.validate(error));runtime.stop();
    policy.memoryBytes=1;CHECK(!runtime.scripts().configure(policy,error));
}
void LinbluxExamples() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto source=[&](const char* name){std::ifstream file(root/"assets/examples/lua"/name);CHECK(file.good());return std::string(std::istreambuf_iterator<char>(file),{});};
    Scene author;author.buildDefaultScene();std::string error;
    auto* platform=author.createPart(PartShape::Block,{20,8,0},{6,1,6},"PlataformaExemplo");
    auto* door=author.createPart(PartShape::Block,{30,6,0},{3,8,1},"PortaExemplo");
    author.createScript(InstanceClass::Script,platform,"Plataforma.lua")->source=source("plataforma.lua");
    author.createScript(InstanceClass::Script,door,"Porta.lua")->source=source("porta.lua");
    author.createScript(InstanceClass::Script,nullptr,"Mundo.lua")->source=source("mundo.lua");
    author.createScript(InstanceClass::Script,nullptr,"Temporizadores.lua")->source=source("temporizadores.lua");
    author.createScript(InstanceClass::Script,nullptr,"Camera.lua")->source=source("camera.lua");
    author.gameAssets().player.cameraFov=80;
    const auto baseline=projectio::toJson(author);
    const auto testDir=root/".turbo"/"lua-examples";std::filesystem::create_directories(testDir);
    const auto path=(testDir/(std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json")).string();
    CHECK(projectio::save(author,path,error));Scene reopened;CHECK(projectio::load(reopened,path,error));
    CHECK(projectio::toJson(reopened)==baseline);
    CHECK(reopened.gameAssets().player.cameraFov==80);
    game::RuntimeSession runtime;CHECK(runtime.start(reopened,error));
    for(int i=0;i<90;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().findPart(platform->id())->position().x>25);
    CHECK(runtime.scene().findPart(door->id())->position().y==13);
    CHECK(std::get<bool>(runtime.scene().findPart(door->id())->attributes().at("aberta")));
    CHECK(runtime.scene().workspace()->attributes().count("timer_concluido"));
    CHECK(runtime.scene().workspace()->attributes().count("camera_concluida"));
    CHECK(runtime.playerCamera().camera().fovy==75);
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Example: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(projectio::toJson(reopened)==baseline);CHECK(runtime.validate(error));runtime.stop();std::filesystem::remove(path);
}
void LinbluxPhysics() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* script=author.createScript(InstanceClass::Script,nullptr,"Fisica.lua");
    script->source=R"(
linblux.physics.set_gravity(10)
assert(linblux.physics.gravity() == 10)
local p = linblux.scene.create("part", {name="FisicaLua", position=linblux.vec3(40,20,0),
    size=linblux.vec3(1,1,1), anchored=false})
p.velocity = linblux.vec3(0,2,0)
p:apply_impulse(linblux.vec3(1,0,0))
p:apply_force(linblux.vec3(0,60,0))
assert(math.abs(p.velocity.x - 1) < 0.01)
local hit = linblux.physics.raycast(linblux.vec3(40,30,0), linblux.vec3(0,-1,0), 15)
assert(hit and hit.object == p and hit.normal.y > 0.9)
assert(math.abs(hit.point.y - 20.5) < 0.1 and math.abs(hit.distance - 9.5) < 0.1)
assert(linblux.physics.raycast(linblux.vec3(40,30,0), linblux.vec3(0,-1,0), 15, p) == nil)
local nearby = linblux.physics.nearby(p.position, 2)
assert(#nearby == 1 and nearby[1] == p)
linblux.scene.root:set_attribute("physics_ready", true)
)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    CHECK(runtime.scene().workspace()->attributes().count("physics_ready"));
    const Part* dynamic=nullptr;for(const auto* part:runtime.scene().parts())if(part->name()=="FisicaLua")dynamic=part;
    CHECK(dynamic!=nullptr);
    if(dynamic) {
        const auto id=dynamic->id();for(int i=0;i<10;++i)runtime.update(1.f/60,{});
        CHECK(dynamic->position().x>40.1f&&dynamic->position().y>20);
        CHECK(runtime.partVelocity(id).x>.9f);
        CHECK(!runtime.partMotion(id,{NAN,0,0},game::PartMotion::Force,error));
    }
    CHECK(!runtime.partMotion(author.parts().front()->id(),{1,0,0},game::PartMotion::Impulse,error));
    CHECK(!runtime.setGravity(0,error));CHECK(runtime.settings().gravity==10);
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Physics Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
}
void LinbluxFixedContacts() {
    std::string error;
    const auto driven=[&](int fps){
        Scene author;author.buildDefaultScene();
        auto* part=author.createPart(PartShape::Block,{80,100,0},{1,1,1},"Propulsor");part->setAnchored(false);
        author.createScript(InstanceClass::Script,part,"PassoFixo.lua")->source=R"(
linblux.physics.set_gravity(1)
local ticks = 0
linblux.events.on("physics_update", function(dt)
    assert(math.abs(dt - 1/60) < 0.000001)
    ticks = ticks + 1
    linblux.self:apply_force(linblux.vec3(20,0,0))
    linblux.self:set_attribute("steps", ticks)
end)
)";
        game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<fps;++i)runtime.update(1.f/fps,{});
        CHECK(std::get<double>(runtime.scene().findPart(part->id())->attributes().at("steps"))==60);
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Fixed Lua: %s\n",message.text.c_str());CHECK(!message.error);}
        const auto speed=runtime.partVelocity(part->id()).x;CHECK(speed>1);CHECK(runtime.validate(error));return speed;
    };
    CHECK(std::fabs(driven(30)-driven(120))<.001f);
    for(const bool destroy:{false,true}){
        Scene author;author.buildDefaultScene();author.workspace()->setAttribute("destroy_on_enter",destroy);
        auto* trigger=author.createPart(PartShape::Block,{80,10,0},{4,4,4},"Sensor");trigger->setAnchored(true);trigger->setCanCollide(false);
        auto* target=author.createPart(PartShape::Block,{80,10,0},{1,1,1},"Alvo");target->setAnchored(false);
        author.createScript(InstanceClass::Script,trigger,"Contato.lua")->source=R"(
local root, sensor = linblux.scene.root, linblux.self
linblux.events.on("collision_enter", function(hit)
    if hit.first_id == 0 or hit.second_id == 0 then root:set_attribute("avatar_contact", (root:get_attribute("avatar_contact") or 0)+1) end
end)
linblux.events.on("collision_enter", function(hit)
    local other = hit.first_id == sensor.id and hit.second or hit.first
    assert(other and other.name == "Alvo")
    sensor:set_attribute("enters", (sensor:get_attribute("enters") or 0)+1)
    if root:get_attribute("destroy_on_enter") then other:destroy()
    else other.position = linblux.vec3(100,10,0) end
end, sensor)
linblux.events.on("collision_exit", function(hit)
    assert(hit.first_id == sensor.id or hit.second_id == sensor.id)
    sensor:set_attribute("exits", (sensor:get_attribute("exits") or 0)+1)
end, sensor)
)";
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        const auto& attrs=runtime.scene().findPart(trigger->id())->attributes();
        CHECK(attrs.count("enters")&&attrs.count("exits"));
        if(attrs.count("enters"))CHECK(std::get<double>(attrs.at("enters"))==1);
        if(attrs.count("exits"))CHECK(std::get<double>(attrs.at("exits"))==1);
        CHECK(runtime.scene().workspace()->attributes().count("avatar_contact"));
        const auto previous=std::get<double>(runtime.scene().workspace()->attributes().at("avatar_contact"));
        CHECK(runtime.respawn(error));for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("avatar_contact"))>previous);
        if(destroy)CHECK(!runtime.scene().find(target->id()));
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Contact Lua: %s\n",message.text.c_str());CHECK(!message.error);}
        CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);
    }
}
void LinbluxCameraControl() {
    Scene author;author.buildDefaultScene();std::string error;
    author.gameAssets().player.cameraFov=85;
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    auto& camera=runtime.playerCamera();CHECK(camera.camera().fovy==85);
    Camera3D pose{{10,8,10},{0,3,0},{0,1,0},90,CAMERA_PERSPECTIVE};
    CHECK(camera.setPose(pose,.2f,error));CHECK(camera.scripted());
    runtime.update(.1f,{});CHECK(camera.transitioning());CHECK(camera.camera().position.x>0&&camera.camera().position.x<10);
    runtime.pause();const auto paused=camera.camera();runtime.update(.1f,{});CHECK(NearlyEqual(paused.position,camera.camera().position));runtime.resume();
    runtime.update(.1f,{});CHECK(!camera.transitioning());CHECK(NearlyEqual(pose.position,camera.camera().position));
    CHECK(NearlyEqual(pose.target,camera.camera().target));CHECK(camera.camera().fovy==90);
    const auto original=camera.camera();pose.up={0,0,0};CHECK(!camera.setPose(pose,0,error));CHECK(NearlyEqual(original.position,camera.camera().position));
    pose=original;pose.fovy=NAN;CHECK(!camera.setPose(pose,0,error));pose=original;
    // Opposite orientations must never interpolate to a zero look/up vector.
    pose.target=Vector3Subtract(pose.position,Vector3Subtract(original.target,original.position));pose.up={0,-1,0};
    CHECK(camera.setPose(pose,.2f,error));
    for(int i=0;i<12;++i){runtime.update(1.f/60,{});const auto c=camera.camera();CHECK(Vector3Length(Vector3CrossProduct(Vector3Subtract(c.target,c.position),c.up))>.01f);}
    camera.release(runtime.characterRoot().position,runtime.settings());runtime.update(1.f/60,{});CHECK(!camera.scripted());CHECK(camera.camera().fovy==85);
    CHECK(projectio::toJson(author)==baseline);CHECK(runtime.validate(error));runtime.stop();
    auto old=nlohmann::json::parse(baseline);old["game"]["player"].erase("cameraFov");Scene loaded;
    CHECK(projectio::fromJson(loaded,old.dump(),error));CHECK(loaded.gameAssets().player.cameraFov==60);
    old["game"]["player"]["cameraFov"]=180;CHECK(!projectio::fromJson(loaded,old.dump(),error));CHECK(loaded.gameAssets().player.cameraFov==60);
}
void LinbluxTaskCancellation() {
    Scene author;author.buildDefaultScene();std::string error;
    author.createScript(InstanceClass::Script,nullptr,"Tarefas.lua")->source=R"(
local root = linblux.scene.root
local never = linblux.time.spawn(function() error("canceled spawn executed") end)
assert(never.active and never:cancel() and not never.active)
assert(not linblux.time.cancel(never))
local finished = linblux.time.after(0.01, function(value) root:set_attribute("finished", value) end, 7)
assert(finished.active)
linblux.time.after(0.05, function()
    assert(root:get_attribute("finished") == 7 and not finished.active)
    local next = linblux.time.spawn(function() root:set_attribute("next", true) end)
    -- Registry slots may be reused; an old task handle must not cancel a new task.
    assert(not linblux.time.cancel(finished) and next.active)
end)
)";
    author.createScript(InstanceClass::Script,nullptr,"IntervaloZero.lua")->source="linblux.time.every(0, function() end)";
    author.createScript(InstanceClass::Script,nullptr,"CancelInvalido.lua")->source="linblux.time.cancel(linblux.player)";
    author.createScript(InstanceClass::Script,nullptr,"IntervaloComErro.lua")->source="linblux.time.every(0.01, function() error('interval failure') end)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<30;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().workspace()->attributes().count("next"));
    bool zero=false,invalid=false;int intervalErrors=0;
    for(const auto& message:runtime.scripts().messages())if(message.error){
        if(message.script=="IntervaloZero.lua")zero=true;
        else if(message.script=="CancelInvalido.lua")invalid=true;
        else if(message.script=="IntervaloComErro.lua")++intervalErrors;
        else {std::printf("Task Lua: %s\n",message.text.c_str());CHECK(false);}
    }
    CHECK(zero&&invalid&&intervalErrors==1);CHECK(runtime.scripts().errorCount()==3);CHECK(runtime.scripts().taskCount()==0);
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    CHECK(runtime.scripts().errorCount()==3);
    Scene clean;clean.buildDefaultScene();CHECK(runtime.start(clean,error));CHECK(runtime.scripts().errorCount()==0);runtime.stop();
}
void LinbluxInput() {
    Scene author;author.buildDefaultScene();std::string error;
    author.createScript(InstanceClass::Script,nullptr,"Entrada.lua")->source=R"(
local root = linblux.scene.root
local function count(name) root:set_attribute(name, (root:get_attribute(name) or 0)+1) end
linblux.events.on("key_pressed", function(key)
    assert(key == "e" and linblux.input.key_down("e") and linblux.input.key_pressed("e"))
    count("key_downs")
end)
linblux.events.on("key_released", function(key)
    assert(key == "e" and not linblux.input.key_down("e") and linblux.input.key_released("e"))
    count("key_ups")
end)
linblux.events.on("mouse_pressed", function(button)
    assert(button == "left" and linblux.input.mouse_pressed("left"))
    local p = linblux.input.pointer()
    assert(p.x == 123 and p.y == 45 and p.dx == 2 and p.wheel == -1)
    count("clicks")
end)
linblux.events.on("gamepad_pressed", function(button)
    assert(button == "south" and linblux.input.gamepad_down("south") and linblux.input.gamepad_connected())
    assert(linblux.input.gamepad_axis("left_x") == 0.5)
    count("pad_downs")
end)
linblux.events.on("gamepad_released", function(button)
    assert(button == "south" and linblux.input.gamepad_released("south"))
    count("pad_ups")
end)
linblux.events.on("porta.alterar", function(open) root:set_attribute("porta_aberta", open) end)
)";
    const auto examplePath=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/examples/lua/interacao.lua";
    std::ifstream example(examplePath);CHECK(example.good());
    author.createScript(InstanceClass::Script,nullptr,"Interacao.lua")->source=std::string(std::istreambuf_iterator<char>(example),{});
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    game::PlayerInput input;input.script.keys.set(KEY_E);input.script.mouse.set(MOUSE_BUTTON_LEFT);
    input.script.pointer={123,45};input.script.delta={2,0};input.script.wheel=-1;
    input.script.gamepad=true;input.script.gamepadButtons.set(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);input.script.gamepadAxes[GAMEPAD_AXIS_LEFT_X]=.5f;
    runtime.update(1.f/60,input);runtime.update(1.f/60,input);
    const auto count=[&](const char* key){const auto& attrs=runtime.scene().workspace()->attributes();CHECK(attrs.count(key));return attrs.count(key)?std::get<double>(attrs.at(key)):0.;};
    CHECK(count("key_downs")==1);CHECK(count("clicks")==1);CHECK(count("pad_downs")==1);
    // Both E and the south button were pressed: two toggles, initially closed.
    runtime.update(1.f/60,input);CHECK(!std::get<bool>(runtime.scene().workspace()->attributes().at("porta_aberta")));
    runtime.pause();runtime.update(1.f/60,{});CHECK(count("key_downs")==1);CHECK(!runtime.scene().workspace()->attributes().count("key_ups"));
    runtime.resume();runtime.update(1.f/60,{});CHECK(count("key_ups")==1);CHECK(count("pad_ups")==1);
    input.script.gamepad=false;input.script.gamepadButtons.reset();input.script.mouse.reset();runtime.update(1.f/60,input);runtime.update(1.f/60,input);
    CHECK(count("key_downs")==2);CHECK(std::get<bool>(runtime.scene().workspace()->attributes().at("porta_aberta")));
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Input Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();CHECK(!runtime.scriptInput().keys.any());
}
void LinbluxPlayer() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* script=author.createScript(InstanceClass::Script,nullptr,"Jogador.lua");
    script->source=R"(
local player, root = linblux.player, linblux.scene.root
linblux.events.on("spawn", function(current)
    assert(current == player)
    root:set_attribute("spawns", (root:get_attribute("spawns") or 0) + 1)
end)
linblux.events.on("death", function() root:set_attribute("died", true) end)
linblux.events.on("health_changed", function(value) root:set_attribute("health", value) end)
player.max_health = 150
player.walk_speed = 22
player.jump_height = 8
player.auto_rotate = false
player:damage(25)
player:heal(10)
assert(player.health == 85)
local clips = linblux.animations.list()
assert(#clips > 0 and clips[1].duration > 0)
player:set_animation("walk", clips[1].id)
linblux.time.after(0.02, function()
    local previous = player.generation
    player:teleport(linblux.vec3(30,12,0))
    player:set_checkpoint()
    player:damage(200)
    assert(player.state == "dead")
    player:respawn()
    assert(player.generation == previous + 1 and player.position.x == 30)
    linblux.camera.configure({mode="first_person", distance=0.3, sensitivity=0.004, allow_shift_lock=true})
    linblux.time.after(0.03, function()
        assert(linblux.camera.state().first_person)
        root:set_attribute("player_ready", true)
    end)
end)
)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    const auto& attributes=runtime.scene().workspace()->attributes();
    CHECK(attributes.count("player_ready"));CHECK(attributes.count("died"));
    CHECK(std::get<double>(attributes.at("spawns"))==2);
    CHECK(runtime.health()==150&&runtime.settings().maxHealth==150&&runtime.settings().walkSpeed==22);
    CHECK(runtime.playerCamera().firstPerson()&&runtime.settings().enableShiftLock);
    CHECK(runtime.position().x==30);
    const auto previous=runtime.position();CHECK(!runtime.teleportPlayer({30,0,0},error));CHECK(NearlyEqual(runtime.position(),previous));
    CHECK(!runtime.setCheckpoint({NAN,0,0},error));
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Player Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    CHECK(runtime.start(author,error));CHECK(std::fabs(runtime.position().x)<.01f);runtime.stop();
}
void LuaFinalizerSafety() {
    Scene scene;scene.buildDefaultScene();std::string error;
    scene.createScript(InstanceClass::Script,nullptr,"Finalizer.lua")->source=R"(setmetatable({}, {__gc=function() while true do end end}))";
    scene.createScript(InstanceClass::Script,nullptr,"FalseFinalizer.lua")->source=R"(setmetatable({}, {__gc=false}))";
    scene.createScript(InstanceClass::Script,nullptr,"Metatable.lua")->source=R"(
local object = setmetatable({}, {__index={value=7}})
assert(object.value == 7)
local meta, retained = {}, {}
setmetatable(retained, meta)
meta.__gc = function() while true do end end
_G.retained = retained
print("metatable works")
)";
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));
    int denied=0;bool valid=false;for(const auto& message:runtime.scripts().messages()) {
        denied+=message.error&&message.text.find("__gc")!=std::string::npos;
        valid|=message.text=="metatable works";
    }
    CHECK(denied==2&&valid);
    const auto before=std::chrono::steady_clock::now();runtime.stop();
    CHECK(std::chrono::duration<double>(std::chrono::steady_clock::now()-before).count()<2);
    CHECK(runtime.scripts().memoryUsed()==0&&runtime.scripts().taskCount()==0);CHECK(runtime.validate(error));
}
void DynamicScriptLifecycle() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* part=author.createPart(PartShape::Block,{25,8,0},{2,2,2},"Original");part->setAnchored(true);
    author.createScript(InstanceClass::Script,part,"Componente.lua")->source=R"(
local owner = linblux.self
linblux.events.on("start", function() owner:set_attribute("starts", (owner:get_attribute("starts") or 0)+1) end)
linblux.events.on("update", function() owner:set_attribute("ticks", (owner:get_attribute("ticks") or 0)+1) end)
)";
    author.createScript(InstanceClass::Script,nullptr,"Controlador.lua")->source=R"(
linblux.time.after(0.02, function()
    local canceled = linblux.scene.create("part")
    local connection = linblux.events.on("destroy", function() error("canceled destroy") end, canceled)
    canceled:destroy()
    local flush = linblux.events.on("nothing", function() end)
    connection:disconnect()
    flush:disconnect()
    local copy = linblux.scene.find("Original"):clone()
    copy.name = "Clone"
    copy:set_attribute("starts", 0)
    copy:set_attribute("ticks", 0)
    local component
    for _, child in ipairs(copy:children()) do if child.type == "script" then component = child end end
    assert(component and component.enabled)
    local root = linblux.scene.root
    root:set_attribute("clone", copy.id)
    linblux.events.on("destroy", function(id) root:set_attribute("removed", id) end, copy)
    linblux.time.after(0.06, function()
        assert(copy:get_attribute("starts") == 1 and copy:get_attribute("ticks") > 0)
        local ticks = copy:get_attribute("ticks")
        component.enabled = false
        linblux.time.after(0.04, function()
            assert(copy:get_attribute("ticks") == ticks)
            component.enabled = true
            linblux.time.after(0.05, function()
                assert(copy:get_attribute("starts") == 2)
                component.enabled = false
                component.enabled = true
                local ticks = copy:get_attribute("ticks")
                linblux.time.after(0.05, function()
                    assert(copy:get_attribute("starts") == 3)
                    -- A restart must leave precisely one update subscription.
                    local before = copy:get_attribute("ticks")
                    linblux.time.after(0, function()
                        assert(copy:get_attribute("ticks") == before + 1)
                        copy:destroy()
                    end)
                end)
            end)
        end)
    end)
end)
)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<40;++i)runtime.update(1.f/60,{});
    const auto& attributes=runtime.scene().workspace()->attributes();CHECK(attributes.count("clone")&&attributes.count("removed"));
    if(attributes.count("removed")) {
        CHECK(std::get<double>(attributes.at("clone"))==std::get<double>(attributes.at("removed")));
        CHECK(runtime.scene().find(InstanceId(std::get<double>(attributes.at("clone"))))==nullptr);
    }
    CHECK(std::get<double>(runtime.scene().findPart(part->id())->attributes().at("starts"))==1);
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Lifecycle Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();CHECK(runtime.scripts().taskCount()==0);
}
void AnimationIds() {
    Scene scene;scene.buildDefaultScene();scene.installDefaultScripts();std::string error;
    auto* service=scene.service(InstanceClass::ServerScriptService);CHECK(service && service->children().size()==1);
    auto* animations=scene.createScript(InstanceClass::Script,service,"CustomCharacterAnimations.lua");
    CHECK(animations);if(!animations)return; // custom scripts remain supported; no automatic animation script
    auto edited=game::MakeWalk(65);edited.id="custom-walk";edited.name="My Walk";
    scene.gameAssets().animations.push_back(edited);
    animations->source=R"(game:GetService("Players").CharacterAdded:Connect(function(character)
character.Animator:SetAnimation("Walk", "anim://custom-walk")
character.Animator:SetAnimation("Jump", "anim://r6-jump")
local info = game:GetService("AnimationService"):GetAnimation("anim://custom-walk")
assert(info.Id == "anim://custom-walk" and info.RigType == "R6" and math.abs(info.Duration - 0.8) < 0.0001)
end))";
    const auto source=animations->source;scene.installDefaultScripts();CHECK(animations->source==source);CHECK(service->children().size()==2);
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));CHECK(runtime.settings().walkAnimation=="anim://custom-walk");
    CHECK(runtime.settings().jumpAnimation=="anim://r6-jump");
    for(const auto& message:runtime.scripts().messages())CHECK(!message.error);
    CHECK(!runtime.setAnimation("Walk","anim://missing",error));CHECK(runtime.settings().walkAnimation=="anim://custom-walk");
    CHECK(!runtime.setAnimation("Dance","anim://custom-walk",error));
    CHECK(runtime.scene().gameAssets().findAnimation(runtime.settings().walkAnimation)->sample(0)[game::LeftHip].rotation.x==edited.sample(0)[game::LeftHip].rotation.x);
    runtime.stop();
    Scene reopened;CHECK(projectio::fromJson(reopened,projectio::toJson(scene),error));
    auto* restored=static_cast<Script*>(reopened.find(animations->id()));CHECK(restored && restored->source==source);
    CHECK(runtime.start(reopened,error));CHECK(runtime.settings().walkAnimation=="anim://custom-walk");runtime.stop();
    auto legacy=scene.gameAssets();legacy.player.jumpAnimation.clear();
    CHECK(legacy.findAnimation("r6-jump")!=nullptr);CHECK(scene.validate(error));
}
void HealthAndVoid() {
    Scene scene;scene.buildDefaultScene();scene.gameAssets().createR6();std::string error;
    auto* script=scene.createScript(InstanceClass::Script,nullptr,"Health.server.lua");
    script->source=R"(game:GetService("Players").CharacterAdded:Connect(function(character)
workspace:SetAttribute("spawns", (workspace:GetAttribute("spawns") or 0) + 1)
local h=character.Humanoid
h.HealthChanged:Connect(function(value) workspace:SetAttribute("health",value) end)
h.Died:Connect(function() workspace:SetAttribute("deaths",(workspace:GetAttribute("deaths") or 0)+1) end)
end))";
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));CHECK(runtime.health()==100);
    CHECK(runtime.takeDamage(25,error));CHECK(runtime.health()==75);runtime.update(0,{});
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("health"))==75);
    CHECK(runtime.takeDamage(50,error));CHECK(runtime.health()==25);CHECK(runtime.heal(10,error));CHECK(runtime.health()==35);
    CHECK(!runtime.takeDamage(std::numeric_limits<float>::quiet_NaN(),error));
    CHECK(!runtime.heal(-1,error));CHECK(runtime.health()==35);
    CHECK(runtime.takeDamage(100,error));CHECK(runtime.health()==0);CHECK(runtime.humanoidState()==game::HumanoidState::Dead);
    CHECK(runtime.takeDamage(1,error));runtime.update(0,{});
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("deaths"))==1);
    CHECK(!runtime.heal(100,error));
    auto generation=runtime.characterGeneration();runtime.pause();
    for(int i=0;i<180;++i)runtime.update(1.f/60,{});CHECK(runtime.characterGeneration()==generation);
    runtime.resume();game::PlayerInput input;input.movement={1,0};const float x=runtime.position().x;
    runtime.update(.1f,input);CHECK(std::fabs(runtime.position().x-x)<.01f);
    for(int i=0;i<130;++i)runtime.update(1.f/60,{});
    CHECK(runtime.characterGeneration()==generation+1);CHECK(runtime.health()==100);
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("spawns"))==2);
    for(int i=0;i<70;++i){CHECK(runtime.resetCharacter(error));runtime.update(0,{});CHECK(runtime.health()==100);CHECK(runtime.validate(error));}
    for(const auto& message:runtime.scripts().messages())CHECK(!message.error);
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("deaths"))==71);
    CHECK(runtime.bodyCount()==scene.parts().size()+1);runtime.stop();
    Scene hazard;hazard.buildDefaultScene();hazard.gameAssets().createR6();
    hazard.findPart(4)->setAttribute("Damage",25.);
    CHECK(runtime.start(hazard,error));for(int i=0;i<30;++i)runtime.update(1.f/60,{});
    CHECK(runtime.health()==75);runtime.stop();
    hazard.findPart(4)->setCanCollide(false);
    CHECK(runtime.start(hazard,error));for(int i=0;i<30;++i)runtime.update(1.f/60,{});
    CHECK(runtime.health()==100);runtime.stop();
    Scene voidScene;voidScene.createPart(PartShape::Block,{10,-30,0},{4,4,4},"Falling");
    voidScene.gameAssets().createR6();voidScene.workspace()->setFallenPartsDestroyHeight(-20);
    const auto author=projectio::toJson(voidScene);CHECK(runtime.start(voidScene,error));
    for(int i=0;i<70;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().parts().empty());CHECK(runtime.health()==0);
    for(int i=0;i<130;++i)runtime.update(1.f/60,{});
    CHECK(runtime.characterGeneration()>=2);CHECK(projectio::toJson(voidScene)==author);
    CHECK(runtime.validate(error));runtime.stop();
    Scene reloaded;CHECK(projectio::fromJson(reloaded,author,error));CHECK(reloaded.workspace()->fallenPartsDestroyHeight()==-20);
    CHECK(!reloaded.workspace()->setFallenPartsDestroyHeight(std::numeric_limits<float>::infinity()));
    Scene scripted;scripted.buildDefaultScene();
    auto* damageScript=scripted.createScript(InstanceClass::Script,nullptr,"Damage.server.lua");
    damageScript->source=R"(game:GetService("Players").CharacterAdded:Connect(function(character)
local h=character.Humanoid
h.Died:Connect(function()workspace:SetAttribute("deaths",(workspace:GetAttribute("deaths") or 0)+1)end)
assert(h.Health==100 and h.MaxHealth==100)
h:TakeDamage(25);assert(h.Health==75)
h:TakeDamage(50);assert(h.Health==25)
h:Heal(5);assert(h.Health==30)
h.Health=0;h:TakeDamage(1)
assert(h.State=="Dead")
end))";
    CHECK(runtime.start(scripted,error));runtime.update(0,{});
    CHECK(runtime.health()==0);
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("deaths"))==1);
    for(const auto& message:runtime.scripts().messages())CHECK(!message.error);
    runtime.stop();
}
void PartChildScripts() {
    Scene scene;scene.buildDefaultScene();
    auto* part=scene.createPart(PartShape::Block,{20,4,0});part->setAnchored(true);
    auto* child=scene.createScript(InstanceClass::Script,part,"PartChild.lua");
    child->source="script.Parent:SetAttribute('childRan', true)";
    std::string error;Scene loaded;
    CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));
    CHECK(loaded.find(child->id())!=nullptr);
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));
    CHECK(runtime.scene().findPart(part->id())->attributes().count("childRan")==1);
    CHECK(part->attributes().empty());runtime.stop();
}
void IndependentFall() {
    Scene scene;scene.buildDefaultScene();scene.gameAssets().createR6();std::string error;
    auto& assets=scene.gameAssets();auto* fall=assets.findAnimation(assets.player.fallAnimation);
    CHECK(fall && fall->id!="r6-jump");if(!fall)return;
    for(const auto& track:fall->tracks)CHECK(track.empty());
    CHECK(!assets.removeAnimation(fall->id,error));
    auto unused=game::MakeFall();unused.id="unused-fall";assets.animations.push_back(unused);
    CHECK(assets.removeAnimation("unused-fall",error));CHECK(assets.validate(error));
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    CHECK(runtime.grounded());game::PlayerInput input;input.jump=true;runtime.update(1.f/60,input);
    bool sawJump=false,sawFall=false;int fallingFrames=0;
    for(int i=0;i<90;++i) {
        runtime.update(1.f/60,{});
        if(runtime.humanoidState()==game::HumanoidState::Jumping)sawJump=true;
        if(runtime.humanoidState()==game::HumanoidState::Freefall) {
            sawFall=true;
            if(++fallingFrames==14)CHECK(NearlyEqual(runtime.pose()[game::LeftShoulder].rotation,QuaternionIdentity(),.02f));
        }
    }
    CHECK(sawJump && sawFall && fallingFrames>=14);runtime.stop();
    assets.findAnimation(assets.player.fallAnimation)->setKey(game::LeftShoulder,{0,{{0,0,0},QuaternionFromEuler(.4f,0,0)}});
    Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));
    CHECK(loaded.gameAssets().player.fallAnimation==assets.player.fallAnimation);
    CHECK(runtime.start(loaded,error));CHECK(runtime.setAnimation("Fall","anim://r6-fall",error));runtime.stop();
    assets.player.fallAnimation="anim://missing";CHECK(!assets.validate(error));
}
void JumpTiming() {
    Scene author;author.buildDefaultScene();author.gameAssets().createR6();std::string error;
    game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    CHECK(runtime.grounded());const float floorY=runtime.position().y;
    for(auto* part:runtime.mutableScene().parts())part->setPosition(Vector3Add(part->position(),{0,-1000,0}));
    runtime.update(1.f/60,{});CHECK(!runtime.grounded());
    game::PlayerInput jump;jump.jump=true;runtime.update(1.f/60,jump);
    CHECK(runtime.velocity().y>1); // press after support was lost, inside 100 ms
    runtime.stop();CHECK(runtime.start(author,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    for(auto* part:runtime.mutableScene().parts())part->setPosition(Vector3Add(part->position(),{0,-1000,0}));
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    runtime.update(1.f/60,jump);CHECK(runtime.velocity().y<0); // grace expired
    runtime.stop();CHECK(runtime.start(author,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    runtime.update(1.f/60,jump);bool queued=false,bounced=false;
    for(int i=0;i<120;++i) {
        if(!queued && runtime.velocity().y< -1 && runtime.position().y<floorY+1.2f && runtime.position().y>floorY+.2f) {
            CHECK(!runtime.grounded());runtime.update(1.f/60,jump);queued=true;
        } else runtime.update(1.f/60,{});
        if(queued && runtime.velocity().y>1){bounced=true;break;}
    }
    CHECK(queued && bounced); // a single press before contact executes after landing
    CHECK(runtime.validate(error));runtime.stop();
}
void ObbyLanding() {
    Scene scene;auto* platform=scene.createSpawnLocation({0,.5f,0},{.4f,1,6});
    scene.gameAssets().createR6();std::string error;game::RuntimeSession runtime;
    CHECK(runtime.start(scene,error));for(int i=0;i<90;++i)runtime.update(1.f/60,{});
    const float rest=runtime.position().y;
    game::PlayerInput jump;jump.jump=true;
    // A key queued in the exact fixed step which makes contact must launch
    // without spending another physics tick standing on the small platform.
    for(float frame:{1.f/60,1.f/30}) {
        runtime.stop();CHECK(runtime.start(scene,error));
        for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        runtime.update(frame,jump);bool pressed=false;
        for(int i=0;i<120;++i) {
            const float gap=runtime.position().y-rest,vy=runtime.velocity().y;
            if(vy< -5 && gap>.13f && gap< -vy*frame+.08f) {
                runtime.update(frame,jump);pressed=true;
                CHECK(runtime.velocity().y>1);break;
            }
            runtime.update(frame,{});
        }
        CHECK(pressed);
        for(int i=0;i<120 && !runtime.grounded();++i)runtime.update(1.f/60,{});
        CHECK(runtime.grounded());
    }
    // Capsule supported at an edge, with no surface under its centre ray.
    auto* support=runtime.mutableScene().findPart(platform->id());
    support->setSize({1.4f,1,6});support->setPosition({.75f,.5f,0});
    for(int i=0;i<6;++i)runtime.update(1.f/60,{});
    CHECK(runtime.grounded());runtime.update(1.f/60,jump);CHECK(runtime.velocity().y>1);
    const float first=runtime.velocity().y;runtime.update(1.f/60,jump);
    CHECK(runtime.velocity().y<first); // no second impulse from old contacts
    CHECK(runtime.validate(error));runtime.stop();
}
void ConsecutivePlatforms() {
    for(float dt:{1.f/60,1.f/30}) {
        Scene scene;scene.createSpawnLocation({0,.5f,0},{1.4f,1,3});
        for(float x:{2.4f,4.8f,7.2f})scene.createPart(PartShape::Block,{x,.5f,0},{1.4f,1,3})->setAnchored(true);
        scene.gameAssets().createR6();scene.gameAssets().player.walkSpeed=6;scene.gameAssets().player.jumpHeight=2;
        game::RuntimeSession runtime;std::string error;CHECK(runtime.start(scene,error));
        for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        const float floor=runtime.position().y;game::PlayerInput input;input.movement={1,0};input.jump=true;
        runtime.update(dt,input);int launches=0;bool queued=false;
        for(int frame=0;frame<150 && launches<3;++frame) {
            const float vy=runtime.velocity().y;
            input.jump=!queued && vy< -1 && runtime.position().y<floor+.5f;
            queued|=input.jump;runtime.update(dt,input);
            if(queued && runtime.velocity().y>1) {++launches;queued=false;CHECK(runtime.position().x>launches*1.7f);}
        }
        CHECK(launches==3);CHECK(runtime.validate(error));runtime.stop();
    }
}
void AnimationDeletion() {
    Scene scene;scene.gameAssets().createR6();auto& assets=scene.gameAssets();std::string error;
    auto clip=game::MakeIdle();clip.id="delete-me";clip.name="Renamed animation";assets.animations.push_back(clip);
    CHECK(assets.removeAnimation(clip.id,error));CHECK(!assets.findAnimation(clip.id));
    const auto jump=assets.player.jumpAnimation,fall=assets.player.fallAnimation;
    CHECK(!assets.removeAnimation(assets.player.walkAnimation,error));
    const auto walk=assets.player.walkAnimation;
    CHECK(assets.removeAnimation(walk,error,true));CHECK(assets.player.walkAnimation.empty());
    CHECK(assets.player.jumpAnimation==jump && assets.player.fallAnimation==fall);
    for(const auto id:{assets.player.idleAnimation,jump,fall})CHECK(assets.removeAnimation(id,error,true));
    CHECK(assets.animations.empty());CHECK(assets.validate(error));
    Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));
    CHECK(loaded.gameAssets().animations.empty());game::RuntimeSession runtime;
    CHECK(runtime.start(loaded,error));runtime.update(.1f,{});CHECK(runtime.validate(error));runtime.stop();
}
void NativeCloneBudget() {
    Scene scene;scene.buildDefaultScene();auto* folder=scene.createContainer(InstanceClass::Folder,nullptr,"Payload");
    for(int i=0;i<17;++i) {
        auto* node=scene.createScript(InstanceClass::ModuleScript,folder,"Data"+std::to_string(i));
        node->source="--"+std::string(262140,'x');
    }
    auto* script=scene.createScript(InstanceClass::Script,nullptr,"CloneBudget.lua");
    script->source="workspace.Payload:Clone()";
    game::RuntimeSession runtime;std::string error;CHECK(runtime.start(scene,error));
    bool rejected=false;for(const auto& message:runtime.scripts().messages())
        rejected|=message.error&&message.text.find("native content budget")!=std::string::npos;
    CHECK(rejected);CHECK(runtime.scene().objects().size()==scene.objects().size());
    CHECK(runtime.validate(error));runtime.stop();
}

// Ferramentas de transformacao: medidas em studs/graus, aplicacao exata a
// partir da pose inicial, formatacao e Undo/Redo de uma operacao por arraste.
void TransformTools() {
    using namespace transformmetrics;
    const gizmomath::TransformSnapshot origin{{0,0,0},{0,0,0,1},{4,4,4}};
    Context move; move.kind=Kind::Move; move.axis=0; move.space=Space::World; move.start=origin;
    gizmomath::TransformSnapshot current=origin;
    current.position={4,0,0};
    Measurement measured=Measure(move,current);
    CHECK(measured.valid);CHECK(NearlyEqual(measured.value,4.0f));
    char text[64];
    CHECK(Format(measured,text,sizeof(text)));
    CHECK(std::string(text)=="X  +4.00 studs");
    // Aplicar 4 studs a partir da pose inicial e reaplicar 2 nao acumula 6.
    CHECK(NearlyEqual(Apply(move,4.0f).position,{4,0,0}));
    CHECK(NearlyEqual(Apply(move,2.0f).position,{2,0,0}));
    CHECK(NearlyEqual(Apply(move,-3.5f).position,{-3.5f,0,0}));
    measured=Measure(move,Apply(move,-3.5f));
    CHECK(NearlyEqual(measured.value,-3.5f));
    CHECK(std::string(text)=="X  +4.00 studs");
    CHECK(Format(measured,text,sizeof(text)));
    CHECK(std::string(text)=="X  -3.50 studs");

    // Local: eixo X da peca girada 90 graus em Y aponta para -Z do mundo.
    Context local=move; local.space=Space::Local;
    local.start.rotation=QuaternionFromAxisAngle({0,1,0},90.0f*DEG2RAD);
    const Vector3 localAxis=AxisVector(local);
    CHECK(NearlyEqual(localAxis,{0,0,-1},0.001f));
    const auto localMoved=Apply(local,2.5f);
    CHECK(NearlyEqual(localMoved.position,{0,0,-2.5f},0.001f));
    CHECK(NearlyEqual(Measure(local,localMoved).value,2.5f,0.001f));
    CHECK(localMoved.rotation.x==local.start.rotation.x&&
          localMoved.rotation.w==local.start.rotation.w);

    // Escala a partir de uma face de uma Part rotacionada: tamanho exato,
    // face oposta estavel e minimo respeitado.
    Context scale; scale.kind=Kind::Size; scale.axis=0; scale.side=1;
    scale.space=Space::Local;
    scale.start=origin;
    scale.start.rotation=QuaternionFromAxisAngle({0,1,0},30.0f*DEG2RAD);
    auto scaled=Apply(scale,8.5f);
    CHECK(NearlyEqual(scaled.size.x,8.5f));CHECK(NearlyEqual(scaled.size.y,4.0f));
    CHECK(NearlyEqual(scaled.rotation,scale.start.rotation));
    CHECK(NearlyEqual(Measure(scale,scaled).value,8.5f));
    // (centro novo - centro velho) . eixoX local == metade do crescimento
    const Vector3 grow=Vector3Subtract(scaled.position,scale.start.position);
    CHECK(NearlyEqual(Vector3DotProduct(grow,gizmomath::Axis(0,scale.start.rotation,true)),2.25f,0.001f));
    // Face oposta: o ponto no lado negativo continua no mesmo lugar.
    const Vector3 negativeFace=Vector3Add(scale.start.position,
        Vector3Scale(gizmomath::Axis(0,scale.start.rotation,true),-2.0f));
    const Vector3 negativeFaceAfter=Vector3Add(scaled.position,
        Vector3Scale(gizmomath::Axis(0,scale.start.rotation,true),-4.25f));
    CHECK(NearlyEqual(negativeFace,negativeFaceAfter,0.001f));
    // Alt: escala centrada deixa o centro parado e move as duas faces.
    Context centered=scale; centered.centered=true;
    auto centeredPose=Apply(centered,10.0f);
    CHECK(NearlyEqual(centeredPose.position,scale.start.position));
    CHECK(NearlyEqual(centeredPose.size.x,10.0f));
    const auto centeredGizmo=gizmomath::ScaleCenteredFace(
        scale.start,0,1,true,3.0f,0.0f);
    CHECK(NearlyEqual(centeredGizmo.position,scale.start.position));
    CHECK(NearlyEqual(centeredGizmo.size.x,10.0f));
    CHECK(NearlyEqual(centeredGizmo.size.y,scale.start.size.y));
    const auto centeredMin=gizmomath::ScaleCenteredFace(
        scale.start,0,1,true,-100.0f,0.0f);
    CHECK(NearlyEqual(centeredMin.size.x,kMinPartSize));
    auto minimum=Apply(scale,0.0f);
    CHECK(NearlyEqual(minimum.size.x,kMinPartSize));
    auto below=Apply(scale,-5.0f);
    CHECK(NearlyEqual(below.size.x,kMinPartSize));CHECK(NearlyEqual(below.size.x,minimum.size.x));
    // Em World o eixo positivo pode apontar para a face local negativa.
    Context worldScale=scale; worldScale.space=Space::World; worldScale.side=1;
    worldScale.start.rotation=QuaternionFromAxisAngle({0,1,0},90.0f*DEG2RAD);
    CHECK(SizeComponent(worldScale)==2);
    const auto worldScaled=Apply(worldScale,6.0f);
    CHECK(NearlyEqual(worldScaled.size.z,6.0f));

    // Rotacao: angulo com sinal, 90 graus, wrap de +190 para -170.
    Context angle; angle.kind=Kind::Angle; angle.axis=1; angle.space=Space::World;
    angle.start=origin;
    const auto turned=Apply(angle,45.0f);
    CHECK(NearlyEqual(Measure(angle,turned).value,45.0f,0.01f));
    CHECK(std::string(text)=="X  -3.50 studs");
    CHECK(Format(Measure(angle,turned),text,sizeof(text)));
    CHECK(std::string(text)=="Y  +45.00 deg");
    CHECK(NearlyEqual(Measure(angle,Apply(angle,-45.0f)).value,-45.0f,0.01f));
    CHECK(NearlyEqual(Measure(angle,Apply(angle,90.0f)).value,90.0f,0.01f));
    CHECK(NearlyEqual(Measure(angle,Apply(angle,190.0f)).value,-170.0f,0.01f));
    // q e -q descrevem a mesma pose: medir -45 nao pode virar +315.
    auto negated=Apply(angle,-45.0f);
    negated.rotation={-negated.rotation.x,-negated.rotation.y,-negated.rotation.z,-negated.rotation.w};
    CHECK(NearlyEqual(Measure(angle,negated).value,-45.0f,0.01f));
    CHECK(NearlyEqual(Apply(angle,90.0f).position,origin.position));
    CHECK(NearlyEqual(Apply(angle,90.0f).size,origin.size));

    // Plano XY: mede os dois eixos e nao inventa valor digitado.
    Context plane; plane.kind=Kind::MovePlane; plane.axis=0; plane.space=Space::World;
    plane.start=origin;
    auto planePose=origin; planePose.position={3,2,0};
    const auto planeMeasure=Measure(plane,planePose);
    CHECK(planeMeasure.valid);CHECK(!planeMeasure.editable());
    CHECK(NearlyEqual(planeMeasure.value,3.0f));CHECK(NearlyEqual(planeMeasure.valueB,2.0f));
    CHECK(planeMeasure.axis==0&&planeMeasure.axisB==1);
    CHECK(Format(planeMeasure,text,sizeof(text)));
    CHECK(std::string(text)=="X +3.00 | Y +2.00 studs");
    CHECK(NearlyEqual(Apply(plane,5.0f).position,origin.position));

    // Formatacao do Properties: sem notacao cientifica e sem -0.
    CHECK(numfmt::Text(0.0f)=="0");
    CHECK(numfmt::Text(-9.7066e-12f)=="0");
    CHECK(numfmt::Text(-0.0f)=="0");
    CHECK(numfmt::Text(1.5f)=="1.5");
    CHECK(numfmt::Text(-2.25f)=="-2.25");
    CHECK(numfmt::Text(8.5f,2)=="8.5");
    CHECK(numfmt::Text(123456.0f)=="123456");
    float parsed=0.0f;
    CHECK(numfmt::Parse("4",parsed));CHECK(NearlyEqual(parsed,4.0f));
    CHECK(numfmt::Parse(" -8.5 ",parsed));CHECK(NearlyEqual(parsed,-8.5f));
    CHECK(!numfmt::Parse("4studs",parsed));
    CHECK(!numfmt::Parse("",parsed));
    CHECK(!numfmt::Parse("nan",parsed));
    CHECK(!numfmt::Parse("1e999",parsed));

    // Undo/Redo: um arraste = uma operacao.
    EditHistory history;
    loblox::Transform before{{0,0,0},{0,0,0,1},{4,4,4}};
    loblox::Transform after=before; after.position={4,0,0};
    CHECK(!history.canUndo());
    CHECK(!history.record(7,before,before)); // clique sem arrastar
    CHECK(history.record(7,before,after));
    CHECK(history.undoCount()==1);CHECK(history.canUndo());CHECK(!history.canRedo());
    TransformEdit edit;
    CHECK(history.undo(edit));CHECK(edit.id==7);CHECK(NearlyEqual(edit.before.position,{0,0,0}));
    CHECK(NearlyEqual(edit.after.position,{4,0,0}));CHECK(history.canRedo());
    CHECK(history.redo(edit));CHECK(NearlyEqual(edit.after.position,{4,0,0}));
    CHECK(history.undo(edit));
    loblox::Transform other=before; other.position={0,5,0};
    CHECK(history.record(7,after,other)); // ramo novo descarta o redo
    CHECK(!history.canRedo());
    // q e -q sao a mesma orientacao: nao geram operacao fantasma.
    loblox::Transform flipped=before;
    flipped.rotation={-before.rotation.x,-before.rotation.y,-before.rotation.z,-before.rotation.w};
    CHECK(!history.record(9,before,flipped));
    EditHistory limited;
    for(int i=0;i<200;++i) {
        loblox::Transform a=before,b=before;a.position.x=float(i);b.position.x=float(i+1);
        limited.record(1,a,b);
    }
    CHECK(limited.undoCount()==EditHistory::kLimit);
    int undos=0;while(limited.undo(edit))++undos;
    CHECK(undos==static_cast<int>(EditHistory::kLimit));
    // O mais antigo preservado e o 72 (128 entradas mais recentes).
    CHECK(NearlyEqual(edit.before.position.x,float(200-EditHistory::kLimit)));
}
}
int main() {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    char temporary[]="/tmp/loblox-studio-XXXXXX";const char* directory=mkdtemp(temporary);
    if(!directory)return 1;diag::configure(std::string(directory)+"/studio.jsonl");
    Hierarchy();Scripts();LinbluxScripts();LinbluxExamples();LinbluxPhysics();LinbluxFixedContacts();LinbluxCameraControl();LinbluxTaskCancellation();LinbluxInput();LinbluxPlayer();LuaFinalizerSafety();DynamicScriptLifecycle();AnimationIds();HealthAndVoid();PartChildScripts();IndependentFall();JumpTiming();ObbyLanding();ConsecutivePlatforms();AnimationDeletion();NativeCloneBudget();TransformTools();
    std::printf("Studio: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);return failures?1:0;
}

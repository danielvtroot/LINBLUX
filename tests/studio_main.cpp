#include <chrono>
#include <cstdio>
#include <nlohmann/json.hpp>
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include "serialization/ProjectIO.h"
#include "serialization/SceneLua.h"
#include "game/Runtime.h"

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
    bool refused=false; try { scenelua::toLua(scene); } catch(...) { refused=true; } CHECK(refused);
    auto* copy=scene.duplicateTree(*folder); CHECK(copy && copy!=folder);
    CHECK(copy && copy->children().size()==2); CHECK(scene.parts().size()==4);
    CHECK(scene.validate(error)); physics.syncScene(scene); CHECK(physics.validate(scene,error));
    scene.select(part); CHECK(scene.remove(folder)); CHECK(scene.find(id)==nullptr);
    CHECK(scene.selection()==nullptr); CHECK(!physics.hasBody(id)); CHECK(scene.validate(error));
    CHECK(!scene.remove(scene.workspace()));
    auto* guarded=scene.createContainer(InstanceClass::Folder);
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
}
int main() { Hierarchy(); Scripts(); std::printf("Studio: %d checks, %d failures\n",checks,failures); return failures?1:0; }

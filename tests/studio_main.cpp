#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <nlohmann/json.hpp>
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include "serialization/ProjectIO.h"
#include "serialization/SceneLua.h"
#include "game/Runtime.h"
#include "core/Diagnostics.h"

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
}
int main() {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    char temporary[]="/tmp/loblox-studio-XXXXXX";const char* directory=mkdtemp(temporary);
    if(!directory)return 1;diag::configure(std::string(directory)+"/studio.jsonl");
    Hierarchy();Scripts();AnimationIds();HealthAndVoid();PartChildScripts();IndependentFall();JumpTiming();NativeCloneBudget();
    std::printf("Studio: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);return failures?1:0;
}

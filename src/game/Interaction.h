#pragma once
#include <map>
#include <set>
#include <string>
#include "objects/Instance.h"
#include "game/ScriptInput.h"
namespace loblox {class Scene;class PhysicsWorld;}
namespace loblox::game {
struct InteractionOptions {std::string label;float distance=6,holdSeconds=.6f;int key=KEY_E;bool automaticItem=false;};
struct InteractionPrompt {InstanceId object=0;std::string label,key;float progress=0;};
class InteractionSystem {
public:
    InteractionSystem(Scene& scene,PhysicsWorld& physics):scene_(scene),physics_(physics){}
    void add(InstanceId object,InteractionOptions options={});
    void remove(InstanceId object,bool suppressAutomatic=true);
    void ensureItem(InstanceId object,const std::string& label);
    void pruneItems(const std::set<InstanceId>& worldItems);
    InstanceId update(Vector3 player,const ScriptInput& input,float dt);
    void cancel();
    const InteractionPrompt& prompt()const{return prompt_;}
private:
    Scene& scene_;PhysicsWorld& physics_;std::map<InstanceId,InteractionOptions> entries_;
    std::set<InstanceId> suppressed_;InteractionPrompt prompt_;float elapsed_=0;bool latched_=false;int latchKey_=KEY_E;
};
}

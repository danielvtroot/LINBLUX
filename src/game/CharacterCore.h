#pragma once
#include "scene/Scene.h"
#include "game/Rig.h"
namespace loblox::game {
// Character definitions are ordinary Models/Parts. IDs and role/controller
// attributes use Scene's existing transactional serialization.
class CharacterCore {
public:
    static bool hidden(const Instance& part);
    static std::string role(const Instance& part);
    static bool setRole(Instance& part,const std::string& role,std::string& error);
    static bool validate(const Scene& scene,InstanceId model,std::string& error);
    static InstanceId spawn(Scene& scene,InstanceId source,RigidTransform root,std::string& error);
    static bool move(Scene& scene,InstanceId character,RigidTransform root,std::string& error);
    static std::vector<InstanceId> parts(const Scene& scene,InstanceId character,const std::string& role={});
};
}

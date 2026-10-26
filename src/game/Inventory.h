#pragma once
#include <memory>
#include <string>
#include <vector>
#include "objects/Instance.h"
namespace loblox {class Scene;class PhysicsWorld;}
namespace loblox::game {
struct ItemOptions {
    std::string name,icon;
    bool equipable=true;
    ModelFrame grip{{1,0,-2},{0,0,0,1},1};
};
struct ItemInfo {InstanceId id=0;std::string type,name,icon,state;bool equipable=true;};
struct InventoryChange {InstanceId id=0;std::string action;};
// Single-player session state. Authoring lives in Scene/scripts, not this bag.
class Inventory {
public:
    Inventory(Scene& scene,PhysicsWorld& physics);
    ~Inventory();
    void registerType(const std::string& type,InstanceId prototype,ItemOptions options={});
    InstanceId prototype(const std::string& type) const;
    InstanceId spawn(const std::string& type,const ModelFrame* pose=nullptr);
    void collect(InstanceId item);
    void equip(InstanceId item);
    void unequip();
    void drop(InstanceId item);
    void remove(InstanceId item);
    ItemInfo info(InstanceId item) const;
    std::vector<InstanceId> list() const;
    InstanceId equipped() const;
    void update(Vector3 player,float yaw);
    std::vector<InventoryChange> takeChanges();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}

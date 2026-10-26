#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "objects/Instance.h"

namespace loblox::game { class RuntimeSession; }
namespace loblox::scripting {
struct Message { bool error=false; std::string script; std::string text; InstanceId scriptId=0; int line=0; };
// Host policy, never loaded automatically from an imported experience.
struct Limits {
    std::size_t memoryBytes=8*1024*1024;
    int instructionsPerTask=100000, instructionsPerFrame=250000;
    std::size_t tasks=128, listeners=128;
    unsigned spatialQueries=8;
};
// One quota-limited in-process Lua 5.4 VM per Play, never an editor evaluator.
// This is a reliability boundary, not an OS security boundary for hostile code.
class LuaRuntime {
public:
    LuaRuntime(); ~LuaRuntime();
    LuaRuntime(const LuaRuntime&)=delete;
    LuaRuntime& operator=(const LuaRuntime&)=delete;
    bool start(game::RuntimeSession& runtime);
    bool configure(const Limits& limits,std::string& error); // only while stopped
    const Limits& limits() const;
    void stop();
    void update(float dt);
    void fixedUpdate(float dt);
    bool wantsContacts() const;
    void contact(InstanceId first,InstanceId second,bool entering);
    void characterAdded();
    void healthChanged(float health,bool died);
    void inventoryChanged(InstanceId item,const std::string& action);
    const std::vector<Message>& messages() const;
    std::size_t errorCount() const; // retained across stop, reset on the next Play
    void hostError(const std::string& text);
    std::size_t memoryUsed() const;
    std::size_t taskCount() const;
    bool active() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace loblox::scripting

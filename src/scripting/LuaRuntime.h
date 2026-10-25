#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "objects/Instance.h"

namespace loblox::game { class RuntimeSession; }
namespace loblox::scripting {
struct Message { bool error=false; std::string script; std::string text; InstanceId scriptId=0; int line=0; };
// One quota-limited in-process Lua 5.4 VM per Play, never an editor evaluator.
// This is a reliability boundary, not an OS security boundary for hostile code.
class LuaRuntime {
public:
    LuaRuntime(); ~LuaRuntime();
    LuaRuntime(const LuaRuntime&)=delete;
    LuaRuntime& operator=(const LuaRuntime&)=delete;
    bool start(game::RuntimeSession& runtime);
    void stop();
    void update(float dt);
    void characterAdded();
    void healthChanged(float health,bool died);
    const std::vector<Message>& messages() const;
    std::size_t memoryUsed() const;
    std::size_t taskCount() const;
    bool active() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace loblox::scripting

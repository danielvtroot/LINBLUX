#pragma once

#include <string>

namespace loblox {
class Scene;

namespace scenelua {

// Declarative literal tables only. These functions never invoke a Lua runtime.
std::string toLua(const Scene& scene);
bool fromLua(Scene& scene, const std::string& text, std::string& error);
bool save(const Scene& scene, const std::string& path, std::string& error);
bool load(Scene& scene, const std::string& path, std::string& error);

}  // namespace scenelua
}  // namespace loblox

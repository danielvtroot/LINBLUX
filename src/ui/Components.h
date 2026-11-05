#pragma once
#include <string>
namespace loblox::ui {
enum class CommandDomain { Studio, Experience };
struct ButtonSpec {
    std::string id,action,label,value,icon,classes,tooltip;
    CommandDomain domain=CommandDomain::Studio;
    bool enabled=true;
};
// RmlUi owns pointer/keyboard activation and focus. This factory centralizes
// official command metadata, escaping, accessible names and icon markup.
std::string ButtonMarkup(const ButtonSpec& spec);
std::string EscapeMarkup(const std::string& value);
}

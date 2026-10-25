#pragma once

namespace loblox {

enum class ToolMode {
    Select,
    Move,
    Scale,
    Rotate,
};

inline const char* ToolModeName(ToolMode mode) {
    switch (mode) {
        case ToolMode::Select:
            return "Select";
        case ToolMode::Move:
            return "Move";
        case ToolMode::Scale:
            return "Scale";
        case ToolMode::Rotate:
            return "Rotate";
    }
    return "Select";
}

}  // namespace loblox

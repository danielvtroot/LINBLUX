#pragma once
#include <array>
#include <bitset>
#include "raylib.h"

namespace loblox::game {
struct InputButton { const char* name; int code; };
inline constexpr InputButton ScriptKeys[]{
    {"space",KEY_SPACE},{"enter",KEY_ENTER},{"tab",KEY_TAB},{"backspace",KEY_BACKSPACE},
    {"left",KEY_LEFT},{"right",KEY_RIGHT},{"up",KEY_UP},{"down",KEY_DOWN},
    {"left_shift",KEY_LEFT_SHIFT},{"right_shift",KEY_RIGHT_SHIFT},
    {"left_ctrl",KEY_LEFT_CONTROL},{"right_ctrl",KEY_RIGHT_CONTROL},
    {"left_alt",KEY_LEFT_ALT},{"right_alt",KEY_RIGHT_ALT},
    {"home",KEY_HOME},{"end",KEY_END},{"page_up",KEY_PAGE_UP},{"page_down",KEY_PAGE_DOWN},
    {"insert",KEY_INSERT},{"delete",KEY_DELETE},
    {"a",KEY_A},{"b",KEY_B},{"c",KEY_C},{"d",KEY_D},{"e",KEY_E},{"f",KEY_F},
    {"g",KEY_G},{"h",KEY_H},{"i",KEY_I},{"j",KEY_J},{"k",KEY_K},{"l",KEY_L},
    {"m",KEY_M},{"n",KEY_N},{"o",KEY_O},{"p",KEY_P},{"q",KEY_Q},{"r",KEY_R},
    {"s",KEY_S},{"t",KEY_T},{"u",KEY_U},{"v",KEY_V},{"w",KEY_W},{"x",KEY_X},{"y",KEY_Y},{"z",KEY_Z},
    {"0",KEY_ZERO},{"1",KEY_ONE},{"2",KEY_TWO},{"3",KEY_THREE},{"4",KEY_FOUR},
    {"5",KEY_FIVE},{"6",KEY_SIX},{"7",KEY_SEVEN},{"8",KEY_EIGHT},{"9",KEY_NINE}
};
inline constexpr InputButton ScriptMouseButtons[]{
    {"left",MOUSE_BUTTON_LEFT},{"right",MOUSE_BUTTON_RIGHT},{"middle",MOUSE_BUTTON_MIDDLE},
    {"side",MOUSE_BUTTON_SIDE},{"extra",MOUSE_BUTTON_EXTRA}
};
inline constexpr InputButton ScriptGamepadButtons[]{
    {"up",GAMEPAD_BUTTON_LEFT_FACE_UP},{"right",GAMEPAD_BUTTON_LEFT_FACE_RIGHT},
    {"down",GAMEPAD_BUTTON_LEFT_FACE_DOWN},{"left",GAMEPAD_BUTTON_LEFT_FACE_LEFT},
    {"north",GAMEPAD_BUTTON_RIGHT_FACE_UP},{"east",GAMEPAD_BUTTON_RIGHT_FACE_RIGHT},
    {"south",GAMEPAD_BUTTON_RIGHT_FACE_DOWN},{"west",GAMEPAD_BUTTON_RIGHT_FACE_LEFT},
    {"left_bumper",GAMEPAD_BUTTON_LEFT_TRIGGER_1},{"right_bumper",GAMEPAD_BUTTON_RIGHT_TRIGGER_1},
    {"left_trigger",GAMEPAD_BUTTON_LEFT_TRIGGER_2},{"right_trigger",GAMEPAD_BUTTON_RIGHT_TRIGGER_2},
    {"select",GAMEPAD_BUTTON_MIDDLE_LEFT},{"start",GAMEPAD_BUTTON_MIDDLE_RIGHT},
    {"left_stick",GAMEPAD_BUTTON_LEFT_THUMB},{"right_stick",GAMEPAD_BUTTON_RIGHT_THUMB}
};
inline constexpr InputButton ScriptGamepadAxes[]{
    {"left_x",GAMEPAD_AXIS_LEFT_X},{"left_y",GAMEPAD_AXIS_LEFT_Y},
    {"right_x",GAMEPAD_AXIS_RIGHT_X},{"right_y",GAMEPAD_AXIS_RIGHT_Y},
    {"left_trigger",GAMEPAD_AXIS_LEFT_TRIGGER},{"right_trigger",GAMEPAD_AXIS_RIGHT_TRIGGER}
};
struct ScriptInput {
    std::bitset<512> keys;
    std::bitset<8> mouse;
    std::bitset<32> gamepadButtons;
    std::array<float,8> gamepadAxes{};
    bool gamepad=false;
    Vector2 pointer{},delta{};
    float wheel=0;
};
// The frontend calls this only after its gameplay ownership decision. No raw
// device polling is exposed to Lua, and Escape/function keys remain host-owned.
inline ScriptInput CaptureScriptInput(Vector2 viewportOrigin={}) {
    ScriptInput input;if(!IsWindowFocused())return input;
    for(const auto& key:ScriptKeys)input.keys.set(key.code,IsKeyDown(key.code));
    for(const auto& button:ScriptMouseButtons)input.mouse.set(button.code,IsMouseButtonDown(button.code));
    const auto pointer=GetMousePosition();input.pointer={pointer.x-viewportOrigin.x,pointer.y-viewportOrigin.y};
    input.delta=GetMouseDelta();input.wheel=GetMouseWheelMove();input.gamepad=IsGamepadAvailable(0);
    if(input.gamepad){
        for(const auto& button:ScriptGamepadButtons)input.gamepadButtons.set(button.code,IsGamepadButtonDown(0,button.code));
        for(const auto& axis:ScriptGamepadAxes)input.gamepadAxes[axis.code]=GetGamepadAxisMovement(0,axis.code);
    }
    return input;
}
} // namespace loblox::game

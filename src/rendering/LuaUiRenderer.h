#pragma once

#include "raylib.h"

namespace loblox::game { class RuntimeSession; }

namespace loblox {
// Draws the script-owned UI inside the viewport rectangle. The Studio calls it
// while rendering the Play texture and the Player calls it on screen, so both
// hosts share one layout and one drawing path.
void DrawLuaUi(const game::RuntimeSession& runtime, int width, int height,
               Font font = GetFontDefault());
} // namespace loblox

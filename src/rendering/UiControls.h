#pragma once
#include "raylib.h"
#include "ui/ThemeColors.generated.h"
namespace loblox::ui {
// Native presentation adapter: shared tokens, geometry and input contracts.
// No editor dependencies, allocations or offscreen buffers per control.
enum class Surface { Base, Raised, Overlay, Interactive };
void DrawSurface(Rectangle bounds, Surface surface, bool highlighted=false);
bool Button(const char* label, Rectangle bounds, Font font, float scale,
            bool enabled, bool focused, bool active=false);
bool Slider(Rectangle bounds, float& value, float minimum, float maximum,
            bool enabled, bool focused, bool& dragging, float scale, bool pointerAvailable=true);
void ProgressBar(Rectangle bounds, float fraction);
}

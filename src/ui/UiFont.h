#pragma once
#include "raylib.h"
namespace loblox::ui {
// Graphics-thread resource, shared by native HUD, menu and gizmo readouts.
Font SystemFont();
void ReleaseSystemFont();
}

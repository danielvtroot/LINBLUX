#pragma once
#include "raylib.h"
namespace loblox::game {class RuntimeSession;}
namespace loblox {
void ClearExperienceOverlayResources();
// Shared UI font so the built-in HUD and script-built UI never mix typefaces.
Font ExperienceOverlayFont();
void DrawHealthHud(const game::RuntimeSession& runtime,int width,int height,Font font=GetFontDefault());
void DrawExperienceOverlay(const game::RuntimeSession& runtime,int width,int height,Font font=GetFontDefault());}

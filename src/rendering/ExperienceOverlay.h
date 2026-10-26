#pragma once
#include "raylib.h"
namespace loblox::game {class RuntimeSession;}
namespace loblox {void ClearExperienceOverlayResources();
void DrawExperienceOverlay(const game::RuntimeSession& runtime,int width,int height,Font font=GetFontDefault());}

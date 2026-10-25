#pragma once
#include "imgui.h"

namespace loblox::studio {
// Shared by the application and graphical fixtures. Class colours are accents,
// never the foreground of an entire Explorer row.
struct Palette {
    ImVec4 background, panel, raised, hover, selected;
    ImVec4 text, secondary, disabled, border, accent, error, overlay;
};
#include "editor/StudioPalette.generated.h"
inline const Palette& Colors(bool = false) {return LinbluxPalette;}
inline bool& Light() { static bool light = false; return light; }
inline void Apply(bool light = false) {
    Light() = light;
    if (light) ImGui::StyleColorsLight(); else ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle(); const auto& p = Colors(light);
    s.WindowRounding=5; s.FrameRounding=3; s.PopupRounding=6;
    s.GrabRounding=3; s.TabRounding=3; s.ScrollbarRounding=4;
    s.WindowPadding={8,7}; s.FramePadding={6,4}; s.ItemSpacing={6,5};
    s.CellPadding={6,4}; s.IndentSpacing=19; s.ScrollbarSize=13;
    s.WindowBorderSize=1; s.FrameBorderSize=0; s.WindowMinSize={120,80};
    auto* c=s.Colors;
    c[ImGuiCol_WindowBg]=p.panel; c[ImGuiCol_ChildBg]=p.panel;
    c[ImGuiCol_PopupBg]=p.raised; c[ImGuiCol_MenuBarBg]=p.background;
    c[ImGuiCol_Text]=p.text; c[ImGuiCol_TextDisabled]=p.secondary;
    c[ImGuiCol_Border]=c[ImGuiCol_Separator]=p.border;
    c[ImGuiCol_TitleBg]=c[ImGuiCol_TitleBgCollapsed]=p.background;
    c[ImGuiCol_TitleBgActive]=p.raised;
    c[ImGuiCol_FrameBg]=c[ImGuiCol_Button]=p.raised;
    c[ImGuiCol_FrameBgHovered]=c[ImGuiCol_ButtonHovered]=p.hover;
    c[ImGuiCol_FrameBgActive]=c[ImGuiCol_ButtonActive]=p.selected;
    c[ImGuiCol_Header]=c[ImGuiCol_HeaderActive]=p.selected;
    c[ImGuiCol_HeaderHovered]=p.hover;
    c[ImGuiCol_Tab]=c[ImGuiCol_TabDimmed]=p.background;
    c[ImGuiCol_TabSelected]=c[ImGuiCol_TabDimmedSelected]=p.raised;
    c[ImGuiCol_TabHovered]=p.hover; c[ImGuiCol_TabSelectedOverline]=p.accent;
    c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=c[ImGuiCol_SliderGrabActive]=p.accent;
    c[ImGuiCol_DockingPreview]={p.accent.x,p.accent.y,p.accent.z,.4f};
}
}

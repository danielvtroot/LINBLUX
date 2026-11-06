#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "raylib.h"
#include "editor/EditorUI.h"
#include "editor/InputRouting.h"
#include "scene/Scene.h"
#include "core/Math.h"

namespace loblox {

namespace {


EditorInput ReadEditorInput() {
    const ImGuiIO& io = ImGui::GetIO();
    EditorInput input;
    input.mouse = {io.MousePos.x, io.MousePos.y};
    input.delta = GetMouseDelta(); // Relative motion while GLFW owns the cursor.
    input.wheel = io.MouseWheel;
    input.leftPressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left, false);
    input.leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    input.rightPressed = ImGui::IsMouseClicked(ImGuiMouseButton_Right, false);
    input.rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    input.middlePressed = ImGui::IsMouseClicked(ImGuiMouseButton_Middle, false);
    input.middleDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    input.escape = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    input.shift = io.KeyShift;
    input.alt = io.KeyAlt;
    input.modified = io.KeyCtrl || io.KeyAlt || io.KeySuper;
    input.play = !input.modified && !input.shift && ImGui::IsKeyPressed(ImGuiKey_F5, false);
    input.stop = !input.modified && input.shift && ImGui::IsKeyPressed(ImGuiKey_F5, false);
    input.jump = !input.modified && ImGui::IsKeyPressed(ImGuiKey_Space, false);
    input.maximizeGame = !input.modified && ImGui::IsKeyPressed(ImGuiKey_F10, false);
    input.fullscreen = !input.modified && ImGui::IsKeyPressed(ImGuiKey_F11, false);
    input.save = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false);
    input.open = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false);
    input.duplicate = io.KeyCtrl && !io.KeyShift && !io.KeyAlt && !io.KeySuper &&
        ImGui::IsKeyPressed(ImGuiKey_D, false);
    input.remove = ImGui::IsKeyPressed(ImGuiKey_Delete, false) ||
                   ImGui::IsKeyPressed(ImGuiKey_Backspace, false);
    input.focus = ImGui::IsKeyPressed(ImGuiKey_F, false);
    input.rename = ImGui::IsKeyPressed(ImGuiKey_F2, false);
    input.copy = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false);
    input.paste = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false);
    input.undo = io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false);
    input.redo = io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
                                (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)));
    input.toggleLocalSpace = io.KeyCtrl && !io.KeyShift &&
        ImGui::IsKeyPressed(ImGuiKey_L, false);
    if (!input.modified) {
        const ImGuiKey tools[] = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4};
        for (int i = 0; i < 4; ++i)
            if (ImGui::IsKeyPressed(tools[i], false)) input.tool = i;
    }
    input.movement = {
        float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A)),
        float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q)),
        float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S))};
    return input;
}

enum class Icon {
    Select,
    Move,
    Scale,
    Rotate,
    Part,
    Sphere,
    Spawn,
    Color,
    Material,
    Anchor,
    Save,
    Open,
    Explorer,
    Properties,
    Rig,
    Clip,
    ScriptPage,
    Output,
    Stop,
};

void DrawIcon(ImDrawList* drawList, ImVec2 center, float size, Icon icon, ImU32 color) {
    const float half = size * 0.5f;
    switch (icon) {
        case Icon::Select:
            drawList->AddTriangleFilled(ImVec2(center.x - half * 0.55f, center.y - half),
                                        ImVec2(center.x + half * 0.75f, center.y),
                                        ImVec2(center.x - half * 0.1f, center.y + half * 0.6f),
                                        color);
            break;
        case Icon::Move:
            drawList->AddLine(ImVec2(center.x - half * .72f, center.y),
                              ImVec2(center.x + half * .72f, center.y), color, 2.0f);
            drawList->AddLine(ImVec2(center.x, center.y - half * .72f),
                              ImVec2(center.x, center.y + half * .72f), color, 2.0f);
            drawList->AddLine(ImVec2(center.x - half * .48f, center.y + half * .48f),
                              ImVec2(center.x + half * .48f, center.y - half * .48f), color, 2.0f);
            drawList->AddTriangleFilled(ImVec2(center.x + half, center.y),
                                        ImVec2(center.x + half * .55f, center.y - half * .25f),
                                        ImVec2(center.x + half * .55f, center.y + half * .25f), color);
            drawList->AddTriangleFilled(ImVec2(center.x - half, center.y),
                                        ImVec2(center.x - half * .55f, center.y - half * .25f),
                                        ImVec2(center.x - half * .55f, center.y + half * .25f), color);
            drawList->AddTriangleFilled(ImVec2(center.x, center.y - half),
                                        ImVec2(center.x - half * .25f, center.y - half * .55f),
                                        ImVec2(center.x + half * .25f, center.y - half * .55f), color);
            drawList->AddTriangleFilled(ImVec2(center.x, center.y + half),
                                        ImVec2(center.x - half * .25f, center.y + half * .55f),
                                        ImVec2(center.x + half * .25f, center.y + half * .55f), color);
            break;
        case Icon::Scale:
            drawList->AddRect(ImVec2(center.x - half * .55f, center.y - half * .55f),
                              ImVec2(center.x + half * .55f, center.y + half * .55f), color,
                              2.0f, 0, 2.0f);
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
                const ImVec2 p(center.x + sx * half * .82f, center.y + sy * half * .82f);
                drawList->AddRectFilled(ImVec2(p.x - 2.2f, p.y - 2.2f),
                                        ImVec2(p.x + 2.2f, p.y + 2.2f), color, 1.0f);
            }
            break;
        case Icon::Rotate:
            drawList->PathArcTo(center, half * 0.8f, 0.5f, 5.3f, 22);
            drawList->PathStroke(color, 0, 2.0f);
            drawList->AddTriangleFilled(ImVec2(center.x + half * 0.7f, center.y - half * 0.55f),
                                        ImVec2(center.x + half * 0.1f, center.y - half * 0.95f),
                                        ImVec2(center.x + half * 0.62f, center.y - half * 0.05f),
                                        color);
            break;
        case Icon::Part:
            {
                const float x = half * .48f;
                const float y = half * .40f;
                const float z = half * .38f;
                const ImVec2 front[4] = {{center.x - x, center.y - y + z},
                                         {center.x + x, center.y - y + z},
                                         {center.x + x, center.y + y + z},
                                         {center.x - x, center.y + y + z}};
                const ImVec2 back[4] = {{center.x - x + z, center.y - y},
                                        {center.x + x + z, center.y - y},
                                        {center.x + x + z, center.y + y},
                                        {center.x - x + z, center.y + y}};
                for (int i = 0; i < 4; ++i) {
                    drawList->AddLine(front[i], front[(i + 1) % 4], color, 2.0f);
                    drawList->AddLine(back[i], back[(i + 1) % 4], color, 2.0f);
                    drawList->AddLine(front[i], back[i], color, 2.0f);
                }
            }
            break;
        case Icon::Sphere:
            drawList->AddCircle(center, half * .78f, color, 24, 2.0f);
            drawList->AddEllipse(center, ImVec2(half * .36f, half * .78f), color,
                                 0.0f, 24, 1.4f);
            drawList->AddLine(ImVec2(center.x - half * .68f, center.y),
                              ImVec2(center.x + half * .68f, center.y), color, 1.4f);
            break;
        case Icon::Spawn:
            drawList->AddRectFilled(ImVec2(center.x - half, center.y + half * 0.2f),
                                    ImVec2(center.x + half, center.y + half * 0.8f), color,
                                    1.5f);
            drawList->AddLine(ImVec2(center.x, center.y + half * 0.2f),
                              ImVec2(center.x, center.y - half), color, 1.8f);
            drawList->AddRectFilled(ImVec2(center.x, center.y - half),
                                    ImVec2(center.x + half * 0.9f, center.y - half * 0.3f),
                                    color, 1.0f);
            break;
        case Icon::Color:
            drawList->AddCircleFilled(ImVec2(center.x - half * 0.45f, center.y + half * 0.3f),
                                      half * 0.55f, IM_COL32(220, 220, 220, 255), 12);
            drawList->AddCircleFilled(ImVec2(center.x + half * 0.45f, center.y + half * 0.3f),
                                      half * 0.55f, IM_COL32(140, 140, 140, 255), 12);
            drawList->AddCircleFilled(ImVec2(center.x, center.y - half * 0.35f), half * 0.55f,
                                      IM_COL32(80, 80, 80, 255), 12);
            break;
        case Icon::Material: {
            for (int layer = 0; layer < 3; ++layer) {
                const float y = center.y - half * .55f + layer * half * .55f;
                const float inset = layer * half * .20f;
                drawList->AddLine(ImVec2(center.x - half * .75f + inset, y),
                                  ImVec2(center.x + half * .75f - inset, y), color, 2.4f);
                drawList->AddLine(ImVec2(center.x - half * .75f + inset, y + half * .26f),
                                  ImVec2(center.x + half * .75f - inset, y + half * .26f), color, 2.4f);
                drawList->AddLine(ImVec2(center.x - half * .75f + inset, y),
                                  ImVec2(center.x - half * .75f + inset, y + half * .26f), color, 1.5f);
                drawList->AddLine(ImVec2(center.x + half * .75f - inset, y),
                                  ImVec2(center.x + half * .75f - inset, y + half * .26f), color, 1.5f);
            }
            break;
        }
        case Icon::Anchor:
            drawList->AddLine(ImVec2(center.x, center.y - half * 0.9f),
                              ImVec2(center.x, center.y + half * 0.9f), color, 1.8f);
            drawList->AddCircle(ImVec2(center.x, center.y - half * 0.8f), 2.4f, color, 10);
            drawList->PathArcTo(ImVec2(center.x, center.y + half * 0.2f), half * 0.7f, 0.2f,
                                2.95f, 16);
            drawList->PathStroke(color, 0, 1.8f);
            break;
        case Icon::Save:
            drawList->AddRect(ImVec2(center.x - half * 0.85f, center.y - half * 0.85f),
                              ImVec2(center.x + half * 0.85f, center.y + half * 0.85f), color,
                              1.0f, 0, 1.6f);
            drawList->AddRectFilled(ImVec2(center.x - half * 0.45f, center.y - half * 0.85f),
                                    ImVec2(center.x + half * 0.45f, center.y - half * 0.15f),
                                    color, 0.5f);
            drawList->AddRect(ImVec2(center.x - half * 0.55f, center.y + half * 0.15f),
                              ImVec2(center.x + half * 0.55f, center.y + half * 0.85f), color,
                              0.5f, 0, 1.2f);
            break;
        case Icon::Open:
            drawList->AddRectFilled(ImVec2(center.x - half, center.y - half * 0.45f),
                                    ImVec2(center.x - half * 0.1f, center.y - half * 0.1f),
                                    color, 1.0f);
            drawList->AddRect(ImVec2(center.x - half, center.y - half * 0.2f),
                              ImVec2(center.x + half, center.y + half * 0.8f), color, 1.5f, 0,
                              1.6f);
            break;
        case Icon::Explorer:
            for (int i = 0; i < 3; ++i) {
                const float y = center.y - half * 0.55f + static_cast<float>(i) * half * 0.55f;
                drawList->AddRectFilled(ImVec2(center.x - half, y - 1.5f),
                                        ImVec2(center.x - half + 3.0f, y + 1.5f), color);
                drawList->AddLine(ImVec2(center.x - half + 6.0f, y),
                                  ImVec2(center.x + half, y), color, 1.4f);
            }
            break;
        case Icon::Properties:
            for (int i = 0; i < 2; ++i) {
                const float y = center.y - half * 0.35f + static_cast<float>(i) * half * 0.7f;
                drawList->AddLine(ImVec2(center.x - half, y), ImVec2(center.x + half, y), color,
                                  1.4f);
                drawList->AddCircleFilled(
                    ImVec2(center.x + (i == 0 ? -half * 0.25f : half * 0.35f), y), 2.4f, color,
                    10);
            }
            break;
        case Icon::Rig:  // Avatar clássico: cabeça, tronco e pernas.
            drawList->AddCircle(ImVec2(center.x, center.y - half * 0.55f), half * 0.30f, color,
                                12, 2.0f);
            drawList->AddLine(ImVec2(center.x, center.y - half * 0.20f),
                              ImVec2(center.x, center.y + half * 0.45f), color, 2.6f);
            drawList->AddLine(ImVec2(center.x - half * 0.55f, center.y + half * 0.05f),
                              ImVec2(center.x + half * 0.55f, center.y + half * 0.05f), color,
                              2.2f);
            drawList->AddLine(ImVec2(center.x, center.y + half * 0.45f),
                              ImVec2(center.x - half * 0.38f, center.y + half), color, 2.2f);
            drawList->AddLine(ImVec2(center.x, center.y + half * 0.45f),
                              ImVec2(center.x + half * 0.38f, center.y + half), color, 2.2f);
            break;
        case Icon::Clip:  // Clipe de animação em execução.
            drawList->AddRect(ImVec2(center.x - half * 0.7f, center.y - half * 0.7f),
                              ImVec2(center.x + half * 0.7f, center.y + half * 0.7f), color,
                              2.0f, 0, 2.0f);
            drawList->AddTriangleFilled(ImVec2(center.x - half * 0.2f, center.y - half * 0.35f),
                                        ImVec2(center.x - half * 0.2f, center.y + half * 0.35f),
                                        ImVec2(center.x + half * 0.35f, center.y), color);
            break;
        case Icon::ScriptPage:  // Página de código com linhas.
            drawList->AddRect(ImVec2(center.x - half * 0.55f, center.y - half * 0.8f),
                              ImVec2(center.x + half * 0.55f, center.y + half * 0.8f), color,
                              1.5f, 0, 1.8f);
            drawList->AddLine(ImVec2(center.x - half * 0.3f, center.y - half * 0.4f),
                              ImVec2(center.x + half * 0.3f, center.y - half * 0.4f), color, 1.4f);
            drawList->AddLine(ImVec2(center.x - half * 0.3f, center.y),
                              ImVec2(center.x + half * 0.1f, center.y), color, 1.4f);
            drawList->AddLine(ImVec2(center.x - half * 0.3f, center.y + half * 0.4f),
                              ImVec2(center.x + half * 0.3f, center.y + half * 0.4f), color,
                              1.4f);
            break;
        case Icon::Output:  // Console: prompt + linhas.
            drawList->AddRect(ImVec2(center.x - half * 0.8f, center.y - half * 0.65f),
                              ImVec2(center.x + half * 0.8f, center.y + half * 0.65f), color,
                              2.0f, 0, 1.8f);
            drawList->AddTriangleFilled(ImVec2(center.x - half * 0.55f, center.y - half * 0.35f),
                                        ImVec2(center.x - half * 0.55f, center.y + half * 0.35f),
                                        ImVec2(center.x - half * 0.2f, center.y), color);
            drawList->AddLine(ImVec2(center.x, center.y), ImVec2(center.x + half * 0.55f, center.y),
                              color, 1.6f);
            drawList->AddLine(ImVec2(center.x - half * 0.55f, center.y + half * 0.35f),
                              ImVec2(center.x + half * 0.55f, center.y + half * 0.35f), color,
                              1.3f);
            break;
        case Icon::Stop:
            drawList->AddRectFilled(ImVec2(center.x - half * 0.55f, center.y - half * 0.55f),
                                    ImVec2(center.x + half * 0.55f, center.y + half * 0.55f),
                                    color, 2.0f);
            break;
    }
}

bool IconButton(const char* id, Icon icon, const char* label, bool active, const char* tooltip,
                bool enabled = true) {
    label=i18n::Label(label);if(tooltip)tooltip=i18n::Label(tooltip);
    ImGui::PushID(id);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float width = std::max(textSize.x + 16.0f, 54.0f);
    const ImVec2 size(width, std::clamp(ImGui::GetContentRegionAvail().y, 44.f, 62.f));
    const ImVec2 position = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##button", size);
    const bool clicked = ImGui::IsItemClicked() && enabled;
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const auto& palette=studio::Colors(studio::Light());
    const ImU32 background = ImGui::GetColorU32(active || held ? ImGuiCol_Header :
        hovered ? ImGuiCol_ButtonHovered : ImGuiCol_WindowBg);
    drawList->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
                            background, 5.0f);
    if (active) {
        drawList->AddRectFilled(ImVec2(position.x + 7.0f, position.y + size.y - 3.0f),
                                ImVec2(position.x + size.x - 7.0f, position.y + size.y - 1.0f),
                                ImGui::ColorConvertFloat4ToU32(palette.accent), 1.0f);
    } else if (hovered) {
        drawList->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
                          ImGui::GetColorU32(ImGuiCol_Border), 5.0f);
    }

    const ImU32 foreground = ImGui::GetColorU32(!enabled ? palette.disabled : palette.text);
    DrawIcon(drawList, ImVec2(position.x + size.x * 0.5f, position.y + size.y*.33f),
             std::min(22.f,size.y*.40f), icon, foreground);
    const ImVec2 labelPosition(position.x + (size.x - textSize.x) * 0.5f,
                               position.y + size.y - textSize.y - 7.0f);
    drawList->AddText(labelPosition, foreground, label);
    if (hovered && tooltip != nullptr) {
        ImGui::SetTooltip(i18n::Label("%s"), tooltip);
    }
    ImGui::PopID();
    return clicked;
}

void ToolbarSeparator() {
    ImGui::SameLine(0.0f, 7.0f);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float height = 62.0f;
    ImGui::Dummy(ImVec2(1.0f, height));
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(cursor.x, cursor.y + 7.0f), ImVec2(cursor.x, cursor.y + height - 7.0f),
        ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
    ImGui::SameLine(0.0f, 7.0f);
}    ImVec4 ClassColor(InstanceClass instanceClass) {
    switch (instanceClass) {
        case InstanceClass::Workspace:
            return ImVec4(0.95f, 0.85f, 0.45f, 1.0f);
        case InstanceClass::Lighting:
            return ImVec4(0.95f, 0.80f, 0.40f, 1.0f);
        case InstanceClass::Baseplate:
            return ImVec4(0.70f, 0.75f, 0.80f, 1.0f);
        case InstanceClass::SpawnLocation:
            return ImVec4(0.45f, 0.80f, 1.00f, 1.0f);
        case InstanceClass::Part:
            return ImVec4(0.82f, 0.86f, 0.92f, 1.0f);
        case InstanceClass::Folder: return ImVec4(.95f,.78f,.3f,1);
        case InstanceClass::Model: return ImVec4(.5f,.75f,1,1);
        case InstanceClass::Script: return ImVec4(.65f,.8f,1,1);
        case InstanceClass::LocalScript: return ImVec4(.5f,.95f,.75f,1);
        case InstanceClass::ModuleScript: return ImVec4(.85f,.7f,1,1);
        case InstanceClass::Decal:
        case InstanceClass::Texture: return ImVec4(.75f,.85f,.7f,1);
        case InstanceClass::ServerScriptService:
        case InstanceClass::ServerStorage:
        case InstanceClass::ReplicatedStorage: return ImVec4(.9f,.7f,.45f,1);
    }
    return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
}

// Badges diferenciam classes que compartilham a mesma silhueta (scripts,
// servicos). Cores fixas para leitura rapida na hierarquia.
ImU32 ClassIconBadge(InstanceClass instanceClass) {
    switch (instanceClass) {
        case InstanceClass::Script: return IM_COL32(70, 150, 255, 255);
        case InstanceClass::LocalScript: return IM_COL32(45, 200, 150, 255);
        case InstanceClass::ModuleScript: return IM_COL32(165, 110, 235, 255);
        case InstanceClass::ServerScriptService: return IM_COL32(230, 125, 160, 255);
        case InstanceClass::ServerStorage: return IM_COL32(170, 125, 230, 255);
        case InstanceClass::ReplicatedStorage: return IM_COL32(245, 155, 55, 255);
        default: return IM_COL32(60, 66, 74, 255);
    }
}

// Icones vetoriais por classe do Explorer: desenhados via ImDrawList a cada
// frame (custo desprezivel), sem texturas nem assets novos.
void DrawClassIcon(ImDrawList* draw, ImVec2 center, InstanceClass instanceClass, ImU32 color) {
    const float x = center.x, y = center.y;
    using C = InstanceClass;
    const C cls = instanceClass;
    // Pagina com canto dobrado: Script/LocalScript/ModuleScript; o badge
    // distingue cada um.
    if (cls == C::Script || cls == C::LocalScript || cls == C::ModuleScript) {
        draw->AddCircleFilled(ImVec2(x + 3.8f, y + 3.6f), 2.6f, IM_COL32(30, 33, 38, 255), 10);
        draw->AddCircleFilled(ImVec2(x + 3.8f, y + 3.6f), 1.7f, ClassIconBadge(cls), 10);
        draw->AddRectFilled(ImVec2(x - 4.2f, y - 4.6f), ImVec2(x + 3.4f, y + 4.6f), color, 1.2f);
        draw->AddTriangleFilled(ImVec2(x + 3.4f, y - 4.6f), ImVec2(x + 3.4f, y - 1.8f),
                                ImVec2(x + 0.8f, y - 4.6f), IM_COL32(30, 33, 38, 90));
        draw->AddLine(ImVec2(x - 2.6f, y + 0.4f), ImVec2(x + 1.8f, y + 0.4f), IM_COL32(30, 33, 38, 200), 1.2f);
        draw->AddLine(ImVec2(x - 2.6f, y + 2.3f), ImVec2(x + 0.6f, y + 2.3f), IM_COL32(30, 33, 38, 200), 1.2f);
        draw->AddLine(ImVec2(x - 2.6f, y - 1.5f), ImVec2(x + 1.0f, y - 1.5f), IM_COL32(30, 33, 38, 200), 1.2f);
        return;
    }
    // Servicos: gaveta com dois volumes + badge colorido.
    if (cls == C::ServerScriptService || cls == C::ServerStorage || cls == C::ReplicatedStorage) {
        draw->AddRectFilled(ImVec2(x - 5, y - 4.4f), ImVec2(x + 5, y + 4.4f), color, 1.5f);
        draw->AddLine(ImVec2(x - 5, y - 0.6f), ImVec2(x + 5, y - 0.6f), IM_COL32(30, 33, 38, 200), 1.3f);
        draw->AddLine(ImVec2(x - 5, y - 2.5f), ImVec2(x + 5, y - 2.5f), IM_COL32(30, 33, 38, 110), 1.2f);
        draw->AddCircleFilled(ImVec2(x + 3.4f, y + 2.9f), 1.8f, ClassIconBadge(cls), 10);
        draw->AddCircle(ImVec2(x + 3.4f, y + 2.9f), 1.8f, IM_COL32(30, 33, 38, 255), 10, 1.2f);
        return;
    }
    switch (cls) {
        case C::Workspace: {
            draw->AddRectFilled(ImVec2(x - 5, y - 5), ImVec2(x + 5, y + 5), color, 2.0f);
            draw->AddLine(ImVec2(x - 4.6f, y + 0.6f), ImVec2(x + 4.6f, y + 0.6f), IM_COL32(30, 33, 38, 235), 1.3f);
            draw->AddLine(ImVec2(x - 4.6f, y - 2.2f), ImVec2(x + 4.6f, y - 2.2f), IM_COL32(30, 33, 38, 170), 1.2f);
            draw->AddLine(ImVec2(x - 4.6f, y + 3.3f), ImVec2(x + 4.6f, y + 3.3f), IM_COL32(30, 33, 38, 130), 1.2f);
            break;
        }
        case C::Lighting: {
            draw->AddCircle(ImVec2(x, y), 3.0f, color, 12, 1.8f);
            for (int i = 0; i < 8; ++i) {
                const float angle = static_cast<float>(i) * (PI / 4.0f);
                const float c = std::cos(angle), s = std::sin(angle);
                draw->AddLine(ImVec2(x + c * 4.2f, y + s * 4.2f), ImVec2(x + c * 6.0f, y + s * 6.0f),
                              color, 1.3f);
            }
            break;
        }
        case C::Baseplate: {
            draw->AddRectFilled(ImVec2(x - 5, y - 5), ImVec2(x + 5, y + 5), color, 1.5f);
            draw->AddRectFilled(ImVec2(x - 5, y - 5), ImVec2(x, y), IM_COL32(46, 50, 56, 120), 0);
            draw->AddRectFilled(ImVec2(x, y), ImVec2(x + 5, y + 5), IM_COL32(46, 50, 56, 120), 0);
            draw->AddRect(ImVec2(x - 5, y - 5), ImVec2(x + 5, y + 5), color, 1.5f, 0, 1.3f);
            break;
        }
        case C::SpawnLocation: {
            draw->AddRectFilled(ImVec2(x - 4.2f, y + 2.6f), ImVec2(x + 4.2f, y + 4.4f), color, 1.0f);
            draw->AddCircleFilled(ImVec2(x, y - 0.6f), 3.4f, color, 12);
            draw->AddCircle(ImVec2(x, y - 0.6f), 3.4f, IM_COL32(28, 32, 38, 200), 12, 1.2f);
            draw->AddCircle(ImVec2(x, y - 0.6f), 1.5f, IM_COL32(28, 32, 38, 170), 10, 1.2f);
            break;
        }
        case C::Folder: {
            draw->AddRectFilled(ImVec2(x - 5, y - 4.4f), ImVec2(x - 0.4f, y - 1.8f), color, 1.0f);
            draw->AddRectFilled(ImVec2(x - 5, y - 2.2f), ImVec2(x + 5, y + 4.4f), color, 1.2f);
            draw->AddLine(ImVec2(x - 3.6f, y + 1.0f), ImVec2(x + 3.6f, y + 1.0f),
                          IM_COL32(30, 33, 38, 130), 1.2f);
            break;
        }
        case C::Model: {
            draw->AddRectFilled(ImVec2(x - 5, y - 4.6f), ImVec2(x + 1.2f, y + 1.6f), color, 1.0f);
            draw->AddRectFilled(ImVec2(x - 1.8f, y - 1.4f), ImVec2(x + 5, y + 4.6f), color, 1.0f);
            draw->AddRect(ImVec2(x - 1.8f, y - 1.4f), ImVec2(x + 5, y + 4.6f),
                          IM_COL32(230, 235, 242, 200), 1.0f);
            break;
        }
        default: {  // Part e demais: bloco com brilho superior.
            draw->AddRectFilled(ImVec2(x - 5, y - 4), ImVec2(x + 5, y + 4), color, 1.5f);
            draw->AddLine(ImVec2(x - 3.8f, y - 2.2f), ImVec2(x + 3.8f, y - 2.2f),
                          IM_COL32(255, 255, 255, 130), 1.4f);
            draw->AddRect(ImVec2(x - 5, y - 4), ImVec2(x + 5, y + 4),
                          IM_COL32(230, 235, 242, 190), 1.5f, 0, 1.2f);
            break;
        }
    }
}

const Color kPalette[] = {
    Color{163, 162, 165, 255}, Color{255, 255, 255, 255}, Color{40, 40, 44, 255},
    Color{220, 60, 50, 255},   Color{240, 150, 40, 255}, Color{245, 220, 70, 255},
    Color{80, 190, 90, 255},   Color{40, 160, 255, 255}, Color{130, 90, 220, 255},
    Color{250, 130, 180, 255}, Color{130, 95, 65, 255},  Color{35, 130, 130, 255},
};

bool ContainsIgnoreCase(const char* value, const char* query) {
    if (query == nullptr || *query == '\0') return true;
    if (value == nullptr) return false;
    for (const char* start = value; *start != '\0'; ++start) {
        const char* a = start;
        const char* b = query;
        while (*a != '\0' && *b != '\0' &&
               std::tolower(static_cast<unsigned char>(*a)) ==
                   std::tolower(static_cast<unsigned char>(*b))) {
            ++a;
            ++b;
        }
        if (*b == '\0') return true;
    }
    return false;
}

bool TreeContains(const Instance* instance, const char* query) {
    if (instance == nullptr || query == nullptr || *query == '\0') return instance != nullptr;
    if (ContainsIgnoreCase(instance->name().c_str(), query) ||
        ContainsIgnoreCase(instance->className(), query)) return true;
    for (const Instance* child : instance->children()) {
        if (TreeContains(child, query)) return true;
    }
    return false;
}


}  // namespace
}

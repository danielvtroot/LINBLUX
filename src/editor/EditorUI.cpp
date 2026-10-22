#include "editor/EditorUI.h"
#include "editor/StudioTheme.h"
#include "game/ImageAssets.h"

#include "core/Diagnostics.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cinttypes>
#include <filesystem>
#include <utility>

#include "imgui.h"
#include "imgui_internal.h"
#include "raylib.h"

#include "objects/Baseplate.h"
#include "objects/Lighting.h"
#include "objects/SpawnLocation.h"
#include "objects/Workspace.h"
#include "physics/PhysicsWorld.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"
#include "serialization/SceneLua.h"

namespace loblox {

// Marca do projeto: carregada uma unica vez, a partir da raiz (caminhos
// relativos; sem caminho absoluto da maquina). Falha permanece !ok e a UI
// usa o fallback vetorial existente.
const LobloxLogo& LoadLobloxLogo() {
    static LobloxLogo logo;
    static const std::string path=game::BuiltinAssetPath("loblox.png");
    const Texture2D texture=game::AssetTexture(path);
    logo.id=texture.id;logo.width=static_cast<float>(texture.width);
    logo.height=static_cast<float>(texture.height);logo.ok=texture.id!=0;
    return logo;
}

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
    input.modified = io.KeyCtrl || io.KeyAlt || io.KeySuper;
    input.play = !input.modified && !input.shift && ImGui::IsKeyPressed(ImGuiKey_F5, false);
    input.stop = !input.modified && input.shift && ImGui::IsKeyPressed(ImGuiKey_F5, false);
    input.jump = !input.modified && ImGui::IsKeyPressed(ImGuiKey_Space, false);
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
                                      half * 0.55f, IM_COL32(235, 90, 80, 255), 12);
            drawList->AddCircleFilled(ImVec2(center.x + half * 0.45f, center.y + half * 0.3f),
                                      half * 0.55f, IM_COL32(90, 200, 110, 255), 12);
            drawList->AddCircleFilled(ImVec2(center.x, center.y - half * 0.35f), half * 0.55f,
                                      IM_COL32(90, 150, 235, 255), 12);
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
        ImGui::SetTooltip("%s", tooltip);
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

EditorUI::EditorUI(Scene& scene, PhysicsWorld& physics, EditorCamera& camera,
                   SceneRenderer& renderer, const AppConfig& config)
    : scene_(scene),
      physics_(physics),
      camera_(camera),
      renderer_(renderer),
      config_(config),
      gizmo_(),
      transform_(gizmo_, physics_),
      projectPath_(config.defaultProjectPath) {
    std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
    cameraMoveSpeed_ = camera_.moveSpeed();
    cameraLookSensitivity_ = camera_.lookSensitivity();
    camera_.reset();
    const char* trace = std::getenv("LOBLOX_TRACE_INPUT");
    traceInput_ = trace && std::strcmp(trace, "1") == 0;
}

EditorUI::~EditorUI() {
    stopPlay();
    cancelRigGizmo();
    camera_.releaseCapture();
    endGizmoSession(false);
    unloadViewport();
}

void EditorUI::endGizmoSession(bool restoreTransform, const char* reason) {
    Part* part = scene_.findPart(transform_.partId());
    if (restoreTransform) transform_.cancel(part, true, reason);
    else transform_.commit(part, reason);
}

void EditorUI::setTool(ToolMode tool, const char* source) {
    if (tool_ == tool) return;
    cancelRigGizmo();
    if (transform_.active()) {
        endGizmoSession(true, "tool_change");
    }
    diag::Event event;
    event.subsystem = "tool";
    event.name = "change";
    event.context = {{"previous", ToolModeName(tool_)}, {"tool", ToolModeName(tool)},
                     {"source", source}, {"consumer", "editor"}};
    event.result = "ok";
    diag::log(event);
    gizmo_.cancel();
    tool_ = tool;
    camera_.releaseCapture();
}

void EditorUI::checkInvariants(const char* operation) {
    std::string error;
    if (!scene_.validate(error) || !physics_.validate(scene_, error)) {
        diag::Event event;
        event.level = diag::Level::Error;
        event.subsystem = "invariants";
        event.name = "violation";
        event.result = "error";
        event.error = error;
        event.context = {{"operation", operation}};
        diag::log(event);
        setStatus(std::string("Invariante violada em ") + operation + ": " + error, true);
    }
}

void EditorUI::setOnSceneLoaded(std::function<void()> callback) {
    onSceneLoaded_ = std::move(callback);
}

void EditorUI::setStatus(std::string message, bool error) {
    status_ = std::move(message);
    statusIsError_ = error;
    statusUntil_ = ImGui::GetTime() + 5.0;
}

void EditorUI::setProjectPath(std::string path) {
    projectPath_ = std::move(path);
    std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
}

void EditorUI::resizeViewport(int width, int height) {
    if (width < 8 || height < 8) {
        return;
    }
    if (viewportReady_ && width == viewportWidth_ && height == viewportHeight_) {
        return;
    }
    unloadViewport();
    viewport_ = LoadRenderTexture(width, height);
    viewportReady_ = IsRenderTextureValid(viewport_);
    viewportWidth_ = width;
    viewportHeight_ = height;
}

void EditorUI::unloadViewport() {
    if (viewportReady_) {
        UnloadRenderTexture(viewport_);
    }
    viewportReady_ = false;
    viewportWidth_ = 0;
    viewportHeight_ = 0;
}

void EditorUI::buildDockLayout() {
    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    const ImGuiID dockId = ImGui::GetID("LobloxDockspace");
    if (ImGui::DockBuilderGetNode(dockId) != nullptr) {
        layoutBuilt_ = true;
        return;
    }
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, mainViewport->WorkSize);

    ImGuiID center = dockId;
    const ImGuiID top =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 120.f/mainViewport->WorkSize.y, nullptr, &center);
    ImGui::DockBuilderGetNode(top)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
    const ImGuiID left =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.19f, nullptr, &center);
    ImGuiID right =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.34f, nullptr, &center);
    const ImGuiID rightTop =
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.42f, nullptr, &right);

    ImGui::DockBuilderDockWindow("##Toolbar", top);
    ImGui::DockBuilderDockWindow("Toolbox", left);
    ImGui::DockBuilderDockWindow("Viewport", center);
    ImGui::DockBuilderDockWindow("Explorer", rightTop);
    ImGui::DockBuilderDockWindow("Properties", right);
    ImGui::DockBuilderFinish(dockId);
    layoutBuilt_ = true;
}

void EditorUI::handleShortcuts(const EditorInput& input, const InputRoute& route) {
    if ((route.editorKeys || route.globalKeys) && input.rename && explorerFocused_) renameSelection();
    if ((route.editorKeys || route.globalKeys) && input.copy && explorerFocused_)
        clipboardId_ = scene_.selection() ? scene_.selection()->id() : 0;
    if ((route.editorKeys || route.globalKeys) && input.paste && explorerFocused_) {
        if (auto* original = scene_.find(clipboardId_)) {
            auto* destination = scene_.selection();
            if (!destination || destination->isPart()) destination = original->parent();
            if (auto* copy = scene_.duplicateTree(*original,destination)) scene_.select(copy);
            else setStatus("Destino de colagem invalido.",true);
        } else setStatus("Origem da copia nao existe mais; copie novamente.",true);
    }
    if ((route.globalKeys || route.editorKeys) && input.play) startPlay();
    if ((route.editorKeys || route.globalKeys) && input.duplicate) {
        duplicateSelection();
    }
    if ((route.editorKeys || (route.globalKeys && explorerFocused_)) && input.remove && !input.modified) {
        deleteSelection();
    }
    if (route.editorKeys && input.focus && !input.modified) {
        focusSelection();
    }
    if (route.editorKeys && input.tool >= 0) {
        const ToolMode tools[] = {ToolMode::Select, ToolMode::Move, ToolMode::Scale, ToolMode::Rotate};
        setTool(tools[input.tool], "shortcut");
    }
    if (route.globalKeys && input.save) {
        saveCurrentProject();
    }
    if (route.globalKeys && input.open) {
        endGizmoSession(true, "open_dialog");
        requestOpenPopup_ = true;
        std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
    }
}

bool EditorUI::saveProjectAt(const std::string& path, std::string& error) {
    endGizmoSession(false, "save");
    cancelRigGizmo(false);
    if (!commitScriptDrafts(error)) return false;
    if (animationDirty_ && !animationSource_.empty()) {
        auto* clip = scene_.gameAssets().findAnimation(animationSource_);
        if (!clip) { error = "Animation asset no longer exists"; return false; }
        if (!animationDraft_.validate(error)) { error = "Animacao nao salva: " + error; return false; }
        *clip = animationDraft_;
    }
    if (!projectio::save(scene_, path, error)) return false;
    animationDirty_ = false;
    for (auto& pair:scriptDrafts_) pair.second.dirty=false;
    return true;
}

bool EditorUI::saveCurrentProject() {
    std::string error;
    if (saveProjectAt(projectPath_, error)) {
        setStatus("Projeto salvo em " + projectPath_);
        return true;
    } else {
        setStatus("Falha ao salvar '" + projectPath_ + "': " + error, true);
        return false;
    }
}

void EditorUI::draw() {
    healthHud_={};
    diag::setFrame(++frame_);
    explorerFocused_=false;
    if (!runtime_.active() && !selectedRig_.empty() && !scene_.gameAssets().findRig(selectedRig_)) {
        cancelRigGizmo(false); selectedRig_.clear(); animationSource_.clear(); previewPlaying_ = false;
    }
    if (!runtime_.active() && scene_.selection()) {
        cancelRigGizmo(); selectedRig_.clear();
    }
    drawMainMenuBar();
    if (!layoutBuilt_) {
        buildDockLayout();
    }
    ImGui::DockSpaceOverViewport(ImGui::GetID("LobloxDockspace"), ImGui::GetMainViewport());

    ImGui::BeginDisabled(runtime_.active());
    drawToolbar();
    const auto visibleInPlay=[&](const char* name) {
        const auto* window=ImGui::FindWindowByName(name);
        return !runtime_.active() || (window && window->DockId!=0);
    };
    if (showToolbox_ && visibleInPlay("Toolbox")) {
        drawToolbox();
    }
    if (showExplorer_ && visibleInPlay("Explorer")) {
        drawExplorer();
    }
    if (showProperties_ && visibleInPlay("Properties")) {
        drawProperties();
    }
    if (!runtime_.active()) {
        if (showRigEditor_) drawRigEditor();
        if (showAnimationEditor_) drawAnimationEditor();
        if (showGameSettings_) drawGameSettings();
        if (showScriptEditor_) drawScriptEditor();
    }
    ImGui::EndDisabled();
    if (showOutput_) drawOutput();
    drawDialogs();
    drawViewport(); // UI actions and popup requests are known before routing.
    drawPauseMenu(); // current-frame gameplay rect; Paused already owns input
}

void EditorUI::drawMainMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    const auto& headerLogo=LoadLobloxLogo();
    if(headerLogo.ok)ImGui::Image(static_cast<ImTextureID>(headerLogo.id),{18.f*headerLogo.width/headerLogo.height,18});
    else ImGui::TextUnformatted("L");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextUnformatted("Loblox");
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::BeginDisabled(runtime_.active());
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
            saveCurrentProject();
        }
        if (ImGui::MenuItem("Save Project As...")) {
            endGizmoSession(true);
            requestSavePopup_ = true;
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
            dialogError_.clear();
        }
        if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {
            endGizmoSession(true);
            requestOpenPopup_ = true;
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
            dialogError_.clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Export Scene as Lua...")) {
            endGizmoSession(true);
            requestLuaExportPopup_ = true;
            std::string luaPath = projectPath_;
            const std::size_t extension = luaPath.find_last_of('.');
            if (extension != std::string::npos) luaPath.resize(extension);
            luaPath += ".lua";
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", luaPath.c_str());
            dialogError_.clear();
        }
        if (ImGui::MenuItem("Import Scene from Lua...")) {
            endGizmoSession(true);
            requestLuaImportPopup_ = true;
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", "projects/scene.lua");
            dialogError_.clear();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Home")) {
        if (ImGui::MenuItem("Select", "1")) setTool(ToolMode::Select, "menu");
        if (ImGui::MenuItem("Move", "2")) setTool(ToolMode::Move, "menu");
        if (ImGui::MenuItem("Scale", "3")) setTool(ToolMode::Scale, "menu");
        if (ImGui::MenuItem("Rotate", "4")) setTool(ToolMode::Rotate, "menu");
        if (ImGui::MenuItem("Insert Block")) insertPart(PartShape::Block);
        if (ImGui::MenuItem("Insert Sphere")) insertPart(PartShape::Sphere);
        if (ImGui::MenuItem("Insert Spawn")) insertSpawnLocation();
        if (ImGui::MenuItem("Insert Folder")) insertContainer(InstanceClass::Folder);
        if (ImGui::MenuItem("Insert Model")) insertContainer(InstanceClass::Model);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Model")) {
        if (ImGui::MenuItem("Anchor", nullptr, false, scene_.selection() && scene_.selection()->isPart())) {
            auto* part = static_cast<Part*>(scene_.selection());
            part->setAnchored(!part->anchored());
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Toolbox", nullptr, &showToolbox_);
        ImGui::MenuItem("Explorer", nullptr, &showExplorer_);
        ImGui::MenuItem("Properties", nullptr, &showProperties_);
        ImGui::MenuItem("Script Editor", nullptr, &showScriptEditor_);
        ImGui::MenuItem("Output", nullptr, &showOutput_);
        ImGui::MenuItem("Rig Editor", nullptr, &showRigEditor_);
        ImGui::MenuItem("Animation Editor", nullptr, &showAnimationEditor_);
        if (ImGui::MenuItem("Reset Layout")) {
            ImGui::DockBuilderRemoveNode(ImGui::GetID("LobloxDockspace"));
            layoutBuilt_ = false; showExplorer_ = showProperties_ = showToolbox_ = true;
        }
        bool light=studio::Light();
        if (ImGui::MenuItem("Light theme",nullptr,&light)) studio::Apply(light);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Script")) {
        if (ImGui::MenuItem("New Script")) insertScript();
        if (ImGui::MenuItem("New LocalScript")) insertScript(InstanceClass::LocalScript);
        if (ImGui::MenuItem("New ModuleScript")) insertScript(InstanceClass::ModuleScript);
        if (ImGui::MenuItem("Install default game scripts")) scene_.installDefaultScripts();
        ImGui::MenuItem("Script Editor",nullptr,&showScriptEditor_);
        ImGui::MenuItem("Output",nullptr,&showOutput_);
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();
    drawGameToolbar();
    const std::string projectName=std::filesystem::path(projectPath_).filename().string();
    const float projectX=ImGui::GetWindowWidth()-ImGui::CalcTextSize(projectName.c_str()).x-16;
    if(projectX>ImGui::GetCursorPosX()+20) {
        ImGui::SameLine(projectX);ImGui::TextDisabled("%s",projectName.c_str());
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",projectPath_.c_str());
    }
    ImGui::EndMainMenuBar();
}

void EditorUI::drawToolbar() {
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 6.0f));
    ImGui::Begin("##Toolbar", nullptr, flags);
    const LobloxLogo& logo = LoadLobloxLogo();
    if (logo.ok) {
        // Proporcao preservada (1600x1600 => quadrado); nao estica nem deforma.
        const float brandHeight = 24.0f;
        ImGui::Image(static_cast<ImTextureID>(logo.id),
                     ImVec2(brandHeight * logo.width / logo.height, brandHeight));
    } else {
        const ImVec2 brandCursor = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(24.0f, ImGui::GetTextLineHeight()));
        DrawIcon(ImGui::GetWindowDrawList(),
                 ImVec2(brandCursor.x + 11.0f, brandCursor.y + ImGui::GetTextLineHeight() * 0.5f),
                 15.0f, Icon::Part, IM_COL32(0, 162, 255, 255));
    }
    ImGui::SameLine(0.0f, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 185, 255, 255));
    ImGui::TextUnformatted("LOBLOX");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 14.0f);
    if(ImGui::Checkbox("Snap",&snapEnabled_)) {endGizmoSession(true,"snap_toggle");cancelRigGizmo();}
    const auto snapMin=ImGui::GetItemRectMin(),snapMax=ImGui::GetItemRectMax();
    snapControl_={snapMin.x,snapMin.y,snapMax.x-snapMin.x,snapMax.y-snapMin.y};
    if(ImGui::IsItemHovered())ImGui::SetTooltip("OFF: continuous fine movement. ON: increments from the drag's initial transform.");
    for(auto control:{std::pair<const char*,float*>{"Move",&moveSnap_},{"Scale",&scaleSnap_},{"Rotate",&rotateSnapDegrees_}}) {
        ImGui::SameLine();ImGui::SetNextItemWidth(52);
        if(ImGui::DragFloat(control.first,control.second,.01f,.001f,control.second==&rotateSnapDegrees_?180.f:128.f,"%.2f",ImGuiSliderFlags_AlwaysClamp)) {
            endGizmoSession(true,"snap_increment");cancelRigGizmo();
        }
        if(ImGui::IsItemHovered())ImGui::SetTooltip(control.second==&rotateSnapDegrees_?"Degrees per step":"Studs per step");
    }
    gizmo_.setSnap(snapEnabled_?moveSnap_:0,snapEnabled_?scaleSnap_:0,snapEnabled_?rotateSnapDegrees_:0);
    ImGui::SameLine(0,14);
    ImGui::BeginTabBar("##RibbonTabs", ImGuiTabBarFlags_FittingPolicyScroll);

    if (ImGui::BeginTabItem("Home")) {
        ImGui::Spacing();
        if (IconButton("select", Icon::Select, "Select", tool_ == ToolMode::Select,
                       "Selecionar objetos (1)")) {
            setTool(ToolMode::Select);
        }
        ImGui::SameLine();
        if (IconButton("move", Icon::Move, "Move", tool_ == ToolMode::Move,
                       "Mover nos eixos X/Y/Z e planos (2)")) setTool(ToolMode::Move);
        ImGui::SameLine();
        if (IconButton("scale", Icon::Scale, "Scale", tool_ == ToolMode::Scale,
                       "Redimensionar pelo gizmo (3)")) setTool(ToolMode::Scale);
        ImGui::SameLine();
        if (IconButton("rotate", Icon::Rotate, "Rotate", tool_ == ToolMode::Rotate,
                       "Girar pelo gizmo (4)")) setTool(ToolMode::Rotate);
        ToolbarSeparator();
        if (IconButton("part", Icon::Part, "Part", false, "Criar um Block")) insertPart(PartShape::Block);
        ImGui::SameLine();
        if (IconButton("sphere", Icon::Sphere, "Sphere", false, "Criar uma Sphere")) insertPart(PartShape::Sphere);
        ImGui::SameLine();
        if (IconButton("spawnloc", Icon::Spawn, "Spawn", false, "Criar SpawnLocation")) insertSpawnLocation();
        ToolbarSeparator();
        Instance* homeSelection = scene_.selection();
        Part* homePart = homeSelection != nullptr && homeSelection->isPart()
                             ? static_cast<Part*>(homeSelection) : nullptr;
        const bool canAnchorHome = homePart != nullptr;
        ImGui::BeginDisabled(!canAnchorHome);
        if (IconButton("anchor_home", Icon::Anchor,
                       homePart != nullptr && homePart->anchored() ? "Unanchor" : "Anchor",
                       homePart != nullptr && homePart->anchored(), "Ancorar ou soltar a peça",
                       canAnchorHome)) homePart->setAnchored(!homePart->anchored());
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (IconButton("color_home", Icon::Color, "Color", false,
                       "Mudar a cor da peça selecionada", homePart != nullptr)) {
            endGizmoSession(true);
            ImGui::OpenPopup("##color_palette");
        }
        ImGui::SameLine();
        if (IconButton("material_home", Icon::Material, "Material", false,
                       "Mudar o material da peça selecionada", homePart != nullptr)) {
            endGizmoSession(true);
            ImGui::OpenPopup("##material_menu");
        }
        if (ImGui::BeginPopup("##color_palette")) {
            if (homePart == nullptr) ImGui::TextDisabled("Selecione uma peça primeiro.");
            else for (int i = 0; i < static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0])); ++i) {
                ImGui::PushID(i);
                const ImVec4 c(kPalette[i].r / 255.0f, kPalette[i].g / 255.0f,
                               kPalette[i].b / 255.0f, 1.0f);
                if (ImGui::ColorButton("##home_swatch", c, ImGuiColorEditFlags_NoTooltip,
                                       ImVec2(26.0f, 22.0f))) {
                    applyColorToSelection(kPalette[i]);
                    ImGui::CloseCurrentPopup();
                }
                if (i % 6 != 5) ImGui::SameLine();
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##material_menu")) {
            if (homePart == nullptr) ImGui::TextDisabled("Selecione uma peça primeiro.");
            else for (int i = 0; i < PartMaterialCount(); ++i) {
                if (ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(i)),
                                      static_cast<int>(homePart->material()) == i)) {
                    applyMaterialToSelection(i);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (IconButton("toolbox_home", Icon::Part, "Toolbox", showToolbox_,
                       "Mostrar/ocultar Toolbox")) showToolbox_ = !showToolbox_;
        ImGui::SameLine();
        if (IconButton("explorer_home", Icon::Explorer, "Explorer", showExplorer_,
                       "Mostrar/ocultar Explorer")) showExplorer_ = !showExplorer_;
        ImGui::SameLine();
        if (IconButton("properties_home", Icon::Properties, "Properties", showProperties_,
                       "Mostrar/ocultar Properties")) showProperties_ = !showProperties_;
        ToolbarSeparator();
        if (IconButton("save_home", Icon::Save, "Save", false, "Salvar projeto (Ctrl+S)")) {
            endGizmoSession(true);
            requestSavePopup_ = true;
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
            dialogError_.clear();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Model")) {
        ImGui::Spacing();
        if (ImGui::Checkbox("Local axes", &gizmoLocalSpace_)) {
            endGizmoSession(true);
            gizmo_.setLocalSpace(gizmoLocalSpace_);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(74.0f);
        ImGui::DragFloat("Move snap", &moveSnap_, 0.1f, 0.0f, 128.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(74.0f);
        ImGui::DragFloat("Scale snap", &scaleSnap_, 0.05f, 0.0f, 128.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(74.0f);
        ImGui::DragFloat("Rotate snap", &rotateSnapDegrees_, 1.0f, 0.0f, 180.0f, "%.1f°");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(84.0f);
        if (ImGui::DragFloat("Camera speed", &cameraMoveSpeed_, 0.5f, 1.0f, 200.0f, "%.0f"))
            camera_.setNavigationSettings(cameraMoveSpeed_, cameraLookSensitivity_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(88.0f);
        if (ImGui::DragFloat("Look sensitivity", &cameraLookSensitivity_, 0.0001f,
                             0.0005f, 0.03f, "%.4f"))
            camera_.setNavigationSettings(cameraMoveSpeed_, cameraLookSensitivity_);
        gizmo_.setSnap(snapEnabled_?moveSnap_:0,snapEnabled_?scaleSnap_:0,snapEnabled_?rotateSnapDegrees_:0);
        ToolbarSeparator();
        Instance* selection = scene_.selection();
        Part* part = selection != nullptr && selection->isPart() ? static_cast<Part*>(selection) : nullptr;
        const bool canToggleAnchor = part != nullptr;
        ImGui::BeginDisabled(!canToggleAnchor);
        if (IconButton("anchor_model", Icon::Anchor,
                       part != nullptr && part->anchored() ? "Unanchor" : "Anchor",
                       part != nullptr && part->anchored(), "Ancorar ou soltar a peça",
                       canToggleAnchor)) {
            part->setAnchored(!part->anchored());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (IconButton("color_model", Icon::Color, "Color", false,
                       "Mudar a cor da peça selecionada", part != nullptr)) {
            endGizmoSession(true);
            ImGui::OpenPopup("##color_palette");
        }
        ImGui::SameLine();
        if (IconButton("material_model", Icon::Material, "Material", false,
                       "Mudar o material da peça selecionada", part != nullptr)) {
            endGizmoSession(true);
            ImGui::OpenPopup("##material_menu");
        }
        if (ImGui::BeginPopup("##color_palette")) {
            if (part == nullptr) ImGui::TextDisabled("Selecione uma peca primeiro.");
            else for (int i = 0; i < static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0])); ++i) {
                ImGui::PushID(i);
                const ImVec4 c(kPalette[i].r / 255.0f, kPalette[i].g / 255.0f,
                               kPalette[i].b / 255.0f, 1.0f);
                if (ImGui::ColorButton("##swatch", c, ImGuiColorEditFlags_NoTooltip,
                                       ImVec2(26.0f, 22.0f))) {
                    applyColorToSelection(kPalette[i]);
                    ImGui::CloseCurrentPopup();
                }
                if (i % 6 != 5) ImGui::SameLine();
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##material_menu")) {
            if (part == nullptr) ImGui::TextDisabled("Selecione uma peca primeiro.");
            else for (int i = 0; i < PartMaterialCount(); ++i) {
                if (ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(i)), static_cast<int>(part->material()) == i)) {
                    applyMaterialToSelection(i);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Avatar")) {
        ImGui::Spacing();
        if (IconButton("avatar_r6", Icon::Rig, "R6", false, "Inserir um Rig R6")) insertR6();
        ImGui::SameLine();
        if (IconButton("avatar_rig", Icon::Rig, "Rig Editor", showRigEditor_,
                       "Abrir o Rig Editor")) showRigEditor_ = true;
        ImGui::SameLine();
        if (IconButton("avatar_anim", Icon::Clip, "Animation Editor", showAnimationEditor_,
                       "Abrir o Animation Editor")) {
            showAnimationEditor_ = true;
            if (animationSource_.empty()) openAnimation(scene_.gameAssets().player.walkAnimation);
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Script")) {
        ImGui::Spacing();
        if (IconButton("script_new", Icon::ScriptPage, "New Script", false,
                       "Criar um Script")) insertScript();
        ImGui::SameLine();
        if (IconButton("script_module", Icon::ScriptPage, "New ModuleScript", false,
                       "Criar um ModuleScript")) insertScript(InstanceClass::ModuleScript);
        ImGui::SameLine();
        if (IconButton("script_editor", Icon::ScriptPage, "Script Editor", showScriptEditor_,
                       "Abrir o Script Editor")) showScriptEditor_ = true;
        ImGui::SameLine();
        if (IconButton("script_output", Icon::Output, "Output", showOutput_,
                       "Abrir o Output")) showOutput_ = true;
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("View")) {
        ImGui::Spacing();
        if (IconButton("toolbox_toggle", Icon::Part, "Toolbox", showToolbox_,
                       "Show or hide the Toolbox")) {
            showToolbox_ = !showToolbox_;
        }
        ImGui::SameLine();
        if (IconButton("explorer_toggle", Icon::Explorer, "Explorer", showExplorer_,
                       "Mostrar/ocultar Explorer")) {
            showExplorer_ = !showExplorer_;
        }
        ImGui::SameLine();
        if (IconButton("properties_toggle", Icon::Properties, "Properties", showProperties_,
                       "Mostrar/ocultar Properties")) {
            showProperties_ = !showProperties_;
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Game")) {
        ImGui::Spacing();
        if (IconButton("game_play", Icon::Clip, "Play (F5)", playing(),
                       "Iniciar o Play e executar os scripts")) startPlay();
        ImGui::SameLine();
        ImGui::BeginDisabled(!playing());
        if (IconButton("game_stop", Icon::Stop, "Stop", false,
                       "Encerrar o Play e restaurar a cena autoral")) stopPlay();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (IconButton("game_settings", Icon::Properties, "Game Settings", showGameSettings_,
                       "Configurações de jogador e câmera")) showGameSettings_ = true;
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void EditorUI::drawToolbox() {
    ImGui::Begin("Toolbox");
    static char search[64] = {};
    ImGui::TextDisabled("PARTS AND TEMPLATES");
    ImGui::TextUnformatted("Build a scene");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##toolbox_search", "Search objects...", search, sizeof(search));
    ImGui::Spacing();

    if (ContainsIgnoreCase("Block", search)) {
        if (IconButton("toolbox_block", Icon::Part, "Block", false,
                       "Insert an anchored block")) insertPart(PartShape::Block);
        ImGui::SameLine();
    }
    if (ContainsIgnoreCase("Sphere", search) &&
        IconButton("toolbox_sphere", Icon::Sphere, "Sphere", false,
                   "Insert an anchored sphere")) insertPart(PartShape::Sphere);
    if (ContainsIgnoreCase("SpawnLocation", search)) {
        if (ContainsIgnoreCase("Block", search) || ContainsIgnoreCase("Sphere", search))
            ImGui::SameLine();
        if (IconButton("toolbox_spawn", Icon::Spawn, "Spawn", false,
                       "Insert a SpawnLocation")) insertSpawnLocation();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Scene");
    ImGui::TextDisabled("%d objects", static_cast<int>(scene_.objects().size()));
    ImGui::SameLine();
    ImGui::TextDisabled("%d parts", static_cast<int>(scene_.parts().size()));
    ImGui::TextDisabled("%d physics bodies", static_cast<int>(physics_.bodyCount()));
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Navigation shortcuts", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("WASD / Q E      Move camera");
        ImGui::BulletText("RMB drag          Orbit / look");
        ImGui::BulletText("Q / E             Down / Up");
        ImGui::BulletText("MMB / Wheel  Pan / Zoom");
        ImGui::BulletText("1 - 4              Select / Move / Scale / Rotate");
        ImGui::BulletText("F / Delete       Focus / Delete");
    }
    ImGui::End();
}

void EditorUI::drawViewport() {
    const EditorInput input = ReadEditorInput();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("Viewport", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (!visible) {
        if (runtime_.active() && input.stop && IsWindowFocused()) stopPlay();
        if (runtime_.state() == game::RuntimeState::Playing) runtime_.pause();
        setGameplayCapture(false);
        endGizmoSession(true, "viewport_hidden");
        cancelRigGizmo();
        camera_.releaseCapture();
        viewportFocused_ = false;
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = std::max(8, static_cast<int>(available.x));
    const int height = std::max(8, static_cast<int>(available.y));
    resizeViewport(width, height);

    if (!viewportReady_) {
        endGizmoSession(true);
        camera_.releaseCapture();
        ImGui::TextDisabled("Viewport indisponivel.");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
    // Image alone has no ID: ImGui assigns a held click to the window's
    // MoveId even with ConfigWindowsMoveFromTitleBarOnly. Register ownership
    // explicitly so a viewport drag cannot be mistaken for an active UI widget.
    const ImVec2 imageSize(static_cast<float>(viewportWidth_), static_cast<float>(viewportHeight_));
    ImGui::InvisibleButton("##viewport_input", imageSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
        ImGuiButtonFlags_MouseButtonMiddle);
    const bool viewportActive = ImGui::IsItemActive();
    ImGui::GetWindowDrawList()->AddImage(
        static_cast<ImTextureID>(static_cast<intptr_t>(viewport_.texture.id)), imagePosition,
        ImVec2(imagePosition.x + imageSize.x, imagePosition.y + imageSize.y),
        ImVec2(0, 1), ImVec2(1, 0));
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();
    const bool clickedViewport = hovered &&
        (input.leftPressed || input.rightPressed || input.middlePressed);
    if (clickedViewport) ImGui::SetWindowFocus();
    viewportFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    InputContext context;
    context.windowFocused = IsWindowFocused();
    context.viewportVisible = true;
    context.hovered = hovered;
    context.focused = viewportFocused_;
    context.popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) ||
        requestSavePopup_ || requestOpenPopup_ || requestLuaExportPopup_ || requestLuaImportPopup_;
    context.widget = ImGui::IsAnyItemActive() && !viewportActive;
    context.text = io.WantTextInput && context.widget;
    context.keyboardCaptured = io.WantCaptureKeyboard;
    if (explorerFocused_ && !context.text && !context.widget && !context.popup)
        context.keyboardCaptured = false;
    context.transforming = transform_.active();
    context.navigating = camera_.cursorCaptured();
    if (runtime_.active()) {
        drawGameplayViewport(input, context, {imagePosition.x, imagePosition.y});
        ImGui::End(); ImGui::PopStyleVar(); return;
    }
    context.transforming = context.transforming || rigGizmo_.dragging();
    const InputRoute route = RouteInput(input, context);
    if (route.owner == InputOwner::Cancel) {
        cancelRigGizmo();
        endGizmoSession(true, route.reason);
        camera_.releaseCapture();
    }
    handleShortcuts(input, route);
    if (runtime_.active()) {
        drawGameplayViewport(input, context, {imagePosition.x, imagePosition.y});
        ImGui::End(); ImGui::PopStyleVar(); return;
    }
    const auto traceDispatch = [&](const char* consumer, const char* outcome) {
      if (traceInput_) {
        const bool mouseEvent = input.leftPressed || input.rightPressed || input.middlePressed ||
            ImGui::IsMouseReleased(0) || ImGui::IsMouseReleased(1) || ImGui::IsMouseReleased(2);
        const bool keyEvent = input.tool >= 0 || input.save || input.open || input.escape ||
            input.remove || input.focus || input.duplicate || input.play || input.stop;
        if (mouseEvent || keyEvent || input.wheel != 0 || route.owner == InputOwner::Cancel) {
            diag::Event event;
            event.subsystem = "input";
            event.name = "dispatch";
            event.entityId = scene_.selection() ? scene_.selection()->id() : kInvalidInstanceId;
            event.result = outcome;
            event.context = {{"consumer", consumer}, {"reason", route.reason}, {"source", "imgui"},
                {"tool", ToolModeName(tool_)}, {"hovered", hovered}, {"focused", viewportFocused_},
                {"capture_mouse", io.WantCaptureMouse}, {"capture_keyboard", io.WantCaptureKeyboard},
                {"left_pressed", input.leftPressed}, {"left_down", input.leftDown},
                {"right_pressed", input.rightPressed}, {"right_down", input.rightDown},
                {"left_released", ImGui::IsMouseReleased(0)},
                {"right_released", ImGui::IsMouseReleased(1)},
                {"middle_pressed", input.middlePressed}, {"middle_down", input.middleDown},
                {"middle_released", ImGui::IsMouseReleased(2)},
                {"key_tool", input.tool}, {"save", input.save}, {"open", input.open},
                {"escape", input.escape}, {"delete", input.remove}, {"duplicate", input.duplicate}};
            diag::log(event);
        }
      }
    };
    Instance* selection = scene_.selection();
    Part* selectedPart =
        selection != nullptr && selection->isPart() ? static_cast<Part*>(selection) : nullptr;
    const Rectangle viewportRect{imagePosition.x, imagePosition.y,
                                 static_cast<float>(viewportWidth_),
                                 static_cast<float>(viewportHeight_)};
    const bool rigConsumed = updateRigGizmo(input, route, viewportRect, hovered,
        context.popup || context.widget || !context.windowFocused);

    GizmoInput gizmoInput;
    gizmoInput.mousePosition = input.mouse;
    gizmoInput.leftPressed = input.leftPressed;
    gizmoInput.leftDown = input.leftDown;
    gizmoInput.windowFocused = context.windowFocused;
    gizmoInput.shiftDown = input.shift;

    // Prioridade unica: o gizmo processa a entrada antes da camera e da
    // selecao. O estado atual (nao o do frame anterior) decide se a camera
    // pode navegar; um clique em handle nunca chega ao raycast de selecao.
    const bool viewportOwns = route.owner == InputOwner::Viewport;
    const bool sessionOwns = route.owner == InputOwner::Transform;
    const auto previousHover = gizmo_.hoveredHandle();
    const GizmoUpdateResult gizmoResult = transform_.update(
        tool_, selectedPart, camera_, viewportRect, hovered,
        (viewportOwns || sessionOwns) && !rigConsumed, gizmoInput);
    if (gizmoResult.ended) {
        checkInvariants(gizmoResult.cancelled ? "transform_cancel" : "transform_commit");
    }
    if (traceInput_ && previousHover != gizmo_.hoveredHandle()) {
        diag::Event event;
        event.subsystem = "gizmo";
        event.name = "hover";
        event.entityId = selectedPart ? selectedPart->id() : kInvalidInstanceId;
        event.context = {{"handle", static_cast<int>(gizmo_.hoveredHandle())},
                         {"tool", ToolModeName(tool_)}};
        diag::log(event);
    }
    const bool cameraOwns = route.owner == InputOwner::Camera;
    camera_.handleInput(input, Vector2{viewportRect.width, viewportRect.height}, cameraOwns,
        !transform_.active() && !gizmoResult.consumed && !rigConsumed &&
            (cameraOwns || (route.editorKeys && !sessionOwns)),
        viewportOwns && !gizmoResult.consumed && !rigConsumed, GetFrameTime());

    if (viewportOwns && !gizmoResult.consumed && !rigConsumed && input.leftPressed) {
        const Vector2 mouse = input.mouse;
        if (mouse.x >= viewportRect.x && mouse.y >= viewportRect.y &&
            mouse.x <= viewportRect.x + viewportRect.width &&
            mouse.y <= viewportRect.y + viewportRect.height) {
            const Ray ray = camera_.rayFromScreen(
                Vector2{mouse.x - viewportRect.x, mouse.y - viewportRect.y},
                Vector2{viewportRect.width, viewportRect.height});
            Part* hit = nullptr;
            Vector3 point{};
            bool partHit = physics_.raycast(ray.position, ray.direction, 5000.0f, &hit, &point);
            float nearest = partHit ? Vector3Distance(ray.position, point) : 5000.f;
            std::string rigHit;
            int bodyHit = -1;
            for (const auto& rig : scene_.gameAssets().rigs) {
                const auto world = rig.evaluate(rig.id == selectedRig_ ? previewPose_ : game::Pose{}, rig.root);
                for (int b = 0; b < game::kBodyCount; ++b) {
                    const auto inverse = game::Inverse(world[b]);
                    Ray local{game::Compose(inverse, {ray.position}).position,
                              Vector3RotateByQuaternion(ray.direction, inverse.rotation)};
                    const auto half = Vector3Scale(rig.bodies[b].size, .5f);
                    const auto collision = GetRayCollisionBox(local, {Vector3Negate(half), half});
                    if (collision.hit && collision.distance < nearest) {
                        nearest = collision.distance; rigHit = rig.id; bodyHit = b;
                    }
                }
            }
            if (!rigHit.empty()) selectRig(rigHit, bodyHit);
            else if (partHit) {
                cancelRigGizmo(); selectedRig_.clear();
                scene_.select(hit);
            } else {
                cancelRigGizmo(); selectedRig_.clear();
                scene_.select(nullptr);
            }
        }
    }

    // Selection may have changed in the raycast above.
    selection = scene_.selection();
    selectedPart = selection && selection->isPart() ? static_cast<Part*>(selection) : nullptr;
    traceDispatch(gizmoResult.consumed ? "gizmo" :
        viewportOwns && input.leftPressed ? "selection" : InputOwnerName(route.owner),
        gizmoResult.started ? "drag_begin" : gizmoResult.ended ?
        (gizmoResult.cancelled ? "drag_cancel" : "drag_commit") : "routed");

    BeginTextureMode(viewport_);
    RigRenderState rigState;
    if (auto* rig = scene_.gameAssets().findRig(selectedRig_)) {
        rigState = {rig, previewPose_, rig->root, selectedRigBody_, showJointPivots_};
    }
    renderer_.drawScene(scene_, camera_, viewportWidth_, viewportHeight_,
                        selection != nullptr ? selection->id() : kInvalidInstanceId,
                        rigState.rig ? &rigState : nullptr);
    if (rigState.rig && showJointPivots_ && tool_!=ToolMode::Select &&
        (tool_!=ToolMode::Scale || rigContext_==RigContext::Edit))
        rigGizmo_.drawOverlay(jointProxy_, tool_, camera_, viewportRect);
    if (selectedPart != nullptr && tool_ != ToolMode::Select) {
        gizmo_.drawOverlay(*selectedPart, tool_, camera_, viewportRect);
    }
    EndTextureMode();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (selectedPart != nullptr) {
        char selectionLabel[128];
        std::snprintf(selectionLabel, sizeof(selectionLabel), "%s (%s)",
                      selectedPart->name().c_str(), selectedPart->className());
        const ImVec2 selectionSize = ImGui::CalcTextSize(selectionLabel);
        const ImVec2 selectionMin(imagePosition.x + 10.0f,
                                  imagePosition.y + viewportRect.height - 31.0f);
        drawList->AddRectFilled(selectionMin,
                                ImVec2(selectionMin.x + selectionSize.x + 16.0f,
                                       selectionMin.y + selectionSize.y + 10.0f),
                                IM_COL32(24, 29, 35, 220), 5.0f);
        drawList->AddText(ImVec2(selectionMin.x + 8.0f, selectionMin.y + 5.0f),
                          IM_COL32(125, 190, 255, 255), selectionLabel);
    }

    if (!status_.empty() && ImGui::GetTime() < statusUntil_) {
        // Buffer fixo: evita alocacao no loop de render.
        char statusText[96];
        std::snprintf(statusText, sizeof(statusText), "%.88s", status_.c_str());
        if (status_.size() > 88) {
            statusText[85] = '.';
            statusText[86] = '.';
            statusText[87] = '.';
            statusText[88] = '\0';
        }
        const ImVec2 textSize = ImGui::CalcTextSize(statusText);
        const ImVec2 statusPosition(imagePosition.x + viewportRect.width - textSize.x - 12.0f,
                                    imagePosition.y + viewportRect.height - 25.0f);
        drawList->AddRectFilled(
            ImVec2(statusPosition.x - 6.0f, statusPosition.y - 4.0f),
            ImVec2(statusPosition.x + textSize.x + 6.0f,
                   statusPosition.y + textSize.y + 4.0f),
            IM_COL32(16, 16, 19, 190), 3.0f);
        const ImU32 color = statusIsError_ ? IM_COL32(255, 130, 120, 255)
                                           : IM_COL32(150, 230, 160, 255);
        drawList->AddText(statusPosition, color, statusText);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::drawExplorerNode(Instance* instance, const char* filter) {
    if (instance == nullptr || !TreeContains(instance, filter)) return;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (instance == scene_.workspace() || instance == scene_.lighting()) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (scene_.selection() && revealedSelection_ != scene_.selection()->id() &&
        instance->isAncestorOf(scene_.selection())) ImGui::SetNextItemOpen(true);
    if (filter != nullptr && *filter != '\0') {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    if (instance->children().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (scene_.selection() == instance) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(
                                            static_cast<intptr_t>(instance->id())),
                                        flags, "  %s", instance->name().c_str());
    const ImVec2 rowMin = ImGui::GetItemRectMin();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    const ImVec2 iconCenter(rowMin.x + 22.0f, (rowMin.y + rowMax.y) * 0.5f);
    const ImVec4 classColor = ClassColor(instance->instanceClass());
    const ImU32 iconColor = ImGui::ColorConvertFloat4ToU32(classColor);
    DrawClassIcon(ImGui::GetWindowDrawList(), iconCenter, instance->instanceClass(), iconColor);
    // Keep the tree item as LastItem until click/context/drag processing ends.
    // Drawing attribute labels here used to steal all interaction from the Part.
    // Indicador de destino de arrasto: contorno realce no no sob o cursor.
    if (ImGui::IsItemHovered() && ImGui::IsDragDropActive()) {
        ImGui::GetWindowDrawList()->AddRect(rowMin, rowMax, IM_COL32(120, 200, 255, 255), 3.0f,
                                            0, 2.0f);
    }
    if (ImGui::IsItemClicked()) {
        endGizmoSession(true);
        scene_.select(instance);
        if (ImGui::IsMouseDoubleClicked(0)) {
            if (IsScriptClass(instance->instanceClass())) openScript(instance->id());
            else if (!ImGui::IsItemToggledOpen()) renameSelection();
        }
    }
    if (ImGui::BeginDragDropSource()) {
        const auto id = instance->id();
        ImGui::SetDragDropPayload("LOBLOX_INSTANCE",&id,sizeof(id));
        ImGui::Text("%s (%s)",instance->name().c_str(),instance->className());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload = ImGui::AcceptDragDropPayload("LOBLOX_INSTANCE")) {
            endGizmoSession(true,"explorer_reparent");
            pendingReparent_ = {*static_cast<const InstanceId*>(payload->Data),instance->id()};
        }
        ImGui::EndDragDropTarget();
    }
    char explorerPopupId[64];
        std::snprintf(explorerPopupId, sizeof(explorerPopupId), "##explorer_context_%" PRIu64, instance->id());
        if (ImGui::BeginPopupContextItem(explorerPopupId)) {
        ImGui::TextDisabled("%s", instance->className());
        ImGui::Separator();
        if (IsScriptClass(instance->instanceClass()) && ImGui::MenuItem("Open Script")) openScript(instance->id());
        if (ImGui::MenuItem("Rename", "F2")) { scene_.select(instance); renameSelection(); }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D",false,instance->deletable())) { scene_.select(instance); pendingDuplicate_=true; }
        if (ImGui::MenuItem("Copy", "Ctrl+C",false,instance->deletable())) clipboardId_ = instance->id();
        if (ImGui::BeginMenu("Insert child (+)", !IsScriptClass(instance->instanceClass()) && instance != scene_.lighting())) {
            if (ImGui::MenuItem("Script")) { scene_.select(instance); pendingInsert_=4; }
            if (ImGui::MenuItem("ModuleScript")) { scene_.select(instance); pendingInsert_=5; }
            if (ImGui::MenuItem("Folder")) { scene_.select(instance); pendingInsert_=1; }
            if (ImGui::MenuItem("Model")) { scene_.select(instance); pendingInsert_=2; }
            if (ImGui::MenuItem("Part")) { scene_.select(instance); pendingInsert_=3; }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Focar na viewport", "F")) {
            endGizmoSession(true);
            scene_.select(instance);
            focusSelection();
        }
        if (ImGui::MenuItem("Excluir", "Del", false, instance->deletable())) {
            endGizmoSession(true);
            scene_.select(instance);
            pendingDelete_ = instance->id();
        }
        ImGui::EndPopup();
    }

    if (open && !instance->children().empty()) {
        for (Instance* child : instance->children()) {
            drawExplorerNode(child, filter);
        }
        ImGui::TreePop();
    }
}

void EditorUI::drawExplorer() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,ImVec2(280,150));
    ImGui::SetNextWindowSize({350, 620}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Explorer", &showExplorer_);
    explorerFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (ImGui::Button("+ Folder")) insertContainer(InstanceClass::Folder);
    ImGui::SameLine(); if (ImGui::Button("+ Model")) insertContainer(InstanceClass::Model);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##explorer_filter", "Search name or class", explorerFilter_,
                             sizeof(explorerFilter_));
    if (scene_.workspace() != nullptr) {
        drawExplorerNode(scene_.workspace(), explorerFilter_);
    }
    if (scene_.lighting() != nullptr) {
        drawExplorerNode(scene_.lighting(), explorerFilter_);
    }
    for (const auto& object:scene_.objects())
        if (!object->parent() && object.get()!=scene_.workspace() && object.get()!=scene_.lighting()) drawExplorerNode(object.get(),explorerFilter_);
    for (const auto& rig : scene_.gameAssets().rigs) {
        ImGui::PushID(rig.id.c_str());
        const bool open = ImGui::TreeNodeEx("##rig_root",
            ImGuiTreeNodeFlags_DefaultOpen | (rig.id == selectedRig_ ? ImGuiTreeNodeFlags_Selected : 0),"%s",rig.name.c_str());
        if (ImGui::IsItemClicked()) selectRig(rig.id);
        
        // Context menu for rig
        std::string rigPopupId = "##rig_context_" + rig.id;
        if (ImGui::BeginPopupContextItem(rigPopupId.c_str())) {
            ImGui::TextDisabled("%s", "Rig");
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2")) { selectRig(rig.id); renameSelection(); }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) { selectRig(rig.id); pendingDuplicate_=true; }
            if (ImGui::MenuItem("Delete", "Del")) { selectRig(rig.id); pendingDeleteRigId_ = rig.id; }
            ImGui::EndPopup();
        }
        
        if (open) {
            ImGui::TextDisabled("HumanoidRootPart (internal)");
            for (int b = 0; b < game::kBodyCount; ++b)
                if (ImGui::Selectable(rig.bodies[b].name.c_str(), rig.id == selectedRig_ && selectedRigBody_ == b)) selectRig(rig.id, b);
            ImGui::TextDisabled("CharacterController / Animator");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    revealedSelection_ = scene_.selection() ? scene_.selection()->id() : 0;
    if (pendingDelete_) { scene_.select(scene_.find(pendingDelete_)); pendingDelete_=0; deleteSelection(); }
    if (!pendingDeleteRigId_.empty()) {
        cancelRigGizmo();previewPlaying_=false;
        const std::string rigId = pendingDeleteRigId_;
        pendingDeleteRigId_.clear();
        if (scene_.gameAssets().removeRig(rigId)) {
            selectedRig_.clear();
            selectedRigBody_ = -1;
            selectedJoint_ = 0;
            checkInvariants("delete_rig");
            setStatus("Rig excluido: " + rigId);
        } else {
            setStatus("Nao foi possivel excluir rig", true);
        }
    }
    if (pendingReparent_.first) {
        std::string error;
        if (!scene_.reparent(scene_.find(pendingReparent_.first),scene_.find(pendingReparent_.second),error)) setStatus(error,true);
        else { revealedSelection_=0; setStatus("Parent atualizado; transform mundial preservado."); checkInvariants("reparent"); }
        pendingReparent_={0,0};
    }
    if (pendingDuplicate_) { pendingDuplicate_=false; duplicateSelection(); }
    if (pendingInsert_) {
        const int action=pendingInsert_; pendingInsert_=0;
        if (action==4 || action==5) insertScript(action==4?InstanceClass::Script:InstanceClass::ModuleScript);
        else if (action==3) insertPart(PartShape::Block);
        else insertContainer(action==1?InstanceClass::Folder:InstanceClass::Model);
    }
    ImGui::Separator();
    if (scene_.selection() != nullptr) {
        ImGui::TextDisabled("Selecionado: %s", scene_.selection()->name().c_str());
    } else {
        ImGui::TextDisabled("Nenhum objeto selecionado");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::drawPartProperties(Part& part) {
    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(part.instanceClass()));
    ImGui::Text("%s", part.className());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("#%llu", static_cast<unsigned long long>(part.id()));
    ImGui::Separator();

    char name[128];
    std::snprintf(name, sizeof(name), "%s", part.name().c_str());
    if (ImGui::InputText("Name", name, sizeof(name))) {
        part.setName(name);
    }

    ImGui::BeginDisabled();
    char className[64];
    std::snprintf(className, sizeof(className), "%s", part.className());
    ImGui::InputText("ClassName", className, sizeof(className));
    ImGui::EndDisabled();

    ImGui::SeparatorText("Transform");
    {
        Vector3 position = part.position();
        float values[3] = {position.x, position.y, position.z};
        if (ImGui::DragFloat3("Position", values, 0.1f, 0.0f, 0.0f, "%.3f")) {
            part.setPosition(Vector3{values[0], values[1], values[2]});
        }
    }
    {
        Vector3 euler = part.rotationEulerDegrees();
        float values[3] = {euler.x, euler.y, euler.z};
        if (ImGui::DragFloat3("Rotation", values, 0.5f, -360.0f, 360.0f, "%.1f")) {
            part.setRotationEulerDegrees(Vector3{values[0], values[1], values[2]});
        }
    }
    {
        Vector3 size = part.size();
        float values[3] = {size.x, size.y, size.z};
        if (ImGui::DragFloat3("Size", values, 0.05f, kMinPartSize, 512.0f, "%.2f")) {
            part.setSize(Vector3{values[0], values[1], values[2]});
        }
    }
    if (!part.anchored()) {
        ImGui::TextDisabled("Peca dinamica: a fisica controla o movimento.");
    }

    ImGui::SeparatorText("Appearance");
    {
        float color[4] = {part.color().r / 255.0f, part.color().g / 255.0f,
                          part.color().b / 255.0f, part.color().a / 255.0f};
        if (ImGui::ColorEdit4("Color", color)) {
            part.setColor(Color{
                static_cast<unsigned char>(std::lround(color[0] * 255.0f)),
                static_cast<unsigned char>(std::lround(color[1] * 255.0f)),
                static_cast<unsigned char>(std::lround(color[2] * 255.0f)),
                static_cast<unsigned char>(std::lround(color[3] * 255.0f))});
        }
    }
    if (ImGui::BeginCombo("Material", PartMaterialName(part.material()))) {
        for (int i = 0; i < PartMaterialCount(); ++i) {
            const bool selected = static_cast<int>(part.material()) == i;
            if (ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(i)), selected)) {
                part.setMaterial(PartMaterialFromIndex(i));
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled();
    char shape[32];
    std::snprintf(shape, sizeof(shape), "%s", PartShapeName(part.shape()));
    ImGui::InputText("Shape", shape, sizeof(shape));
    ImGui::EndDisabled();

    ImGui::SeparatorText("Physics");
    bool anchored = part.anchored();
    if (ImGui::Checkbox("Anchored", &anchored)) {
        part.setAnchored(anchored);
    }
    bool canCollide = part.canCollide();
    if (ImGui::Checkbox("CanCollide", &canCollide)) {
        part.setCanCollide(canCollide);
    }

    ImGui::Separator();
    if (ImGui::Button("Focar (F)", ImVec2(-FLT_MIN, 0.0f))) {
        focusSelection();
    }
    ImGui::BeginDisabled(!part.deletable());
    if (ImGui::Button("Excluir", ImVec2(-FLT_MIN, 0.0f))) {
        deleteSelection();
    }
    ImGui::EndDisabled();
    if (!part.deletable()) {
        ImGui::TextDisabled("Elemento estrutural protegido.");
    }
}

void EditorUI::drawLightingProperties() {
    Lighting* lighting = scene_.lighting();
    if (lighting == nullptr) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(InstanceClass::Lighting));
    ImGui::TextUnformatted("Lighting");
    ImGui::PopStyleColor();
    ImGui::Separator();

    char name[128];
    std::snprintf(name, sizeof(name), "%s", lighting->name().c_str());
    if (ImGui::InputText("Name", name, sizeof(name))) {
        lighting->setName(name);
    }

    float sky[3] = {lighting->skyColor().r / 255.0f, lighting->skyColor().g / 255.0f,
                    lighting->skyColor().b / 255.0f};
    if (ImGui::ColorEdit3("SkyColor", sky)) {
        lighting->setSkyColor(Color{static_cast<unsigned char>(std::lround(sky[0] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sky[1] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sky[2] * 255.0f)),
                                    255});
    }
    float ambient[3] = {lighting->ambientColor().r / 255.0f,
                        lighting->ambientColor().g / 255.0f,
                        lighting->ambientColor().b / 255.0f};
    if (ImGui::ColorEdit3("Ambient", ambient)) {
        lighting->setAmbientColor(
            Color{static_cast<unsigned char>(std::lround(ambient[0] * 255.0f)),
                  static_cast<unsigned char>(std::lround(ambient[1] * 255.0f)),
                  static_cast<unsigned char>(std::lround(ambient[2] * 255.0f)), 255});
    }
    Vector3 sun = lighting->sunDirection();
    float sunValues[3] = {sun.x, sun.y, sun.z};
    if (ImGui::DragFloat3("SunDirection", sunValues, 0.02f, -1.0f, 1.0f, "%.2f")) {
        lighting->setSunDirection(Vector3{sunValues[0], sunValues[1], sunValues[2]});
    }
    float intensity = lighting->sunIntensity();
    if (ImGui::SliderFloat("SunIntensity", &intensity, 0.0f, 3.0f, "%.2f")) {
        lighting->setSunIntensity(intensity);
    }
    float brightness = lighting->brightness();
    if (ImGui::SliderFloat("Brightness", &brightness, 0.2f, 2.0f, "%.2f")) {
        lighting->setBrightness(brightness);
    }
    ImGui::SeparatorText("Fog");
    float fog[3] = {lighting->fogColor().r / 255.0f, lighting->fogColor().g / 255.0f,
                    lighting->fogColor().b / 255.0f};
    if (ImGui::ColorEdit3("FogColor", fog)) {
        lighting->setFogColor(Color{static_cast<unsigned char>(std::lround(fog[0] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(fog[1] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(fog[2] * 255.0f)), 255});
    }
    float fogDensity = lighting->fogDensity();
    if (ImGui::SliderFloat("FogDensity", &fogDensity, 0.0f, 1.0f, "%.3f")) {
        lighting->setFogDensity(fogDensity);
    }

    ImGui::SeparatorText("Sky");
    bool skyEnabled = lighting->skyEnabled();
    if (ImGui::Checkbox("Sky Enabled", &skyEnabled)) {
        lighting->setSkyEnabled(skyEnabled);
        if (!skyEnabled)
            ImGui::SetTooltip("O fundo volta para a cor sólida do SkyColor");
    }

    float zenith[3] = {lighting->skyZenithColor().r / 255.0f,
                       lighting->skyZenithColor().g / 255.0f,
                       lighting->skyZenithColor().b / 255.0f};
    if (ImGui::ColorEdit3("Zenith Color", zenith)) {
        lighting->setSkyZenithColor(Color{static_cast<unsigned char>(std::lround(zenith[0] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(zenith[1] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(zenith[2] * 255.0f)), 255});
    }

    float horizon[3] = {lighting->skyHorizonColor().r / 255.0f,
                        lighting->skyHorizonColor().g / 255.0f,
                        lighting->skyHorizonColor().b / 255.0f};
    if (ImGui::ColorEdit3("Horizon Color", horizon)) {
        lighting->setSkyHorizonColor(Color{static_cast<unsigned char>(std::lround(horizon[0] * 255.0f)),
                                           static_cast<unsigned char>(std::lround(horizon[1] * 255.0f)),
                                           static_cast<unsigned char>(std::lround(horizon[2] * 255.0f)), 255});
    }

    float ground[3] = {lighting->skyGroundColor().r / 255.0f,
                       lighting->skyGroundColor().g / 255.0f,
                       lighting->skyGroundColor().b / 255.0f};
    if (ImGui::ColorEdit3("Ground Color", ground)) {
        lighting->setSkyGroundColor(Color{static_cast<unsigned char>(std::lround(ground[0] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(ground[1] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(ground[2] * 255.0f)), 255});
    }

    float sunCol[3] = {lighting->sunColor().r / 255.0f,
                       lighting->sunColor().g / 255.0f,
                       lighting->sunColor().b / 255.0f};
    if (ImGui::ColorEdit3("Sun Color", sunCol)) {
        lighting->setSunColor(Color{static_cast<unsigned char>(std::lround(sunCol[0] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sunCol[1] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sunCol[2] * 255.0f)), 255});
    }

    float sunSize = lighting->sunSize();
    if (ImGui::SliderFloat("Sun Size", &sunSize, 0.0f, 1.0f, "%.3f")) {
        lighting->setSunSize(sunSize);
    }

    float atmoStrength = lighting->atmosphereStrength();
    if (ImGui::SliderFloat("Atmosphere Strength", &atmoStrength, 0.0f, 1.0f, "%.2f")) {
        lighting->setAtmosphereStrength(atmoStrength);
    }

    ImGui::SeparatorText("Clouds");
    float cloudDensity = lighting->cloudDensity();
    if (ImGui::SliderFloat("Cloud Density", &cloudDensity, 0.0f, 1.0f, "%.2f")) {
        lighting->setCloudDensity(cloudDensity);
    }

    float cloudScale = lighting->cloudScale();
    if (ImGui::SliderFloat("Cloud Scale", &cloudScale, 0.1f, 10.0f, "%.2f")) {
        lighting->setCloudScale(cloudScale);
    }

    float cloudSpeed = lighting->cloudSpeed();
    if (ImGui::SliderFloat("Cloud Speed", &cloudSpeed, 0.0f, 5.0f, "%.2f")) {
        lighting->setCloudSpeed(cloudSpeed);
    }

    float cloudSoftness = lighting->cloudSoftness();
    if (ImGui::SliderFloat("Cloud Softness", &cloudSoftness, 0.0f, 1.0f, "%.2f")) {
        lighting->setCloudSoftness(cloudSoftness);
    }

    float skyRot = lighting->skyRotation();
    if (ImGui::DragFloat("Sky Rotation (deg)", &skyRot, 0.5f, -360.0f, 360.0f, "%.1f")) {
        lighting->setSkyRotation(skyRot);
    }
}

void EditorUI::drawWorkspaceProperties() {
    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(InstanceClass::Workspace));
    ImGui::TextUnformatted("Workspace");
    ImGui::PopStyleColor();
    ImGui::TextDisabled("#%llu",
                        static_cast<unsigned long long>(scene_.workspace()->id()));
    ImGui::Separator();
    char name[128];
    std::snprintf(name, sizeof(name), "%s", scene_.workspace()->name().c_str());
    if (ImGui::InputText("Name", name, sizeof(name))) {
        scene_.workspace()->setName(name);
    }
    float height=scene_.workspace()->fallenPartsDestroyHeight();
    if(ImGui::InputFloat("FallenPartsDestroyHeight",&height,10,100,"%.1f",ImGuiInputTextFlags_EnterReturnsTrue) &&
        !scene_.workspace()->setFallenPartsDestroyHeight(height))setStatus("Void height invalido.",true);
    ImGui::SeparatorText("Projeto");
    ImGui::Text("Objetos: %d", static_cast<int>(scene_.objects().size()));
    ImGui::Text("Pecas: %d", static_cast<int>(scene_.parts().size()));
    ImGui::Text("Corpos fisicos: %d", static_cast<int>(physics_.bodyCount()));
    ImGui::TextWrapped("Arquivo: %s", projectPath_.c_str());
}

void EditorUI::drawProperties() {
    ImGui::SetNextWindowSize({330, 560}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Properties");
    Instance* selection = scene_.selection();
    if (selection == nullptr) {
        if (auto* rig = scene_.gameAssets().findRig(selectedRig_)) {
            ImGui::Text("%s (R6 rig)", rig->name.c_str());
            ImGui::Text("Root: %.2f, %.2f, %.2f", rig->root.position.x, rig->root.position.y, rig->root.position.z);
            if (selectedRigBody_ >= 0) ImGui::Text("Body: %s", rig->bodies[selectedRigBody_].name.c_str());
            ImGui::Text("Joint: %s", rig->joints[selectedJoint_].name.c_str());
            if (ImGui::Button("Open Rig Editor")) showRigEditor_ = true;
            ImGui::End(); return;
        }
        ImGui::Spacing();
        ImGui::TextUnformatted("Properties");
        ImGui::TextWrapped("Select an object in the Explorer or viewport to edit its properties.");
    } else if (selection == scene_.lighting()) {
        drawLightingProperties();
    } else if (selection == scene_.workspace()) {
        drawWorkspaceProperties();
    } else if (selection->isPart() || selection->instanceClass()==InstanceClass::Folder ||
               selection->instanceClass()==InstanceClass::Model || IsScriptClass(selection->instanceClass()) ||
               IsRootServiceClass(selection->instanceClass())) {
        drawInstanceProperties(*selection);
    }
    ImGui::End();
}

void EditorUI::drawDialogs() {
    if (renameId_ || !renameRigId_.empty()) ImGui::OpenPopup("Rename instance");
    if (ImGui::BeginPopupModal("Rename instance",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter=ImGui::InputText("Name",renameBuffer_,sizeof(renameBuffer_),ImGuiInputTextFlags_EnterReturnsTrue);
        if (enter || ImGui::Button("Rename")) {
            if (auto* node=scene_.find(renameId_); node && renameBuffer_[0]) node->setName(renameBuffer_);
            if(auto* rig=scene_.gameAssets().findRig(renameRigId_);rig && renameBuffer_[0])rig->name=renameBuffer_;
            renameId_=0;renameRigId_.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
            renameId_=0;renameRigId_.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (requestSavePopup_) {
        ImGui::OpenPopup("Salvar projeto");
        requestSavePopup_ = false;
    }
    if (requestOpenPopup_) {
        ImGui::OpenPopup("Abrir projeto");
        requestOpenPopup_ = false;
    }
    if (requestLuaExportPopup_) {
        ImGui::OpenPopup("Exportar cena Lua");
        requestLuaExportPopup_ = false;
    }
    if (requestLuaImportPopup_) {
        ImGui::OpenPopup("Importar cena Lua");
        requestLuaImportPopup_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Salvar projeto", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Caminho do arquivo (.json):");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##save_path", pathBuffer_, sizeof(pathBuffer_));
        if (!dialogError_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.40f, 1.0f), "%s", dialogError_.c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button("Salvar", ImVec2(120.0f, 0.0f))) {
            std::string error;
            std::error_code pathError;
            const std::filesystem::path chosen = std::filesystem::absolute(pathBuffer_, pathError);
            const std::string savePath = pathError ? std::string(pathBuffer_) : chosen.string();
            if (saveProjectAt(savePath, error)) {
                projectPath_ = savePath;
                dialogError_.clear();
                setStatus("Projeto salvo em " + projectPath_);
                ImGui::CloseCurrentPopup();
            } else {
                dialogError_ = "Falha ao salvar '" + savePath + "': " + error;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120.0f, 0.0f))) {
            dialogError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Abrir projeto", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Caminho do arquivo (.json):");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##open_path", pathBuffer_, sizeof(pathBuffer_));
        ImGui::TextDisabled("Carregar substitui a cena atual.");
        if (!dialogError_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.40f, 1.0f), "%s", dialogError_.c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button("Abrir", ImVec2(120.0f, 0.0f))) {
            std::string error;
            endGizmoSession(true);
            if (projectio::load(scene_, pathBuffer_, error)) {
                scriptDrafts_.clear();activeScript_=0;
                cancelRigGizmo(false); selectedRig_.clear(); animationSource_.clear();
                previewPlaying_ = false; previewPose_ = {}; animationDirty_ = false;
                std::error_code pathError;
                const std::filesystem::path opened = std::filesystem::absolute(pathBuffer_, pathError);
                projectPath_ = pathError ? std::string(pathBuffer_) : opened.string();
                dialogError_.clear();
                camera_.releaseCapture();
                if (onSceneLoaded_) {
                    onSceneLoaded_();
                }
                checkInvariants("load");
                setStatus("Projeto carregado: " + projectPath_);
                ImGui::CloseCurrentPopup();
            } else {
                dialogError_ = error;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120.0f, 0.0f))) {
            dialogError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(540.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Exportar cena Lua", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Exportar cena declarativa (.lua):");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##lua_export_path", pathBuffer_, sizeof(pathBuffer_));
        ImGui::TextDisabled("Contem apenas dados da cena; nenhum script e executado.");
        if (!dialogError_.empty()) ImGui::TextColored(ImVec4(0.78f, 0.12f, 0.10f, 1), "%s", dialogError_.c_str());
        if (ImGui::Button("Exportar", ImVec2(120, 0))) {
            std::string error;
            if (scenelua::save(scene_, pathBuffer_, error)) {
                setStatus("Cena exportada: " + std::string(pathBuffer_));
                dialogError_.clear();
                ImGui::CloseCurrentPopup();
            } else dialogError_ = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar##lua_export", ImVec2(120, 0))) {
            dialogError_.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Importar cena Lua", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Importar cena declarativa (.lua):");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##lua_import_path", pathBuffer_, sizeof(pathBuffer_));
        ImGui::TextWrapped("Aceita somente `return` seguido por tabelas literais do formato loblox-scene. O arquivo nunca e executado.");
        ImGui::TextDisabled("Uma importacao valida substitui a cena atual.");
        if (!dialogError_.empty()) ImGui::TextColored(ImVec4(0.78f, 0.12f, 0.10f, 1), "%s", dialogError_.c_str());
        if (ImGui::Button("Importar", ImVec2(120, 0))) {
            std::string error;
            endGizmoSession(true);
            if (scenelua::load(scene_, pathBuffer_, error)) {
                cancelRigGizmo(false); selectedRig_.clear(); animationSource_.clear();
                previewPlaying_ = false; previewPose_ = {}; animationDirty_ = false;
                camera_.releaseCapture();
                if (onSceneLoaded_) onSceneLoaded_();
                checkInvariants("lua_import");
                setStatus("Cena declarativa importada.");
                dialogError_.clear();
                ImGui::CloseCurrentPopup();
            } else dialogError_ = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar##lua_import", ImVec2(120, 0))) {
            dialogError_.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorUI::insertPart(PartShape shape) {
    endGizmoSession(true);
    Instance* parent=scene_.selection();
    const Vector3 focus = camera_.target();
    const Vector3 position{focus.x, std::max(6.0f, focus.y + 4.0f), focus.z};
    const Vector3 size = shape == PartShape::Sphere ? Vector3{3.0f, 3.0f, 3.0f}
                                                    : Vector3{4.0f, 4.0f, 4.0f};
    Part* part = scene_.createPart(shape, position, size);
    if (part == nullptr) {
        setStatus("Falha ao criar a peca.", true);
        return;
    }
    // In edit mode inserted parts must stay where the user places them.
    part->setAnchored(true);
    if (parent && (parent->instanceClass()==InstanceClass::Folder || parent->instanceClass()==InstanceClass::Model)) {
        std::string error; if (!scene_.reparent(part,parent,error)) setStatus(error,true);
    }
    scene_.select(part);
    setStatus(shape == PartShape::Sphere ? "Sphere criada e ancorada."
                                         : "Block criado e ancorado.");
}

void EditorUI::insertSpawnLocation() {
    endGizmoSession(true);
    const Vector3 focus = camera_.target();
    SpawnLocation* spawn =
        scene_.createSpawnLocation(Vector3{focus.x, 0.5f, focus.z});
    if (spawn == nullptr) {
        setStatus("Falha ao criar SpawnLocation.", true);
        return;
    }
    scene_.select(spawn);
    setStatus("SpawnLocation criada.");
}

void EditorUI::applyColorToSelection(Color color) {
    Instance* selection = scene_.selection();
    if (selection == nullptr || !selection->isPart()) {
        setStatus("Selecione uma peca para aplicar cor.", true);
        return;
    }
    static_cast<Part*>(selection)->setColor(color);
    setStatus("Cor aplicada.");
}

void EditorUI::applyMaterialToSelection(int materialIndex) {
    Instance* selection = scene_.selection();
    if (selection == nullptr || !selection->isPart()) {
        setStatus("Selecione uma peca para aplicar material.", true);
        return;
    }
    static_cast<Part*>(selection)->setMaterial(PartMaterialFromIndex(materialIndex));
    setStatus("Material aplicado.");
}

void EditorUI::duplicateSelection() {
    if(auto* rig=scene_.gameAssets().findRig(selectedRig_)) {
        cancelRigGizmo();auto copy=*rig;
        copy.id="rig-"+diag::newCorrelationId();copy.name=copy.name.substr(0,122)+" Copy";
        copy.root.position.x+=5;
        const auto id=copy.id;scene_.gameAssets().rigs.push_back(std::move(copy));
        selectRig(id);checkInvariants("duplicate_rig");setStatus("Rig duplicated");return;
    }
    Instance* selection = scene_.selection();
    if (!selection) return;
    endGizmoSession(false, "duplicate");
    Instance* copy = scene_.duplicateTree(*selection);
    if (!copy) {
        setStatus("Nao foi possivel duplicar a peca selecionada.", true);
        return;
    }
    physics_.syncScene(scene_);
    scene_.select(copy);
    checkInvariants("duplicate");
    setStatus("Duplicado: " + copy->name());
}

void EditorUI::renameSelection() {
    if(auto* rig=scene_.gameAssets().findRig(selectedRig_)) {
        cancelRigGizmo();renameId_=0;renameRigId_=rig->id;
        std::snprintf(renameBuffer_,sizeof(renameBuffer_),"%s",rig->name.c_str());return;
    }
    auto* selected=scene_.selection(); if (!selected) return;
    endGizmoSession(true,"rename"); renameId_=selected->id();
    std::snprintf(renameBuffer_,sizeof(renameBuffer_),"%s",selected->name().c_str());
}
void EditorUI::insertContainer(InstanceClass type, Instance* parent) {
    endGizmoSession(true,"insert_container");
    if (!parent) parent=scene_.selection();
    if (!parent || parent->isPart() || parent==scene_.lighting()) parent=scene_.workspace();
    if (auto* object=scene_.createContainer(type,parent)) { scene_.select(object); setStatus("Criado: "+object->name()); }
    else setStatus("Nao foi possivel criar container nesse Parent.",true);
}

void EditorUI::deleteSelection() {
    // Check if a rig is selected (rigs are not Instances, handled separately)
    if (!selectedRig_.empty()) {
        cancelRigGizmo();previewPlaying_=false;
        endGizmoSession(true, "delete");
        const std::string name = selectedRig_;
        if (scene_.gameAssets().removeRig(selectedRig_)) {
            selectedRig_.clear();
            selectedRigBody_ = -1;
            selectedJoint_ = 0;
            checkInvariants("delete_rig");
            setStatus("Rig excluido: " + name);
            return;
        } else {
            setStatus("Nao foi possivel excluir rig", true);
            return;
        }
    }

    Instance* selection = scene_.selection();
    if (selection == nullptr) {
        return;
    }
    if (!selection->deletable()) {
        setStatus("Elemento estrutural protegido: " + selection->name(), true);
        return;
    }
    const std::string name = selection->name();
    endGizmoSession(true, "delete");
    if (scene_.remove(selection)) {
        checkInvariants("delete");
        setStatus("Excluido: " + name);
    } else {
        setStatus("Nao foi possivel excluir " + name, true);
    }
}

void EditorUI::focusSelection() {
    // Enquadrar move a camera; nunca durante um arraste do gizmo.
    if (transform_.active() || rigGizmo_.dragging()) return;
    if (auto* rig = scene_.gameAssets().findRig(selectedRig_)) {
        camera_.focusOn(rig->root.position, 5); return;
    }
    Instance* selection = scene_.selection();
    if (selection == nullptr) {
        return;
    }
    if (!selection->isPart()) {
        camera_.reset();
        return;
    }
    Part* part = static_cast<Part*>(selection);
    const Vector3 size = part->size();
    const float radius = 0.5f * Vector3Length(size);
    camera_.focusOn(part->position(), std::max(0.5f, radius));
}

}  // namespace loblox

#include "ui/Localization.h"
#include "editor/EditorUI.h"
#include "ui/ExperienceUI.h"
#include "editor/StudioTheme.h"
#include "ui/StudioFrontend.h"
#include "game/ImageAssets.h"

#include "core/Diagnostics.h"
#include "core/NumberFormat.h"
#include "editor/GizmoLayout.h"

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

#include "game/Spawn.h"
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

namespace {

// FXAA 3.11-style edge blend. The viewport is an offscreen RenderTexture and
// raylib exposes no multisampled FBO path, so one cheap full-screen pass is
// the practical antialiasing option for the integrated GPU.
const char* kFxaaVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kFxaaFragmentShader = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 resolution;
out vec4 finalColor;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / resolution;
    vec3 rgbM  = texture(texture0, uv).rgb;
    vec3 rgbNW = texture(texture0, uv + vec2(-1.0, -1.0) * texel).rgb;
    vec3 rgbNE = texture(texture0, uv + vec2( 1.0, -1.0) * texel).rgb;
    vec3 rgbSW = texture(texture0, uv + vec2(-1.0,  1.0) * texel).rgb;
    vec3 rgbSE = texture(texture0, uv + vec2( 1.0,  1.0) * texel).rgb;

    float lumaM  = luma(rgbM);
    float lumaNW = luma(rgbNW);
    float lumaNE = luma(rgbNE);
    float lumaSW = luma(rgbSW);
    float lumaSE = luma(rgbSE);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(0.0312, lumaMax * 0.125)) {
        finalColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)),
                     ((lumaNW + lumaSW) - (lumaNE + lumaSE)));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.0625, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texel;

    vec3 rgbA = 0.5 * (texture(texture0, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(texture0, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 *
        (texture(texture0, uv - dir * 0.5).rgb +
         texture(texture0, uv + dir * 0.5).rgb);
    float lumaB = luma(rgbB);
    finalColor = vec4((lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB, 1.0);
}
)";

}  // namespace

// Marca do projeto: carregada uma unica vez, a partir da raiz (caminhos
// relativos; sem caminho absoluto da maquina). Falha permanece !ok e a UI
// usa o fallback vetorial existente.
const LinbluxLogo& LoadLinbluxLogo() {
    static LinbluxLogo logo;
    static const std::string path=game::BuiltinAssetPath("logo.png");
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
    frontend_.reset();
    stopPlay();
    cancelRigGizmo();
    camera_.releaseCapture();
    endGizmoSession(false);
    unloadViewport();
}

bool EditorUI::enableModernFrontend(std::string& error) {
    if(frontend_){error.clear();return true;}
    auto candidate=std::make_unique<StudioFrontend>(*this);
    if(!candidate->initialize(error))return false;
    // Modern splits have their own preference file. Do not overwrite the
    // legacy docking layout with the embedded components' fixed positions.
    ImGui::GetIO().IniFilename=nullptr;
    frontend_=std::move(candidate);return true;
}
void EditorUI::renderModernFrontend(){if(frontend_)frontend_->render();}

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
    editStartValid_ = false;
    clearTransformReadout();
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

void EditorUI::clearTransformReadout() {
    readout_ = TransformReadout{};
}

void EditorUI::beginTransformReadout(Part* part, ToolMode mode) {
    if (part == nullptr || !gizmo_.dragging()) {
        clearTransformReadout();
        return;
    }
    transformmetrics::Context context;
    context.space = gizmo_.localSpace() ? transformmetrics::Space::Local
                                        : transformmetrics::Space::World;
    // Snapshot da Part, nao do Transform cru: Sphere mede o diametro que o
    // proprio gizmo desenha e redimensiona.
    context.start = gizmolayout::MakeSnapshot(*part);
    const gizmomath::HandleId handle = gizmo_.activeHandle();
    const bool planeHandle = handle == gizmomath::HandleId::XYPlane ||
                             handle == gizmomath::HandleId::XZPlane ||
                             handle == gizmomath::HandleId::YZPlane;
    if (mode == ToolMode::Move) {
        context.side = gizmo_.activeHandleSide() < 0 ? -1 : 1;
        context.kind = planeHandle ? transformmetrics::Kind::MovePlane
                                   : transformmetrics::Kind::Move;
        if (planeHandle) {
            context.axis = handle == gizmomath::HandleId::XYPlane ? 0
                         : handle == gizmomath::HandleId::XZPlane ? 1 : 2;
        } else {
            context.axis = gizmo_.activeAxis();
        }
    } else if (mode == ToolMode::Scale) {
        context.space=transformmetrics::Space::Local;
        context.kind = transformmetrics::Kind::Size;
        context.axis = gizmo_.activeAxis();
        context.side = gizmo_.activeHandleSide() < 0 ? -1 : 1;
        context.centered = gizmo_.centeredScale();
    } else if (mode == ToolMode::Rotate) {
        context.kind = transformmetrics::Kind::Angle;
        context.axis = gizmo_.activeAxis();
    } else {
        clearTransformReadout();
        return;
    }
    if (!context.valid()) {
        clearTransformReadout();
        return;
    }
    readout_.partId = part->id();
    readout_.context = context;
    readout_.visible = true;
    readout_.editing = false;
    readout_.focusPending = false;
    readout_.expiresAt = 0.0;
    updateTransformReadout(part);
}

void EditorUI::updateTransformReadout(Part* part) {
    if (!readout_.visible || part == nullptr || part->id() != readout_.partId) return;
    readout_.measurement = transformmetrics::Measure(readout_.context,
                                                     gizmolayout::MakeSnapshot(*part));
    if (!readout_.measurement.valid) readout_.visible = false;
}

void EditorUI::applyReadoutValue(float value) {
    Part* part = scene_.findPart(readout_.partId);
    if (part == nullptr || !readout_.context.valid()) {
        setStatus("A peca desta medida nao existe mais.", true);
        clearTransformReadout();
        return;
    }
    const Transform before = part->transform();
    gizmomath::TransformSnapshot applied;
    if (part->shape() == PartShape::Sphere &&
        readout_.context.kind == transformmetrics::Kind::Size) {
        // Sphere escala de forma uniforme; a medida digitada e o diametro.
        const float startSize = transformmetrics::Measure(readout_.context,
                                                          readout_.context.start).value;
        applied = gizmomath::ScaleUniformFace(
            readout_.context.start, transformmetrics::AxisVector(readout_.context),
            readout_.context.side, value - startSize, 0.0f);
    } else {
        applied = transformmetrics::Apply(readout_.context, value);
    }
    part->setTransform(Transform{applied.position, applied.rotation, applied.size});
    transform_.commit(part, "numeric_edit");
    recordTransformEdit(part->id(), before, part->transform());
    readout_.editing = false;
    readout_.expiresAt = ImGui::GetTime() + 2.5;
    updateTransformReadout(part);
    char label[64];
    if (transformmetrics::Format(readout_.measurement, label, sizeof(label))) {
        char message[96];
        std::snprintf(message, sizeof(message), "%.60s", label);
        setStatus(message);
    }
    checkInvariants("numeric_edit");
}

void EditorUI::recordTransformEdit(InstanceId id, const Transform& before,
                                   const Transform& after) {
    history_.record(id, before, after);
}

void EditorUI::undoTransform() {
    if (transform_.active()) endGizmoSession(true, "undo");
    cancelRigGizmo();
    clearTransformReadout();
    TransformEdit edit;
    if (!history_.undo(edit)) {
        setStatus("Nada para desfazer.", true);
        return;
    }
    Part* part = scene_.findPart(edit.id);
    if (part == nullptr) {
        setStatus("A peca desta operacao nao existe mais.", true);
        history_.clear();
        return;
    }
    part->setTransform(edit.before);
    physics_.applyChanges(part);
    scene_.select(part);
    checkInvariants("undo");
    setStatus("Transformacao desfeita (Ctrl+Z).");
}

void EditorUI::redoTransform() {
    if (transform_.active()) endGizmoSession(true, "redo");
    cancelRigGizmo();
    clearTransformReadout();
    TransformEdit edit;
    if (!history_.redo(edit)) {
        setStatus("Nada para refazer.", true);
        return;
    }
    Part* part = scene_.findPart(edit.id);
    if (part == nullptr) {
        setStatus("A peca desta operacao nao existe mais.", true);
        history_.clear();
        return;
    }
    part->setTransform(edit.after);
    physics_.applyChanges(part);
    scene_.select(part);
    checkInvariants("redo");
    setStatus("Transformacao refeita (Ctrl+Y).");
}

void EditorUI::drawTransformReadout(const EditorInput& input, Vector2 imagePosition) {
    if (!readout_.visible) return;
    Part* part = scene_.findPart(readout_.partId);
    if (part == nullptr) {
        clearTransformReadout();
        return;
    }
    if (!readout_.editing && readout_.expiresAt > 0.0 && ImGui::GetTime() > readout_.expiresAt) {
        clearTransformReadout();
        return;
    }
    char label[64];
    if (!transformmetrics::Format(readout_.measurement, label, sizeof(label))) {
        clearTransformReadout();
        return;
    }

    const Rectangle viewportRect{imagePosition.x, imagePosition.y,
                                 static_cast<float>(viewportWidth_),
                                 static_cast<float>(viewportHeight_)};
    const gizmolayout::Layout layout = gizmolayout::Build(
        *part, tool_, camera_, viewportRect,
        readout_.context.space == transformmetrics::Space::Local, gizmo_.visualSettings());
    Vector2 anchor{imagePosition.x + viewportRect.width * 0.5f,
                   imagePosition.y + viewportRect.height * 0.5f};
    if (layout.valid) {
        const int axis = std::clamp(readout_.context.axis, 0, 2);
        switch (readout_.context.kind) {
            case transformmetrics::Kind::Move:
                anchor = layout.moveHandle[static_cast<std::size_t>(axis * 2 + (readout_.context.side > 0))];
                break;
            case transformmetrics::Kind::MovePlane:
                anchor = layout.planeCenter[static_cast<std::size_t>(axis)];
                break;
            case transformmetrics::Kind::Size:
                anchor = layout.scaleHandle[static_cast<std::size_t>(
                    axis * 2 + (readout_.context.side > 0 ? 1 : 0))];
                break;
            case transformmetrics::Kind::Angle:
                anchor = layout.rotateMarker[static_cast<std::size_t>(axis)];
                break;
            case transformmetrics::Kind::None:
                break;
        }
        anchor = Vector2{anchor.x + imagePosition.x + 18.0f,
                         anchor.y + imagePosition.y - 30.0f};
    }
    // Viewports menores que o chip nao podem gerar limites invertidos.
    const float minX = imagePosition.x + 6.0f;
    const float maxX = std::max(minX, imagePosition.x + viewportRect.width - 190.0f);
    const float minY = imagePosition.y + 6.0f;
    const float maxY = std::max(minY, imagePosition.y + viewportRect.height - 34.0f);
    anchor.x = std::clamp(anchor.x, minX, maxX);
    anchor.y = std::clamp(anchor.y, minY, maxY);

    ImGui::PushID("##transform_readout");
    ImGui::SetCursorScreenPos(ImVec2(anchor.x, anchor.y));
    ImGui::BeginGroup();
    if (readout_.editing) {
        if (readout_.focusPending) {
            ImGui::SetKeyboardFocusHere();
            readout_.focusPending = false;
        }
        ImGui::SetNextItemWidth(126.0f);
        const bool submitted = ImGui::InputText(
            i18n::Label("##value"), readout_.text, sizeof(readout_.text),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal |
                ImGuiInputTextFlags_AutoSelectAll);
        const bool deactivated = ImGui::IsItemDeactivated();
        const bool escaped = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        const bool clickedOutside = input.leftPressed &&
            !CheckCollisionPointRec(input.mouse, readoutRect_);
        if (submitted) {
            float value = 0.0f;
            if (numfmt::Parse(readout_.text, value)) {
                applyReadoutValue(value);
            } else {
                setStatus("Valor invalido; nada foi alterado.", true);
                readout_.editing = false;
            }
        } else if (escaped || clickedOutside) {
            readout_.editing = false;
            if (escaped) setStatus("Edicao numerica cancelada.");
        } else if (deactivated) {
            float value = 0.0f;
            if (numfmt::Parse(readout_.text, value)) applyReadoutValue(value);
            else readout_.editing = false;
        }
        readoutRect_ = Rectangle{anchor.x, anchor.y, 126.0f, 26.0f};
    } else {
        // O botao invisivel que cobre toda a viewport e dono do hover do ImGui,
        // entao o chip e desenhado e testado aqui por retangulo. Assim o
        // clique no valor nunca vira selecao de objeto nem movimento de camera.
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 min(anchor.x, anchor.y);
        const ImVec2 max(min.x + textSize.x + 20.0f, min.y + textSize.y + 12.0f);
        // Area de clique um pouco maior que o chip: alvo folgado sem mostrar
        // um retangulo maior do que o texto.
        readoutRect_ = Rectangle{min.x - 3.0f, min.y - 3.0f,
                                 max.x - min.x + 6.0f, max.y - min.y + 6.0f};
        const bool hovered = CheckCollisionPointRec(input.mouse, readoutRect_);
        const bool editable = readout_.measurement.editable();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, IM_COL32(18, 22, 28, 236), 4.0f);
        drawList->AddRect(min, max,
                          hovered ? IM_COL32(255, 195, 42, 255) : IM_COL32(108, 140, 178, 225),
                          4.0f);
        drawList->AddText(ImVec2(min.x + 10.0f, min.y + 6.0f), IM_COL32(232, 240, 248, 255),
                          label);
        if (hovered) {
            const char* hint = editable
                ? "Click to type an exact value. Enter applies from the pose the drag started at; Esc cancels."
                : "Two-axis move: drag only. Numeric editing applies to single-axis moves, size and angle.";
            drawList->AddText(ImVec2(min.x, max.y + 4.0f), IM_COL32(170, 186, 204, 235), hint);
        }
        if (hovered && input.leftPressed && editable) {
            readout_.editing = true;
            readout_.focusPending = true;
            readout_.expiresAt = 0.0;
            numfmt::Format(readout_.text, sizeof(readout_.text), readout_.measurement.value, 4);
        }
    }
    ImGui::EndGroup();
    ImGui::PopID();
}

void EditorUI::setOnSceneLoaded(std::function<void()> callback) {
    onSceneLoaded_ = std::move(callback);
}

void EditorUI::setStatus(std::string message, bool error) {
    status_ = i18n::Text(message);
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
    if (!viewportReady_) return;

    fxaa_ = LoadShaderFromMemory(kFxaaVertexShader, kFxaaFragmentShader);
    fxaaResolutionLoc_ = fxaa_.id != 0 ? GetShaderLocation(fxaa_, "resolution") : -1;
    if (fxaa_.id == 0) {
        TraceLog(LOG_WARNING, "FXAA shader unavailable; viewport uses native sampling");
        return;
    }
    resolvedViewport_ = LoadRenderTexture(width, height);
    resolvedViewportReady_ = IsRenderTextureValid(resolvedViewport_);
    if (!resolvedViewportReady_) {
        UnloadShader(fxaa_);
        fxaa_ = Shader{};
        fxaaResolutionLoc_ = -1;
        TraceLog(LOG_WARNING, "FXAA target unavailable; viewport uses native sampling");
    }
}

void EditorUI::resolveViewport() {
    if (!viewportReady_ || !resolvedViewportReady_ || fxaa_.id == 0) return;
    BeginTextureMode(resolvedViewport_);
    ClearBackground(BLANK);
    BeginShaderMode(fxaa_);
    const float resolution[2] = {static_cast<float>(viewportWidth_),
                                 static_cast<float>(viewportHeight_)};
    SetShaderValue(fxaa_, fxaaResolutionLoc_, resolution, SHADER_UNIFORM_VEC2);
    DrawTextureRec(viewport_.texture,
                   Rectangle{0.0f, 0.0f, static_cast<float>(viewportWidth_),
                             -static_cast<float>(viewportHeight_)},
                   Vector2{0.0f, 0.0f}, WHITE);
    EndShaderMode();
    EndTextureMode();
}

void EditorUI::unloadViewport() {
    if (resolvedViewportReady_) {
        UnloadRenderTexture(resolvedViewport_);
    }
    resolvedViewportReady_ = false;
    if (fxaa_.id != 0) {
        UnloadShader(fxaa_);
        fxaa_ = Shader{};
        fxaaResolutionLoc_ = -1;
    }
    if (viewportReady_) {
        UnloadRenderTexture(viewport_);
    }
    viewportReady_ = false;
    viewportWidth_ = 0;
    viewportHeight_ = 0;
}

void EditorUI::buildDockLayout() {
    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    const ImGuiID dockId = ImGui::GetID("LinbluxDockspace");
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

    ImGui::DockBuilderDockWindow(i18n::Label("##Toolbar"), top);
    ImGui::DockBuilderDockWindow(i18n::Label("Toolbox"), left);
    ImGui::DockBuilderDockWindow(i18n::Label("Viewport"), center);
    ImGui::DockBuilderDockWindow(i18n::Label("Explorer"), rightTop);
    ImGui::DockBuilderDockWindow(i18n::Label("Properties"), right);
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
    if (route.globalKeys && input.undo) undoTransform();
    if (route.globalKeys && input.redo) redoTransform();
    if (route.editorKeys && input.toggleLocalSpace) {
        endGizmoSession(true, "space_toggle");
        cancelRigGizmo();
        gizmoLocalSpace_ = !gizmoLocalSpace_;
        gizmo_.setLocalSpace(gizmoLocalSpace_);
        clearTransformReadout();
        setStatus(gizmoLocalSpace_ ? "Gizmo: Local space" : "Gizmo: World space");
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
    if(experienceUi_&&experienceUi_->handlesSave()) {
        if(!experienceUi_->save(error))return false;
    } else if (!projectio::save(scene_, path, error)) return false;
    animationDirty_ = false;
    for (auto& pair:scriptDrafts_) {
        pair.second.dirty=false;
        pair.second.baseline=pair.second.text;
    }
    if(experienceUi_)experienceUi_->documentOpened();
    return true;
}

bool EditorUI::saveCurrentProject() {
    std::string error;
    if (saveProjectAt(projectPath_, error)) {
        setStatus(experienceUi_&&experienceUi_->editingEngineDefault()?"Rascunho salvo. Use Salvar como padrão da engine para publicar as alterações.":"Projeto salvo em " + projectPath_);
        return true;
    } else {
        setStatus("Falha ao salvar '" + projectPath_ + "': " + error, true);
        return false;
    }
}

void EditorUI::draw() {
    // Runtime preparation owns its fields on a worker until the future is
    // joined. Home/loading has exclusive input and never reads that runtime.
    if(frontend_&&experienceUi_&&experienceUi_->overlayActive()){frontend_->draw();return;}
    healthHud_={};
    diag::setFrame(++frame_);
    explorerFocused_=false;
    pruneScriptDocuments();
    if (!runtime_.active() && !selectedRig_.empty() && !scene_.gameAssets().findRig(selectedRig_)) {
        cancelRigGizmo(false); selectedRig_.clear(); animationSource_.clear(); previewPlaying_ = false;
    }
    if (!runtime_.active() && scene_.selection()) {
        cancelRigGizmo(); selectedRig_.clear();
    }
    if(frontend_) {frontend_->draw();return;}
    drawMainMenuBar();
    if (!layoutBuilt_) {
        buildDockLayout();
    }
    ImGui::DockSpaceOverViewport(ImGui::GetID("LinbluxDockspace"), ImGui::GetMainViewport());

    ImGui::BeginDisabled(runtime_.active());
    drawToolbar();
    const auto visibleInPlay=[&](const char* name) {
        const auto* window=ImGui::FindWindowByName(i18n::Label(name));
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

bool EditorUI::playing() const {return (experienceUi_&&experienceUi_->loading())||runtime_.active();}
bool EditorUI::experienceOverlay() const {return experienceUi_&&experienceUi_->overlayActive();}

void EditorUI::drawMainMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    const auto& headerLogo=LoadLinbluxLogo();
    if(headerLogo.ok)ImGui::Image(static_cast<ImTextureID>(headerLogo.id),{18.f*headerLogo.width/headerLogo.height,18});
    else ImGui::TextUnformatted(i18n::Label("L"));
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextUnformatted(i18n::Label("LINBLUX"));
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::BeginDisabled(runtime_.active());
    if (ImGui::BeginMenu(i18n::Label("File"))) {
        if (ImGui::MenuItem(i18n::Label("Save Project"), "Ctrl+S")) {
            saveCurrentProject();
        }
        if (ImGui::MenuItem(i18n::Label("Save Project As..."))) {
            endGizmoSession(true);
            requestSavePopup_ = true;
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
            dialogError_.clear();
        }
        if (ImGui::MenuItem(i18n::Label("Open Project..."), "Ctrl+O")) {
            endGizmoSession(true);
            requestOpenPopup_ = true;
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
            dialogError_.clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(i18n::Label("Export Scene as Lua..."))) {
            endGizmoSession(true);
            requestLuaExportPopup_ = true;
            std::string luaPath = projectPath_;
            const std::size_t extension = luaPath.find_last_of('.');
            if (extension != std::string::npos) luaPath.resize(extension);
            luaPath += ".lua";
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", luaPath.c_str());
            dialogError_.clear();
        }
        if (ImGui::MenuItem(i18n::Label("Import Scene from Lua..."))) {
            endGizmoSession(true);
            requestLuaImportPopup_ = true;
            std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", "projects/scene.lua");
            dialogError_.clear();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(i18n::Label("Home"))) {
        if (ImGui::MenuItem(i18n::Label("Select"), "1")) setTool(ToolMode::Select, "menu");
        if (ImGui::MenuItem(i18n::Label("Move"), "2")) setTool(ToolMode::Move, "menu");
        if (ImGui::MenuItem(i18n::Label("Scale"), "3")) setTool(ToolMode::Scale, "menu");
        if (ImGui::MenuItem(i18n::Label("Rotate"), "4")) setTool(ToolMode::Rotate, "menu");
        if (ImGui::MenuItem(i18n::Label("Insert Block"))) insertPart(PartShape::Block);
        if (ImGui::MenuItem(i18n::Label("Insert Sphere"))) insertPart(PartShape::Sphere);
        if (ImGui::MenuItem(i18n::Label("Insert Spawn"))) insertSpawnLocation();
        if (ImGui::MenuItem(i18n::Label("Insert Folder"))) insertContainer(InstanceClass::Folder);
        if (ImGui::MenuItem(i18n::Label("Insert Model"))) insertContainer(InstanceClass::Model);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(i18n::Label("Model"))) {
        if (ImGui::MenuItem(i18n::Label("Anchor"), nullptr, false, scene_.selection() && scene_.selection()->isPart())) {
            auto* part = static_cast<Part*>(scene_.selection());
            part->setAnchored(!part->anchored());
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(i18n::Label("View"))) {
        ImGui::MenuItem(i18n::Label("Toolbox"), nullptr, &showToolbox_);
        ImGui::MenuItem(i18n::Label("Explorer"), nullptr, &showExplorer_);
        ImGui::MenuItem(i18n::Label("Properties"), nullptr, &showProperties_);
        ImGui::MenuItem(i18n::Label("Script Editor"), nullptr, &showScriptEditor_);
        ImGui::MenuItem(i18n::Label("Output"), nullptr, &showOutput_);
        ImGui::MenuItem(i18n::Label("Rig Editor"), nullptr, &showRigEditor_);
        ImGui::MenuItem(i18n::Label("Animation Editor"), nullptr, &showAnimationEditor_);
        if (ImGui::MenuItem(i18n::Label("Reset Layout"))) {
            ImGui::DockBuilderRemoveNode(ImGui::GetID("LinbluxDockspace"));
            layoutBuilt_ = false; showExplorer_ = showProperties_ = showToolbox_ = true;
        }
        bool light=studio::Light();
        if (ImGui::MenuItem(i18n::Label("Light theme"),nullptr,&light)) studio::Apply(light);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(i18n::Label("Script"))) {
        if (ImGui::MenuItem(i18n::Label("New Script"))) insertScript();
        if (ImGui::MenuItem(i18n::Label("New LocalScript"))) insertScript(InstanceClass::LocalScript);
        if (ImGui::MenuItem(i18n::Label("New ModuleScript"))) insertScript(InstanceClass::ModuleScript);
        if (ImGui::MenuItem(i18n::Label("Install default game scripts"))) scene_.installDefaultScripts();
        ImGui::MenuItem(i18n::Label("Script Editor"),nullptr,&showScriptEditor_);
        ImGui::MenuItem(i18n::Label("Output"),nullptr,&showOutput_);
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();
    drawGameToolbar();
    const std::string projectName=std::filesystem::path(projectPath_).filename().string();
    const float projectX=ImGui::GetWindowWidth()-ImGui::CalcTextSize(projectName.c_str()).x-16;
    if(projectX>ImGui::GetCursorPosX()+20) {
        ImGui::SameLine(projectX);ImGui::TextDisabled(i18n::Label("%s"),projectName.c_str());
        if(ImGui::IsItemHovered()) ImGui::SetTooltip(i18n::Label("%s"),projectPath_.c_str());
    }
    ImGui::EndMainMenuBar();
}

void EditorUI::drawToolbar() {
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 6.0f));
    ImGui::Begin(i18n::Label("##Toolbar"), nullptr, flags);
    const LinbluxLogo& logo = LoadLinbluxLogo();
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
    ImGui::TextUnformatted(i18n::Label("LINBLUX"));
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 14.0f);
    if(ImGui::Checkbox(i18n::Label("Snap"),&snapEnabled_)) {endGizmoSession(true,"snap_toggle");cancelRigGizmo();}
    const auto snapMin=ImGui::GetItemRectMin(),snapMax=ImGui::GetItemRectMax();
    snapControl_={snapMin.x,snapMin.y,snapMax.x-snapMin.x,snapMax.y-snapMin.y};
    if(ImGui::IsItemHovered())ImGui::SetTooltip(i18n::Label("OFF: continuous fine movement. ON: increments from the drag's initial transform."));
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

    if (ImGui::BeginTabItem(i18n::Label("Home"))) {
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
        ImGui::SameLine();
        // Espaco dos eixos explicito na ribbon: World | Local (Ctrl+L).
        for (int spaceIndex = 0; spaceIndex < 2; ++spaceIndex) {
            const bool active = (spaceIndex == 1) == gizmoLocalSpace_;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, studio::Colors().accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, studio::Colors().hover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, studio::Colors().selected);
            }
            if (ImGui::Button(spaceIndex == 0 ? "World" : "Local", ImVec2(52.0f, 0.0f))) {
                if (!active) {
                    endGizmoSession(true, "space_toggle");
                    cancelRigGizmo();
                    gizmoLocalSpace_ = spaceIndex == 1;
                    gizmo_.setLocalSpace(gizmoLocalSpace_);
                    clearTransformReadout();
                }
            }
            if (active) ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(gizmoLocalSpace_
                    ? "Local: eixos da peca. Ctrl+L alterna."
                    : "World: eixos globais. Ctrl+L alterna.");
            }
            if (spaceIndex == 0) ImGui::SameLine();
        }
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
            if (homePart == nullptr) ImGui::TextDisabled(i18n::Label("Selecione uma peça primeiro."));
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
            if (homePart == nullptr) ImGui::TextDisabled(i18n::Label("Selecione uma peça primeiro."));
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

    if (ImGui::BeginTabItem(i18n::Label("Model"))) {
        ImGui::Spacing();
        if (ImGui::Checkbox(i18n::Label("Local axes"), &gizmoLocalSpace_)) {
            endGizmoSession(true);
            cancelRigGizmo();
            gizmo_.setLocalSpace(gizmoLocalSpace_);
            clearTransformReadout();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(74.0f);
        ImGui::DragFloat(i18n::Label("Move snap"), &moveSnap_, 0.1f, 0.0f, 128.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(74.0f);
        ImGui::DragFloat(i18n::Label("Scale snap"), &scaleSnap_, 0.05f, 0.0f, 128.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(74.0f);
        ImGui::DragFloat(i18n::Label("Rotate snap"), &rotateSnapDegrees_, 1.0f, 0.0f, 180.0f, "%.1f°");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(84.0f);
        if (ImGui::DragFloat(i18n::Label("Camera speed"), &cameraMoveSpeed_, 0.5f, 1.0f, 200.0f, "%.0f"))
            camera_.setNavigationSettings(cameraMoveSpeed_, cameraLookSensitivity_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(88.0f);
        if (ImGui::DragFloat(i18n::Label("Look sensitivity"), &cameraLookSensitivity_, 0.0001f,
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
            if (part == nullptr) ImGui::TextDisabled(i18n::Label("Selecione uma peca primeiro."));
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
            if (part == nullptr) ImGui::TextDisabled(i18n::Label("Selecione uma peca primeiro."));
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

    if (ImGui::BeginTabItem(i18n::Label("Avatar"))) {
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

    if (ImGui::BeginTabItem(i18n::Label("Script"))) {
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

    if (ImGui::BeginTabItem(i18n::Label("View"))) {
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

    if (ImGui::BeginTabItem(i18n::Label("Game"))) {
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
    ImGui::Begin(i18n::Label("Toolbox"));
    static char search[64] = {};
    ImGui::TextDisabled(i18n::Label("PARTS AND TEMPLATES"));
    ImGui::TextUnformatted(i18n::Label("Build a scene"));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint(i18n::Label("##toolbox_search"), "Search objects...", search, sizeof(search));
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
    ImGui::SeparatorText(i18n::Label("Scene"));
    ImGui::TextDisabled(i18n::Label("%d objects"), static_cast<int>(scene_.objects().size()));
    ImGui::SameLine();
    ImGui::TextDisabled(i18n::Label("%d parts"), static_cast<int>(scene_.parts().size()));
    ImGui::TextDisabled(i18n::Label("%d physics bodies"), static_cast<int>(physics_.bodyCount()));
    ImGui::Spacing();
    if (ImGui::CollapsingHeader(i18n::Label("Navigation shortcuts"), ImGuiTreeNodeFlags_DefaultOpen)) {
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
    EditorInput input = ReadEditorInput();
    if(input.fullscreen)toggleGameFullscreen();
    if(input.maximizeGame)toggleGameMaximized();
    const bool centralScript=frontend_&&activeDocument_!=0&&!runtime_.active();
    if(frontend_) {
        const auto r=frontend_->viewport();
        ImGui::SetNextWindowPos({r.x,r.y},ImGuiCond_Always);ImGui::SetNextWindowSize({r.width,r.height},ImGuiCond_Always);
        ImGui::SetNextWindowDockID(0,ImGuiCond_Always);
    }
    if(centralScript)ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.105f,.115f,.135f,1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin(i18n::Label("Viewport"), nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                 (frontend_?(ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoDocking):0));
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
        if(centralScript)ImGui::PopStyleColor();
        return;
    }

    if(centralScript) {
        camera_.releaseCapture();
        endGizmoSession(true,"script_document");
        cancelRigGizmo();
        drawCentralScriptWorkspace();
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = std::max(8, static_cast<int>(available.x));
    const int height = std::max(8, static_cast<int>(available.y));
    resizeViewport(width, height);

    if (!viewportReady_) {
        endGizmoSession(true);
        camera_.releaseCapture();
        ImGui::TextDisabled(i18n::Label("Viewport indisponivel."));
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
    const unsigned int presentedTexture =
        resolvedViewportReady_ ? resolvedViewport_.texture.id : viewport_.texture.id;
    ImGui::GetWindowDrawList()->AddImage(
        static_cast<ImTextureID>(static_cast<intptr_t>(presentedTexture)), imagePosition,
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
    // O chip de medida tem prioridade sobre camera e selecao: e um campo de
    // texto modal enquanto edita e um alvo de clique quando so mostra o valor
    // do ultimo arraste.
    // Nao usar context.widget aqui: o chip bloqueia ponteiro (camera, gizmo,
    // selecao), mas os atalhos de teclado continuam valendo, como Ctrl+Z.
    const bool readoutHovered = readout_.visible && !readout_.editing &&
        CheckCollisionPointRec(input.mouse, readoutRect_);
    context.text = io.WantTextInput && context.widget;
    if (readout_.editing) context.text = true;
    context.keyboardCaptured = io.WantCaptureKeyboard;
    if (explorerFocused_ && !context.text && !context.widget && !context.popup)
        context.keyboardCaptured = false;
    context.transforming = transform_.active();
    context.navigating = camera_.cursorCaptured();
    if(frontend_)frontend_->filter(input,context);
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
    gizmoInput.altDown = input.alt;

    // Prioridade unica: o gizmo processa a entrada antes da camera e da
    // selecao. O estado atual (nao o do frame anterior) decide se a camera
    // pode navegar; um clique em handle nunca chega ao raycast de selecao.
    const bool viewportOwns = route.owner == InputOwner::Viewport;
    const bool sessionOwns = route.owner == InputOwner::Transform;
    const auto previousHover = gizmo_.hoveredHandle();
    const GizmoUpdateResult gizmoResult = transform_.update(
        tool_, selectedPart, camera_, viewportRect, hovered,
        (viewportOwns || sessionOwns) && !rigConsumed && !readout_.editing && !readoutHovered,
        gizmoInput);
    if (gizmoResult.started && selectedPart != nullptr) {
        // A transformacao real do inicio (nao o snapshot de desenho) e o que
        // o Undo precisa restaurar.
        editStart_ = gizmo_.startTransform();
        editStartValid_ = true;
        beginTransformReadout(selectedPart, tool_);
    } else if (gizmoResult.ended) {
        if (gizmoResult.cancelled) {
            editStartValid_ = false;
            clearTransformReadout();
        } else if (selectedPart != nullptr && readout_.visible &&
                   readout_.partId == gizmoResult.partId) {
            updateTransformReadout(selectedPart);
            readout_.expiresAt = ImGui::GetTime() + 3.0;
            if (editStartValid_) {
                recordTransformEdit(selectedPart->id(), editStart_, selectedPart->transform());
                editStartValid_ = false;
            }
        }
    } else if (transform_.active() && selectedPart != nullptr) {
        updateTransformReadout(selectedPart);
    }
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
        !transform_.active() && !gizmoResult.consumed && !rigConsumed && !readoutHovered &&
            (cameraOwns || (route.editorKeys && !sessionOwns)),
        viewportOwns && !gizmoResult.consumed && !rigConsumed && !readoutHovered, GetFrameTime());

    if (viewportOwns && !gizmoResult.consumed && !rigConsumed && !readoutHovered &&
        input.leftPressed) {
        const Vector2 mouse = input.mouse;
        if (mouse.x >= viewportRect.x && mouse.y >= viewportRect.y &&
            mouse.x <= viewportRect.x + viewportRect.width &&
            mouse.y <= viewportRect.y + viewportRect.height) {
            const Ray ray = camera_.rayFromScreen(
                Vector2{mouse.x - viewportRect.x, mouse.y - viewportRect.y},
                Vector2{viewportRect.width, viewportRect.height});
            Part* hit = nullptr;
            Vector3 point{};
            hit=scene_.pickPart(ray,5000.f,&point);const bool partHit=hit!=nullptr;
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
    if (readout_.visible && !readout_.editing &&
        (selectedPart == nullptr || selectedPart->id() != readout_.partId)) {
        clearTransformReadout();
    }
    traceDispatch(gizmoResult.consumed ? "gizmo" :
        viewportOwns && input.leftPressed ? "selection" : InputOwnerName(route.owner),
        gizmoResult.started ? "drag_begin" : gizmoResult.ended ?
        (gizmoResult.cancelled ? "drag_cancel" : "drag_commit") : "routed");

    BeginTextureMode(viewport_);
    RigRenderState rigState;
    if (auto* rig = scene_.gameAssets().findRig(selectedRig_)) {
        rigState = {rig, previewPose_, rig->root, selectedRigBody_<0?-2:selectedRigBody_, showJointPivots_&&selectedRigBody_>=0};
    }
    renderer_.drawScene(scene_, camera_, viewportWidth_, viewportHeight_,
                        selection != nullptr ? selection->id() : kInvalidInstanceId,
                        rigState.rig ? &rigState : nullptr);
    if(showCollisionShapes_) {
        physics_.syncScene(scene_);
        renderer_.drawCollisionShapes(scene_,physics_,camera_,selection?selection->id():0);
    }
    if (rigState.rig && (showJointPivots_||selectedRigBody_<0) && tool_!=ToolMode::Select &&
        (tool_!=ToolMode::Scale || rigContext_==RigContext::Edit))
        rigGizmo_.drawOverlay(jointProxy_, tool_, camera_, viewportRect);
    if (selectedPart != nullptr && tool_ != ToolMode::Select) {
        gizmo_.drawOverlay(*selectedPart, tool_, camera_, viewportRect);
    }
    EndTextureMode();
    resolveViewport();

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

    drawTransformReadout(input, Vector2{imagePosition.x, imagePosition.y});
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
        ImGui::SetDragDropPayload("LINBLUX_INSTANCE",&id,sizeof(id));
        ImGui::Text(i18n::Label("%s (%s)"),instance->name().c_str(),instance->className());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload = ImGui::AcceptDragDropPayload("LINBLUX_INSTANCE")) {
            endGizmoSession(true,"explorer_reparent");
            pendingReparent_ = {*static_cast<const InstanceId*>(payload->Data),instance->id()};
        }
        ImGui::EndDragDropTarget();
    }
    char explorerPopupId[64];
        std::snprintf(explorerPopupId, sizeof(explorerPopupId), "##explorer_context_%" PRIu64, instance->id());
        if (ImGui::BeginPopupContextItem(explorerPopupId)) {
        ImGui::TextDisabled(i18n::Label("%s"), instance->className());
        ImGui::Separator();
        if (IsScriptClass(instance->instanceClass()) && ImGui::MenuItem(i18n::Label("Open Script"))) openScript(instance->id());
        if (ImGui::MenuItem(i18n::Label("Rename"), "F2")) { scene_.select(instance); renameSelection(); }
        if (ImGui::MenuItem(i18n::Label("Duplicate"), "Ctrl+D",false,instance->deletable())) { scene_.select(instance); pendingDuplicate_=true; }
        if (ImGui::MenuItem(i18n::Label("Copy"), "Ctrl+C",false,instance->deletable())) clipboardId_ = instance->id();
        if (ImGui::BeginMenu(i18n::Label("Insert child (+)"), !IsScriptClass(instance->instanceClass()) && instance != scene_.lighting())) {
            if (ImGui::MenuItem(i18n::Label("Script"))) { scene_.select(instance); pendingInsert_=4; }
            if (ImGui::MenuItem(i18n::Label("ModuleScript"))) { scene_.select(instance); pendingInsert_=5; }
            if (ImGui::MenuItem(i18n::Label("Folder"))) { scene_.select(instance); pendingInsert_=1; }
            if (ImGui::MenuItem(i18n::Label("Model"))) { scene_.select(instance); pendingInsert_=2; }
            if (ImGui::MenuItem(i18n::Label("Part"))) { scene_.select(instance); pendingInsert_=3; }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(i18n::Label("Focar na viewport"), "F")) {
            endGizmoSession(true);
            scene_.select(instance);
            focusSelection();
        }
        if (ImGui::MenuItem(i18n::Label("Excluir"), "Del", false, instance->deletable())) {
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
    ImGui::Begin(i18n::Label("Explorer"), &showExplorer_);
    explorerFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (ImGui::Button(i18n::Label("+ Folder"))) insertContainer(InstanceClass::Folder);
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("+ Model"))) insertContainer(InstanceClass::Model);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint(i18n::Label("##explorer_filter"), "Search name or class", explorerFilter_,
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
            ImGui::TextDisabled(i18n::Label("%s"), "Rig");
            ImGui::Separator();
            if (ImGui::MenuItem(i18n::Label("Rename"), "F2")) { selectRig(rig.id); renameSelection(); }
            if (ImGui::MenuItem(i18n::Label("Duplicate"), "Ctrl+D")) { selectRig(rig.id); pendingDuplicate_=true; }
            if (ImGui::MenuItem(i18n::Label("Delete"), "Del")) { selectRig(rig.id); pendingDeleteRigId_ = rig.id; }
            ImGui::EndPopup();
        }
        
        if (open) {
            ImGui::TextDisabled(i18n::Label("HumanoidRootPart (internal)"));
            for (int b = 0; b < game::kBodyCount; ++b)
                if (ImGui::Selectable(rig.bodies[b].name.c_str(), rig.id == selectedRig_ && selectedRigBody_ == b)) selectRig(rig.id, b);
            ImGui::TextDisabled(i18n::Label("CharacterController / Animator"));
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
        ImGui::TextDisabled(i18n::Label("Selecionado: %s"), scene_.selection()->name().c_str());
    } else {
        ImGui::TextDisabled(i18n::Label("Nenhum objeto selecionado"));
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::drawPartProperties(Part& part) {
    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(part.instanceClass()));
    ImGui::Text(i18n::Label("%s"), part.className());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled(i18n::Label("#%llu"), static_cast<unsigned long long>(part.id()));
    ImGui::Separator();

    char name[128];
    std::snprintf(name, sizeof(name), "%s", part.name().c_str());
    if (ImGui::InputText(i18n::Label("Name"), name, sizeof(name))) {
        part.setName(name);
    }

    ImGui::BeginDisabled();
    char className[64];
    std::snprintf(className, sizeof(className), "%s", part.className());
    ImGui::InputText(i18n::Label("ClassName"), className, sizeof(className));
    ImGui::EndDisabled();

    ImGui::SeparatorText(i18n::Label("Transform"));
    {
        Vector3 position = part.position();
        float values[3] = {position.x, position.y, position.z};
        if (ImGui::DragFloat3(i18n::Label("Position"), values, 0.1f, 0.0f, 0.0f, "%.3f")) {
            part.setPosition(Vector3{values[0], values[1], values[2]});
        }
    }
    {
        Vector3 euler = part.rotationEulerDegrees();
        float values[3] = {euler.x, euler.y, euler.z};
        if (ImGui::DragFloat3(i18n::Label("Rotation"), values, 0.5f, -360.0f, 360.0f, "%.1f")) {
            part.setRotationEulerDegrees(Vector3{values[0], values[1], values[2]});
        }
    }
    {
        Vector3 size = part.size();
        float values[3] = {size.x, size.y, size.z};
        if (ImGui::DragFloat3(i18n::Label("Size"), values, 0.05f, kMinPartSize, 512.0f, "%.2f")) {
            part.setSize(Vector3{values[0], values[1], values[2]});
        }
    }
    if (!part.anchored()) {
        ImGui::TextDisabled(i18n::Label("Peca dinamica: a fisica controla o movimento."));
    }

    ImGui::SeparatorText(i18n::Label("Appearance"));
    {
        float color[4] = {part.color().r / 255.0f, part.color().g / 255.0f,
                          part.color().b / 255.0f, part.color().a / 255.0f};
        if (ImGui::ColorEdit4(i18n::Label("Color"), color)) {
            part.setColor(Color{
                static_cast<unsigned char>(std::lround(color[0] * 255.0f)),
                static_cast<unsigned char>(std::lround(color[1] * 255.0f)),
                static_cast<unsigned char>(std::lround(color[2] * 255.0f)),
                static_cast<unsigned char>(std::lround(color[3] * 255.0f))});
        }
    }
    if (ImGui::BeginCombo(i18n::Label("Material"), PartMaterialName(part.material()))) {
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
    ImGui::InputText(i18n::Label("Shape"), shape, sizeof(shape));
    ImGui::EndDisabled();

    ImGui::SeparatorText(i18n::Label("Spawn"));
    const bool legacySpawn=part.instanceClass()==InstanceClass::SpawnLocation;
    bool spawnEnabled=legacySpawn||game::IsSpawnPart(part);
    if(legacySpawn) {
        ImGui::TextDisabled(i18n::Label("Legacy SpawnLocation (always a spawn point)"));
    } else if(ImGui::Checkbox(i18n::Label("Spawn point"),&spawnEnabled)) {
        if(spawnEnabled)part.setAttribute("spawn",true);
        else part.removeAttribute("spawn");
    }
    if(spawnEnabled) {
        float priority=static_cast<float>(game::SpawnPriority(part));
        if(ImGui::DragFloat(i18n::Label("Spawn priority"),&priority,0.1f,-1000000.0f,1000000.0f,"%.2f"))
            part.setAttribute("spawn_priority",static_cast<double>(priority));
        ImGui::TextDisabled(i18n::Label("Highest priority wins; ties use the oldest instance."));
    }

    ImGui::SeparatorText(i18n::Label("Physics"));
    bool anchored = part.anchored();
    if (ImGui::Checkbox(i18n::Label("Anchored"), &anchored)) {
        part.setAnchored(anchored);
    }
    bool canCollide = part.canCollide();
    if (ImGui::Checkbox(i18n::Label("CanCollide"), &canCollide)) {
        part.setCanCollide(canCollide);
    }

    ImGui::Separator();
    if (ImGui::Button(i18n::Label("Focar (F)"), ImVec2(-FLT_MIN, 0.0f))) {
        focusSelection();
    }
    ImGui::BeginDisabled(!part.deletable());
    if (ImGui::Button(i18n::Label("Excluir"), ImVec2(-FLT_MIN, 0.0f))) {
        deleteSelection();
    }
    ImGui::EndDisabled();
    if (!part.deletable()) {
        ImGui::TextDisabled(i18n::Label("Elemento estrutural protegido."));
    }
}

void EditorUI::drawLightingProperties() {
    Lighting* lighting = scene_.lighting();
    if (lighting == nullptr) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(InstanceClass::Lighting));
    ImGui::TextUnformatted(i18n::Label("Lighting"));
    ImGui::PopStyleColor();
    ImGui::Separator();

    char name[128];
    std::snprintf(name, sizeof(name), "%s", lighting->name().c_str());
    if (ImGui::InputText(i18n::Label("Name"), name, sizeof(name))) {
        lighting->setName(name);
    }

    float sky[3] = {lighting->skyColor().r / 255.0f, lighting->skyColor().g / 255.0f,
                    lighting->skyColor().b / 255.0f};
    if (ImGui::ColorEdit3(i18n::Label("SkyColor"), sky)) {
        lighting->setSkyColor(Color{static_cast<unsigned char>(std::lround(sky[0] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sky[1] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sky[2] * 255.0f)),
                                    255});
    }
    float ambient[3] = {lighting->ambientColor().r / 255.0f,
                        lighting->ambientColor().g / 255.0f,
                        lighting->ambientColor().b / 255.0f};
    if (ImGui::ColorEdit3(i18n::Label("Ambient"), ambient)) {
        lighting->setAmbientColor(
            Color{static_cast<unsigned char>(std::lround(ambient[0] * 255.0f)),
                  static_cast<unsigned char>(std::lround(ambient[1] * 255.0f)),
                  static_cast<unsigned char>(std::lround(ambient[2] * 255.0f)), 255});
    }
    Vector3 sun = lighting->sunDirection();
    float sunValues[3] = {sun.x, sun.y, sun.z};
    if (ImGui::DragFloat3(i18n::Label("SunDirection"), sunValues, 0.02f, -1.0f, 1.0f, "%.2f")) {
        lighting->setSunDirection(Vector3{sunValues[0], sunValues[1], sunValues[2]});
    }
    float intensity = lighting->sunIntensity();
    if (ImGui::SliderFloat(i18n::Label("SunIntensity"), &intensity, 0.0f, 3.0f, "%.2f")) {
        lighting->setSunIntensity(intensity);
    }
    float brightness = lighting->brightness();
    if (ImGui::SliderFloat(i18n::Label("Brightness"), &brightness, 0.2f, 2.0f, "%.2f")) {
        lighting->setBrightness(brightness);
    }
    ImGui::SeparatorText(i18n::Label("Fog"));
    float fog[3] = {lighting->fogColor().r / 255.0f, lighting->fogColor().g / 255.0f,
                    lighting->fogColor().b / 255.0f};
    if (ImGui::ColorEdit3(i18n::Label("FogColor"), fog)) {
        lighting->setFogColor(Color{static_cast<unsigned char>(std::lround(fog[0] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(fog[1] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(fog[2] * 255.0f)), 255});
    }
    float fogDensity = lighting->fogDensity();
    if (ImGui::SliderFloat(i18n::Label("FogDensity"), &fogDensity, 0.0f, 1.0f, "%.3f")) {
        lighting->setFogDensity(fogDensity);
    }

    ImGui::SeparatorText(i18n::Label("Sky"));
    bool skyEnabled = lighting->skyEnabled();
    if (ImGui::Checkbox(i18n::Label("Sky Enabled"), &skyEnabled)) {
        lighting->setSkyEnabled(skyEnabled);
        if (!skyEnabled)
            ImGui::SetTooltip(i18n::Label("O fundo volta para a cor sólida do SkyColor"));
    }

    float zenith[3] = {lighting->skyZenithColor().r / 255.0f,
                       lighting->skyZenithColor().g / 255.0f,
                       lighting->skyZenithColor().b / 255.0f};
    if (ImGui::ColorEdit3(i18n::Label("Zenith Color"), zenith)) {
        lighting->setSkyZenithColor(Color{static_cast<unsigned char>(std::lround(zenith[0] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(zenith[1] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(zenith[2] * 255.0f)), 255});
    }

    float horizon[3] = {lighting->skyHorizonColor().r / 255.0f,
                        lighting->skyHorizonColor().g / 255.0f,
                        lighting->skyHorizonColor().b / 255.0f};
    if (ImGui::ColorEdit3(i18n::Label("Horizon Color"), horizon)) {
        lighting->setSkyHorizonColor(Color{static_cast<unsigned char>(std::lround(horizon[0] * 255.0f)),
                                           static_cast<unsigned char>(std::lround(horizon[1] * 255.0f)),
                                           static_cast<unsigned char>(std::lround(horizon[2] * 255.0f)), 255});
    }

    float ground[3] = {lighting->skyGroundColor().r / 255.0f,
                       lighting->skyGroundColor().g / 255.0f,
                       lighting->skyGroundColor().b / 255.0f};
    if (ImGui::ColorEdit3(i18n::Label("Ground Color"), ground)) {
        lighting->setSkyGroundColor(Color{static_cast<unsigned char>(std::lround(ground[0] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(ground[1] * 255.0f)),
                                          static_cast<unsigned char>(std::lround(ground[2] * 255.0f)), 255});
    }

    float sunCol[3] = {lighting->sunColor().r / 255.0f,
                       lighting->sunColor().g / 255.0f,
                       lighting->sunColor().b / 255.0f};
    if (ImGui::ColorEdit3(i18n::Label("Sun Color"), sunCol)) {
        lighting->setSunColor(Color{static_cast<unsigned char>(std::lround(sunCol[0] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sunCol[1] * 255.0f)),
                                    static_cast<unsigned char>(std::lround(sunCol[2] * 255.0f)), 255});
    }

    float sunSize = lighting->sunSize();
    if (ImGui::SliderFloat(i18n::Label("Sun Size"), &sunSize, 0.0f, 1.0f, "%.3f")) {
        lighting->setSunSize(sunSize);
    }

    float atmoStrength = lighting->atmosphereStrength();
    if (ImGui::SliderFloat(i18n::Label("Atmosphere Strength"), &atmoStrength, 0.0f, 1.0f, "%.2f")) {
        lighting->setAtmosphereStrength(atmoStrength);
    }

    ImGui::SeparatorText(i18n::Label("Clouds"));
    float cloudDensity = lighting->cloudDensity();
    if (ImGui::SliderFloat(i18n::Label("Cloud Density"), &cloudDensity, 0.0f, 1.0f, "%.2f")) {
        lighting->setCloudDensity(cloudDensity);
    }

    float cloudScale = lighting->cloudScale();
    if (ImGui::SliderFloat(i18n::Label("Cloud Scale"), &cloudScale, 0.1f, 10.0f, "%.2f")) {
        lighting->setCloudScale(cloudScale);
    }

    float cloudSpeed = lighting->cloudSpeed();
    if (ImGui::SliderFloat(i18n::Label("Cloud Speed"), &cloudSpeed, 0.0f, 5.0f, "%.2f")) {
        lighting->setCloudSpeed(cloudSpeed);
    }

    float cloudSoftness = lighting->cloudSoftness();
    if (ImGui::SliderFloat(i18n::Label("Cloud Softness"), &cloudSoftness, 0.0f, 1.0f, "%.2f")) {
        lighting->setCloudSoftness(cloudSoftness);
    }

    float skyRot = lighting->skyRotation();
    if (ImGui::DragFloat(i18n::Label("Sky Rotation (deg)"), &skyRot, 0.5f, -360.0f, 360.0f, "%.1f")) {
        lighting->setSkyRotation(skyRot);
    }
}

void EditorUI::drawWorkspaceProperties() {
    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(InstanceClass::Workspace));
    ImGui::TextUnformatted(i18n::Label("Workspace"));
    ImGui::PopStyleColor();
    ImGui::TextDisabled(i18n::Label("#%llu"),
                        static_cast<unsigned long long>(scene_.workspace()->id()));
    ImGui::Separator();
    char name[128];
    std::snprintf(name, sizeof(name), "%s", scene_.workspace()->name().c_str());
    if (ImGui::InputText(i18n::Label("Name"), name, sizeof(name))) {
        scene_.workspace()->setName(name);
    }
    float height=scene_.workspace()->fallenPartsDestroyHeight();
    if(ImGui::InputFloat(i18n::Label("FallenPartsDestroyHeight"),&height,10,100,"%.1f",ImGuiInputTextFlags_EnterReturnsTrue) &&
        !scene_.workspace()->setFallenPartsDestroyHeight(height))setStatus("Void height invalido.",true);
    ImGui::SeparatorText(i18n::Label("Projeto"));
    ImGui::Text(i18n::Label("Objetos: %d"), static_cast<int>(scene_.objects().size()));
    ImGui::Text(i18n::Label("Pecas: %d"), static_cast<int>(scene_.parts().size()));
    ImGui::Text(i18n::Label("Corpos fisicos: %d"), static_cast<int>(physics_.bodyCount()));
    ImGui::TextWrapped(i18n::Label("Arquivo: %s"), projectPath_.c_str());
}

void EditorUI::drawProperties() {
    ImGui::SetNextWindowSize({330, 560}, ImGuiCond_FirstUseEver);
    ImGui::Begin(i18n::Label("Properties"));
    Instance* selection = scene_.selection();
    if (selection == nullptr) {
        if (auto* rig = scene_.gameAssets().findRig(selectedRig_)) {
            ImGui::Text(i18n::Label("%s (R6 rig)"), rig->name.c_str());
            ImGui::Text(i18n::Label("Root: %.2f, %.2f, %.2f"), rig->root.position.x, rig->root.position.y, rig->root.position.z);
            if (selectedRigBody_ >= 0) ImGui::Text(i18n::Label("Body: %s"), rig->bodies[selectedRigBody_].name.c_str());
            ImGui::Text(i18n::Label("Joint: %s"), rig->joints[selectedJoint_].name.c_str());
            if (ImGui::Button(i18n::Label("Open Rig Editor"))) showRigEditor_ = true;
            ImGui::End(); return;
        }
        ImGui::Spacing();
        ImGui::TextUnformatted(i18n::Label("Properties"));
        ImGui::TextWrapped(i18n::Label("Select an object in the Explorer or viewport to edit its properties."));
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
        const bool enter=ImGui::InputText(i18n::Label("Name"),renameBuffer_,sizeof(renameBuffer_),ImGuiInputTextFlags_EnterReturnsTrue);
        if (enter || ImGui::Button(i18n::Label("Rename"))) {
            if (auto* node=scene_.find(renameId_); node && renameBuffer_[0]) node->setName(renameBuffer_);
            if(auto* rig=scene_.gameAssets().findRig(renameRigId_);rig && renameBuffer_[0])rig->name=renameBuffer_;
            renameId_=0;renameRigId_.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button(i18n::Label("Cancel")) || ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
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
        ImGui::TextUnformatted(i18n::Label("Caminho do arquivo (.json):"));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText(i18n::Label("##save_path"), pathBuffer_, sizeof(pathBuffer_));
        if (!dialogError_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.40f, 1.0f), "%s", dialogError_.c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button(i18n::Label("Salvar"), ImVec2(120.0f, 0.0f))) {
            std::string error;
            std::error_code pathError;
            const std::filesystem::path chosen = std::filesystem::absolute(pathBuffer_, pathError);
            const std::string savePath = pathError ? std::string(pathBuffer_) : chosen.string();
            if (saveProjectAt(savePath, error)) {
                projectPath_ = savePath;
                dialogError_.clear();
                setStatus(experienceUi_&&experienceUi_->editingEngineDefault()?"Rascunho salvo. Use Salvar como padrão da engine para publicar as alterações.":"Projeto salvo em " + projectPath_);
                ImGui::CloseCurrentPopup();
            } else {
                dialogError_ = "Falha ao salvar '" + savePath + "': " + error;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(i18n::Label("Cancelar"), ImVec2(120.0f, 0.0f))) {
            dialogError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Abrir projeto", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(i18n::Label("Caminho do arquivo (.json):"));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText(i18n::Label("##open_path"), pathBuffer_, sizeof(pathBuffer_));
        ImGui::TextDisabled(i18n::Label("Carregar substitui a cena atual."));
        if (!dialogError_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.40f, 1.0f), "%s", dialogError_.c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button(i18n::Label("Abrir"), ImVec2(120.0f, 0.0f))) {
            std::string error;
            endGizmoSession(true);
            if (projectio::load(scene_, pathBuffer_, error)) {
                scriptDrafts_.clear();openDocuments_.clear();activeScript_=activeDocument_=requestedScript_=0;
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
        if (ImGui::Button(i18n::Label("Cancelar"), ImVec2(120.0f, 0.0f))) {
            dialogError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(540.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Exportar cena Lua", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(i18n::Label("Exportar cena declarativa (.lua):"));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText(i18n::Label("##lua_export_path"), pathBuffer_, sizeof(pathBuffer_));
        ImGui::TextDisabled(i18n::Label("Contem apenas dados da cena; nenhum script e executado."));
        if (!dialogError_.empty()) ImGui::TextColored(ImVec4(0.78f, 0.12f, 0.10f, 1), "%s", dialogError_.c_str());
        if (ImGui::Button(i18n::Label("Exportar"), ImVec2(120, 0))) {
            std::string error;
            if (scenelua::save(scene_, pathBuffer_, error)) {
                setStatus("Cena exportada: " + std::string(pathBuffer_));
                dialogError_.clear();
                ImGui::CloseCurrentPopup();
            } else dialogError_ = error;
        }
        ImGui::SameLine();
        if (ImGui::Button(i18n::Label("Cancelar##lua_export"), ImVec2(120, 0))) {
            dialogError_.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Importar cena Lua", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(i18n::Label("Importar cena declarativa (.lua):"));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText(i18n::Label("##lua_import_path"), pathBuffer_, sizeof(pathBuffer_));
        ImGui::TextWrapped(i18n::Label("Aceita somente `return` seguido por tabelas literais do formato loblox-scene. O arquivo nunca e executado."));
        ImGui::TextDisabled(i18n::Label("Uma importacao valida substitui a cena atual."));
        if (!dialogError_.empty()) ImGui::TextColored(ImVec4(0.78f, 0.12f, 0.10f, 1), "%s", dialogError_.c_str());
        if (ImGui::Button(i18n::Label("Importar"), ImVec2(120, 0))) {
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
        if (ImGui::Button(i18n::Label("Cancelar##lua_import"), ImVec2(120, 0))) {
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
        camera_.focusOn(rig->authoringPivot().position, 5); return;
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

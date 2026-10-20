#include "editor/EditorUI.h"

#include "core/Diagnostics.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
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
    input.save = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false);
    input.open = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false);
    input.duplicate = io.KeyCtrl && !io.KeyShift && !io.KeyAlt && !io.KeySuper &&
        ImGui::IsKeyPressed(ImGuiKey_D, false);
    input.remove = ImGui::IsKeyPressed(ImGuiKey_Delete, false) ||
                   ImGui::IsKeyPressed(ImGuiKey_Backspace, false);
    input.focus = ImGui::IsKeyPressed(ImGuiKey_F, false);
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
    }
}

bool IconButton(const char* id, Icon icon, const char* label, bool active, const char* tooltip,
                bool enabled = true) {
    ImGui::PushID(id);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float width = std::clamp(textSize.x + 18.0f, 54.0f, 78.0f);
    const ImVec2 size(width, 62.0f);
    const ImVec2 position = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##button", size);
    const bool clicked = ImGui::IsItemClicked() && enabled;
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 background = active ? IM_COL32(222, 237, 253, 255)
                              : held ? IM_COL32(210, 229, 250, 255)
                              : hovered ? IM_COL32(235, 240, 246, 255)
                                        : IM_COL32(248, 249, 251, 255);
    drawList->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
                            background, 5.0f);
    if (active) {
        drawList->AddRectFilled(ImVec2(position.x + 7.0f, position.y + size.y - 3.0f),
                                ImVec2(position.x + size.x - 7.0f, position.y + size.y - 1.0f),
                                IM_COL32(25, 112, 205, 255), 1.0f);
    } else if (hovered) {
        drawList->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
                          IM_COL32(190, 201, 214, 255), 5.0f);
    }

    const ImU32 foreground = !enabled ? IM_COL32(160, 166, 174, 255)
                              : active ? IM_COL32(25, 103, 190, 255)
                                       : IM_COL32(50, 57, 66, 255);
    DrawIcon(drawList, ImVec2(position.x + size.x * 0.5f, position.y + 23.0f),
             22.0f, icon, foreground);
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
        IM_COL32(202, 208, 216, 255), 1.0f);
    ImGui::SameLine(0.0f, 7.0f);
}

ImVec4 ClassColor(InstanceClass instanceClass) {
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
    }
    return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
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
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.15f, nullptr, &center);
    const ImGuiID left =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.19f, nullptr, &center);
    ImGuiID right =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
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
    if ((route.editorKeys || route.globalKeys) && input.duplicate) {
        duplicateSelection();
    }
    if (route.editorKeys && input.remove && !input.modified) {
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

void EditorUI::saveCurrentProject() {
    endGizmoSession(false, "save");
    std::string error;
    if (animationDirty_ && !animationSource_.empty()) {
        auto* clip = scene_.gameAssets().findAnimation(animationSource_);
        if (!clip || !animationDraft_.validate(error)) { setStatus("Animacao nao salva: " + error, true); return; }
        *clip = animationDraft_;
    }
    if (projectio::save(scene_, projectPath_, error)) {
        animationDirty_ = false;
        setStatus("Projeto salvo em " + projectPath_);
    } else {
        setStatus("Falha ao salvar '" + projectPath_ + "': " + error, true);
    }
}

void EditorUI::draw() {
    diag::setFrame(++frame_);
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
    if (showToolbox_) {
        drawToolbox();
    }
    if (showExplorer_) {
        drawExplorer();
    }
    if (showProperties_) {
        drawProperties();
    }
    if (!runtime_.active()) {
        if (showRigEditor_) drawRigEditor();
        if (showAnimationEditor_) drawAnimationEditor();
        if (showGameSettings_) drawGameSettings();
    }
    ImGui::EndDisabled();
    drawDialogs();
    drawViewport(); // UI actions and popup requests are known before routing.
    drawPauseMenu();
}

void EditorUI::drawMainMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    ImGui::TextColored(ImVec4(0.10f, 0.43f, 0.78f, 1.0f), "L");
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
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Model")) {
        if (ImGui::MenuItem("Anchor", nullptr, false, scene_.selection() && scene_.selection()->isPart())) {
            auto* part = static_cast<Part*>(scene_.selection());
            if (part->instanceClass() != InstanceClass::Baseplate) part->setAnchored(!part->anchored());
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Toolbox", nullptr, &showToolbox_);
        ImGui::MenuItem("Explorer", nullptr, &showExplorer_);
        ImGui::MenuItem("Properties", nullptr, &showProperties_);
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();
    drawGameToolbar();
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(projectPath_.c_str()).x - 16.0f);
    ImGui::TextDisabled("%s", projectPath_.c_str());
    ImGui::EndMainMenuBar();
}

void EditorUI::drawToolbar() {
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 6.0f));
    ImGui::Begin("##Toolbar", nullptr, flags);
    const ImVec2 brandCursor = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(24.0f, ImGui::GetTextLineHeight()));
    DrawIcon(ImGui::GetWindowDrawList(),
             ImVec2(brandCursor.x + 11.0f, brandCursor.y + ImGui::GetTextLineHeight() * 0.5f),
             15.0f, Icon::Part, IM_COL32(0, 162, 255, 255));
    ImGui::SameLine(0.0f, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 185, 255, 255));
    ImGui::TextUnformatted("LOBLOX");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 14.0f);
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
        const bool canAnchorHome = homePart != nullptr &&
                                   homePart->instanceClass() != InstanceClass::Baseplate;
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
        gizmo_.setSnap(moveSnap_, scaleSnap_, rotateSnapDegrees_);
        ToolbarSeparator();
        Instance* selection = scene_.selection();
        Part* part = selection != nullptr && selection->isPart() ? static_cast<Part*>(selection) : nullptr;
        const bool canToggleAnchor = part != nullptr &&
                                     part->instanceClass() != InstanceClass::Baseplate;
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
        if (ImGui::Button("Insert R6", ImVec2(105, 35))) insertR6();
        ImGui::SameLine();
        if (ImGui::Button("Rig Editor", ImVec2(105, 35))) showRigEditor_ = true;
        ImGui::SameLine();
        if (ImGui::Button("Animation Editor", ImVec2(140, 35))) {
            showAnimationEditor_ = true;
            if (animationSource_.empty()) openAnimation(scene_.gameAssets().player.walkAnimation);
        }
        ImGui::SameLine();
        if (ImGui::Button("Game Settings", ImVec2(120, 35))) showGameSettings_ = true;
        ImGui::SameLine();
        if (ImGui::Button("Play (F5)", ImVec2(105, 35))) startPlay();
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("Viewport", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (!visible) {
        if (runtime_.state() == game::RuntimeState::Playing) runtime_.pause();
        setGameplayCapture(false);
        endGizmoSession(true, "viewport_hidden");
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
    const EditorInput input = ReadEditorInput();
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
    context.transforming = transform_.active();
    context.navigating = camera_.cursorCaptured();
    if (runtime_.active()) {
        drawGameplayViewport(hovered, viewportFocused_, context.popup || context.widget,
                             {imagePosition.x, imagePosition.y});
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
    const auto traceDispatch = [&](const char* consumer, const char* outcome) {
      if (traceInput_) {
        const bool mouseEvent = input.leftPressed || input.rightPressed || input.middlePressed ||
            ImGui::IsMouseReleased(0) || ImGui::IsMouseReleased(1) || ImGui::IsMouseReleased(2);
        const bool keyEvent = input.tool >= 0 || input.save || input.open || input.escape ||
            input.remove || input.focus || input.duplicate;
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
    if (rigState.rig && showJointPivots_ && tool_ == ToolMode::Rotate)
        rigGizmo_.drawOverlay(jointProxy_, ToolMode::Rotate, camera_, viewportRect);
    if (selectedPart != nullptr && tool_ != ToolMode::Select) {
        gizmo_.drawOverlay(*selectedPart, tool_, camera_, viewportRect);
    }
    EndTextureMode();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    char overlay[192];
    std::snprintf(overlay, sizeof(overlay), "%s%s | %.0f FPS | WASD move | RMB look | Q/E vertical",
                  ToolModeName(tool_), viewportFocused_ ? " [focus]" : "", ImGui::GetIO().Framerate);
    const ImVec2 overlaySize = ImGui::CalcTextSize(overlay);
    const ImVec2 overlayMin(imagePosition.x + 10.0f, imagePosition.y + 9.0f);
    drawList->AddRectFilled(overlayMin,
                            ImVec2(overlayMin.x + overlaySize.x + 16.0f,
                                   overlayMin.y + overlaySize.y + 10.0f),
                            IM_COL32(24, 29, 35, 220), 5.0f);
    drawList->AddText(ImVec2(overlayMin.x + 8.0f, overlayMin.y + 5.0f),
                      IM_COL32(230, 236, 242, 245), overlay);

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
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (filter != nullptr && *filter != '\0') {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    if (instance->children().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (scene_.selection() == instance) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(instance->instanceClass()));
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(
                                            static_cast<intptr_t>(instance->id())),
                                        flags, "  %s", instance->name().c_str());
    const ImVec2 rowMin = ImGui::GetItemRectMin();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    const ImVec2 iconCenter(rowMin.x + 22.0f, (rowMin.y + rowMax.y) * 0.5f);
    const ImVec4 classColor = ClassColor(instance->instanceClass());
    const ImU32 iconColor = ImGui::ColorConvertFloat4ToU32(classColor);
    if (instance->instanceClass() == InstanceClass::Lighting) {
        ImGui::GetWindowDrawList()->AddCircle(iconCenter, 5.0f, iconColor, 16, 1.7f);
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(iconCenter.x + 1.5f, iconCenter.y - 1.5f), 1.3f, iconColor, 8);
    } else if (instance->instanceClass() == InstanceClass::Workspace) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(iconCenter.x - 5.0f, iconCenter.y - 5.0f),
            ImVec2(iconCenter.x + 5.0f, iconCenter.y + 5.0f), iconColor, 2.0f);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(iconCenter.x - 2.5f, iconCenter.y),
                                            ImVec2(iconCenter.x + 2.5f, iconCenter.y),
                                            IM_COL32(35, 38, 42, 255), 1.5f);
    } else {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(iconCenter.x - 5.0f, iconCenter.y - 4.0f),
            ImVec2(iconCenter.x + 5.0f, iconCenter.y + 4.0f), iconColor, 1.5f);
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(iconCenter.x - 5.0f, iconCenter.y - 4.0f),
            ImVec2(iconCenter.x + 5.0f, iconCenter.y + 4.0f),
            IM_COL32(230, 235, 242, 190), 1.5f);
    }
    ImGui::PopStyleColor();

    if (ImGui::IsItemClicked()) {
        endGizmoSession(true);
        scene_.select(instance);
    }
    if (ImGui::BeginPopupContextItem("##explorer_context")) {
        ImGui::TextDisabled("%s", instance->className());
        ImGui::Separator();
        if (ImGui::MenuItem("Focar na viewport", "F")) {
            endGizmoSession(true);
            scene_.select(instance);
            focusSelection();
        }
        if (ImGui::MenuItem("Excluir", "Del", false, instance->deletable())) {
            endGizmoSession(true);
            scene_.select(instance);
            deleteSelection();
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
    ImGui::Begin("Explorer");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##explorer_filter", "Filter workspace", explorerFilter_,
                             sizeof(explorerFilter_));
    if (scene_.workspace() != nullptr) {
        drawExplorerNode(scene_.workspace(), explorerFilter_);
    }
    if (scene_.lighting() != nullptr) {
        drawExplorerNode(scene_.lighting(), explorerFilter_);
    }
    for (const auto& rig : scene_.gameAssets().rigs) {
        ImGui::PushID(rig.id.c_str());
        const bool open = ImGui::TreeNodeEx(rig.name.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen | (rig.id == selectedRig_ ? ImGuiTreeNodeFlags_Selected : 0));
        if (ImGui::IsItemClicked()) selectRig(rig.id);
        if (open) {
            ImGui::TextDisabled("HumanoidRootPart (internal)");
            for (int b = 0; b < game::kBodyCount; ++b)
                if (ImGui::Selectable(rig.bodies[b].name.c_str(), rig.id == selectedRig_ && selectedRigBody_ == b)) selectRig(rig.id, b);
            ImGui::TextDisabled("CharacterController / Animator");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    if (scene_.selection() != nullptr) {
        ImGui::TextDisabled("Selecionado: %s", scene_.selection()->name().c_str());
    } else {
        ImGui::TextDisabled("Nenhum objeto selecionado");
    }
    ImGui::End();
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
    const bool structural = part.instanceClass() == InstanceClass::Baseplate;
    ImGui::BeginDisabled(structural);
    bool anchored = part.anchored();
    if (ImGui::Checkbox("Anchored", &anchored)) {
        part.setAnchored(anchored);
    }
    bool canCollide = part.canCollide();
    if (ImGui::Checkbox("CanCollide", &canCollide)) {
        part.setCanCollide(canCollide);
    }
    ImGui::EndDisabled();
    if (structural) {
        ImGui::TextDisabled("Baseplate sempre ancorada e com colisao.");
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
    ImGui::SeparatorText("Projeto");
    ImGui::Text("Objetos: %d", static_cast<int>(scene_.objects().size()));
    ImGui::Text("Pecas: %d", static_cast<int>(scene_.parts().size()));
    ImGui::Text("Corpos fisicos: %d", static_cast<int>(physics_.bodyCount()));
    ImGui::TextWrapped("Arquivo: %s", projectPath_.c_str());
}

void EditorUI::drawProperties() {
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
        ImGui::TextDisabled("Nenhum objeto selecionado.");
        ImGui::Separator();
        ImGui::TextWrapped("Selecione um objeto na viewport ou no Explorer para editar Name, Position, Size, Color e fisica.");
        ImGui::SeparatorText("Projeto");
        ImGui::Text("Objetos: %d", static_cast<int>(scene_.objects().size()));
        ImGui::Text("Pecas: %d", static_cast<int>(scene_.parts().size()));
        ImGui::Text("Arquivo: %s", projectPath_.c_str());
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
    } else if (selection->isPart()) {
        drawPartProperties(*static_cast<Part*>(selection));
    } else if (selection == scene_.lighting()) {
        drawLightingProperties();
    } else if (selection == scene_.workspace()) {
        drawWorkspaceProperties();
    }
    ImGui::End();
}

void EditorUI::drawDialogs() {
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
            if (projectio::save(scene_, savePath, error)) {
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
    Instance* selection = scene_.selection();
    if (!selection || !selection->isPart()) return;
    endGizmoSession(false, "duplicate");
    Part* copy = scene_.duplicatePart(*static_cast<Part*>(selection));
    if (!copy) {
        setStatus("Nao foi possivel duplicar a peca selecionada.", true);
        return;
    }
    physics_.syncScene(scene_);
    scene_.select(copy);
    checkInvariants("duplicate");
    setStatus("Duplicado: " + copy->name());
}

void EditorUI::deleteSelection() {
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
    if (transform_.active()) return;
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

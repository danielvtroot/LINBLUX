#include "editor/EditorUI.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
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

namespace loblox {

namespace {

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
            drawList->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y),
                              color, 1.6f);
            drawList->AddLine(ImVec2(center.x, center.y - half), ImVec2(center.x, center.y + half),
                              color, 1.6f);
            drawList->AddTriangleFilled(ImVec2(center.x + half, center.y),
                                        ImVec2(center.x + half - 4.0f, center.y - 3.0f),
                                        ImVec2(center.x + half - 4.0f, center.y + 3.0f), color);
            drawList->AddTriangleFilled(ImVec2(center.x, center.y - half),
                                        ImVec2(center.x - 3.0f, center.y - half + 4.0f),
                                        ImVec2(center.x + 3.0f, center.y - half + 4.0f), color);
            break;
        case Icon::Scale:
            drawList->AddRect(ImVec2(center.x - half, center.y - half),
                              ImVec2(center.x + half * 0.35f, center.y + half * 0.35f), color,
                              1.5f);
            drawList->AddLine(ImVec2(center.x - half * 0.1f, center.y + half * 0.1f),
                              ImVec2(center.x + half, center.y + half), color, 1.6f);
            drawList->AddTriangleFilled(ImVec2(center.x + half, center.y + half),
                                        ImVec2(center.x + half - 5.0f, center.y + half - 1.0f),
                                        ImVec2(center.x + half - 1.0f, center.y + half - 5.0f),
                                        color);
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
            drawList->AddRectFilled(ImVec2(center.x - half * 0.85f, center.y - half * 0.7f),
                                    ImVec2(center.x + half * 0.85f, center.y + half * 0.85f),
                                    color, 2.0f);
            break;
        case Icon::Sphere:
            drawList->AddCircleFilled(center, half * 0.85f, color, 18);
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
            const float gap = 2.0f;
            const float cell = (size - gap) * 0.5f;
            drawList->AddRectFilled(ImVec2(center.x - half, center.y - half),
                                    ImVec2(center.x - half + cell, center.y - half + cell), color,
                                    1.0f);
            drawList->AddRectFilled(ImVec2(center.x - half + cell + gap, center.y - half),
                                    ImVec2(center.x + half, center.y - half + cell), color, 1.0f);
            drawList->AddRectFilled(ImVec2(center.x - half, center.y - half + cell + gap),
                                    ImVec2(center.x - half + cell, center.y + half), color, 1.0f);
            drawList->AddRectFilled(ImVec2(center.x - half + cell + gap,
                                           center.y - half + cell + gap),
                                    ImVec2(center.x + half, center.y + half), color, 1.0f);
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
    ImGuiStyle& style = ImGui::GetStyle();
    const float iconSize = 15.0f;
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float height = ImGui::GetFrameHeight();
    const ImVec2 size(textSize.x + iconSize + style.FramePadding.x * 3.0f, height);
    const ImVec2 position = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##button", size);
    const bool clicked = ImGui::IsItemClicked() && enabled;
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 background = active ? IM_COL32(56, 102, 158, 255)
                              : hovered ? IM_COL32(66, 66, 74, 255)
                                        : IM_COL32(48, 48, 54, 255);
    if (!enabled) {
        background = IM_COL32(40, 40, 44, 255);
    }
    drawList->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
                            background, 4.0f);
    if (active) {
        drawList->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
                          IM_COL32(110, 170, 240, 255), 4.0f);
    }

    const ImU32 foreground = enabled ? IM_COL32(232, 232, 238, 255) : IM_COL32(115, 115, 120, 255);
    DrawIcon(drawList, ImVec2(position.x + style.FramePadding.x + iconSize * 0.5f,
                              position.y + size.y * 0.5f),
             iconSize, icon, foreground);
    drawList->AddText(ImVec2(position.x + style.FramePadding.x + iconSize + 4.0f,
                             position.y + (size.y - textSize.y) * 0.5f),
                      foreground, label);
    if (hovered && tooltip != nullptr) {
        ImGui::SetTooltip("%s", tooltip);
    }
    ImGui::PopID();
    return clicked;
}

void ToolbarSeparator() {
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0.0f, 6.0f);
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

}  // namespace

EditorUI::EditorUI(Scene& scene, PhysicsWorld& physics, EditorCamera& camera,
                   SceneRenderer& renderer, const AppConfig& config)
    : scene_(scene),
      physics_(physics),
      camera_(camera),
      renderer_(renderer),
      config_(config),
      projectPath_(config.defaultProjectPath) {
    std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
    camera_.reset();
}

EditorUI::~EditorUI() {
    unloadViewport();
}

void EditorUI::setOnSceneLoaded(std::function<void()> callback) {
    onSceneLoaded_ = std::move(callback);
}

void EditorUI::setStatus(std::string message, bool error) {
    status_ = std::move(message);
    statusIsError_ = error;
    statusUntil_ = ImGui::GetTime() + 5.0;
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
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, mainViewport->WorkSize);

    ImGuiID center = dockId;
    const ImGuiID top =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.085f, nullptr, &center);
    const ImGuiID left =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.17f, nullptr, &center);
    ImGuiID right =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
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

void EditorUI::handleShortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        deleteSelection();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        focusSelection();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        std::string error;
        if (projectio::save(scene_, projectPath_, error)) {
            setStatus("Projeto salvo em " + projectPath_);
        } else {
            setStatus("Falha ao salvar: " + error, true);
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        requestOpenPopup_ = true;
        std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        gizmo_.cancel();
    }
}

void EditorUI::draw() {
    if (!layoutBuilt_) {
        buildDockLayout();
    }
    ImGui::DockSpaceOverViewport(ImGui::GetID("LobloxDockspace"), ImGui::GetMainViewport());

    handleShortcuts();

    drawToolbar();
    if (showToolbox_) {
        drawToolbox();
    }
    drawViewport();
    if (showExplorer_) {
        drawExplorer();
    }
    if (showProperties_) {
        drawProperties();
    }
    drawDialogs();
}

void EditorUI::drawToolbar() {
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::Begin("##Toolbar", nullptr, flags);

    if (IconButton("select", Icon::Select, "Select", tool_ == ToolMode::Select,
                   "Selecionar objetos (clique na viewport)")) {
        tool_ = ToolMode::Select;
        gizmo_.cancel();
    }
    ImGui::SameLine();
    if (IconButton("move", Icon::Move, "Move", tool_ == ToolMode::Move,
                   "Mover nos eixos X/Y/Z (arraste o gizmo)")) {
        tool_ = ToolMode::Move;
    }
    ImGui::SameLine();
    if (IconButton("scale", Icon::Scale, "Scale", tool_ == ToolMode::Scale,
                   "Redimensionar pelo gizmo")) {
        tool_ = ToolMode::Scale;
    }
    ImGui::SameLine();
    if (IconButton("rotate", Icon::Rotate, "Rotate", tool_ == ToolMode::Rotate,
                   "Girar pelo gizmo")) {
        tool_ = ToolMode::Rotate;
    }

    ToolbarSeparator();

    if (IconButton("part", Icon::Part, "Part", false, "Criar um Block no Workspace")) {
        insertPart(PartShape::Block);
    }
    ImGui::SameLine();
    if (IconButton("sphere", Icon::Sphere, "Sphere", false, "Criar uma Sphere no Workspace")) {
        insertPart(PartShape::Sphere);
    }
    ImGui::SameLine();
    if (IconButton("spawnloc", Icon::Spawn, "Spawn", false, "Criar uma SpawnLocation")) {
        insertSpawnLocation();
    }

    ToolbarSeparator();

    if (IconButton("color", Icon::Color, "Color", false, "Paleta de cores da peca selecionada")) {
        ImGui::OpenPopup("##color_palette");
    }
    if (ImGui::BeginPopup("##color_palette")) {
        Instance* selection = scene_.selection();
        Part* part = selection != nullptr && selection->isPart() ? static_cast<Part*>(selection)
                                                                 : nullptr;
        if (part == nullptr) {
            ImGui::TextDisabled("Selecione uma peca primeiro.");
        } else {
            ImGui::TextUnformatted("Cor da peca");
            for (int i = 0; i < static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0])); ++i) {
                if (i % 4 != 0) {
                    ImGui::SameLine();
                }
                ImGui::PushID(i);
                const ImVec4 color(kPalette[i].r / 255.0f, kPalette[i].g / 255.0f,
                                   kPalette[i].b / 255.0f, 1.0f);
                if (ImGui::ColorButton("##swatch", color,
                                       ImGuiColorEditFlags_NoTooltip |
                                           ImGuiColorEditFlags_NoBorder,
                                       ImVec2(30.0f, 24.0f))) {
                    applyColorToSelection(kPalette[i]);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            float custom[4] = {part->color().r / 255.0f, part->color().g / 255.0f,
                               part->color().b / 255.0f, part->color().a / 255.0f};
            if (ImGui::ColorPicker4("##custom", custom,
                                    ImGuiColorEditFlags_AlphaBar |
                                        ImGuiColorEditFlags_NoSidePreview)) {
                applyColorToSelection(Color{
                    static_cast<unsigned char>(std::lround(custom[0] * 255.0f)),
                    static_cast<unsigned char>(std::lround(custom[1] * 255.0f)),
                    static_cast<unsigned char>(std::lround(custom[2] * 255.0f)),
                    static_cast<unsigned char>(std::lround(custom[3] * 255.0f))});
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (IconButton("material", Icon::Material, "Material", false,
                   "Material da peca selecionada")) {
        ImGui::OpenPopup("##material_menu");
    }
    if (ImGui::BeginPopup("##material_menu")) {
        Instance* selection = scene_.selection();
        Part* part = selection != nullptr && selection->isPart() ? static_cast<Part*>(selection)
                                                                 : nullptr;
        if (part == nullptr) {
            ImGui::TextDisabled("Selecione uma peca primeiro.");
        } else {
            ImGui::TextUnformatted("Material");
            ImGui::Separator();
            for (int i = 0; i < PartMaterialCount(); ++i) {
                const bool selected = static_cast<int>(part->material()) == i;
                if (ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(i)), selected)) {
                    applyMaterialToSelection(i);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    {
        Instance* selection = scene_.selection();
        Part* part = selection != nullptr && selection->isPart() ? static_cast<Part*>(selection)
                                                                 : nullptr;
        const bool anchored = part != nullptr && part->anchored();
        const bool anchorLocked =
            part != nullptr && part->instanceClass() == InstanceClass::Baseplate;
        if (IconButton("anchor", Icon::Anchor, "Anchor", anchored,
                       "Ancorar/desancorar a peca selecionada",
                       part != nullptr && !anchorLocked)) {
            part->setAnchored(!part->anchored());
            setStatus(part->anchored() ? "Peca ancorada." : "Peca solta: a fisica assume.");
        }
    }

    ToolbarSeparator();

    if (IconButton("explorer_toggle", Icon::Explorer, "Explorer", showExplorer_,
                   "Mostrar/ocultar o Explorer")) {
        showExplorer_ = !showExplorer_;
    }
    ImGui::SameLine();
    if (IconButton("properties_toggle", Icon::Properties, "Properties", showProperties_,
                   "Mostrar/ocultar Properties")) {
        showProperties_ = !showProperties_;
    }

    ToolbarSeparator();

    if (IconButton("save", Icon::Save, "Save", false, "Salvar projeto (Ctrl+S)")) {
        requestSavePopup_ = true;
        std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
        dialogError_.clear();
    }
    ImGui::SameLine();
    if (IconButton("open", Icon::Open, "Open", false, "Abrir projeto (Ctrl+O)")) {
        requestOpenPopup_ = true;
        std::snprintf(pathBuffer_, sizeof(pathBuffer_), "%s", projectPath_.c_str());
        dialogError_.clear();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void EditorUI::drawToolbox() {
    ImGui::Begin("Toolbox");
    ImGui::TextDisabled("Inserir objeto");
    ImGui::Separator();

    if (ImGui::Button("Block", ImVec2(-FLT_MIN, 34.0f))) {
        insertPart(PartShape::Block);
    }
    if (ImGui::Button("Sphere", ImVec2(-FLT_MIN, 34.0f))) {
        insertPart(PartShape::Sphere);
    }
    if (ImGui::Button("SpawnLocation", ImVec2(-FLT_MIN, 34.0f))) {
        insertSpawnLocation();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Clique para inserir na frente da camera.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Cena");
    ImGui::Text("%d objetos", static_cast<int>(scene_.objects().size()));
    ImGui::Text("%d pecas", static_cast<int>(scene_.parts().size()));
    ImGui::Text("%d corpos fisicos", static_cast<int>(physics_.bodyCount()));
    ImGui::Separator();
    ImGui::TextWrapped(
        "RMB: orbitar  |  MMB: deslocar\n"
        "Roda: zoom  |  F: focar selecao\n"
        "Delete: excluir peca selecionada");
    ImGui::End();
}

void EditorUI::drawViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = std::max(8, static_cast<int>(available.x));
    const int height = std::max(8, static_cast<int>(available.y));
    resizeViewport(width, height);

    if (!viewportReady_) {
        ImGui::TextDisabled("Viewport indisponivel.");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(viewport_.texture.id)),
                 ImVec2(static_cast<float>(viewportWidth_), static_cast<float>(viewportHeight_)),
                 ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();
    const bool allowInput = hovered && !io.WantTextInput && !ImGui::IsAnyItemActive();

    camera_.handleInput(Vector2{static_cast<float>(viewportWidth_),
                                static_cast<float>(viewportHeight_)},
                        hovered, !gizmo_.dragging());

    Instance* selection = scene_.selection();
    Part* selectedPart =
        selection != nullptr && selection->isPart() ? static_cast<Part*>(selection) : nullptr;
    const Rectangle viewportRect{imagePosition.x, imagePosition.y,
                                 static_cast<float>(viewportWidth_),
                                 static_cast<float>(viewportHeight_)};

    const bool gizmoConsumed = gizmo_.update(tool_, selectedPart, camera_, viewportRect, hovered,
                                             allowInput);

    if (allowInput && !gizmoConsumed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const ImVec2 mouse = ImGui::GetMousePos();
        if (mouse.x >= viewportRect.x && mouse.y >= viewportRect.y &&
            mouse.x <= viewportRect.x + viewportRect.width &&
            mouse.y <= viewportRect.y + viewportRect.height) {
            const Ray ray = camera_.rayFromScreen(
                Vector2{mouse.x - viewportRect.x, mouse.y - viewportRect.y},
                Vector2{viewportRect.width, viewportRect.height});
            Part* hit = nullptr;
            Vector3 point{};
            if (physics_.raycast(ray.position, ray.direction, 5000.0f, &hit, &point)) {
                scene_.select(hit);
            } else {
                scene_.select(nullptr);
            }
        }
    }

    if (hovered && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F)) {
        focusSelection();
    }

    BeginTextureMode(viewport_);
    renderer_.drawScene(scene_, camera_, viewportWidth_, viewportHeight_,
                        selection != nullptr ? selection->id() : kInvalidInstanceId);
    if (selectedPart != nullptr && tool_ != ToolMode::Select) {
        BeginMode3D(camera_.camera());
        gizmo_.draw(*selectedPart, tool_);
        EndMode3D();
    }
    EndTextureMode();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    char overlay[192];
    std::snprintf(overlay, sizeof(overlay), "%s  |  %.0f FPS  |  RMB orbitar, MMB deslocar, roda zoom",
                  ToolModeName(tool_), ImGui::GetIO().Framerate);
    drawList->AddText(ImVec2(imagePosition.x + 10.0f, imagePosition.y + 8.0f),
                      IM_COL32(0, 0, 0, 170), overlay);
    drawList->AddText(ImVec2(imagePosition.x + 9.0f, imagePosition.y + 7.0f),
                      IM_COL32(240, 244, 250, 235), overlay);

    if (selectedPart != nullptr) {
        char selectionLabel[128];
        std::snprintf(selectionLabel, sizeof(selectionLabel), "%s (%s)",
                      selectedPart->name().c_str(), selectedPart->className());
        drawList->AddText(ImVec2(imagePosition.x + 10.0f,
                                 imagePosition.y + viewportRect.height - 24.0f),
                          IM_COL32(0, 0, 0, 170), selectionLabel);
        drawList->AddText(ImVec2(imagePosition.x + 9.0f,
                                 imagePosition.y + viewportRect.height - 25.0f),
                          IM_COL32(255, 225, 130, 235), selectionLabel);
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

void EditorUI::drawExplorerNode(Instance* instance) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (instance->children().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (scene_.selection() == instance) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ClassColor(instance->instanceClass()));
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(
                                            static_cast<intptr_t>(instance->id())),
                                        flags, "%s", instance->name().c_str());
    ImGui::PopStyleColor();

    if (ImGui::IsItemClicked()) {
        scene_.select(instance);
    }
    if (ImGui::BeginPopupContextItem("##explorer_context")) {
        ImGui::TextDisabled("%s", instance->className());
        ImGui::Separator();
        if (ImGui::MenuItem("Focar na viewport", "F")) {
            scene_.select(instance);
            focusSelection();
        }
        if (ImGui::MenuItem("Excluir", "Del", false, instance->deletable())) {
            scene_.select(instance);
            deleteSelection();
        }
        ImGui::EndPopup();
    }

    if (open && !instance->children().empty()) {
        for (Instance* child : instance->children()) {
            drawExplorerNode(child);
        }
        ImGui::TreePop();
    }
}

void EditorUI::drawExplorer() {
    ImGui::Begin("Explorer");
    if (scene_.workspace() != nullptr) {
        drawExplorerNode(scene_.workspace());
    }
    if (scene_.lighting() != nullptr) {
        drawExplorerNode(scene_.lighting());
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
        ImGui::TextDisabled("Nenhum objeto selecionado.");
        ImGui::Separator();
        ImGui::TextUnformatted("Selecione um objeto na viewport ou no Explorer");
        ImGui::TextUnformatted("para editar Name, Position, Size, Color e fisica.");
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
            if (projectio::save(scene_, pathBuffer_, error)) {
                projectPath_ = pathBuffer_;
                dialogError_.clear();
                setStatus("Projeto salvo em " + projectPath_);
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
            if (projectio::load(scene_, pathBuffer_, error)) {
                projectPath_ = pathBuffer_;
                dialogError_.clear();
                gizmo_.cancel();
                if (onSceneLoaded_) {
                    onSceneLoaded_();
                }
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
}

void EditorUI::insertPart(PartShape shape) {
    const Vector3 focus = camera_.target();
    const Vector3 position{focus.x, std::max(6.0f, focus.y + 4.0f), focus.z};
    const Vector3 size = shape == PartShape::Sphere ? Vector3{3.0f, 3.0f, 3.0f}
                                                    : Vector3{4.0f, 4.0f, 4.0f};
    Part* part = scene_.createPart(shape, position, size);
    if (part == nullptr) {
        setStatus("Falha ao criar a peca.", true);
        return;
    }
    part->setAnchored(false);
    scene_.select(part);
    setStatus(shape == PartShape::Sphere ? "Sphere criada (dinamica)."
                                         : "Block criado (dinamico).");
}

void EditorUI::insertSpawnLocation() {
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
    gizmo_.cancel();
    if (scene_.remove(selection)) {
        setStatus("Excluido: " + name);
    } else {
        setStatus("Nao foi possivel excluir " + name, true);
    }
}

void EditorUI::focusSelection() {
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
    const float radius = std::max(size.x, std::max(size.y, size.z));
    camera_.focusOn(part->position(), std::max(1.5f, radius));
}

}  // namespace loblox

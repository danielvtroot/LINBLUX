#pragma once

#include <functional>
#include <string>

#include "core/Config.h"
#include "core/Math.h"
#include "editor/Gizmo.h"
#include "editor/InputRouting.h"
#include "editor/Tool.h"
#include "editor/TransformSession.h"
#include "raylib.h"

namespace loblox {

class Scene;
class PhysicsWorld;
class EditorCamera;
class SceneRenderer;
class Instance;
class Part;

class EditorUI {
public:
    EditorUI(Scene& scene, PhysicsWorld& physics, EditorCamera& camera, SceneRenderer& renderer,
             const AppConfig& config);
    ~EditorUI();

    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    void draw();

    void setOnSceneLoaded(std::function<void()> callback);
    void setStatus(std::string message, bool error = false);
    void setProjectPath(std::string path);

    ToolMode tool() const { return tool_; }

private:
    friend struct EditorUITestAccess;
    void buildDockLayout();
    void handleShortcuts(const EditorInput& input, const InputRoute& route);
    void saveCurrentProject();
    void setTool(ToolMode tool, const char* source = "toolbar");
    void endGizmoSession(bool restoreTransform, const char* reason = "ui_action");
    void checkInvariants(const char* operation);

    void drawToolbar();
    void drawMainMenuBar();
    void drawToolbox();
    void drawViewport();
    void drawExplorer();
    void drawExplorerNode(Instance* instance, const char* filter);
    void drawProperties();
    void drawPartProperties(Part& part);
    void drawLightingProperties();
    void drawWorkspaceProperties();
    void drawDialogs();

    void insertPart(PartShape shape);
    void insertSpawnLocation();
    void applyColorToSelection(Color color);
    void applyMaterialToSelection(int materialIndex);
    void deleteSelection();
    void duplicateSelection();
    void focusSelection();

    void resizeViewport(int width, int height);
    void unloadViewport();

    Scene& scene_;
    PhysicsWorld& physics_;
    EditorCamera& camera_;
    SceneRenderer& renderer_;
    const AppConfig& config_;

    Gizmo gizmo_;
    TransformSession transform_;
    ToolMode tool_ = ToolMode::Select;
    bool viewportFocused_ = false;
    unsigned long long frame_ = 0;
    bool traceInput_ = false;
    bool gizmoLocalSpace_ = false;
    float moveSnap_ = 1.0f;
    float scaleSnap_ = 0.5f;
    float rotateSnapDegrees_ = 15.0f;
    float cameraMoveSpeed_ = 30.0f;
    float cameraLookSensitivity_ = 0.0065f;

    RenderTexture2D viewport_{};
    bool viewportReady_ = false;
    int viewportWidth_ = 0;
    int viewportHeight_ = 0;

    bool layoutBuilt_ = false;
    bool showToolbox_ = true;
    bool showExplorer_ = true;
    bool showProperties_ = true;

    bool requestSavePopup_ = false;
    bool requestOpenPopup_ = false;
    bool requestLuaExportPopup_ = false;
    bool requestLuaImportPopup_ = false;
    std::string dialogError_;

    std::function<void()> onSceneLoaded_;
    std::string projectPath_;
    char pathBuffer_[512] = {};
    char explorerFilter_[128] = {};

    std::string status_;
    bool statusIsError_ = false;
    double statusUntil_ = 0.0;
};

}  // namespace loblox

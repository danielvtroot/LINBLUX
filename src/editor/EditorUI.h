#pragma once

#include <functional>
#include <string>
#include <map>
#include <optional>

#include "core/Config.h"
#include "core/Math.h"
#include "editor/Gizmo.h"
#include "editor/InputRouting.h"
#include "editor/Tool.h"
#include "editor/TransformSession.h"
#include "game/Runtime.h"
#include "raylib.h"

namespace loblox {

class Scene;
class PhysicsWorld;
class EditorCamera;
class SceneRenderer;
class Instance;
class Part;

// Marca Loblox: textura carregada lazy na raiz do projeto (nunca caminho
// absoluto da máquina). ok=false = usar o fallback vetorial da toolbar.
struct LobloxLogo {
    unsigned long long id = 0;
    float width = 0, height = 0;
    bool ok = false;
};
const LobloxLogo& LoadLobloxLogo();

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
    bool playing() const { return runtime_.active(); }

private:
    friend struct EditorUITestAccess;
    void buildDockLayout();
    void handleShortcuts(const EditorInput& input, const InputRoute& route);
    bool saveCurrentProject();
    bool saveProjectAt(const std::string& path, std::string& error);
    void setTool(ToolMode tool, const char* source = "toolbar");
    void endGizmoSession(bool restoreTransform, const char* reason = "ui_action");
    void checkInvariants(const char* operation);

    void drawToolbar();
    void drawMainMenuBar();
    void drawToolbox();
    void drawViewport();
    void drawExplorer();
    void drawExplorerNode(Instance* instance, const char* filter);
    void insertContainer(InstanceClass type, Instance* parent = nullptr);
    void renameSelection();
    void insertScript(InstanceClass type = InstanceClass::Script);
    void openScript(InstanceId id);
    void drawScriptEditor();
    void drawOutput();
    bool commitScriptDrafts(std::string& error);
    void drawProperties();
    void drawPartProperties(Part& part);
    void drawInstanceProperties(Instance& instance);
    void drawLightingProperties();
    void drawWorkspaceProperties();
    void drawDialogs();
    void drawGameToolbar();
    void drawRigEditor();
    void drawAnimationEditor();
    void drawGameSettings();
    void drawPauseMenu();
    void startPlay();
    void stopPlay();
    void setGameplayCapture(bool capture);
    void drawGameplayViewport(const EditorInput& input, InputContext context, Vector2 imagePosition);
    void insertR6();
    void selectRig(const std::string& id, int body = -1);
    void openAnimation(const std::string& id);
    bool saveAnimation();
    void cancelRigGizmo(bool restore = true);
    bool updateRigGizmo(const EditorInput& input, const InputRoute& route, Rectangle viewport,
                        bool hovered, bool blocked);

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
    game::RuntimeSession runtime_;
    bool gameplayCapture_ = false;
    bool gameplaySkipDelta_ = true;
    Vector2 gameplayRestoreCursor_{};
    int pausePage_ = 0;
    // Rects do menu Esc capturados no desenho (min.x,min.y,max.x,max.y); o
    // harness de teste usa o centro real dos widgets em vez de adivinhar
    // coordenadas de leiaute.
    float pauseTabRect_[4] = {}, pauseSensitivityRect_[4] = {}, pauseResetRect_[4] = {};
    bool showRigEditor_ = false, showAnimationEditor_ = false, showGameSettings_ = false;
    enum class RigContext { Edit, Pose, Animation };
    RigContext rigContext_ = RigContext::Edit;
    std::string selectedRig_;
    int selectedRigBody_ = -1, selectedJoint_ = 0;
    bool showJointPivots_ = true;
    game::Pose previewPose_{};
    game::AnimationClip animationDraft_;
    std::string animationSource_;
    std::string animationDelete_;
    bool animationDirty_ = false, previewPlaying_ = false;
    float previewTime_ = 0;
    int keyInterpolation_ = 1;
    game::RigidTransform copiedKey_;
    bool hasCopiedKey_ = false;
    int timelineDragJoint_ = -1;
    float timelineDragTime_ = 0;
    game::Keyframe timelineDragInitial_;
    Gizmo rigGizmo_;
    Part jointProxy_{~InstanceId{0}-2, "Joint pivot", PartShape::Block};
    struct RigDrag {
        std::string id, correlation;
        ToolMode tool=ToolMode::Select;
        RigContext context=RigContext::Edit;
        int joint=0, body=0;
        game::Rig initial;
        game::Pose preview;
        game::RigidTransform base;
    };
    std::optional<RigDrag> rigDrag_;
    Rectangle rigBodyRows_[game::kBodyCount]{},rigColorControl_{},rigToolControl_{};
    bool gizmoLocalSpace_ = false;
    bool snapEnabled_ = true;
    Rectangle snapControl_{};
    float moveSnap_ = 1.0f;
    float scaleSnap_ = 0.5f;
    float rotateSnapDegrees_ = 15.0f;
    float cameraMoveSpeed_ = 30.0f;
    float cameraLookSensitivity_ = 0.0065f;

    RenderTexture2D viewport_{};
    bool viewportReady_ = false;
    int viewportWidth_ = 0;
    int viewportHeight_ = 0;

    // Retângulo real da imagem de gameplay (para o menu Esc responder à
    // viewport do jogo, não ao desktop inteiro).
    float gameViewportX_ = 0, gameViewportY_ = 0;
    float gameViewportW_ = 0, gameViewportH_ = 0;
    Rectangle healthHud_{};

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
    char propertiesFilter_[128] = {};
    bool explorerFocused_ = false;
    InstanceId renameId_ = 0, clipboardId_ = 0, revealedSelection_ = 0, pendingDelete_ = 0;
    std::string renameRigId_;
    std::string pendingDeleteRigId_;
    char renameBuffer_[129] = {};
    std::pair<InstanceId,InstanceId> pendingReparent_{0,0};
    bool pendingDuplicate_ = false;
    int pendingInsert_ = 0;
    struct ScriptDraft { std::string text; bool dirty=false; char search[128]{}; int line=1,pendingLine=0; };
    std::map<InstanceId,ScriptDraft> scriptDrafts_;
    InstanceId activeScript_=0;
    InstanceId requestedScript_=0;
    Rectangle scriptSourceControl_{};
    bool showScriptEditor_=false, showOutput_=false;
    float displayedHealth_=100;

    std::string status_;
    bool statusIsError_ = false;
    double statusUntil_ = 0.0;
};

}  // namespace loblox

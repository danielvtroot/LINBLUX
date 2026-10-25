#pragma once

#include <functional>
#include <string>
#include <map>
#include <optional>
#include <memory>
#include <vector>

#include "core/Config.h"
#include "core/Math.h"
#include "editor/EditHistory.h"
#include "editor/Gizmo.h"
#include "editor/InputRouting.h"
#include "editor/Tool.h"
#include "editor/TransformMetrics.h"
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
class StudioFrontend;
class EditorActions;
class ExperienceUI;

// Marca LINBLUX: textura carregada lazy na raiz do projeto (nunca caminho
// absoluto da máquina). ok=false = usar o fallback vetorial da toolbar.
struct LinbluxLogo {
    unsigned long long id = 0;
    float width = 0, height = 0;
    bool ok = false;
};
const LinbluxLogo& LoadLinbluxLogo();

enum class GameCursorVisual { System, Hidden, MouseLock };
struct GameplayCursorState {
    bool captured = false;
    GameCursorVisual visual = GameCursorVisual::System;
    Vector2 position{}; // logical window coordinates, never a world position
};

class EditorUI {
public:
    EditorUI(Scene& scene, PhysicsWorld& physics, EditorCamera& camera, SceneRenderer& renderer,
             const AppConfig& config);
    ~EditorUI();

    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    void draw();
    bool enableModernFrontend(std::string& error);
    void renderModernFrontend();
    StudioFrontend* frontend() const {return frontend_.get();}

    void setOnSceneLoaded(std::function<void()> callback);
    void setStatus(std::string message, bool error = false);
    void setProjectPath(std::string path);

    ToolMode tool() const { return tool_; }
    bool playing() const;
    bool experienceOverlay() const;
    GameplayCursorState gameplayCursor() const;

private:
    friend struct EditorUITestAccess;
    friend class StudioFrontend;
    friend class EditorActions;
    friend class ExperienceUI;
    ExperienceUI* experienceUi_ = nullptr; // owned by StudioFrontend
    void finishPlayStart();
    struct ScriptDraft;
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
    void openScriptAtLine(InstanceId id, int line);
    void openScriptEditor();
    void activateDocument(InstanceId id);
    void closeDocument(InstanceId id);
    void pruneScriptDocuments();
    void drawCentralScriptWorkspace();
    void drawScriptEditor();
    void drawScriptEditorBody(Instance& object, ScriptDraft& draft, bool central);
    void drawOutput();
    bool commitScriptDrafts(std::string& error);
    bool gameImmersive() const;
    void toggleGameMaximized();
    void toggleGameFullscreen();
    void exitGameDisplayModes();
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
    // Leitura numerica do arraste (studs/graus) e edicao exata por texto.
    void beginTransformReadout(Part* part, ToolMode mode);
    void updateTransformReadout(Part* part);
    void drawTransformReadout(const EditorInput& input, Vector2 imagePosition);
    void applyReadoutValue(float value);
    void clearTransformReadout();
    void recordTransformEdit(InstanceId id, const Transform& before, const Transform& after);
    void undoTransform();
    void redoTransform();
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
    void resolveViewport();

    Scene& scene_;
    PhysicsWorld& physics_;
    EditorCamera& camera_;
    SceneRenderer& renderer_;
    const AppConfig& config_;

    Gizmo gizmo_;
    TransformSession transform_;
    ToolMode tool_ = ToolMode::Select;
    EditHistory history_;
    // Um unico readout por arraste. `context` guarda a transformacao inicial;
    // nada e recalculado a partir do frame anterior.
    struct TransformReadout {
        bool visible = false;
        bool editing = false;
        bool focusPending = false;
        InstanceId partId = kInvalidInstanceId;
        transformmetrics::Context context{};
        transformmetrics::Measurement measurement{};
        char text[32]{};
        double expiresAt = 0.0;  // tempo ImGui; 0 = nunca expira (arrastando)
    } readout_;
    // Retangulo do chip no frame anterior: mantem clique/hover fora da
    // camera e do raycast de selecao mesmo antes do ImGui atualizar o hover.
    Rectangle readoutRect_{};
    Transform editStart_{};
    bool editStartValid_ = false;
    bool viewportFocused_ = false;
    unsigned long long frame_ = 0;
    bool traceInput_ = false;
    bool showCollisionShapes_ = false;
    std::unique_ptr<StudioFrontend> frontend_;
    game::RuntimeSession runtime_;
    bool gameplayCapture_ = false;
    bool gameplaySkipDelta_ = true;
    bool gameplayRestoreNoCursorChange_ = false, gameplayRestoreDrawCursor_ = false;
    Vector2 gameplayRestoreCursor_{};
    int pausePage_ = 0;
    // Rects do menu Esc capturados no desenho (min.x,min.y,max.x,max.y); o
    // harness de teste usa o centro real dos widgets em vez de adivinhar
    // coordenadas de leiaute.
    float pauseTabRect_[4] = {}, pauseSensitivityRect_[4] = {}, pauseResetRect_[4] = {};
    // Screen-space Output rows captured while the panel is drawn so the
    // integration harness can click a real error line instead of guessing.
    std::vector<Rectangle> outputMessageRects_;
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
    // Total sweep of the last rotate drag in the Animation context. Inserting a
    // key right after it stores that signed arc on the segment that starts at
    // the pose the drag began from, so a deliberate >180 turn survives the
    // quaternion keyframe format.
    struct AuthoredArc {
        int joint = -1;
        Quaternion from{};      // pose rotation when the drag started
        Quaternion base{};      // composed world base rotation of that drag
        Vector3 worldAxis{1, 0, 0};
        float radians = 0;
    };
    AuthoredArc authoredArc_;
    void applyAuthoredArc(int joint, float time);
    Part jointProxy_{~InstanceId{0}-2, "Joint pivot", PartShape::Block};
    struct RigDrag {
        std::string id, correlation;
        ToolMode tool=ToolMode::Select;
        RigContext context=RigContext::Edit;
        int joint=0, body=0;
        game::Rig initial;
        game::Pose preview;
        game::RigidTransform base;
        game::PlayerSettings player;
    };
    std::optional<RigDrag> rigDrag_;
    Rectangle rigBodyRows_[game::kBodyCount]{},rigColorControl_{},rigToolControl_{},
        rigImageControl_{},rigImageRemoveControl_{};
    bool gizmoLocalSpace_ = false;
    bool snapEnabled_ = true;
    Rectangle snapControl_{};
    float moveSnap_ = 1.0f;
    float scaleSnap_ = 0.5f;
    float rotateSnapDegrees_ = 15.0f;
    float cameraMoveSpeed_ = 30.0f;
    float cameraLookSensitivity_ = 0.0065f;

    RenderTexture2D viewport_{};
    RenderTexture2D resolvedViewport_{};
    Shader fxaa_{};
    int fxaaResolutionLoc_ = -1;
    bool viewportReady_ = false;
    bool resolvedViewportReady_ = false;
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
    struct ScriptUndo { std::string text; int cursor=0; };
    struct ScriptDraft {
        std::string text;
        std::string baseline;
        bool dirty=false;
        char search[128]{};
        int line=1,pendingLine=0;
        int cursor=0;
        int pendingCursor=-1;
        float scrollY=0;
        bool restoreFocus=false;
        std::vector<ScriptUndo> undo,redo;
        double lastEditTime=-1;
    };
    std::map<InstanceId,ScriptDraft> scriptDrafts_;
    std::vector<InstanceId> openDocuments_;
    InstanceId activeScript_=0, activeDocument_=0, requestedScript_=0;
    Rectangle scriptSourceControl_{};
    bool showScriptEditor_=false, showOutput_=false;
    bool gameMaximized_=false, gameFullscreen_=false;
    int savedWindowX_=0, savedWindowY_=0, savedWindowWidth_=0, savedWindowHeight_=0;
    float displayedHealth_=100;

    std::string status_;
    bool statusIsError_ = false;
    double statusUntil_ = 0.0;
};

}  // namespace loblox

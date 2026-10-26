#include "rendering/ExperienceOverlay.h"
#include "rendering/LuaUiRenderer.h"
#include "ui/Localization.h"
#include "ui/ExperienceUI.h"
#include "editor/EditorUI.h"
#include "ui/ExperienceUI.h"
#include "editor/StudioTheme.h"
#include "ui/StudioFrontend.h"
#include <filesystem>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "core/Diagnostics.h"
#include "game/ImageAssets.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "serialization/ProjectIO.h"

namespace loblox {
namespace {
bool EditTransform(const char* id, game::RigidTransform& transform, bool position = true) {
    const auto before = transform;
    ImGui::PushID(id);
    bool changed = position && ImGui::DragFloat3(i18n::Label("Position"), &transform.position.x, .02f, -10000, 10000);
    // Round-trip pair (ZYX). raylib's ToEuler/FromEuler use different orders,
    // so converting through them changes the orientation whenever more than
    // one axis is used; an edit here must never silently rotate the joint.
    auto degrees = EulerDegreesZYXFromQuat(transform.rotation);
    if (ImGui::DragFloat3(i18n::Label("Rotation (degrees)"), &degrees.x, .5f, -360, 360)) {
        transform.rotation = QuatFromEulerDegreesZYX(degrees); changed = true;
    }
    if (changed && !game::Valid(transform)) {
        transform = before; changed = false;
        ImGui::TextColored({1,.3f,.2f,1}, i18n::Label("Transform rejected: use finite values and a valid quaternion."));
        diag::Event event; event.level = diag::Level::Warning;
        event.subsystem = "rig"; event.name = "validation_error";
        event.error = "nonfinite UI transform rejected"; event.context = {{"field", id}}; diag::log(event);
    }
    ImGui::PopID(); return changed;
}
void CameraSettings(game::PlayerSettings& p, float* sensitivityRect = nullptr) {
    const auto previous = p;
    int mode = static_cast<int>(p.cameraMode);
    if (ImGui::Combo(i18n::Label("Camera mode"), &mode, "Livre\0Acompanhar\0Primeira pessoa\0")) p.cameraMode = static_cast<game::CameraMode>(mode);
    ImGui::SliderFloat(i18n::Label("Mouse sensitivity"), &p.sensitivity, .0001f, .02f, "%.4f");
    if (sensitivityRect) {
        const auto mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        sensitivityRect[0] = mn.x; sensitivityRect[1] = mn.y;
        sensitivityRect[2] = mx.x; sensitivityRect[3] = mx.y;
    }
    ImGui::Checkbox(i18n::Label("Camera lock"), &p.cameraLock);
    ImGui::Checkbox(i18n::Label("Enable Shift Lock (Left Shift toggles during Play)"), &p.enableShiftLock);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(i18n::Label("Toggle pelo Left Shift durante o Play. Suspenso no menu Esc e liberado no Stop"));
    ImGui::Checkbox(i18n::Label("Allow zoom to first person"), &p.allowFirstPerson);
    ImGui::SliderFloat(i18n::Label("Min zoom"), &p.minZoom, .1f, 10);
    ImGui::SliderFloat(i18n::Label("Max zoom"), &p.maxZoom, std::max(p.minZoom, 5.f), 100);
    p.maxZoom = std::max(p.minZoom, p.maxZoom);
    p.cameraDistance = std::clamp(p.cameraDistance, p.minZoom, p.maxZoom);
    ImGui::SliderFloat(i18n::Label("Initial distance"), &p.cameraDistance, p.minZoom, p.maxZoom);
    ImGui::SliderFloat(i18n::Label("Vertical field of view"), &p.cameraFov, 1,170,"%.1f°");
    std::string error;
    if (!p.validate(error)) {
        p = previous;
        ImGui::TextColored({1,.3f,.2f,1}, i18n::Label("Camera settings rejected: finite, in-range values required."));
    }
}
void AnimationChoice(const char* label, std::string& id, const game::GameAssets& assets,bool optional=false) {
    auto* current = assets.findAnimation(id);
    if (ImGui::BeginCombo(label, current ? current->name.c_str() : optional?"None (native fallback)":"Missing")) {
        if(optional && ImGui::Selectable(i18n::Label("None (native fallback)"),id.empty()))id.clear();
        for (const auto& clip : assets.animations)
            if (ImGui::Selectable(clip.name.c_str(), clip.id == id)) id = clip.id;
        ImGui::EndCombo();
    }
}
}

void EditorUI::insertR6() {
    endGizmoSession(true, "insert_rig"); cancelRigGizmo(); camera_.releaseCapture();
    const std::string id = scene_.gameAssets().createLinbluxCharacter().id;
    selectRig(id); camera_.focusOn(scene_.gameAssets().findRig(id)->root.position, 5);
    setStatus("Personagem LINBLUX criado; animações disponíveis no editor de animação.");
}
void EditorUI::selectRig(const std::string& id, int body) {
    auto* rig = scene_.gameAssets().findRig(id);
    if (!rig) return;
    endGizmoSession(true, "rig_selection"); cancelRigGizmo(); scene_.select(nullptr);
    if (selectedRig_ != id) previewPose_ = {};
    selectedRig_ = id; selectedRigBody_ = body; showRigEditor_ = true;
    if(body<0){rigContext_=RigContext::Edit;selectedJoint_=game::RootJoint;}
    if(tool_==ToolMode::Select)setTool(body<0?ToolMode::Move:ToolMode::Rotate,"rig_selection");
    for (int j = 0; j < game::kJointCount; ++j) if (rig->joints[j].child == body) selectedJoint_ = j;
}
void EditorUI::openAnimation(const std::string& id) {
    auto* clip = scene_.gameAssets().findAnimation(id);
    if (!clip) { setStatus("Insira um R6 antes de abrir uma animacao.", true); return; }
    if (selectedRig_.empty()) {
        const auto rigId = scene_.gameAssets().defaultRig;
        selectRig(rigId);
    }
    cancelRigGizmo();
    animationDraft_ = *clip; animationSource_ = id; animationDirty_ = false;
    timelineDragJoint_ = -1; authoredArc_ = {};
    rigContext_ = RigContext::Animation; previewTime_ = 0; previewPlaying_ = false;
    if(tool_==ToolMode::Scale || tool_==ToolMode::Select)setTool(ToolMode::Rotate,"animation_context");
    previewPose_ = animationDraft_.sample(0); showAnimationEditor_ = true;
}
bool EditorUI::saveAnimation() {
    std::string error;
    if (!animationDraft_.validate(error)) { setStatus(error, true); return false; }
    auto* asset = scene_.gameAssets().findAnimation(animationSource_);
    if (!asset) { setStatus("Animation asset no longer exists", true); return false; }
    animationDirty_ = true;
    return saveCurrentProject();
}
void EditorUI::drawGameToolbar() {
    if (ImGui::BeginMenu(i18n::Label("Game"))) {
        ImGui::BeginDisabled(runtime_.active());
        if (ImGui::MenuItem(i18n::Label("Insert R6"))) insertR6();
        if (ImGui::MenuItem(i18n::Label("Rig Editor"))) showRigEditor_ = true;
        if (ImGui::MenuItem(i18n::Label("Animation Editor"))) {
            showAnimationEditor_ = true;
            if (animationSource_.empty()) openAnimation(scene_.gameAssets().player.walkAnimation);
        }
        if (ImGui::MenuItem(i18n::Label("Game Settings"))) showGameSettings_ = true;
        if (ImGui::MenuItem(i18n::Label("Play"), "F5")) startPlay();
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!runtime_.active());
        if (ImGui::MenuItem(runtime_.state() == game::RuntimeState::Paused ? "Resume" : "Pause")) {
            if (runtime_.state() == game::RuntimeState::Paused) runtime_.resume();
            else { runtime_.pause(); setGameplayCapture(false); }
        }
        if (ImGui::MenuItem(i18n::Label("Stop / Return to Editor"), "Shift+F5")) stopPlay();
        ImGui::EndDisabled(); ImGui::EndMenu();
    }
    ImGui::SameLine();
    if (!runtime_.active()) {
        if (ImGui::SmallButton(i18n::Label("Play [F5]"))) startPlay();
    } else {
        if (ImGui::SmallButton(i18n::Label("Stop [Shift+F5]"))) stopPlay();
    }
}
void EditorUI::startPlay() {
    if(experienceUi_){experienceUi_->requestPlay();return;}
    if (runtime_.active()) return;
    endGizmoSession(true, "play"); cancelRigGizmo(); camera_.releaseCapture();
    exitGameDisplayModes();
    previewPlaying_ = false;
    std::string error;
    if (!commitScriptDrafts(error)) { setStatus(error,true); return; }
    game::ProgressOptions progress;std::error_code pathError;
    const auto source=std::filesystem::weakly_canonical(config_.defaultProjectPath,pathError);
    if(!pathError&&std::filesystem::is_regular_file(source,pathError))progress.identity="scene:"+source.string();
    if (!runtime_.start(scene_, error,progress)) { setStatus(error, true); return; }
    finishPlayStart();
}
void EditorUI::finishPlayStart() {
    for (const auto& message:runtime_.scripts().messages()) if (message.error) showOutput_=true;
    pausePage_ = 0; gameplaySkipDelta_ = true; gameMaximized_=false; gameFullscreen_=false;
    ImGui::SetWindowFocus(i18n::Label("Viewport"));
    setStatus("Play iniciado. F10 maximiza, F11 tela cheia; Esc abre o menu.");
}
void EditorUI::stopPlay() {
    exitGameDisplayModes();
    setGameplayCapture(false);
    if (!runtime_.active()) return;
    runtime_.stop(); pausePage_ = 0; camera_.releaseCapture();
    setStatus("Play finalizado. Alterações da sessão foram descartadas.");
    checkInvariants("runtime_stop");
}
bool EditorUI::gameImmersive() const {
    return runtime_.active()&&(gameMaximized_||gameFullscreen_);
}
void EditorUI::toggleGameMaximized() {
    if(!runtime_.active()) {setStatus("Maximize a view do jogo durante o Play (F10).",true);return;}
    gameMaximized_=!gameMaximized_;
    diag::Event event; event.subsystem="display"; event.name="maximize_toggle";
    event.context={{"enabled",gameMaximized_},{"screen_width",GetScreenWidth()},{"screen_height",GetScreenHeight()}};
    diag::log(event);
    setStatus(gameMaximized_?"Jogo maximizado (F10 para restaurar).":"Layout do Studio restaurado.");
}
void EditorUI::toggleGameFullscreen() {
    if(!runtime_.active()) {setStatus("Tela cheia do jogo disponivel durante o Play (F11).",true);return;}
    if(!gameFullscreen_) {
        savedWindowX_=GetWindowPosition().x;savedWindowY_=GetWindowPosition().y;
        savedWindowWidth_=GetScreenWidth();savedWindowHeight_=GetScreenHeight();
        gameFullscreen_=true;
        ToggleBorderlessWindowed();
        setStatus("Tela cheia (F11 para sair). Esc abre o menu do jogo.");
    } else {
        gameFullscreen_=false;
        ToggleBorderlessWindowed();
        if(savedWindowWidth_>0&&savedWindowHeight_>0)SetWindowSize(savedWindowWidth_,savedWindowHeight_);
        SetWindowPosition(savedWindowX_,savedWindowY_);
        setStatus("Janela do Studio restaurada.");
    }
    diag::Event event; event.subsystem="display"; event.name="fullscreen_toggle";
    event.context={{"enabled",gameFullscreen_},{"borderless",IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)},
        {"screen_width",GetScreenWidth()},{"screen_height",GetScreenHeight()}};
    diag::log(event);
}
void EditorUI::exitGameDisplayModes() {
    if(gameFullscreen_) {
        gameFullscreen_=false;
        ToggleBorderlessWindowed();
        if(savedWindowWidth_>0&&savedWindowHeight_>0)SetWindowSize(savedWindowWidth_,savedWindowHeight_);
        SetWindowPosition(savedWindowX_,savedWindowY_);
    }
    gameMaximized_=false;
}
void EditorUI::setGameplayCapture(bool capture) {
    if (gameplayCapture_ == capture) return;
    gameplaySkipDelta_ = true;
    if (capture) {
        auto& io=ImGui::GetIO();
        gameplayRestoreNoCursorChange_=(io.ConfigFlags&ImGuiConfigFlags_NoMouseCursorChange)!=0;
        gameplayRestoreDrawCursor_=io.MouseDrawCursor;
        // rlImGui otherwise calls ShowCursor when its cursor shape changes,
        // which switches GLFW out of relative/disabled mode behind our back.
        io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;io.MouseDrawCursor=false;
        gameplayRestoreCursor_ = GetMousePosition(); DisableCursor();
    } else {
        EnableCursor();SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        auto& io=ImGui::GetIO();
        if(!gameplayRestoreNoCursorChange_)io.ConfigFlags&=~ImGuiConfigFlags_NoMouseCursorChange;
        io.MouseDrawCursor=gameplayRestoreDrawCursor_;
        // Alt+Tab must not warp the pointer into the now-unfocused game.
        if(IsWindowFocused())SetMousePosition(static_cast<int>(gameplayRestoreCursor_.x),
                                              static_cast<int>(gameplayRestoreCursor_.y));
    }
    gameplayCapture_ = capture;
    diag::Event event; event.subsystem = "camera"; event.name = "capture_change";
    event.context = {{"context", "gameplay"}, {"captured", capture}}; diag::log(event);
}
GameplayCursorState EditorUI::gameplayCursor() const {
    GameplayCursorState cursor;
    cursor.position={gameViewportX_+gameViewportW_*.5f,gameViewportY_+gameViewportH_*.5f};
    cursor.captured=gameplayCapture_&&IsWindowFocused()&&IsCursorHidden();
    if(cursor.captured)cursor.visual=runtime_.shiftLockActive()?GameCursorVisual::MouseLock:GameCursorVisual::Hidden;
    return cursor;
}
void EditorUI::drawGameplayViewport(const EditorInput& raw, InputContext context, Vector2 image) {
    const bool resized=std::fabs(gameViewportX_-image.x)>.5f||std::fabs(gameViewportY_-image.y)>.5f||
        gameViewportW_!=viewportWidth_||gameViewportH_!=viewportHeight_;
    if(resized)gameplaySkipDelta_=true;
    gameViewportX_=image.x;gameViewportY_=image.y;
    gameViewportW_=static_cast<float>(viewportWidth_);gameViewportH_=static_cast<float>(viewportHeight_);
    const Rectangle menuButton{image.x+12,image.y+12,78,38};
    const bool menuHovered=!frontend_ && context.hovered && !gameplayCapture_ && CheckCollisionPointRec(raw.mouse,menuButton);
    EditorInput gameInput=raw;
    if(menuHovered && raw.leftPressed)gameInput.escape=true; // HUD action enters the same Pause/Resume router
    if(menuHovered)context.hovered=false;
    context.mode = runtime_.state() == game::RuntimeState::Paused ? InputMode::Paused : InputMode::Playing;
    context.menuDepth = pausePage_;
    const InputRoute route = RouteInput(gameInput, context);
    if (traceInput_ && (raw.escape || raw.stop || raw.jump || raw.rightPressed || raw.leftPressed || raw.wheel != 0)) {
        diag::Event e; e.subsystem = "input"; e.name = "dispatch";
        e.context = {{"consumer", InputOwnerName(route.owner)}, {"reason", route.reason},
                     {"mode", "play"}, {"tool", ToolModeName(tool_)}}; diag::log(e);
    }
    if (route.owner == InputOwner::Stop) { stopPlay(); return; }
    if (route.owner == InputOwner::Pause) {
        runtime_.pause(); pausePage_ = 0;
        setGameplayCapture(false);
    } else if (route.owner == InputOwner::Back) pausePage_ = 0;
    else if (route.owner == InputOwner::Resume) {
        runtime_.resume(); ImGui::SetWindowFocus(i18n::Label("Viewport"));
    }
    game::PlayerInput input;
    const bool owns = route.owner == InputOwner::Gameplay || route.owner == InputOwner::Resume;
    if (owns) {
        input.script=game::CaptureScriptInput(image);
        if(!context.hovered&&!gameplayCapture_){input.script.mouse.reset();input.script.delta={};input.script.wheel=0;}
        if (!raw.modified) {
            input.movement = {raw.movement.x, raw.movement.z};
            input.jump = raw.jump;
            input.shiftKey = raw.shift; // toggle do Shift Lock (edge no Runtime)
        }
        if (gameplayCapture_ && !gameplaySkipDelta_) input.look = raw.delta;
        if (context.hovered || gameplayCapture_) input.zoom = raw.wheel;
    }
    gameplaySkipDelta_ = false;
    if(runtime_.state()==game::RuntimeState::Playing) {
        // Script UI uses viewport-local pixels. The right-button capture keeps
        // owning the pointer, so a camera drag never clicks a HUD button.
        const Vector2 local{raw.mouse.x-image.x,raw.mouse.y-image.y};
        if(local.x>=0&&local.y>=0&&local.x<viewportWidth_&&local.y<viewportHeight_&&!gameplayCapture_) {
            runtime_.uiPointer(local,float(viewportWidth_),float(viewportHeight_));
            if(raw.leftPressed&&runtime_.uiClickAt(local,float(viewportWidth_),float(viewportHeight_)))context.hovered=false;
        }
    }
    runtime_.update(GetFrameTime(), input);
    // The runtime consumes the Shift edge once. Capture follows its resulting
    // state in this same frame, including zoom to first person and respawn.
    const bool wantsCapture=owns&&IsWindowFocused()&&runtime_.state()==game::RuntimeState::Playing&&!runtime_.playerCamera().scripted()&&
        (runtime_.playerCamera().firstPerson()||runtime_.settings().cameraLock||runtime_.shiftLockActive()||
         (context.hovered&&raw.rightPressed)||(gameplayCapture_&&raw.rightDown));
    setGameplayCapture(wantsCapture);
    // Retângulo real do gameplay para o menu Esc (evita coordenadas sobre o
    // desktop inteiro):
    gameViewportX_ = image.x;
    gameViewportY_ = image.y;
    gameViewportW_ = static_cast<float>(viewportWidth_);
    gameViewportH_ = static_cast<float>(viewportHeight_);
    BeginTextureMode(viewport_);
    RigRenderState character{&runtime_.rig(), runtime_.pose(), runtime_.renderCharacterRoot()};
    renderer_.drawScene(runtime_.scene(), runtime_.playerCamera().camera(), viewportWidth_, viewportHeight_,
        kInvalidInstanceId, runtime_.playerCamera().avatarHidden() ? nullptr : &character, false);
    DrawExperienceOverlay(runtime_,viewportWidth_,viewportHeight_);
    DrawLuaUi(runtime_,viewportWidth_,viewportHeight_);
    EndTextureMode();
    resolveViewport();
    if(frontend_)return; // RML owns gameplay presentation; no duplicate HUD/menu.
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({menuButton.x,menuButton.y},{menuButton.x+menuButton.width,menuButton.y+menuButton.height},
        menuHovered?IM_COL32(54,68,88,245):IM_COL32(20,25,33,235),8);
    const LinbluxLogo& brand = LoadLinbluxLogo();
    if (brand.ok) {
        dl->AddImage(static_cast<ImTextureID>(brand.id),
                     {menuButton.x+5,menuButton.y+4},{menuButton.x+5+30.f*brand.width/brand.height,menuButton.y+34});
    }
    for(int i=0;i<3;++i)dl->AddLine({menuButton.x+47,menuButton.y+12+i*7.f},
        {menuButton.x+65,menuButton.y+12+i*7.f},IM_COL32(238,243,250,255),2);
    if(menuHovered)ImGui::SetTooltip(i18n::Label("Game menu (Esc)"));
    // Software cursor for the captured virtual pointer, never a world marker.
    if (gameplayCursor().visual==GameCursorVisual::MouseLock) {
        const auto cursor=gameplayCursor().position;const float cx=cursor.x,cy=cursor.y;
        const Texture2D texture{}; // LINBLUX functional crosshair uses the native geometry below.
        if(texture.id) {
            const float scale=28.f/std::max(texture.width,texture.height);
            const float halfW=texture.width*scale*.5f,halfH=texture.height*scale*.5f;
            dl->AddImage(static_cast<ImTextureID>(texture.id),{cx-halfW,cy-halfH},{cx+halfW,cy+halfH});
        } else {
            dl->AddCircle({cx,cy},6,IM_COL32(240,235,239,255),20,1.5f);
            dl->AddLine({cx-10,cy},{cx-4,cy},IM_COL32(235,65,82,255),2);
            dl->AddLine({cx+4,cy},{cx+10,cy},IM_COL32(235,65,82,255),2);
        }
    }
    const float health=runtime_.health(),maximum=runtime_.settings().maxHealth;
    const ImU32 color=IM_COL32(215,38,63,255);
    healthHud_={image.x+viewportWidth_-93,image.y+17,82,82};
    const auto& settings=runtime_.settings();
    if (!runtime_.playerCamera().avatarHidden() && (settings.healthDisplayType!=2 || settings.nameDisplayDistance>0)) {
        const auto head=runtime_.rig().evaluate(runtime_.pose(),runtime_.renderCharacterRoot())[game::Head];
        const auto eye=runtime_.playerCamera().camera();
        const auto p=Vector3Add(head.position,{0,runtime_.rig().bodies[game::Head].size.y*.5f+.6f,0});
        const float distance=Vector3Distance(eye.position,p);
        if (Vector3DotProduct(Vector3Subtract(p,eye.position),Vector3Subtract(eye.target,eye.position))>0 &&
            runtime_.cameraClearance(eye.position,p,.02f)>.98f) {
            const auto screen=GetWorldToScreenEx(p,eye,viewportWidth_,viewportHeight_);
            const ImVec2 at{image.x+screen.x,image.y+screen.y};
            dl->PushClipRect({image.x,image.y},{image.x+viewportWidth_,image.y+viewportHeight_},true);
            if (settings.healthDisplayType!=2 && distance<=settings.healthDisplayDistance &&
                (settings.healthDisplayType==0 || health<maximum)) {
                dl->AddRectFilled({at.x-38,at.y},{at.x+38,at.y+8},IM_COL32(25,30,35,220),2);
                dl->AddRectFilled({at.x-37,at.y+1},{at.x-37+74*health/maximum,at.y+7},color,2);
            }
            if (distance<=settings.nameDisplayDistance) {
                const auto size=ImGui::CalcTextSize(runtime_.rig().name.c_str());
                dl->AddText({at.x-size.x*.5f,at.y-size.y-3},IM_COL32(245,245,245,255),runtime_.rig().name.c_str());
            }
            dl->PopClipRect();
        }
    }
}
void EditorUI::drawPauseMenu() {
    if (runtime_.state() != game::RuntimeState::Paused || gameViewportW_ < 80 || gameViewportH_ < 80) return;
    setGameplayCapture(false);
    const ImVec2 origin{gameViewportX_, gameViewportY_};
    const ImVec2 extent{gameViewportW_, gameViewportH_};
    const float margin = std::min(16.f, std::min(extent.x, extent.y) * .04f);
    const float width = std::min(extent.x - margin*2, std::clamp(extent.x*.88f, 540.f, 940.f));
    const float height = std::min(extent.y - margin*2, std::clamp(extent.y*.86f, 360.f, 690.f));
    const ImVec2 center{origin.x+extent.x*.5f, origin.y+extent.y*.5f};
    // Dim only the gameplay image, on its own draw list (not the desktop or
    // modal dim layer). The panel uses the same current-frame screen rect.
    if (auto* viewport = ImGui::FindWindowByName(i18n::Label("Viewport"))) {
        auto* draw = viewport->DrawList;
        draw->PushClipRect(origin, {origin.x+extent.x,origin.y+extent.y}, true);
        draw->AddRectFilled(origin,{origin.x+extent.x,origin.y+extent.y},
                           ImGui::ColorConvertFloat4ToU32(studio::Colors().overlay));
        draw->PopClipRect();
    }
    const auto& p = studio::Colors(); // gameplay remains dark with either Studio theme
    ImGui::PushStyleColor(ImGuiCol_WindowBg,{p.background.x,p.background.y,p.background.z,.97f});
    ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_Text,p.text);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,p.secondary);
    ImGui::PushStyleColor(ImGuiCol_Border,p.border);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,p.raised);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,p.hover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,p.selected);
    ImGui::PushStyleColor(ImGuiCol_Button,p.raised);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,p.hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,p.selected);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,14});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,{1,1});
    ImGui::SetNextWindowPos(center,ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin(i18n::Label("Play paused"),nullptr,flags);
    const auto& logo=LoadLinbluxLogo();
    if(logo.ok) {
        ImGui::Image(static_cast<ImTextureID>(logo.id),{34.f*logo.width/logo.height,34});
        ImGui::SameLine(0,10);
    }
    ImGui::BeginGroup();
    ImGui::TextUnformatted(i18n::Label("LINBLUX"));
    ImGui::TextDisabled(i18n::Label("Local playtest paused"));
    ImGui::EndGroup();
    ImGui::Spacing();
    const auto remember=[](float* rect) {
        const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
        rect[0]=lo.x;rect[1]=lo.y;rect[2]=hi.x;rect[3]=hi.y;
    };
    const char* tabs[]{"Game","Settings","Controls","About"};
    const float tabWidth=(ImGui::GetContentRegionAvail().x-18)/4;
    for(int i=0;i<4;++i) {
        if(i) ImGui::SameLine(0,6);
        if(pausePage_==i) ImGui::PushStyleColor(ImGuiCol_Button,p.selected);
        const bool clicked=ImGui::Button(i18n::Label(tabs[i]),{tabWidth,32});
        if(pausePage_==i) ImGui::PopStyleColor();
        if(i==1) remember(pauseTabRect_);
        if(clicked) pausePage_=i;
    }
    ImGui::Separator();
    const bool narrow=width<430;
    const float footerHeight=narrow?118.f:42.f;
    const float contentHeight=std::max(1.f,ImGui::GetContentRegionAvail().y-footerHeight-10);
    ImGui::BeginChild("##PauseContent",{0,contentHeight},ImGuiChildFlags_None);
    if(pausePage_==0) {
        ImGui::Spacing();
        ImGui::TextUnformatted(i18n::Label("Your game"));
        ImGui::Separator();
        ImGui::TextWrapped(i18n::Label("%s"), std::filesystem::path(projectPath_).filename().string().c_str());
        ImGui::Spacing();
        ImGui::TextUnformatted(i18n::Label("Local player"));
        ImGui::TextWrapped(i18n::Label("%s"),runtime_.rig().name.c_str());
        ImGui::Text(i18n::Label("Health  %.0f / %.0f"),runtime_.health(),runtime_.settings().maxHealth);
        ImGui::Spacing();
        ImGui::TextWrapped(i18n::Label("The game is paused. Resume to keep playing, respawn at the spawn point, or return to your map in the Studio."));
    } else if(pausePage_==1) {
        auto& settings=runtime_.settings();
        const auto previous=settings;
        ImGui::TextDisabled(i18n::Label("Camera preferences for this play session"));
        if(ImGui::BeginTable("##PauseSettings",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Setting",0,.44f);
            ImGui::TableSetupColumn("Value",0,.56f);
            const auto row=[](const char* label) {
                ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();
                ImGui::TextWrapped(i18n::Label("%s"),label);ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
            };
            row("Camera mode"); int mode=static_cast<int>(settings.cameraMode);
            if(ImGui::Combo(i18n::Label("##mode"),&mode,"Livre\0Acompanhar\0Primeira pessoa\0")) settings.cameraMode=static_cast<game::CameraMode>(mode);
            row("Mouse sensitivity");
            ImGui::SliderFloat(i18n::Label("##sensitivity"),&settings.sensitivity,.0001f,.02f,"%.4f");
            remember(pauseSensitivityRect_);
            row("Enable Shift Lock");ImGui::Checkbox(i18n::Label("##shift"),&settings.enableShiftLock);
            row("Camera lock");ImGui::Checkbox(i18n::Label("##lock"),&settings.cameraLock);
            row("First person zoom");ImGui::Checkbox(i18n::Label("##first"),&settings.allowFirstPerson);
            row("Camera distance");ImGui::SliderFloat(i18n::Label("##distance"),&settings.cameraDistance,settings.minZoom,settings.maxZoom,"%.1f");
            ImGui::EndTable();
        }
        std::string error;
        if(!settings.validate(error)) settings=previous;
        if(settings.cameraDistance!=previous.cameraDistance) runtime_.playerCamera().setDistance(settings.cameraDistance);
    } else if(pausePage_==2) {
        const char* keys[]{"W A S D","Space","Right mouse","Scroll","Left Shift","Esc","Shift + F5"};
        const char* help[]{"Move relative to the camera","Jump from the ground","Orbit the camera","Zoom / first person",
            "Toggle Shift Lock when enabled","Pause / resume","Stop and return to Studio"};
        for(int i=0;i<7;++i) { ImGui::TextUnformatted(keys[i]);ImGui::SameLine(120);ImGui::TextWrapped(i18n::Label("%s"),i18n::Label(help[i])); }
    } else {
        if(logo.ok) ImGui::Image(static_cast<ImTextureID>(logo.id),{80.f*logo.width/logo.height,80});
        ImGui::TextUnformatted(i18n::Label("LINBLUX Studio"));
        ImGui::TextWrapped(i18n::Label("An open-source editor and local game engine."));
        ImGui::Spacing();ImGui::TextDisabled(i18n::Label("C++17 / Lua 5.4 / Bullet / raylib"));
    }
    ImGui::EndChild();
    ImGui::Spacing();
    const float buttonWidth=narrow?ImGui::GetContentRegionAvail().x:(ImGui::GetContentRegionAvail().x-16)/3;
    const float buttonHeight=narrow?34.f:42.f;
    // Defer lifecycle actions until all widgets have been drawn.
    const bool stop=ImGui::Button(i18n::Label("Stop Test"),{buttonWidth,buttonHeight});
    if(!narrow) ImGui::SameLine(0,8);
    const bool reset=ImGui::Button(i18n::Label("Respawn"),{buttonWidth,buttonHeight});
    remember(pauseResetRect_);
    if(!narrow) ImGui::SameLine(0,8);
    ImGui::PushStyleColor(ImGuiCol_Button,studio::Colors().accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,studio::Colors().hover);
    const bool resume=ImGui::Button(i18n::Label("Resume"),{buttonWidth,buttonHeight});
    ImGui::PopStyleColor(2);
    ImGui::End();
    ImGui::PopStyleVar(3);ImGui::PopStyleColor(11);
    if(stop) stopPlay();
    else if(reset) { std::string error; if(!runtime_.resetCharacter(error)) setStatus(error,true); }
    else if(resume) { runtime_.resume();ImGui::SetWindowFocus(i18n::Label("Viewport")); }
}

void EditorUI::drawRigEditor() {
    if (!ImGui::FindWindowByName(i18n::Label("Rig Editor"))) {
        if (auto* properties = ImGui::FindWindowByName(i18n::Label("Properties")))
            ImGui::SetNextWindowDockID(properties->DockId, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize({390, 550}, ImGuiCond_FirstUseEver);
    if(frontend_)frontend_->prepareAdvanced();
    if (!ImGui::Begin(i18n::Label("Rig Editor"), &showRigEditor_,frontend_?(ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoDocking):0)) { ImGui::End(); return; }
    const bool integrated=frontend_!=nullptr;
    if(integrated) {
        ImGui::BeginChild("Rig controls",{std::min(250.f,ImGui::GetContentRegionAvail().x*.34f),0});
        ImGui::PushItemWidth(std::max(90.f,ImGui::GetContentRegionAvail().x-45));
    }
    if (ImGui::Button(i18n::Label("Create R6"))) insertR6();
    if(experienceUi_&&ImGui::Button("Importar personagem…"))experienceUi_->enqueue("import-character");
    auto* rig = scene_.gameAssets().findRig(selectedRig_);
    if (ImGui::BeginCombo(i18n::Label("Rig"), rig ? rig->name.c_str() : "Select a rig")) {
        for (const auto& r : scene_.gameAssets().rigs)
            if (ImGui::Selectable(r.name.c_str(), r.id == selectedRig_)) selectRig(r.id);
        ImGui::EndCombo();
    }
    rig = scene_.gameAssets().findRig(selectedRig_);
    if (!rig) { ImGui::TextWrapped(i18n::Label("Create an R6 or select one in Explorer."));if(integrated){ImGui::PopItemWidth();ImGui::EndChild();} ImGui::End(); return; }
    ImGui::InputText(i18n::Label("Name"), &rig->name);
    if (rig->name.empty()) rig->name = "R6Character";
    ImGui::TextUnformatted("Controlador: LINBLUX · Movimento nativo");
    if (ImGui::Button(i18n::Label("Use as Player"))) scene_.gameAssets().defaultRig = rig->id;
    if(experienceUi_&&ImGui::Button("Definir como padrão da engine"))experienceUi_->enqueue("default-r6");
    if(selectedRigBody_<0) {
        ImGui::TextUnformatted(i18n::Label("Whole rig · torso pivot · uniform scale"));
        if(ImGui::Button(i18n::Label("Center internal root"))){cancelRigGizmo();std::string error;if(!rig->recenterAuthoringRoot(error))setStatus(error,true);}
        if(ImGui::IsItemHovered())ImGui::SetTooltip(i18n::Label("Bakes a stale root translation without moving any visible body or rewriting animation keys."));
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n::Label("Focus rig"))) camera_.focusOn(rig->authoringPivot().position, 5);
    int mode = rigContext_ == RigContext::Edit ? 0 : 1;
    if (ImGui::RadioButton(i18n::Label("Edit Rig"), mode == 0)) { cancelRigGizmo(); rigContext_ = RigContext::Edit; previewPlaying_ = false; previewPose_ = {}; }
    ImGui::SameLine();
    if (ImGui::RadioButton(i18n::Label("Pose Rig"), mode == 1)) {
        cancelRigGizmo();rigContext_=RigContext::Pose;previewPlaying_=false;
        if(tool_==ToolMode::Scale)setTool(ToolMode::Rotate,"rig_pose_context");
    }
    ImGui::Checkbox(i18n::Label("Show joint pivots / gizmo"), &showJointPivots_);
    if (ImGui::Button(i18n::Label("Reset pose"))) { cancelRigGizmo(); previewPose_ = {}; previewPlaying_ = false; }
    ImGui::SameLine();
    if (ImGui::Button(i18n::Label("Save project"))) saveCurrentProject();

    ImGui::Separator();
    const char* toolNames[] = {"Rotate", "Move", "Scale"};
    const ToolMode toolModes[]{ToolMode::Rotate,ToolMode::Move,ToolMode::Scale};
    if(ImGui::BeginCombo(i18n::Label("Gizmo Tool"),ToolModeName(tool_))) {
        for(int i=0;i<3;++i) {
            ImGui::BeginDisabled(toolModes[i]==ToolMode::Scale && rigContext_!=RigContext::Edit);
            if(ImGui::Selectable(i18n::Label(toolNames[i]),tool_==toolModes[i]))setTool(toolModes[i],"rig_editor");
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();rigToolControl_={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};}
    if(rigContext_!=RigContext::Edit)ImGui::TextDisabled(i18n::Label("Scale changes bind proportions: use Edit Rig."));
    if(integrated) {
        ImGui::PopItemWidth();ImGui::EndChild();ImGui::SameLine();
        ImGui::BeginChild("Rig inspector",{0,0});
    }
    if (ImGui::CollapsingHeader(i18n::Label("Root / HumanoidRootPart"), integrated?0:ImGuiTreeNodeFlags_DefaultOpen)) EditTransform("root", rig->root);
    if (ImGui::CollapsingHeader(i18n::Label("Body parts"), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < game::kBodyCount; ++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(rig->bodies[i].name.c_str(), selectedRigBody_ == i)) {
                cancelRigGizmo(); selectedRigBody_ = i;
                for (int j = 0; j < game::kJointCount; ++j) if (rig->joints[j].child == i) selectedJoint_ = j;
            }
            {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();rigBodyRows_[i]={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};}
            ImGui::PopID();
        }
        if (selectedRigBody_ >= 0) {
            auto& body = rig->bodies[selectedRigBody_];
            const auto previousName=body.name;
            if(ImGui::InputText(i18n::Label("Nome do membro"),&body.name)&&body.name.empty())body.name=previousName;
            float color[]{body.color.r/255.f, body.color.g/255.f, body.color.b/255.f};
            if (ImGui::ColorEdit3(i18n::Label("Body color"), color) && std::isfinite(color[0]) &&
                std::isfinite(color[1]) && std::isfinite(color[2])) body.color = {
                static_cast<unsigned char>(color[0]*255), static_cast<unsigned char>(color[1]*255), static_cast<unsigned char>(color[2]*255), 255};
            {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();rigColorControl_={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};}
            int material = static_cast<int>(body.material);
            if (ImGui::BeginCombo(i18n::Label("Material"), PartMaterialName(body.material))) {
                for (int m = 0; m < PartMaterialCount(); ++m)
                    if (ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(m)), material == m)) body.material = PartMaterialFromIndex(m);
                ImGui::EndCombo();
            }
            const auto images = game::ListImages(game::AssetsDir());
            const std::string imageLabel = body.image.empty() ? "None" :
                std::filesystem::path(body.image).filename().string();
            if (ImGui::BeginCombo(i18n::Label("Front image (legacy)"), imageLabel.c_str())) {
                if (ImGui::Selectable(i18n::Label("None"), body.image.empty())) body.image.clear();
                for (const auto& image : images) {
                    const bool selected = std::filesystem::path(body.image).filename().string() == image.filename;
                    if (ImGui::Selectable(image.filename.c_str(), selected)) body.image = image.filename;
                }
                ImGui::EndCombo();
            }
            {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();rigImageControl_={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};}
            rigImageRemoveControl_ = {};
            if (!body.image.empty()) {
                if (ImGui::Button(i18n::Label("Remove body image"))) body.image.clear();
                const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
                rigImageRemoveControl_={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(i18n::Label("Single Front image. For multiple faces use Explorer > body > Properties > Add Decal."));
            auto size = body.size;
            ImGui::BeginDisabled(rigContext_ != RigContext::Edit);
            if (ImGui::DragFloat3(i18n::Label("Dimensions"), &size.x, .05f, .2f, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp)) rig->resizeBody(selectedRigBody_, size);
            if (ImGui::Button(i18n::Label("Reset member size/color"))) {
                auto original = game::MakeR6().bodies[selectedRigBody_];
                rig->resizeBody(selectedRigBody_, original.size); body.color = original.color; body.material = original.material;
            }
            ImGui::EndDisabled();
        }
    }
    if (ImGui::CollapsingHeader(i18n::Label("Joints"), integrated?0:ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int j = 0; j < game::kJointCount; ++j) {
            if (ImGui::Selectable(rig->joints[j].name.c_str(), selectedJoint_ == j)) {
                cancelRigGizmo(); selectedJoint_ = j; selectedRigBody_ = rig->joints[j].child;
                setTool(ToolMode::Rotate, "rig_joint");
            }
        }
        auto& joint = rig->joints[selectedJoint_];
        if (rigContext_ == RigContext::Edit) {
            const auto previousName=joint.name;
            if(ImGui::InputText(i18n::Label("Nome da articulação"),&joint.name)) {
                std::string error;if(!rig->validate(error)){joint.name=previousName;setStatus(error,true);}
            }
            ImGui::BeginDisabled(selectedJoint_==0);
            const char* parentName=joint.parent<0?"Raiz interna":rig->bodies[joint.parent].name.c_str();
            if(ImGui::BeginCombo("Articulação ligada a",parentName)) {
                for(int earlier=0;earlier<selectedJoint_;++earlier) {
                    const int body=rig->joints[earlier].child;
                    if(ImGui::Selectable(rig->bodies[body].name.c_str(),joint.parent==body)) {
                        const int previous=joint.parent;joint.parent=body;std::string error;
                        if(!rig->validate(error)){joint.parent=previous;setStatus(error,true);}
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            EditTransform("parent anchor", joint.parentAnchor);
            ImGui::TextDisabled(i18n::Label("Child attachment")); EditTransform("child anchor", joint.childAnchor);
            ImGui::TextDisabled(i18n::Label("Rest orientation (not animation)")); EditTransform("rest", rig->restPose[selectedJoint_]);
        } else {
            ImGui::TextDisabled(i18n::Label("Pose only - rest offsets are unchanged"));
            EditTransform("pose", previewPose_[selectedJoint_]);
        }
    }
    if (ImGui::Button(i18n::Label("Reset entire rig"))) ImGui::OpenPopup("Reset R6?");
    if (ImGui::BeginPopupModal("Reset R6?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(i18n::Label("Reset proportions, colors, attachments and rest pose?\nAnimation assets are kept."));
        if (ImGui::Button(i18n::Label("Reset"))) {
            cancelRigGizmo(); auto fresh = game::MakeR6(); fresh.id = rig->id; fresh.name = rig->name; fresh.root = rig->root;
            *rig = std::move(fresh); previewPose_ = {}; ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button(i18n::Label("Cancel"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(integrated)ImGui::EndChild();
    ImGui::End();
}

void EditorUI::applyAuthoredArc(int joint, float time) {
    const AuthoredArc arc = authoredArc_;
    authoredArc_ = {};
    if (arc.joint != joint || !std::isfinite(arc.radians)) return;
    // Shorter sweeps are already the unique shortest arc; 180 degrees is
    // ambiguous and needs the authored direction pinned down.
    if (std::fabs(arc.radians) < PI - .001f) return;
    auto& track = animationDraft_.tracks[joint];
    game::Keyframe* segmentStart = nullptr;
    for (auto& key : track) {
        if (key.time >= time - .0001f) break;
        const float alignment = std::fabs(QuaternionDotProduct(
            UnitQuaternion(key.pose.rotation), UnitQuaternion(arc.from)));
        // The arc belongs to the segment that starts where the drag started.
        if (alignment > .99995f) segmentStart = &key;
    }
    if (!segmentStart) return;
    // The ring axis is world-space; the keyframe lives in the joint's pose
    // frame, so rotate it through the drag's base transform first.
    const Vector3 poseAxis = Vector3Normalize(
        Vector3RotateByQuaternion(arc.worldAxis, QuaternionInvert(arc.base)));
    game::Keyframe target;
    target.pose.rotation = previewPose_[joint].rotation;
    segmentStart->arcDegrees = game::AnimationClip::authoredArcDegrees(
        *segmentStart, target, poseAxis, arc.radians * RAD2DEG);
}

void EditorUI::cancelRigGizmo(bool restore) {
    if(rigDrag_) {
        if(restore) if(auto* rig=scene_.gameAssets().findRig(rigDrag_->id)) {
            rig->root=rigDrag_->initial.root;
            if(rigDrag_->body<0&&rigDrag_->context==RigContext::Edit&&rigDrag_->tool==ToolMode::Scale&&scene_.gameAssets().defaultRig==rigDrag_->id) {
                scene_.gameAssets().player.collisionHeight=rigDrag_->player.collisionHeight;
                scene_.gameAssets().player.collisionRadius=rigDrag_->player.collisionRadius;
            }
            rig->bodies=rigDrag_->initial.bodies;
            rig->joints=rigDrag_->initial.joints;
            rig->restPose=rigDrag_->initial.restPose;
            previewPose_=rigDrag_->preview;
        }
        diag::Event event;event.subsystem="rig";event.name=restore?"drag_cancel":"drag_commit";
        event.correlationId=rigDrag_->correlation;event.result=restore?"restored":"ok";
        event.context={{"rig",rigDrag_->id},{"tool",ToolModeName(rigDrag_->tool)},{"body",rigDrag_->body}};
        diag::log(event);
    }
    rigGizmo_.cancel(&jointProxy_,false);rigDrag_.reset();
}
bool EditorUI::updateRigGizmo(const EditorInput& input,const InputRoute& route,Rectangle viewport,
                              bool hovered,bool blocked) {
    rigGizmo_.setLocalSpace(gizmoLocalSpace_);
    rigGizmo_.setSnap(snapEnabled_?moveSnap_:0,snapEnabled_?scaleSnap_:0,snapEnabled_?rotateSnapDegrees_:0);
    auto* rig=scene_.gameAssets().findRig(selectedRig_);
    const bool editing=rigContext_==RigContext::Edit;
    const bool whole=editing&&selectedRigBody_<0;
    const bool invalid=!rig || (!showJointPivots_&&!whole) || input.escape || tool_==ToolMode::Select ||
        selectedJoint_<0 || selectedJoint_>=game::kJointCount ||
        (tool_==ToolMode::Scale && !editing);
    const bool changed=rigDrag_ && (rigDrag_->id!=selectedRig_ || rigDrag_->tool!=tool_ ||
        rigDrag_->context!=rigContext_ || rigDrag_->joint!=selectedJoint_ ||
        rigDrag_->body!=selectedRigBody_);
    if(invalid || changed) { const bool consumed=rigGizmo_.dragging();cancelRigGizmo();return consumed; }
    const auto& joint=rig->joints[selectedJoint_];
    const int body=joint.child;
    const auto world=rig->evaluate(editing?game::Pose{}:previewPose_,rig->root);
    auto base=game::Compose(joint.parent<0?rig->root:world[joint.parent],joint.parentAnchor);
    if(!editing)base=game::Compose(base,rig->restPose[selectedJoint_]);
    auto& pose=editing?rig->restPose[selectedJoint_]:previewPose_[selectedJoint_];
    if(!rigGizmo_.dragging()) {
        const auto proxy=whole?rig->authoringPivot():tool_==ToolMode::Rotate?game::Compose(base,pose):world[body];
        jointProxy_.setPosition(proxy.position);jointProxy_.setRotation(proxy.rotation);
        jointProxy_.setSize(whole?rig->authoringSize():tool_==ToolMode::Rotate?Vector3{1,1,1}:rig->bodies[body].size);
    }
    GizmoInput gi{input.mouse,input.leftPressed,input.leftDown,IsWindowFocused(),input.shift};
    gi.altDown=input.alt;
    // A panel/popup owning the pointer must block the rig gizmo's input, but it
    // must not freeze jointProxy_. The overlay is drawn from the proxy, so a
    // stale proxy makes the handles jump when the pointer returns to the
    // viewport (and makes RML-driven clicks miss them).
    if(blocked) {
        if(rigGizmo_.dragging()) { const bool consumed=true; cancelRigGizmo(); return consumed; }
        rigGizmo_.update(tool_,&jointProxy_,camera_,viewport,false,false,gi);
        return false;
    }
    // Capture before Gizmo::update edits the proxy. Both drawing and hit testing
    // consume jointProxy_; pose coordinates are solved through the same bind chain.
    RigDrag initial;
    if(!rigDrag_ && input.leftPressed) initial={selectedRig_,{},tool_,rigContext_,selectedJoint_,selectedRigBody_,
                           *rig,previewPose_,whole?rig->authoringPivot():base,scene_.gameAssets().player};
    const auto result=rigGizmo_.update(tool_,&jointProxy_,camera_,viewport,hovered,
        route.owner==InputOwner::Viewport || route.owner==InputOwner::Transform,gi);
    if(result.started) {
        initial.correlation=diag::newCorrelationId();rigDrag_=std::move(initial);
        authoredArc_ = {};
        previewPlaying_=false;camera_.releaseCapture();
        diag::Event event;event.subsystem="rig";event.name="drag_begin";event.result="ok";
        event.correlationId=rigDrag_->correlation;
        event.context={{"rig",selectedRig_},{"tool",ToolModeName(tool_)},{"body",body},{"scope",whole?"rig":"joint"}};
        diag::log(event);
    }
    if(result.cancelled) {cancelRigGizmo();return result.consumed;}
    if(rigDrag_ && (rigGizmo_.dragging() || result.ended)) {
        const auto& drag=*rigDrag_;
        if(whole) {
            *rig=drag.initial;
            float factor=1;game::RigidTransform target{jointProxy_.position(),jointProxy_.rotation()};
            if(tool_==ToolMode::Scale) {
                const auto startSize=rigGizmo_.startTransform().size;const auto size=jointProxy_.size();const int axis=rigGizmo_.activeAxis();
                const int component=std::clamp(axis,0,2);const float raw=(&size.x)[component]/(&startSize.x)[component];
                float lower=0,upper=10000;
                for(const auto& b:rig->bodies)for(const float dim:{b.size.x,b.size.y,b.size.z}){lower=std::max(lower,.2f/dim);upper=std::min(upper,10.f/dim);}
                if(scene_.gameAssets().defaultRig==selectedRig_){lower=std::max(lower,.2f/drag.player.collisionRadius);upper=std::min({upper,3.f/drag.player.collisionRadius,15.f/drag.player.collisionHeight});}
                factor=std::clamp(raw,lower,upper);
                if(std::fabs(raw-1)>.00001f)target.position=Vector3Add(drag.base.position,Vector3Scale(Vector3Subtract(target.position,drag.base.position),(factor-1)/(raw-1)));
                jointProxy_.setSize(Vector3Scale(startSize,factor));jointProxy_.setPosition(target.position);
                if(scene_.gameAssets().defaultRig==selectedRig_){scene_.gameAssets().player.collisionHeight=drag.player.collisionHeight*factor;scene_.gameAssets().player.collisionRadius=drag.player.collisionRadius*factor;}
            }
            std::string error;if(!rig->transformAuthoring(target,factor,error)){setStatus(error,true);cancelRigGizmo();return true;}
            if(result.ended)cancelRigGizmo(false);
            return result.consumed;
        }
        // Scale from the transaction snapshot, never from last frame's anchors.
        if(tool_==ToolMode::Scale) {
            rig->bodies=drag.initial.bodies;rig->joints=drag.initial.joints;
            rig->restPose=drag.initial.restPose;
            auto size=jointProxy_.size();
            const auto rawSize=size;
            size={std::clamp(size.x,.2f,10.f),std::clamp(size.y,.2f,10.f),std::clamp(size.z,.2f,10.f)};
            const auto startBody=drag.initial.evaluate({},drag.initial.root)[body];
            const auto delta=Vector3Subtract(jointProxy_.position(),startBody.position);
            auto centre=jointProxy_.position();
            const float old[]{drag.initial.bodies[body].size.x,drag.initial.bodies[body].size.y,drag.initial.bodies[body].size.z};
            const float raw[]{rawSize.x,rawSize.y,rawSize.z},limited[]{size.x,size.y,size.z};
            for(int axis=0;axis<3;++axis)if(std::fabs(raw[axis]-old[axis])>.00001f) {
                const auto direction=gizmomath::Axis(axis,startBody.rotation,true);
                const float ratio=(limited[axis]-old[axis])/(raw[axis]-old[axis]);
                centre=Vector3Add(centre,Vector3Scale(direction,Vector3DotProduct(delta,direction)*(ratio-1)));
            }
            // Clamp both size and centre: the opposite face must stay fixed at
            // the rig's 10-unit limit, just as at the generic minimum size.
            jointProxy_.setPosition(centre);
            jointProxy_.setSize(size);
            rig->resizeBody(body,size); // resizeBody already updates every attachment once
        }
        auto& target=editing?rig->restPose[selectedJoint_]:previewPose_[selectedJoint_];
        if(tool_==ToolMode::Rotate) {
            target.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionInvert(drag.base.rotation),jointProxy_.rotation()));
        } else {
            const game::RigidTransform desiredBody{jointProxy_.position(),jointProxy_.rotation()};
            const auto desiredJoint=game::Compose(desiredBody,rig->joints[selectedJoint_].childAnchor);
            target.position=game::Compose(game::Inverse(drag.base),desiredJoint).position;
        }
        std::string error;
        if(!rig->validate(error)) {setStatus("Rig transform rejected: "+error,true);cancelRigGizmo();return true;}
        if(result.ended) {
            // The drag total is the only place the editor still knows the
            // authored sweep of a >180 degree turn; keep it until the next key
            // is inserted.
            if(tool_==ToolMode::Rotate && !editing && !result.cancelled)
                authoredArc_ = {selectedJoint_, drag.preview[selectedJoint_].rotation,
                                drag.base.rotation, rigGizmo_.rotationRingAxis(),
                                rigGizmo_.rotationArcRadians()};
            cancelRigGizmo(false);
        }
    }
    return result.consumed;
}

void EditorUI::drawGameSettings() {
    if (!ImGui::FindWindowByName(i18n::Label("Game Settings"))) {
        if (auto* properties = ImGui::FindWindowByName(i18n::Label("Properties")))
            ImGui::SetNextWindowDockID(properties->DockId, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize({420, 540}, ImGuiCond_FirstUseEver);
    if(frontend_)frontend_->prepareAdvanced();
    if (!ImGui::Begin(i18n::Label("Game Settings"), &showGameSettings_,frontend_?(ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoDocking):0)) { ImGui::End(); return; }
    auto& a = scene_.gameAssets(); auto& p = a.player;
    const auto previousSettings = p;
    if (a.rigs.empty()) {
        ImGui::TextWrapped(i18n::Label("No character asset yet. Play uses a temporary R6; create one to customize and save it."));
        if (ImGui::Button(i18n::Label("Create default R6"))) insertR6();
    }
    if (ImGui::BeginCombo(i18n::Label("Default character"), a.defaultRig.c_str())) {
        for (const auto& r : a.rigs) if (ImGui::Selectable(r.name.c_str(), r.id == a.defaultRig)) a.defaultRig = r.id;
        ImGui::EndCombo();
    }
    AnimationChoice("Walk animation", p.walkAnimation, a,true); AnimationChoice("Idle animation", p.idleAnimation, a,true);
    AnimationChoice("Jump animation", p.jumpAnimation, a,true); AnimationChoice("Fall animation", p.fallAnimation, a,true);
    if(p.fallAnimation.empty() && ImGui::Button(i18n::Label("Create empty Fall animation"))) {
        if(!a.findAnimation("r6-fall"))a.animations.push_back(game::MakeFall());
        p.fallAnimation="r6-fall";
    }
    ImGui::SliderFloat(i18n::Label("WalkSpeed (units/s)"), &p.walkSpeed, .1f, 100);
    ImGui::SliderFloat(i18n::Label("Turn response"), &p.turnSpeed, .1f, 50);
    ImGui::SliderFloat(i18n::Label("Acceleration"), &p.acceleration, 1, 500);
    ImGui::SliderFloat(i18n::Label("Deceleration"), &p.deceleration, 1, 500);
    ImGui::SliderFloat(i18n::Label("JumpHeight (units)"), &p.jumpHeight, .1f, 50);
    ImGui::SliderFloat(i18n::Label("Gravity (units/s2)"), &p.gravity, 1, 500);
    ImGui::Checkbox(i18n::Label("AutoRotate"), &p.autoRotate);
    ImGui::SliderFloat(i18n::Label("MaxHealth"), &p.maxHealth, 1, 10000);
    p.health = std::clamp(p.health, 0.f, p.maxHealth);
    ImGui::SliderFloat(i18n::Label("Initial Health"), &p.health, 0, p.maxHealth);
    ImGui::SliderFloat(i18n::Label("Respawn delay (s)"),&p.respawnDelay,.1f,30);
    ImGui::Combo(i18n::Label("Overhead health"),&p.healthDisplayType,"Sempre\0Ao sofrer dano\0Oculta\0");
    ImGui::SliderFloat(i18n::Label("Health display distance"),&p.healthDisplayDistance,0,500);
    ImGui::SliderFloat(i18n::Label("Name display distance"),&p.nameDisplayDistance,0,500);
    ImGui::SliderFloat(i18n::Label("Capsule radius"), &p.collisionRadius, .2f, 3);
    p.collisionHeight = std::max(p.collisionHeight, p.collisionRadius*2);
    ImGui::SliderFloat(i18n::Label("Capsule total height"), &p.collisionHeight, 2*p.collisionRadius, 15);
    ImGui::SeparatorText(i18n::Label("Project camera defaults")); CameraSettings(p);
    std::string error;
    if (!p.validate(error)) { p = previousSettings; setStatus(error, true); }
    if(p.gravity!=previousSettings.gravity)physics_.setGravity({0,-p.gravity,0});
    if (ImGui::Button(i18n::Label("Save game settings"))) saveCurrentProject();
    ImGui::End();
}

void EditorUI::drawAnimationEditor() {
    if (!ImGui::FindWindowByName(i18n::Label("Animation Editor"))) {
        if (auto* view = ImGui::FindWindowByName(i18n::Label("Viewport")); view && view->DockId != 0) {
            ImGuiID upper;
            const ImGuiID lower = ImGui::DockBuilderSplitNode(view->DockId, ImGuiDir_Down, .5f, nullptr, &upper);
            ImGui::DockBuilderDockWindow(i18n::Label("Viewport"), upper);
            ImGui::DockBuilderDockWindow(i18n::Label("Animation Editor"), lower);
            ImGui::DockBuilderFinish(ImGui::GetID("LinbluxDockspace"));
        }
    }
    ImGui::SetNextWindowSize({800, 600}, ImGuiCond_FirstUseEver);
    if(frontend_)frontend_->prepareAdvanced();
    if (!ImGui::Begin(i18n::Label("Animation Editor"), &showAnimationEditor_,frontend_?(ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoDocking):0)) { ImGui::End(); return; }
    auto& assets = scene_.gameAssets();
    if (assets.animations.empty()) {
        ImGui::TextUnformatted(i18n::Label("No animation assets. Create a new clip to begin."));
        if(ImGui::Button(i18n::Label("New animation"))) {
            auto clip=game::MakeIdle();clip.id="animation-"+diag::newCorrelationId();clip.name="New Animation";
            assets.animations.push_back(clip);openAnimation(clip.id);
        }
        if(assets.rigs.empty() && ImGui::Button(i18n::Label("Create animated R6"))) {insertR6();openAnimation(assets.player.walkAnimation);}
        ImGui::End(); return;
    }
    if (animationSource_.empty()) openAnimation(assets.findAnimation(assets.player.walkAnimation)?assets.player.walkAnimation:assets.animations.front().id);
    if (ImGui::BeginCombo(i18n::Label("Animation"), animationDraft_.name.c_str())) {
        for (const auto& clip : assets.animations)
            if (ImGui::Selectable(clip.name.c_str(), clip.id == animationSource_)) {
                if (animationDirty_) setStatus("Save edits or use Revert before opening another animation.", true);
                else openAnimation(clip.id);
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const std::string animationUri="anim://"+animationDraft_.id;
    if (ImGui::SmallButton(i18n::Label("Copy ID"))) { ImGui::SetClipboardText(animationUri.c_str());setStatus("ID copiado: "+animationUri); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(i18n::Label("%s\nRig: %s | %.3f s"),animationUri.c_str(),animationDraft_.rigType.c_str(),animationDraft_.duration);
    if (ImGui::Button(i18n::Label("New"))) {
        if (animationDirty_) setStatus("Save or revert the current animation first.", true);
        else {
            auto clip = game::MakeIdle(); clip.id = "animation-" + diag::newCorrelationId(); clip.name = "New Animation";
            assets.animations.push_back(clip); openAnimation(clip.id);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n::Label("Duplicate"))) {
        auto copy = animationDraft_; copy.id = "animation-" + diag::newCorrelationId(); copy.name += " Copy";
        assets.animations.push_back(copy); openAnimation(copy.id);
    }
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("Save Animation + Project"))) saveAnimation();
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("Revert"))) openAnimation(animationSource_);
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("Use as Walk")) && saveAnimation()) {
        assets.player.walkAnimation = animationSource_; saveCurrentProject();
    }
    const auto eraseAnimation = [&](const std::string& id,bool confirmed) {
        std::string error;
        if(!assets.removeAnimation(id,error,confirmed)) {setStatus(error,true);return;}
            cancelRigGizmo();previewPlaying_=false;animationDirty_=false;
            timelineDragJoint_=-1;hasCopiedKey_=false;previewTime_=0;previewPose_={};
            if(!assets.animations.empty())openAnimation(assets.animations.front().id);
            else {animationSource_.clear();animationDraft_={};previewPose_={};}
            setStatus("Animation deleted. Save the project to persist this change.");
    };
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("Delete"))) {
        const auto* clip=assets.findAnimation(animationSource_);bool assigned=false;
        for(const auto* ref:{&assets.player.walkAnimation,&assets.player.idleAnimation,&assets.player.jumpAnimation,&assets.player.fallAnimation})
            assigned|=clip && assets.findAnimation(*ref)==clip;
        if(assigned) {animationDelete_=animationSource_;ImGui::OpenPopup("Delete assigned animation?");}
        else eraseAnimation(animationSource_,false);
    }
    if(ImGui::BeginPopupModal("Delete assigned animation?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(i18n::Label("This clip is assigned in Game Config. Delete and clear its assignments?"));
        ImGui::TextUnformatted(i18n::Label("Cleared states use neutral/native fallback. Custom Lua IDs are not rewritten."));
        if(ImGui::Button(i18n::Label("Delete and clear"))) {
            eraseAnimation(animationDelete_,true);animationDelete_.clear();ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();if(ImGui::Button(i18n::Label("Cancel"))) {animationDelete_.clear();ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
    if(animationSource_.empty()) {ImGui::End();return;}
    const bool integrated=frontend_!=nullptr;
    const bool compact = ImGui::IsWindowDocked()||integrated;
    if(integrated) {
        ImGui::BeginChild("Clip inspector",{std::min(230.f,ImGui::GetContentRegionAvail().x*.28f),0});
        ImGui::PushItemWidth(std::max(75.f,ImGui::GetContentRegionAvail().x*.45f));
    }
    if (integrated || !compact || ImGui::CollapsingHeader(i18n::Label("Clip settings: name / duration / FPS / loop / priority"))) {
        animationDirty_ |= ImGui::InputText(i18n::Label("Name"), &animationDraft_.name);
        float duration = animationDraft_.duration;
        if (ImGui::DragFloat(i18n::Label("Duration (s)"), &duration, .05f, .05f, 120, "%.2f", ImGuiSliderFlags_AlwaysClamp) &&
            std::isfinite(duration) && duration >= .05f && duration <= 120) {
            const float scale = duration/animationDraft_.duration;
            for (auto& track : animationDraft_.tracks) for (auto& key : track) key.time *= scale;
            previewTime_ *= scale; animationDraft_.duration = duration; animationDirty_ = true;
        }
        if (ImGui::SliderInt(i18n::Label("Authoring FPS"), &animationDraft_.fps, 1, 120)) animationDirty_ = true;
        animationDraft_.fps = std::clamp(animationDraft_.fps, 1, 240);
        if (ImGui::Checkbox(i18n::Label("Loop"), &animationDraft_.loop)) animationDirty_ = true;
        int priority = static_cast<int>(animationDraft_.priority);
        if (ImGui::Combo(i18n::Label("Priority"), &priority, "Repouso\0Movimento\0Ação\0")) {
            animationDraft_.priority = static_cast<game::AnimationPriority>(priority); animationDirty_ = true;
        }
    }
    auto* rig = assets.findRig(selectedRig_);
    if(integrated){ImGui::PopItemWidth();ImGui::EndChild();ImGui::SameLine();ImGui::BeginChild("Animation timeline",{0,0});}
    if (!rig) { ImGui::TextDisabled(i18n::Label("Select a rig in Explorer to preview."));if(integrated)ImGui::EndChild();ImGui::End(); return; }
    if (ImGui::Button(previewPlaying_ ? "Pause preview" : "Play preview")) {
        cancelRigGizmo(); rigContext_ = RigContext::Animation; previewPlaying_ = !previewPlaying_;
    }
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("Stop preview"))) { previewPlaying_ = false; previewTime_ = 0; previewPose_ = animationDraft_.sample(0); }
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("|<"))) { previewTime_ = 0; previewPose_ = animationDraft_.sample(0); }
    ImGui::SameLine(); if (ImGui::Button(i18n::Label(">|"))) { previewTime_ = animationDraft_.duration; previewPose_ = animationDraft_.sample(previewTime_); }
    if (previewPlaying_) {
        previewTime_ += std::min(GetFrameTime(), .1f);
        if (previewTime_ > animationDraft_.duration) {
            if (animationDraft_.loop) previewTime_ = std::fmod(previewTime_, animationDraft_.duration);
            else { previewTime_ = animationDraft_.duration; previewPlaying_ = false; }
        }
        previewPose_ = animationDraft_.sample(previewTime_);
    }
    if (compact) ImGui::SetNextItemWidth(std::max(100.f, ImGui::GetContentRegionAvail().x-(integrated?90:280)));
    bool seek = ImGui::SliderFloat(integrated?"##Time / scrub":"Time / scrub", &previewTime_, 0, animationDraft_.duration, "%.3f s");
    if (compact) { ImGui::SameLine(); ImGui::SetNextItemWidth(70); }
    seek |= ImGui::InputFloat(integrated?"##Exact time (s)":"Exact time (s)", &previewTime_, 0, 0, "%.3f");
    previewTime_ = std::isfinite(previewTime_) ? std::clamp(previewTime_, 0.f, animationDraft_.duration) : 0;
    if (seek) {
        cancelRigGizmo(); previewPlaying_ = false; rigContext_ = RigContext::Animation;
        previewPose_ = animationDraft_.sample(previewTime_);
    }
    ImGui::TextDisabled(i18n::Label("Preview: selected rig in main viewport. Drag diamonds to retime; click track to scrub."));
    bool trackActive = false;
    for (int j = 0; j < game::kJointCount; ++j) {
        ImGui::PushID(j);
        if (ImGui::Selectable(rig->joints[j].name.c_str(), selectedJoint_ == j, 0, {145, 22})) {
            cancelRigGizmo(); selectedJoint_ = j; selectedRigBody_ = rig->joints[j].child;
            rigContext_ = RigContext::Animation; setTool(ToolMode::Rotate, "animation_joint");
        }
        ImGui::SameLine();
        const auto start = ImGui::GetCursorScreenPos();
        const float width = std::max(80.f, ImGui::GetContentRegionAvail().x);
        ImGui::InvisibleButton("track", {width, 22});
        trackActive |= ImGui::IsItemActive();
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(start, {start.x+width, start.y+22}, j == selectedJoint_ ? IM_COL32(55,85,110,255) : IM_COL32(40,45,52,255));
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
            selectedJoint_ = j; selectedRigBody_ = rig->joints[j].child;
            previewPlaying_ = false; rigContext_ = RigContext::Animation;
            previewTime_ = std::clamp((ImGui::GetIO().MousePos.x-start.x)/width, 0.f, 1.f)*animationDraft_.duration;
            if (ImGui::IsItemActivated()) {
                cancelRigGizmo(); timelineDragJoint_ = -1;
                for (const auto& key : animationDraft_.tracks[j])
                    if (std::fabs(start.x+key.time/animationDraft_.duration*width-ImGui::GetIO().MousePos.x) < 7) {
                        timelineDragJoint_ = j; timelineDragInitial_ = key; timelineDragTime_ = key.time;
                        previewTime_ = key.time; break;
                    }
            }
            if (timelineDragJoint_ == j) {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                    animationDraft_.eraseKey(j, timelineDragTime_);
                    animationDraft_.setKey(j, timelineDragInitial_);
                    previewTime_ = timelineDragInitial_.time; timelineDragJoint_ = -1;
                } else if (ImGui::IsMouseDragging(0, 3)) {
                    const float time = std::clamp(std::round(previewTime_*animationDraft_.fps)/animationDraft_.fps, 0.f, animationDraft_.duration);
                    bool occupied = false;
                    for (const auto& key : animationDraft_.tracks[j])
                        if (std::fabs(key.time-time) < .0001f && std::fabs(key.time-timelineDragTime_) >= .0001f) occupied = true;
                    if (!occupied) {
                        animationDraft_.eraseKey(j, timelineDragTime_);
                        auto moved = timelineDragInitial_; moved.time = time;
                        animationDraft_.setKey(j, moved); timelineDragTime_ = time; animationDirty_ = true;
                    }
                    previewTime_ = timelineDragTime_;
                } else previewTime_ = timelineDragTime_;
            }
            previewPose_ = animationDraft_.sample(previewTime_);
        }
        for (const auto& key : animationDraft_.tracks[j]) {
            const float x = start.x + key.time/animationDraft_.duration*width;
            dl->AddQuadFilled({x,start.y+4}, {x+5,start.y+11}, {x,start.y+18}, {x-5,start.y+11}, IM_COL32(240,185,65,255));
        }
        const float x = start.x+previewTime_/animationDraft_.duration*width;
        dl->AddLine({x,start.y}, {x,start.y+22}, IM_COL32(100,200,255,255), 2);
        ImGui::PopID();
    }
    if (!trackActive) timelineDragJoint_ = -1;
    ImGui::SeparatorText(rig->joints[selectedJoint_].name.c_str());
    if (EditTransform("animation pose", previewPose_[selectedJoint_])) { previewPlaying_ = false; rigContext_ = RigContext::Animation; }
    ImGui::Combo(i18n::Label("Interpolation"), &keyInterpolation_, "Linear\0Suave\0Constante\0");
    if (ImGui::Button(i18n::Label("Insert / Update key"))) {
        animationDraft_.setKey(selectedJoint_, {previewTime_, previewPose_[selectedJoint_], static_cast<game::Interpolation>(keyInterpolation_)});
        applyAuthoredArc(selectedJoint_, previewTime_);
        animationDirty_ = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(i18n::Label("Start from a keyed pose, drag more than 180 degrees, then insert the next key: the long way round is kept."));
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("Remove key"))) animationDirty_ |= animationDraft_.eraseKey(selectedJoint_, previewTime_);
    ImGui::SameLine(); if (ImGui::Button(i18n::Label("Copy pose/key"))) { copiedKey_ = previewPose_[selectedJoint_]; hasCopiedKey_ = true; }
    ImGui::SameLine(); ImGui::BeginDisabled(!hasCopiedKey_);
    if (ImGui::Button(i18n::Label("Paste key"))) { animationDraft_.setKey(selectedJoint_, {previewTime_, copiedKey_}); animationDirty_ = true; }
    ImGui::EndDisabled();
    if (ImGui::Button(i18n::Label("Reset selected pose"))) { previewPose_[selectedJoint_] = {}; authoredArc_ = {}; }
    ImGui::SameLine(); ImGui::TextDisabled(i18n::Label("%s / preview is separate from saved asset"), animationDirty_ ? "Unsaved keys" : "Saved");
    if(integrated)ImGui::EndChild();
    ImGui::End();
}
} // namespace loblox

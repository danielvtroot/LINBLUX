#include "editor/EditorUI.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "core/Diagnostics.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "serialization/ProjectIO.h"

namespace loblox {
namespace {
bool EditTransform(const char* id, game::RigidTransform& transform, bool position = true) {
    const auto before = transform;
    ImGui::PushID(id);
    bool changed = position && ImGui::DragFloat3("Position", &transform.position.x, .02f, -10000, 10000);
    auto degrees = EulerDegreesFromQuat(transform.rotation);
    if (ImGui::DragFloat3("Rotation (degrees)", &degrees.x, .5f, -360, 360)) {
        transform.rotation = QuaternionNormalize(QuatFromEulerDegrees(degrees)); changed = true;
    }
    if (changed && !game::Valid(transform)) {
        transform = before; changed = false;
        ImGui::TextColored({1,.3f,.2f,1}, "Transform rejected: use finite values and a valid quaternion.");
        diag::Event event; event.level = diag::Level::Warning;
        event.subsystem = "rig"; event.name = "validation_error";
        event.error = "nonfinite UI transform rejected"; event.context = {{"field", id}}; diag::log(event);
    }
    ImGui::PopID(); return changed;
}
void CameraSettings(game::PlayerSettings& p) {
    const auto previous = p;
    int mode = static_cast<int>(p.cameraMode);
    if (ImGui::Combo("Camera mode", &mode, "Classic\0Follow\0First Person\0")) p.cameraMode = static_cast<game::CameraMode>(mode);
    ImGui::SliderFloat("Mouse sensitivity", &p.sensitivity, .0001f, .02f, "%.4f");
    ImGui::Checkbox("Camera lock", &p.cameraLock);
    ImGui::Checkbox("Allow zoom to first person", &p.allowFirstPerson);
    ImGui::SliderFloat("Min zoom", &p.minZoom, .1f, 10);
    ImGui::SliderFloat("Max zoom", &p.maxZoom, std::max(p.minZoom, 5.f), 100);
    p.maxZoom = std::max(p.minZoom, p.maxZoom);
    p.cameraDistance = std::clamp(p.cameraDistance, p.minZoom, p.maxZoom);
    ImGui::SliderFloat("Initial distance", &p.cameraDistance, p.minZoom, p.maxZoom);
    std::string error;
    if (!p.validate(error)) {
        p = previous;
        ImGui::TextColored({1,.3f,.2f,1}, "Camera settings rejected: finite, in-range values required.");
    }
}
void AnimationChoice(const char* label, std::string& id, const game::GameAssets& assets) {
    auto* current = assets.findAnimation(id);
    if (ImGui::BeginCombo(label, current ? current->name.c_str() : "Missing")) {
        for (const auto& clip : assets.animations)
            if (ImGui::Selectable(clip.name.c_str(), clip.id == id)) id = clip.id;
        ImGui::EndCombo();
    }
}
}

void EditorUI::insertR6() {
    endGizmoSession(true, "insert_rig"); cancelRigGizmo(); camera_.releaseCapture();
    const std::string id = scene_.gameAssets().createR6().id;
    selectRig(id); camera_.focusOn(scene_.gameAssets().findRig(id)->root.position, 5);
    setStatus("R6 criado; caminhada R6_Walk disponivel no Animation Editor.");
}
void EditorUI::selectRig(const std::string& id, int body) {
    auto* rig = scene_.gameAssets().findRig(id);
    if (!rig) return;
    endGizmoSession(true, "rig_selection"); cancelRigGizmo(); scene_.select(nullptr);
    if (selectedRig_ != id) previewPose_ = {};
    selectedRig_ = id; selectedRigBody_ = body; showRigEditor_ = true;
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
    timelineDragJoint_ = -1;
    rigContext_ = RigContext::Animation; previewTime_ = 0; previewPlaying_ = false;
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
    if (ImGui::BeginMenu("Game")) {
        ImGui::BeginDisabled(runtime_.active());
        if (ImGui::MenuItem("Insert R6")) insertR6();
        if (ImGui::MenuItem("Rig Editor")) showRigEditor_ = true;
        if (ImGui::MenuItem("Animation Editor")) {
            showAnimationEditor_ = true;
            if (animationSource_.empty()) openAnimation(scene_.gameAssets().player.walkAnimation);
        }
        if (ImGui::MenuItem("Game Settings")) showGameSettings_ = true;
        if (ImGui::MenuItem("Play", "F5")) startPlay();
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!runtime_.active());
        if (ImGui::MenuItem(runtime_.state() == game::RuntimeState::Paused ? "Resume" : "Pause")) {
            if (runtime_.state() == game::RuntimeState::Paused) runtime_.resume();
            else { runtime_.pause(); setGameplayCapture(false); }
        }
        if (ImGui::MenuItem("Stop / Return to Editor", "Shift+F5")) stopPlay();
        ImGui::EndDisabled(); ImGui::EndMenu();
    }
    ImGui::SameLine();
    if (!runtime_.active()) {
        if (ImGui::SmallButton("Play [F5]")) startPlay();
    } else {
        if (ImGui::SmallButton("Stop [Shift+F5]")) stopPlay();
    }
}
void EditorUI::startPlay() {
    if (runtime_.active()) return;
    endGizmoSession(true, "play"); cancelRigGizmo(); camera_.releaseCapture();
    previewPlaying_ = false;
    std::string error;
    if (!commitScriptDrafts(error)) { setStatus(error,true); return; }
    if (!runtime_.start(scene_, error)) { setStatus(error, true); return; }
    for (const auto& message:runtime_.scripts().messages()) if (message.error) showOutput_=true;
    pausePage_ = 0; gameplaySkipDelta_ = true;
    ImGui::SetWindowFocus("Viewport");
    setStatus("Play iniciado. WASD, Space, RMB para camera; Esc para menu.");
}
void EditorUI::stopPlay() {
    setGameplayCapture(false);
    if (!runtime_.active()) return;
    runtime_.stop(); pausePage_ = 0; camera_.releaseCapture();
    checkInvariants("runtime_stop");
}
void EditorUI::setGameplayCapture(bool capture) {
    if (gameplayCapture_ == capture) return;
    gameplaySkipDelta_ = true;
    if (capture) {
        gameplayRestoreCursor_ = GetMousePosition(); DisableCursor();
    } else {
        EnableCursor(); SetMousePosition(static_cast<int>(gameplayRestoreCursor_.x),
                                         static_cast<int>(gameplayRestoreCursor_.y));
    }
    gameplayCapture_ = capture;
    diag::Event event; event.subsystem = "camera"; event.name = "capture_change";
    event.context = {{"context", "gameplay"}, {"captured", capture}}; diag::log(event);
}
void EditorUI::drawGameplayViewport(const EditorInput& raw, InputContext context, Vector2 image) {
    context.mode = runtime_.state() == game::RuntimeState::Paused ? InputMode::Paused : InputMode::Playing;
    context.menuDepth = pausePage_;
    const InputRoute route = RouteInput(raw, context);
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
        runtime_.resume(); ImGui::SetWindowFocus("Viewport");
    }
    game::PlayerInput input;
    const bool owns = route.owner == InputOwner::Gameplay;
    const bool wantsCapture = owns && (runtime_.playerCamera().firstPerson() || runtime_.settings().cameraLock ||
        (context.hovered && raw.rightPressed) || (gameplayCapture_ && raw.rightDown));
    setGameplayCapture(wantsCapture);
    if (owns) {
        if (!raw.modified) {
            input.movement = {raw.movement.x, raw.movement.z};
            input.jump = raw.jump;
        }
        if (gameplayCapture_ && !gameplaySkipDelta_) input.look = raw.delta;
        if (context.hovered || gameplayCapture_) input.zoom = raw.wheel;
    }
    gameplaySkipDelta_ = false;
    runtime_.update(GetFrameTime(), input);
    BeginTextureMode(viewport_);
    RigRenderState character{&runtime_.rig(), runtime_.pose(), runtime_.characterRoot()};
    renderer_.drawScene(runtime_.scene(), runtime_.playerCamera().camera(), viewportWidth_, viewportHeight_,
        kInvalidInstanceId, runtime_.playerCamera().avatarHidden() ? nullptr : &character, false);
    EndTextureMode();
    const char* hint = runtime_.state() == game::RuntimeState::Paused ? "PAUSED | Esc to resume" :
        "PLAY | WASD move | Space jump | RMB look | Wheel zoom | Esc menu | Shift+F5 stop";
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({image.x+8, image.y+8}, {image.x+viewportWidth_-8.f, image.y+36}, IM_COL32(20,25,30,210), 4);
    dl->AddText({image.x+14, image.y+14}, IM_COL32(235,240,245,255), hint);
    const float health=runtime_.health(), maximum=runtime_.settings().maxHealth;
    displayedHealth_=Lerp(displayedHealth_,health,1-std::exp(-10*std::min(GetFrameTime(),.1f)));
    const float fraction=std::clamp(displayedHealth_/maximum,0.f,1.f);
    const ImU32 color=health<=0?IM_COL32(220,60,60,255):fraction>.6f?IM_COL32(70,195,105,255):
        fraction>.25f?IM_COL32(230,190,65,255):IM_COL32(225,70,65,255);
    const ImVec2 bar{image.x+16,image.y+viewportHeight_-53.f};
    dl->AddRectFilled({bar.x-6,bar.y-6},{bar.x+218,bar.y+35},IM_COL32(20,25,30,215),5);
    dl->AddRectFilled(bar,{bar.x+210,bar.y+10},IM_COL32(65,70,75,255),3);
    dl->AddRectFilled(bar,{bar.x+210*fraction,bar.y+10},color,3);
    char healthText[80];std::snprintf(healthText,sizeof(healthText),health>0?"Health %.0f / %.0f":"DEAD | Respawning... %.0f / %.0f",health,maximum);
    dl->AddText({bar.x,bar.y+15},IM_COL32(240,245,250,255),healthText);
    const auto& settings=runtime_.settings();
    if (!runtime_.playerCamera().avatarHidden() && (settings.healthDisplayType!=2 || settings.nameDisplayDistance>0)) {
        const auto head=runtime_.rig().evaluate(runtime_.pose(),runtime_.characterRoot())[game::Head];
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
    if (runtime_.state() != game::RuntimeState::Paused) {
        // Close only our popup, never an unrelated editor modal.
        if (ImGui::BeginPopupModal("Play paused", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::CloseCurrentPopup(); ImGui::EndPopup(); ImGui::SetWindowFocus("Viewport");
        }
        return;
    }
    setGameplayCapture(false);
    ImGui::OpenPopup("Play paused");
    ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {.5f,.5f});
    if (ImGui::BeginPopupModal("Play paused", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (pausePage_ == 0) {
            // Marca como overlay no canto do modal: nada muda o fluxo de
            // widgets (os alvos de clique do layout ficam nas mesmas posições).
            const LobloxLogo& logo = LoadLobloxLogo();
            if (logo.ok) {
                constexpr float brandSide = 44.0f;
                const ImVec2 sprite = ImGui::CalcTextSize("LOBLOX / Local playtest");
                (void)sprite;
                const ImVec2 windowPos = ImGui::GetWindowPos();
                const float windowWidth = ImGui::GetWindowWidth();
                ImGui::GetWindowDrawList()->AddImage(
                    static_cast<ImTextureID>(logo.id),
                    ImVec2(windowPos.x + windowWidth - brandSide - 10.0f, windowPos.y + 10.0f),
                    ImVec2(windowPos.x + windowWidth - 10.0f, windowPos.y + 10.0f + brandSide));
            }
            ImGui::TextUnformatted("LOBLOX / Local playtest"); ImGui::Separator();
            if (ImGui::Button("Resume", {-1, 32})) { runtime_.resume(); ImGui::CloseCurrentPopup(); ImGui::SetWindowFocus("Viewport"); }
            if (ImGui::Button("Settings", {-1, 32})) pausePage_ = 1;
            if (ImGui::Button("Reset Character", {-1, 32})) {
                std::string error;
                if (!runtime_.resetCharacter(error)) setStatus(error, true);
            }
            if (ImGui::Button("Controls", {-1, 32})) pausePage_ = 2;
            if (ImGui::Button("Stop Test / Return to Editor", {-1, 32})) { stopPlay(); ImGui::CloseCurrentPopup(); }
        } else if (pausePage_ == 1) {
            ImGui::TextDisabled("Camera preferences for this play session");
            const float previousDistance = runtime_.settings().cameraDistance;
            CameraSettings(runtime_.settings());
            if (runtime_.settings().cameraDistance != previousDistance)
                runtime_.playerCamera().setDistance(runtime_.settings().cameraDistance);
            if (ImGui::Button("Back")) pausePage_ = 0;
        } else {
            ImGui::TextUnformatted("WASD: camera-relative movement\nSpace: jump (ground only)\nRight mouse: orbit camera\nWheel: zoom / first person\nEsc: pause / resume\nShift+F5: return to editor");
            if (ImGui::Button("Back")) pausePage_ = 0;
        }
        ImGui::EndPopup();
    }
}

void EditorUI::drawRigEditor() {
    if (!ImGui::FindWindowByName("Rig Editor")) {
        if (auto* properties = ImGui::FindWindowByName("Properties"))
            ImGui::SetNextWindowDockID(properties->DockId, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize({390, 550}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Rig Editor", &showRigEditor_)) { ImGui::End(); return; }
    if (ImGui::Button("Create R6")) insertR6();
    auto* rig = scene_.gameAssets().findRig(selectedRig_);
    if (ImGui::BeginCombo("Rig", rig ? rig->name.c_str() : "Select a rig")) {
        for (const auto& r : scene_.gameAssets().rigs)
            if (ImGui::Selectable(r.name.c_str(), r.id == selectedRig_)) selectRig(r.id);
        ImGui::EndCombo();
    }
    rig = scene_.gameAssets().findRig(selectedRig_);
    if (!rig) { ImGui::TextWrapped("Create an R6 or select one in Explorer."); ImGui::End(); return; }
    ImGui::InputText("Name", &rig->name);
    if (rig->name.empty()) rig->name = "R6Character";
    if (ImGui::Button("Use as Player")) scene_.gameAssets().defaultRig = rig->id;
    ImGui::SameLine();
    if (ImGui::Button("Focus rig")) camera_.focusOn(rig->root.position, 5);
    int mode = rigContext_ == RigContext::Edit ? 0 : 1;
    if (ImGui::RadioButton("Edit Rig", mode == 0)) { cancelRigGizmo(); rigContext_ = RigContext::Edit; previewPlaying_ = false; previewPose_ = {}; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Pose Rig", mode == 1)) { cancelRigGizmo(); rigContext_ = RigContext::Pose; previewPlaying_ = false; }
    ImGui::Checkbox("Show joint pivots / gizmo", &showJointPivots_);
    if (ImGui::Button("Reset pose")) { cancelRigGizmo(); previewPose_ = {}; previewPlaying_ = false; }
    ImGui::SameLine();
    if (ImGui::Button("Save project")) saveCurrentProject();
    if (ImGui::CollapsingHeader("Root / HumanoidRootPart", ImGuiTreeNodeFlags_DefaultOpen)) EditTransform("root", rig->root);
    if (ImGui::CollapsingHeader("Body parts", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < game::kBodyCount; ++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(rig->bodies[i].name.c_str(), selectedRigBody_ == i)) {
                cancelRigGizmo(); selectedRigBody_ = i;
                for (int j = 0; j < game::kJointCount; ++j) if (rig->joints[j].child == i) selectedJoint_ = j;
            }
            ImGui::PopID();
        }
        if (selectedRigBody_ >= 0) {
            auto& body = rig->bodies[selectedRigBody_];
            float color[]{body.color.r/255.f, body.color.g/255.f, body.color.b/255.f};
            if (ImGui::ColorEdit3("Body color", color) && std::isfinite(color[0]) &&
                std::isfinite(color[1]) && std::isfinite(color[2])) body.color = {
                static_cast<unsigned char>(color[0]*255), static_cast<unsigned char>(color[1]*255), static_cast<unsigned char>(color[2]*255), 255};
            int material = static_cast<int>(body.material);
            if (ImGui::BeginCombo("Material", PartMaterialName(body.material))) {
                for (int m = 0; m < PartMaterialCount(); ++m)
                    if (ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(m)), material == m)) body.material = PartMaterialFromIndex(m);
                ImGui::EndCombo();
            }
            auto size = body.size;
            ImGui::BeginDisabled(rigContext_ != RigContext::Edit);
            if (ImGui::DragFloat3("Dimensions", &size.x, .05f, .2f, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp)) rig->resizeBody(selectedRigBody_, size);
            if (ImGui::Button("Reset member size/color")) {
                auto original = game::MakeR6().bodies[selectedRigBody_];
                rig->resizeBody(selectedRigBody_, original.size); body.color = original.color; body.material = original.material;
            }
            ImGui::EndDisabled();
        }
    }
    if (ImGui::CollapsingHeader("Joints", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int j = 0; j < game::kJointCount; ++j) {
            if (ImGui::Selectable(rig->joints[j].name.c_str(), selectedJoint_ == j)) {
                cancelRigGizmo(); selectedJoint_ = j; selectedRigBody_ = rig->joints[j].child;
                setTool(ToolMode::Rotate, "rig_joint");
            }
        }
        auto& joint = rig->joints[selectedJoint_];
        if (rigContext_ == RigContext::Edit) {
            EditTransform("parent anchor", joint.parentAnchor);
            ImGui::TextDisabled("Child attachment"); EditTransform("child anchor", joint.childAnchor);
            ImGui::TextDisabled("Rest orientation (not animation)"); EditTransform("rest", rig->restPose[selectedJoint_]);
        } else {
            ImGui::TextDisabled("Pose only - rest offsets are unchanged");
            EditTransform("pose", previewPose_[selectedJoint_]);
        }
    }
    if (ImGui::Button("Reset entire rig")) ImGui::OpenPopup("Reset R6?");
    if (ImGui::BeginPopupModal("Reset R6?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Reset proportions, colors, attachments and rest pose?\nAnimation assets are kept.");
        if (ImGui::Button("Reset")) {
            cancelRigGizmo(); auto fresh = game::MakeR6(); fresh.id = rig->id; fresh.name = rig->name; fresh.root = rig->root;
            *rig = std::move(fresh); previewPose_ = {}; ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

void EditorUI::cancelRigGizmo(bool restore) {
    if (rigGizmo_.dragging() && restore && rigDragJoint_ >= 0) {
        if (auto* rig = scene_.gameAssets().findRig(rigDragId_)) {
            if (rigDragContext_ == RigContext::Edit) rig->restPose[rigDragJoint_] = rigDragInitial_;
            else previewPose_[rigDragJoint_] = rigDragInitial_;
        }
    }
    rigGizmo_.cancel(&jointProxy_, false); rigDragJoint_ = -1; rigDragId_.clear();
}
bool EditorUI::updateRigGizmo(const EditorInput& input, const InputRoute& route, Rectangle viewport,
                             bool hovered, bool blocked) {
    rigGizmo_.setLocalSpace(gizmoLocalSpace_);
    rigGizmo_.setSnap(moveSnap_, scaleSnap_, rotateSnapDegrees_);
    auto* rig = scene_.gameAssets().findRig(selectedRig_);
    if (!rig || tool_ != ToolMode::Rotate || !showJointPivots_ || blocked || input.escape) {
        const bool consumed = rigGizmo_.dragging(); cancelRigGizmo(); return consumed;
    }
    const auto world = rig->evaluate(previewPose_, rig->root);
    const auto& joint = rig->joints[selectedJoint_];
    auto base = game::Compose(joint.parent < 0 ? rig->root : world[joint.parent], joint.parentAnchor);
    if (rigContext_ != RigContext::Edit) base = game::Compose(base, rig->restPose[selectedJoint_]);
    auto& pose = rigContext_ == RigContext::Edit ? rig->restPose[selectedJoint_] : previewPose_[selectedJoint_];
    if (!rigGizmo_.dragging()) {
        jointProxy_.setPosition(game::Compose(base, {pose.position}).position);
        jointProxy_.setRotation(QuaternionMultiply(base.rotation, pose.rotation));
        jointProxy_.setSize({1,1,1});
    }
    const auto before = pose;
    GizmoInput gi{input.mouse, input.leftPressed, input.leftDown, IsWindowFocused(), input.shift};
    const auto result = rigGizmo_.update(ToolMode::Rotate, &jointProxy_, camera_, viewport, hovered,
        route.owner == InputOwner::Viewport || route.owner == InputOwner::Transform, gi);
    if (result.started) {
        rigDragInitial_ = before; rigDragContext_ = rigContext_; rigDragJoint_ = selectedJoint_; rigDragId_ = selectedRig_;
        previewPlaying_ = false; camera_.releaseCapture();
    }
    if (rigGizmo_.dragging() || (result.ended && !result.cancelled)) {
        pose.rotation = QuaternionNormalize(QuaternionMultiply(QuaternionInvert(base.rotation), jointProxy_.rotation()));
    }
    if (result.cancelled) pose = rigDragInitial_;
    if (result.ended) { rigDragJoint_ = -1; rigDragId_.clear(); }
    return result.consumed;
}

void EditorUI::drawGameSettings() {
    if (!ImGui::FindWindowByName("Game Settings")) {
        if (auto* properties = ImGui::FindWindowByName("Properties"))
            ImGui::SetNextWindowDockID(properties->DockId, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize({420, 540}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Settings", &showGameSettings_)) { ImGui::End(); return; }
    auto& a = scene_.gameAssets(); auto& p = a.player;
    const auto previousSettings = p;
    if (a.rigs.empty()) {
        ImGui::TextWrapped("No character asset yet. Play uses a temporary R6; create one to customize and save it.");
        if (ImGui::Button("Create default R6")) insertR6();
    }
    if (ImGui::BeginCombo("Default character", a.defaultRig.c_str())) {
        for (const auto& r : a.rigs) if (ImGui::Selectable(r.name.c_str(), r.id == a.defaultRig)) a.defaultRig = r.id;
        ImGui::EndCombo();
    }
    AnimationChoice("Walk animation", p.walkAnimation, a); AnimationChoice("Idle animation", p.idleAnimation, a);
    AnimationChoice("Jump animation",p.jumpAnimation,a);
    ImGui::SliderFloat("WalkSpeed (units/s)", &p.walkSpeed, .1f, 100);
    ImGui::SliderFloat("Turn response", &p.turnSpeed, .1f, 50);
    ImGui::SliderFloat("Acceleration", &p.acceleration, 1, 500);
    ImGui::SliderFloat("Deceleration", &p.deceleration, 1, 500);
    ImGui::SliderFloat("JumpHeight (units)", &p.jumpHeight, .1f, 50);
    ImGui::SliderFloat("Gravity (units/s2)", &p.gravity, 1, 500);
    ImGui::Checkbox("AutoRotate", &p.autoRotate);
    ImGui::SliderFloat("MaxHealth", &p.maxHealth, 1, 10000);
    p.health = std::clamp(p.health, 0.f, p.maxHealth);
    ImGui::SliderFloat("Initial Health", &p.health, 0, p.maxHealth);
    ImGui::SliderFloat("Respawn delay (s)",&p.respawnDelay,.1f,30);
    ImGui::Combo("Overhead health",&p.healthDisplayType,"AlwaysOn\0DisplayWhenDamaged\0AlwaysOff\0");
    ImGui::SliderFloat("Health display distance",&p.healthDisplayDistance,0,500);
    ImGui::SliderFloat("Name display distance",&p.nameDisplayDistance,0,500);
    ImGui::SliderFloat("Capsule radius", &p.collisionRadius, .2f, 3);
    p.collisionHeight = std::max(p.collisionHeight, p.collisionRadius*2);
    ImGui::SliderFloat("Capsule total height", &p.collisionHeight, 2*p.collisionRadius, 15);
    ImGui::SeparatorText("Project camera defaults"); CameraSettings(p);
    std::string error;
    if (!p.validate(error)) { p = previousSettings; setStatus(error, true); }
    if (ImGui::Button("Save game settings")) saveCurrentProject();
    ImGui::End();
}

void EditorUI::drawAnimationEditor() {
    if (!ImGui::FindWindowByName("Animation Editor")) {
        if (auto* view = ImGui::FindWindowByName("Viewport"); view && view->DockId != 0) {
            ImGuiID upper;
            const ImGuiID lower = ImGui::DockBuilderSplitNode(view->DockId, ImGuiDir_Down, .5f, nullptr, &upper);
            ImGui::DockBuilderDockWindow("Viewport", upper);
            ImGui::DockBuilderDockWindow("Animation Editor", lower);
            ImGui::DockBuilderFinish(ImGui::GetID("LobloxDockspace"));
        }
    }
    ImGui::SetNextWindowSize({800, 600}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Animation Editor", &showAnimationEditor_)) { ImGui::End(); return; }
    auto& assets = scene_.gameAssets();
    if (assets.animations.empty()) {
        ImGui::TextUnformatted("Create an R6 to generate the editable R6_Walk and R6_Idle.");
        if (ImGui::Button("Create animated R6")) { insertR6(); openAnimation(assets.player.walkAnimation); }
        ImGui::End(); return;
    }
    if (animationSource_.empty()) openAnimation(assets.player.walkAnimation);
    if (ImGui::BeginCombo("Animation", animationDraft_.name.c_str())) {
        for (const auto& clip : assets.animations)
            if (ImGui::Selectable(clip.name.c_str(), clip.id == animationSource_)) {
                if (animationDirty_) setStatus("Save edits or use Revert before opening another animation.", true);
                else openAnimation(clip.id);
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const std::string animationUri="anim://"+animationDraft_.id;
    if (ImGui::SmallButton("Copy ID")) { ImGui::SetClipboardText(animationUri.c_str());setStatus("ID copiado: "+animationUri); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nRig: %s | %.3f s",animationUri.c_str(),animationDraft_.rigType.c_str(),animationDraft_.duration);
    if (ImGui::Button("New")) {
        if (animationDirty_) setStatus("Save or revert the current animation first.", true);
        else {
            auto clip = game::MakeIdle(); clip.id = "animation-" + diag::newCorrelationId(); clip.name = "New Animation";
            assets.animations.push_back(clip); openAnimation(clip.id);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        auto copy = animationDraft_; copy.id = "animation-" + diag::newCorrelationId(); copy.name += " Copy";
        assets.animations.push_back(copy); openAnimation(copy.id);
    }
    ImGui::SameLine(); if (ImGui::Button("Save Animation + Project")) saveAnimation();
    ImGui::SameLine(); if (ImGui::Button("Revert")) openAnimation(animationSource_);
    ImGui::SameLine(); if (ImGui::Button("Use as Walk") && saveAnimation()) {
        assets.player.walkAnimation = animationSource_; saveCurrentProject();
    }
    const bool compact = ImGui::IsWindowDocked();
    if (!compact || ImGui::CollapsingHeader("Clip settings: name / duration / FPS / loop / priority")) {
        animationDirty_ |= ImGui::InputText("Name", &animationDraft_.name);
        float duration = animationDraft_.duration;
        if (ImGui::DragFloat("Duration (s)", &duration, .05f, .05f, 120, "%.2f", ImGuiSliderFlags_AlwaysClamp) &&
            std::isfinite(duration) && duration >= .05f && duration <= 120) {
            const float scale = duration/animationDraft_.duration;
            for (auto& track : animationDraft_.tracks) for (auto& key : track) key.time *= scale;
            previewTime_ *= scale; animationDraft_.duration = duration; animationDirty_ = true;
        }
        if (ImGui::SliderInt("Authoring FPS", &animationDraft_.fps, 1, 120)) animationDirty_ = true;
        animationDraft_.fps = std::clamp(animationDraft_.fps, 1, 240);
        if (ImGui::Checkbox("Loop", &animationDraft_.loop)) animationDirty_ = true;
        int priority = static_cast<int>(animationDraft_.priority);
        if (ImGui::Combo("Priority", &priority, "Idle\0Movement\0Action\0")) {
            animationDraft_.priority = static_cast<game::AnimationPriority>(priority); animationDirty_ = true;
        }
    }
    auto* rig = assets.findRig(selectedRig_);
    if (!rig) { ImGui::TextDisabled("Select a rig in Explorer to preview."); ImGui::End(); return; }
    if (ImGui::Button(previewPlaying_ ? "Pause preview" : "Play preview")) {
        cancelRigGizmo(); rigContext_ = RigContext::Animation; previewPlaying_ = !previewPlaying_;
    }
    ImGui::SameLine(); if (ImGui::Button("Stop preview")) { previewPlaying_ = false; previewTime_ = 0; previewPose_ = animationDraft_.sample(0); }
    ImGui::SameLine(); if (ImGui::Button("|<")) { previewTime_ = 0; previewPose_ = animationDraft_.sample(0); }
    ImGui::SameLine(); if (ImGui::Button(">|")) { previewTime_ = animationDraft_.duration; previewPose_ = animationDraft_.sample(previewTime_); }
    if (previewPlaying_) {
        previewTime_ += std::min(GetFrameTime(), .1f);
        if (previewTime_ > animationDraft_.duration) {
            if (animationDraft_.loop) previewTime_ = std::fmod(previewTime_, animationDraft_.duration);
            else { previewTime_ = animationDraft_.duration; previewPlaying_ = false; }
        }
        previewPose_ = animationDraft_.sample(previewTime_);
    }
    if (compact) ImGui::SetNextItemWidth(std::max(100.f, ImGui::GetContentRegionAvail().x-280));
    bool seek = ImGui::SliderFloat("Time / scrub", &previewTime_, 0, animationDraft_.duration, "%.3f s");
    if (compact) { ImGui::SameLine(); ImGui::SetNextItemWidth(70); }
    seek |= ImGui::InputFloat("Exact time (s)", &previewTime_, 0, 0, "%.3f");
    previewTime_ = std::isfinite(previewTime_) ? std::clamp(previewTime_, 0.f, animationDraft_.duration) : 0;
    if (seek) {
        cancelRigGizmo(); previewPlaying_ = false; rigContext_ = RigContext::Animation;
        previewPose_ = animationDraft_.sample(previewTime_);
    }
    ImGui::TextDisabled("Preview: selected rig in main viewport. Drag diamonds to retime; click track to scrub.");
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
    ImGui::Combo("Interpolation", &keyInterpolation_, "Linear\0Ease in/out\0Constant\0");
    if (ImGui::Button("Insert / Update key")) {
        animationDraft_.setKey(selectedJoint_, {previewTime_, previewPose_[selectedJoint_], static_cast<game::Interpolation>(keyInterpolation_)});
        animationDirty_ = true;
    }
    ImGui::SameLine(); if (ImGui::Button("Remove key")) animationDirty_ |= animationDraft_.eraseKey(selectedJoint_, previewTime_);
    ImGui::SameLine(); if (ImGui::Button("Copy pose/key")) { copiedKey_ = previewPose_[selectedJoint_]; hasCopiedKey_ = true; }
    ImGui::SameLine(); ImGui::BeginDisabled(!hasCopiedKey_);
    if (ImGui::Button("Paste key")) { animationDraft_.setKey(selectedJoint_, {previewTime_, copiedKey_}); animationDirty_ = true; }
    ImGui::EndDisabled();
    if (ImGui::Button("Reset selected pose")) previewPose_[selectedJoint_] = {};
    ImGui::SameLine(); ImGui::TextDisabled("%s / preview is separate from saved asset", animationDirty_ ? "Unsaved keys" : "Saved");
    ImGui::End();
}
} // namespace loblox

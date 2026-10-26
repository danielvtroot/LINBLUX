#include "editor/TransformSession.h"

#include "core/Diagnostics.h"
#include "physics/PhysicsWorld.h"

namespace loblox {

TransformSession::TransformSession(Gizmo& gizmo, PhysicsWorld& physics)
    : gizmo_(gizmo), physics_(physics) {}

TransformSession::~TransformSession() {
    // Se a UI for destruida durante um arraste, nenhum corpo pode continuar
    // suspenso. O Gizmo pode ja ter restaurado a Part antes da destruicao.
    if (gizmo_.dragging()) {
        endDiagnostics(true, gizmo_.sessionPartId(), "session_destroyed");
    }
    releasePhysics(gizmo_.sessionPartId());
}

GizmoUpdateResult TransformSession::update(ToolMode mode, Part* part,
                                           const EditorCamera& camera, Rectangle viewport,
                                           bool hovered, bool allowInput,
                                           const GizmoInput& input,bool physical) {
    const GizmoUpdateResult result =
        gizmo_.update(mode, part, camera, viewport, hovered, allowInput, input);
    if (result.error && result.error != lastError_) {
        diag::Event event;
        event.level = diag::Level::Warning;
        event.subsystem = "gizmo";
        event.name = "drag_update_error";
        event.correlationId = correlationId_;
        event.entityId = partId();
        event.result = "unchanged";
        event.error = result.error;
        diag::log(event);
    }
    lastError_ = result.error;
    if (result.started) {
        physical_=physical;
        if(physical_)physics_.setPartEditing(result.partId, true);
        beginDiagnostics(mode);
    }
    if(physical_&&part&&(gizmo_.dragging()||result.ended))physics_.applyChanges(part);
    if (result.ended) {
        if (physical_&&part && part->id() == result.partId) physics_.applyChanges(part);
        releasePhysics(result.partId);
        endDiagnostics(result.cancelled, result.partId,
                       result.cancelled ? "context_changed" : "mouse_release");
    }
    return result;
}

void TransformSession::cancel(Part* part, bool restoreTransform, const char* reason) {
    const InstanceId id = gizmo_.sessionPartId();
    if (gizmo_.dragging()) {
        endDiagnostics(true, id, reason);
    }
    gizmo_.cancel(part, restoreTransform);
    if (physical_&&id != kInvalidInstanceId && part && part->id() == id) physics_.applyChanges(part);
    releasePhysics(id);
}

void TransformSession::commit(Part* part, const char* reason) {
    const InstanceId id = gizmo_.sessionPartId();
    if (gizmo_.dragging()) endDiagnostics(false, id, reason);
    gizmo_.cancel(part, false);
    if (physical_&&id != kInvalidInstanceId && part && part->id() == id) physics_.applyChanges(part);
    releasePhysics(id);
}

void TransformSession::releasePhysics(InstanceId id) {
    if (physical_&&id != kInvalidInstanceId) {
        physics_.setPartEditing(id, false);
    }
}

void TransformSession::beginDiagnostics(ToolMode mode) {
    correlationId_ = diag::newCorrelationId();
    startedAt_ = std::chrono::steady_clock::now();
    diag::Event event;
    event.subsystem = "gizmo";
    event.name = "drag_begin";
    event.correlationId = correlationId_;
    event.entityId = gizmo_.sessionPartId();
    event.context = {{"tool", ToolModeName(mode)}, {"consumer", "gizmo"},
                     {"handle", static_cast<int>(gizmo_.hoveredHandle())}};
    diag::log(event);
}

void TransformSession::endDiagnostics(bool cancelled, InstanceId id, const char* reason) {
    diag::Event event;
    event.level = cancelled ? diag::Level::Warning : diag::Level::Info;
    event.subsystem = "gizmo";
    event.name = cancelled ? "drag_cancel" : "drag_commit";
    event.correlationId = correlationId_;
    event.entityId = id;
    event.durationMs = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - startedAt_).count();
    event.result = cancelled ? "cancelled" : "committed";
    event.context = {{"reason", reason}};
    diag::log(event);
    correlationId_.clear();
}

}  // namespace loblox

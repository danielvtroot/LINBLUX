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
        endDiagnostics(true, gizmo_.sessionPartId());
    }
    releasePhysics(gizmo_.sessionPartId());
}

GizmoUpdateResult TransformSession::update(ToolMode mode, Part* part,
                                           const EditorCamera& camera, Rectangle viewport,
                                           bool hovered, bool allowInput,
                                           const GizmoInput& input) {
    const GizmoUpdateResult result =
        gizmo_.update(mode, part, camera, viewport, hovered, allowInput, input);
    if (result.started) {
        physics_.setPartEditing(result.partId, true);
        beginDiagnostics(mode);
    }
    if (result.ended) {
        releasePhysics(result.partId);
        endDiagnostics(result.cancelled, result.partId);
    }
    return result;
}

void TransformSession::cancel(Part* part, bool restoreTransform) {
    const InstanceId id = gizmo_.sessionPartId();
    if (gizmo_.dragging()) {
        endDiagnostics(true, id);
    }
    gizmo_.cancel(part, restoreTransform);
    releasePhysics(id);
}

void TransformSession::commit(Part* part) {
    const InstanceId id = gizmo_.sessionPartId();
    if (gizmo_.dragging()) endDiagnostics(false, id);
    gizmo_.cancel(part, false);
    releasePhysics(id);
}

void TransformSession::releasePhysics(InstanceId id) {
    if (id != kInvalidInstanceId) {
        physics_.setPartEditing(id, false);
    }
}

void TransformSession::beginDiagnostics(ToolMode mode) {
    correlationId_ = diag::newCorrelationId();
    startedAt_ = std::chrono::steady_clock::now();
    diag::Event event;
    event.subsystem = "transform";
    event.name = "drag_begin";
    event.correlationId = correlationId_;
    event.entityId = gizmo_.sessionPartId();
    event.context = {{"tool", static_cast<int>(mode)}};
    diag::log(event);
}

void TransformSession::endDiagnostics(bool cancelled, InstanceId id) {
    diag::Event event;
    event.level = cancelled ? diag::Level::Warning : diag::Level::Info;
    event.subsystem = "transform";
    event.name = cancelled ? "drag_cancel" : "drag_commit";
    event.correlationId = correlationId_;
    event.entityId = id;
    event.durationMs = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - startedAt_).count();
    event.result = cancelled ? "cancelled" : "committed";
    diag::log(event);
    correlationId_.clear();
}

}  // namespace loblox

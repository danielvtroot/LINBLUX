#include "editor/TransformSession.h"

#include "physics/PhysicsWorld.h"

namespace loblox {

TransformSession::TransformSession(Gizmo& gizmo, PhysicsWorld& physics)
    : gizmo_(gizmo), physics_(physics) {}

TransformSession::~TransformSession() {
    // Se a UI for destruida durante um arraste, nenhum corpo pode continuar
    // suspenso. O Gizmo pode ja ter restaurado a Part antes da destruicao.
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
    }
    if (result.ended) {
        releasePhysics(result.partId);
    }
    return result;
}

void TransformSession::cancel(Part* part, bool restoreTransform) {
    const InstanceId id = gizmo_.sessionPartId();
    gizmo_.cancel(part, restoreTransform);
    releasePhysics(id);
}

void TransformSession::releasePhysics(InstanceId id) {
    if (id != kInvalidInstanceId) {
        physics_.setPartEditing(id, false);
    }
}

}  // namespace loblox

#pragma once

#include <chrono>
#include <string>

#include "core/Math.h"
#include "editor/Gizmo.h"
#include "editor/Tool.h"
#include "objects/Part.h"

namespace loblox {

class EditorCamera;
class PhysicsWorld;

// Dono unico da transicao entre um arraste do gizmo e a suspensao do corpo
// fisico. O Gizmo continua sendo a fonte de verdade do estado do arraste;
// esta classe garante que cada inicio termine em confirmacao, cancelamento ou
// liberacao explicita, sem uma flag paralela na UI.
class TransformSession {
public:
    TransformSession(Gizmo& gizmo, PhysicsWorld& physics);
    ~TransformSession();

    TransformSession(const TransformSession&) = delete;
    TransformSession& operator=(const TransformSession&) = delete;

    GizmoUpdateResult update(ToolMode mode, Part* part, const EditorCamera& camera,
                             Rectangle viewport, bool hovered, bool allowInput,
                             const GizmoInput& input);
    void cancel(Part* part, bool restoreTransform, const char* reason = "cancel");
    void commit(Part* part, const char* reason = "commit");

    bool active() const { return gizmo_.dragging(); }
    InstanceId partId() const { return gizmo_.sessionPartId(); }
    Gizmo& gizmo() { return gizmo_; }
    const Gizmo& gizmo() const { return gizmo_; }

private:
    void releasePhysics(InstanceId id);
    void beginDiagnostics(ToolMode mode);
    void endDiagnostics(bool cancelled, InstanceId id, const char* reason);

    Gizmo& gizmo_;
    PhysicsWorld& physics_;
    std::string correlationId_;
    const char* lastError_ = nullptr;
    std::chrono::steady_clock::time_point startedAt_{};
};

}  // namespace loblox

#pragma once
#include <memory>
#include "editor/EditHistory.h"
#include "editor/TransformSession.h"
#include "scene/Scene.h"

namespace loblox {
// Reuses the normal gizmo; the proxy is never adopted by Scene or Bullet.
class ModelTransformSession {
public:
    ModelTransformSession(Scene& scene,PhysicsWorld& physics,TransformSession& transform,EditHistory& history);
    ~ModelTransformSession();
    GizmoUpdateResult update(ToolMode mode,Instance* model,const EditorCamera& camera,Rectangle viewport,
                            bool hovered,bool allowInput,const GizmoInput& input);
    bool active() const { return edit_.id!=0; }
    const Part* proxy() const { return proxy_.get(); }
    void end(bool restore,const char* reason);
    bool applyProperty(InstanceId model,const ModelFrame& target,bool pivotOnly,std::string& error);
    bool applyEdit(const TransformEdit& edit,bool after,std::string& error);
private:
    bool capture(InstanceId model,TransformEdit& edit,std::string& error);
    void complete(TransformEdit& edit);
    void release();
    Scene& scene_;PhysicsWorld& physics_;TransformSession& transform_;EditHistory& history_;
    std::unique_ptr<Part> proxy_;
    TransformEdit edit_;
    ModelFrame startFrame_;
    Transform startProxy_;
};
}

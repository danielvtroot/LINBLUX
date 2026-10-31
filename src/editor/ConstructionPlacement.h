#pragma once
#include "editor/EditHistory.h"
#include "editor/GizmoMath.h"
#include "scene/Scene.h"
namespace loblox {
class PhysicsWorld; class ModelTransformSession;
// A proposal only: hover never changes Scene, Bullet, persistence or history.
class ConstructionPlacement {
public:
    bool begin(const Scene& scene,InstanceId id,std::string& error);
    void cancel(){id_=0;before_.clear();preview_.clear();contact_={};}
    bool active() const{return id_!=0;}
    InstanceId id() const{return id_;}
    const auto& preview() const{return preview_;}
    const auto& contact() const{return contact_;}
    const ModelFrame& frame() const{return frame_;}
    void update(const Scene& scene,Ray ray,gizmomath::SnapFeature feature,float offset,float grid,bool align);
    bool commit(Scene& scene,PhysicsWorld& physics,ModelTransformSession& models,EditHistory& history,std::string& error);
    Vector3 duplicateStep(int axis,float gap) const;
private:
    bool unchanged(const Scene& scene) const;
    InstanceId id_=0;bool model_=false;
    ModelFrame frame_{};
    std::vector<gizmomath::SurfaceBox> before_,preview_;
    gizmomath::SurfaceSnap contact_{};
};
}

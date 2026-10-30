#include "editor/ConstructionPlacement.h"
#include "editor/ModelTransformSession.h"
#include "physics/PhysicsWorld.h"
#include <cmath>
namespace loblox {
bool ConstructionPlacement::begin(const Scene& scene,InstanceId id,std::string& error){
    cancel();const auto* selection=scene.find(id);
    if(!selection||(!selection->isPart()&&selection->instanceClass()!=InstanceClass::Model)){
        error="Selecione uma Part ou Modelo para encaixar.";return false;}
    if(!scene.isWorldObject(selection)){error="Crie uma cópia no Mundo antes de encaixar um objeto armazenado.";return false;}
    model_=selection->instanceClass()==InstanceClass::Model;
    for(const auto* part:scene.parts())if(part==selection||selection->isAncestorOf(part)){
        if(part->shape()!=PartShape::Block){cancel();error="Encaixe disponível para caixas; esfera: use Mover.";return false;}
        before_.push_back({{part->position(),part->rotation(),part->size()},part->id()});
    }
    if(before_.empty()){error="Seleção sem peças.";return false;}
    if(model_){if(!scene.modelFrame(id,frame_,error)){cancel();return false;}}
    else {frame_={};frame_.position=before_[0].pose.position;frame_.rotation=before_[0].pose.rotation;}
    id_=id;error.clear();return true;
}
bool ConstructionPlacement::unchanged(const Scene& scene) const {
    const auto* selection=scene.find(id_);if(!selection||!scene.isWorldObject(selection))return false;
    size_t count=0;for(const auto* part:scene.parts())if(part==selection||selection->isAncestorOf(part))++count;
    if(count!=before_.size())return false;
    for(const auto& box:before_){const auto* part=scene.findPart(box.id);
        if(!part||(part!=selection&&!selection->isAncestorOf(part))||!SameTransform(part->transform(),{box.pose.position,box.pose.rotation,box.pose.size}))return false;}
    if(model_){ModelFrame current;std::string error;if(!scene.modelFrame(id_,current,error)||
        !SameTransform({current.position,current.rotation,{current.scale,current.scale,current.scale}},
                       {frame_.position,frame_.rotation,{frame_.scale,frame_.scale,frame_.scale}}))return false;}
    return true;
}
void ConstructionPlacement::update(const Scene& scene,Ray ray,gizmomath::SnapFeature feature,float offset,float grid,bool align){
    if(!active())return;if(!unchanged(scene)){cancel();return;}
    std::vector<gizmomath::SurfaceBox> targets;float nearestSphere=INFINITY;
    const auto* selection=scene.find(id_);
    for(const auto* part:scene.worldParts())if(part!=selection&&!selection->isAncestorOf(part)&&part->color().a>0){
        if(part->shape()==PartShape::Block)targets.push_back({{part->position(),part->rotation(),part->size()},part->id()});
        else {const auto hit=part->intersect(ray);if(hit.hit)nearestSphere=std::min(nearestSphere,hit.distance);}
    }
    contact_=gizmomath::SnapUnderCursor(before_,targets,frame_.position,ray,feature,align,frame_.rotation,contact_.target,offset,grid,true);
    preview_.clear();if(!contact_.target)return;
    if(nearestSphere<Vector3Distance(ray.position,contact_.point)-.01f){contact_={};contact_.reason="Aponte para uma superfície plana.";return;}
    for(auto box:before_){box.pose.position=Vector3Add(Vector3Add(frame_.position,Vector3RotateByQuaternion(Vector3Subtract(box.pose.position,frame_.position),contact_.rotationDelta)),contact_.correction);
        box.pose.rotation=QuaternionNormalize(QuaternionMultiply(contact_.rotationDelta,box.pose.rotation));preview_.push_back(box);}
}
bool ConstructionPlacement::commit(Scene& scene,PhysicsWorld& physics,ModelTransformSession& models,EditHistory& history,std::string& error){
    if(!active()||!contact_.snapped||!unchanged(scene)){error="Prévia inválida; aponte novamente para a superfície.";return false;}
    if(model_){auto target=frame_;target.position=Vector3Add(target.position,contact_.correction);
        target.rotation=QuaternionNormalize(QuaternionMultiply(contact_.rotationDelta,target.rotation));
        if(!models.applyProperty(id_,target,false,error))return false;
    }else {auto* part=scene.findPart(id_);const auto before=part->transform();const auto& pose=preview_.front().pose;
        part->setTransform({pose.position,pose.rotation,pose.size});history.record(id_,before,part->transform());physics.syncScene(scene);}
    cancel();error.clear();return true;
}
Vector3 ConstructionPlacement::duplicateStep(int axis,float gap) const {
    if(!active()||axis<0||axis>2||!std::isfinite(gap))return {};
    const auto direction=gizmomath::Axis(axis,frame_.rotation,true);float low=INFINITY,high=-INFINITY;
    for(const auto& box:before_)for(const auto point:OrientedBox{box.pose.position,box.pose.rotation,HalfExtents(box.pose.size)}.corners()){
        const float projection=Vector3DotProduct(Vector3Subtract(point,frame_.position),direction);low=std::min(low,projection);high=std::max(high,projection);}
    return Vector3Scale(direction,high-low+gap);
}
}

#include "editor/ModelTransformSession.h"
#include "physics/PhysicsWorld.h"
#include <algorithm>
#include <cmath>

namespace loblox {
ModelTransformSession::ModelTransformSession(Scene& scene,PhysicsWorld& physics,TransformSession& transform,EditHistory& history)
    :scene_(scene),physics_(physics),transform_(transform),history_(history){}
ModelTransformSession::~ModelTransformSession(){if(active())end(true,"model_session_destroyed");}
bool ModelTransformSession::capture(InstanceId id,TransformEdit& edit,std::string& error) {
    auto* model=scene_.find(id);if(!model||model->instanceClass()!=InstanceClass::Model){error="Modelo inexistente.";return false;}
    edit={};edit.id=id;
    for(const auto* part:scene_.parts())if(model->isAncestorOf(part))edit.parts.push_back({part->id(),part->transform(),part->transform()});
    for(const auto& object:scene_.objects())if(object->instanceClass()==InstanceClass::Model&&(object.get()==model||model->isAncestorOf(object.get()))) {
        ModelFrame frame;if(!scene_.modelFrame(object->id(),frame,error))return false;
        const bool explicitPivot=static_cast<const Container&>(*object).hasModelFrame();
        edit.pivots.push_back({object->id(),frame,frame,explicitPivot,explicitPivot});
    }
    error.clear();return true;
}
void ModelTransformSession::complete(TransformEdit& edit) {
    for(auto& part:edit.parts)if(const auto* object=scene_.findPart(part.id))part.after=object->transform();
    for(auto& pivot:edit.pivots)if(const auto* object=scene_.find(pivot.id);object&&object->instanceClass()==InstanceClass::Model){
        std::string ignored;scene_.modelFrame(pivot.id,pivot.after,ignored);pivot.afterExplicit=static_cast<const Container&>(*object).hasModelFrame();}
}
bool ModelTransformSession::applyEdit(const TransformEdit& edit,bool after,std::string& error) {
    const auto* model=scene_.find(edit.id);
    if(!model||model->instanceClass()!=InstanceClass::Model){error="O Modelo desta operação não existe mais.";return false;}
    for(const auto& part:edit.parts)if(!scene_.findPart(part.id)||!model->isAncestorOf(scene_.findPart(part.id))){error="As peças do Modelo mudaram; a operação não foi aplicada.";return false;}
    for(const auto& pivot:edit.pivots){const auto* node=scene_.find(pivot.id);if(!node||node->instanceClass()!=InstanceClass::Model||(node!=model&&!model->isAncestorOf(node))){error="A hierarquia do Modelo mudou.";return false;}}
    for(const auto& part:edit.parts)scene_.findPart(part.id)->setTransform(after?part.after:part.before);
    for(const auto& pivot:edit.pivots){const bool explicitPivot=after?pivot.afterExplicit:pivot.beforeExplicit;
        if(explicitPivot)scene_.setModelPivot(pivot.id,after?pivot.after:pivot.before,error);else scene_.clearModelPivot(pivot.id,error);}
    physics_.syncScene(scene_);error.clear();return true;
}
bool ModelTransformSession::applyProperty(InstanceId id,const ModelFrame& target,bool pivotOnly,std::string& error) {
    TransformEdit edit;if(!capture(id,edit,error))return false;
    if(!(pivotOnly?scene_.setModelPivot(id,target,error):scene_.transformModel(id,target,error)))return false;
    complete(edit);history_.record(std::move(edit));physics_.syncScene(scene_);return true;
}
void ModelTransformSession::release(){for(const auto& part:edit_.parts)physics_.setPartEditing(part.id,false);edit_={};}
void ModelTransformSession::end(bool restore,const char* reason) {
    if(!active())return;
    if(restore)transform_.cancel(proxy_.get(),true,reason);else transform_.commit(proxy_.get(),reason);
    std::string ignored;
    if(restore)applyEdit(edit_,false,ignored);else {complete(edit_);history_.record(edit_);}
    release();
}
GizmoUpdateResult ModelTransformSession::update(ToolMode mode,Instance* model,const EditorCamera& camera,
    Rectangle viewport,bool hovered,bool allowInput,const GizmoInput& input) {
    if(active()&&(!model||model->id()!=edit_.id)){end(true,"model_selection_changed");return {};}
    if(!model||model->instanceClass()!=InstanceClass::Model)return {};
    std::string error;
    if(!active()) {
        ModelFrame frame;if(!scene_.modelFrame(model->id(),frame,error))return {};
        if(!proxy_||proxy_->id()!=model->id())proxy_=std::make_unique<Part>(model->id(),"Pivô do Modelo",PartShape::Block);
        Vector3 half{.5f,.5f,.5f};
        const auto inverse=QuaternionInvert(frame.rotation);
        for(const auto* part:scene_.parts())if(model->isAncestorOf(part))for(int bits=0;bits<8;++bits){const auto h=part->halfExtents();
            const auto world=Vector3Add(part->position(),Vector3RotateByQuaternion({(bits&1)?h.x:-h.x,(bits&2)?h.y:-h.y,(bits&4)?h.z:-h.z},part->rotation()));
            const auto local=Vector3RotateByQuaternion(Vector3Subtract(world,frame.position),inverse);
            half={std::max(half.x,std::fabs(local.x)),std::max(half.y,std::fabs(local.y)),std::max(half.z,std::fabs(local.z))};}
        proxy_->setTransform({frame.position,frame.rotation,Vector3Scale(half,2)});
    }
    auto result=transform_.update(mode,proxy_.get(),camera,viewport,hovered,allowInput,input,false);
    if(result.started) {
        if(!capture(model->id(),edit_,error)){transform_.cancel(proxy_.get(),true,"model_capture_failed");return {};}
        scene_.modelFrame(model->id(),startFrame_,error);startProxy_=transform_.gizmo().startTransform();
        physics_.syncScene(scene_);for(const auto& part:edit_.parts)physics_.setPartEditing(part.id,true);
    }
    if(active()&&(transform_.active()||result.ended)) {
        if(result.cancelled){applyEdit(edit_,false,error);release();return result;}
        const auto pose=proxy_->transform();
        if(SameTransform(pose,startProxy_)){applyEdit(edit_,false,error);if(result.ended)release();return result;}
        ModelFrame target=startFrame_;target.position=pose.position;if(mode==ToolMode::Rotate)target.rotation=pose.rotation;
        if(mode==ToolMode::Scale){float ratio=1,change=0;for(int i=0;i<3;++i){const float value=(&pose.size.x)[i]/(&startProxy_.size.x)[i];if(std::fabs(value-1)>change){change=std::fabs(value-1);ratio=value;}}target.scale*=ratio;}
        if(!applyEdit(edit_,false,error)||!scene_.transformModel(edit_.id,target,error)){
            end(true,"model_transform_rejected");result.ended=true;result.cancelled=true;result.consumed=true;return result;}
        physics_.syncScene(scene_);
        if(mode==ToolMode::Scale){auto display=proxy_->transform();display.size=Vector3Scale(startProxy_.size,target.scale/startFrame_.scale);proxy_->setTransform(display);}
        if(result.ended){complete(edit_);history_.record(edit_);release();}
    }
    return result;
}
}

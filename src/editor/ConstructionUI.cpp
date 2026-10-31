#include "editor/EditorUI.h"
#include "rendering/EditorCamera.h"
#include "physics/PhysicsWorld.h"
#include "imgui.h"
#include "rlgl.h"
namespace loblox {
void EditorUI::beginSurfacePlacement(){
    endGizmoSession(true,"surface_placement");cancelRigGizmo();clearTransformReadout();
    std::string error;const auto* selection=scene_.selection();
    if(!placement_.begin(scene_,selection?selection->id():0,error)){setStatus(error,true);return;}
    setStatus("Encaixar à superfície: aponte, confira a prévia e clique. ESC cancela.");
}
bool EditorUI::updateSurfacePlacement(const EditorInput& input,Rectangle view,bool allow){
    if(!placement_.active())return false;
    if(!scene_.selection()||scene_.selection()->id()!=placement_.id()||input.escape||runtime_.active()){
        placement_.cancel();return true;}
    if(allow&&CheckCollisionPointRec(input.mouse,view)){
        placement_.update(scene_,camera_.rayFromScreen({input.mouse.x-view.x,input.mouse.y-view.y},{view.width,view.height}),
            snapFeature_,placementOffset_,snapEnabled_?moveSnap_:0,alignSnapNormal_);
        if(input.leftPressed){std::string error;const auto contact=placement_.contact();
            if(placement_.commit(scene_,physics_,modelTransform_,history_,error)){
                assemblyStep_=contact.correction;setStatus("Encaixe confirmado. Ctrl+Z desfaz.");
            }else {const auto* blocker=scene_.find(contact.blocker);setStatus(blocker?"Sobreposição com "+blocker->name():error,true);}}
    }
    return true; // This click must never select the destination or start a gizmo.
}
void EditorUI::drawSurfacePlacement(Rectangle view){
    if(!placement_.active())return;
    const auto& contact=placement_.contact();const Color color=contact.snapped?Color{65,207,159,65}:Color{237,65,80,65};
    BeginMode3D(camera_.camera());rlDrawRenderBatchActive();rlEnableDepthTest();rlDisableDepthMask();rlDisableBackfaceCulling();
    const int faces[6][4]={{0,4,7,3},{1,2,6,5},{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2}};
    for(const auto& box:placement_.preview()){
        const auto corners=OrientedBox{box.pose.position,box.pose.rotation,HalfExtents(box.pose.size)}.corners();
        for(const auto& face:faces){DrawTriangle3D(corners[face[0]],corners[face[1]],corners[face[2]],color);DrawTriangle3D(corners[face[0]],corners[face[2]],corners[face[3]],color);
            for(int i=0;i<4;++i)DrawLine3D(corners[face[i]],corners[face[(i+1)%4]],{color.r,color.g,color.b,180});}
    }
    if(const auto* target=scene_.findPart(contact.target)){
        const auto corners=target->bounds().corners();
        for(const auto& face:faces){bool matching=true;for(int i:face)matching&=std::fabs(Vector3DotProduct(Vector3Subtract(corners[i],contact.point),contact.normal))<.002f;
            if(!matching)continue;const auto offset=Vector3Scale(contact.normal,camera_.worldUnitsPerPixel(contact.point,view.height)*.4f);
            DrawTriangle3D(Vector3Add(corners[face[0]],offset),Vector3Add(corners[face[1]],offset),Vector3Add(corners[face[2]],offset),{color.r,color.g,color.b,24});
            DrawTriangle3D(Vector3Add(corners[face[0]],offset),Vector3Add(corners[face[2]],offset),Vector3Add(corners[face[3]],offset),{color.r,color.g,color.b,24});}
    }
    rlDrawRenderBatchActive();rlEnableBackfaceCulling();rlEnableDepthMask();EndMode3D();
    std::string label="Aponte para uma superfície · ESC cancela";
    if(contact.snapped)label="Encaixe válido · clique para confirmar · ESC cancela";
    else if(contact.blocked){const auto* blocker=scene_.find(contact.blocker);label="Sobreposição: "+(blocker?blocker->name():std::string("outra peça"))+" · escolha outra posição";}
    else if(contact.reason)label=contact.reason;
    const float width=std::min(view.width-24,float(MeasureText(label.c_str(),18)+20));
    DrawRectangleRounded({12,12,width,34},.15f,4,{24,27,33,240});DrawText(label.c_str(),22,20,18,{color.r,color.g,color.b,255});
}
void EditorUI::assemblyAction(const std::string& action){
    if(action!="side"&&action!="above"&&action!="repeat"){setStatus("Ação de montagem inválida.",true);return;}
    ConstructionPlacement source;std::string error;auto* selected=scene_.selection();
    if(!source.begin(scene_,selected?selected->id():0,error)){setStatus(error,true);return;}
    const auto delta=action=="repeat"?assemblyStep_:source.duplicateStep(action=="above"?1:0,placementOffset_);
    if(Vector3LengthSqr(delta)<1e-10f){setStatus("Faça um encaixe ou duplicação antes de repetir.",true);return;}
    endGizmoSession(true,"assembly");
    auto* copy=scene_.duplicateTree(*selected);if(!copy){setStatus("Não foi possível duplicar.",true);return;}
    if(copy->isPart()){auto* part=static_cast<Part*>(copy);part->setPosition(Vector3Add(part->position(),delta));}
    else {ModelFrame frame;if(!scene_.modelFrame(copy->id(),frame,error)){scene_.remove(copy);setStatus(error,true);return;}
        frame.position=Vector3Add(frame.position,delta);if(!scene_.transformModel(copy->id(),frame,error)){scene_.remove(copy);setStatus(error,true);return;}}
    assemblyStep_=delta;physics_.syncScene(scene_);scene_.select(copy);setStatus("Cópia posicionada; tamanho e pivô preservados.");
}
}

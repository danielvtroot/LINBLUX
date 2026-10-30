#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include "serialization/ProjectIO.h"
#include "serialization/SceneLua.h"
#include "game/Runtime.h"
#include "core/Diagnostics.h"
#include "core/NumberFormat.h"
#include "editor/EditHistory.h"
#include "editor/ConstructionPlacement.h"
#include "editor/ModelTransformSession.h"
#include "editor/GizmoLayout.h"
#include "rendering/EditorCamera.h"
#include "editor/TransformMetrics.h"

using namespace loblox;
namespace {
int checks=0, failures=0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL %d: %s\n",__LINE__,#x); } } while(false)
void Hierarchy() {
    Scene scene; scene.buildDefaultScene(); PhysicsWorld physics; physics.attach(scene);
    std::string error;
    auto* folder=scene.createContainer(InstanceClass::Folder,nullptr,"Buildings");
    auto* model=scene.createContainer(InstanceClass::Model,folder,"House");
    auto* detail=scene.createContainer(InstanceClass::Folder,folder,"Details");
    auto* part=scene.createPart(PartShape::Block,{30,4,0}); part->setAnchored(true);
    const auto id=part->id(); const auto transform=part->transform();
    physics.syncScene(scene);
    CHECK(scene.reparent(part,model,error)); CHECK(part->parent()==model);
    CHECK(scene.reparent(part,detail,error)); CHECK(part->parent()==detail);
    CHECK(scene.find(id)==part); CHECK(NearlyEqual(part->position(),transform.position));
    CHECK(scene.parts().size()==3); CHECK(physics.hasBody(id));
    Part* hit=nullptr;
    CHECK(physics.raycast({30,20,0},{0,-1,0},30,&hit)); CHECK(hit==part);
    CHECK(physics.validate(scene,error)); CHECK(scene.validate(error));
    CHECK(!scene.reparent(folder,detail,error)); CHECK(!scene.reparent(folder,folder,error));
    CHECK(!scene.reparent(part,scene.lighting(),error));
    CHECK(!scene.reparent(scene.workspace(),folder,error));
    Scene other; CHECK(!scene.reparent(part,other.workspace(),error));
    auto json=projectio::toJson(scene); CHECK(nlohmann::json::parse(json)["version"]==3);
    Scene loaded; CHECK(projectio::fromJson(loaded,json,error));
    CHECK(loaded.find(id)->parent()->id()==detail->id());
    CHECK(projectio::toJson(loaded)==json);
    PhysicsWorld restored; restored.attach(loaded); CHECK(restored.validate(loaded,error));
    CHECK(restored.raycast({30,20,0},{0,-1,0},30,&hit)); CHECK(hit->id()==id);
    auto invalid=nlohmann::json::parse(json);
    for(auto& object:invalid["objects"]) if(object["id"]==folder->id()) object["parent"]=detail->id();
    CHECK(!projectio::fromJson(loaded,invalid.dump(),error)); CHECK(projectio::toJson(loaded)==json);
    auto movedIds=nlohmann::json::parse(json);
    for(auto& object:movedIds["objects"]) {
        if(object["id"]==1)object["id"]=101;
        if(object["id"]==2)object["id"]=102;
        if(object["parent"]==1)object["parent"]=101;
    }
    Scene changedIds;CHECK(projectio::fromJson(changedIds,movedIds.dump(),error));
    CHECK(changedIds.workspace()->id()==101);CHECK(changedIds.lighting()->id()==102);
    CHECK(changedIds.find(id)->parent()->id()==detail->id());
    bool refused=false; try { scenelua::toLua(scene); } catch(...) { refused=true; } CHECK(refused);
    auto* copy=scene.duplicateTree(*folder); CHECK(copy && copy!=folder);
    CHECK(copy && copy->children().size()==2); CHECK(scene.parts().size()==4);
    CHECK(scene.validate(error)); physics.syncScene(scene); CHECK(physics.validate(scene,error));
    scene.select(part); CHECK(scene.remove(folder)); CHECK(scene.find(id)==nullptr);
    CHECK(scene.selection()==nullptr); CHECK(!physics.hasBody(id)); CHECK(scene.validate(error));
    CHECK(!scene.remove(scene.workspace()));
    auto* guarded=scene.createContainer(InstanceClass::Folder);
    scene.parts()[0]->setDeletable(false);
    CHECK(scene.reparent(scene.parts()[0],guarded,error)); CHECK(!scene.remove(guarded));
    auto* depth=scene.createContainer(InstanceClass::Folder); int levels=0;
    while(auto* next=scene.createContainer(InstanceClass::Folder,depth)) { depth=next; ++levels; }
    CHECK(levels<129); CHECK(scene.validate(error));
    Scene large;
    auto begin=std::chrono::steady_clock::now();
    for(int i=0;i<1000;++i) CHECK(large.createContainer(InstanceClass::Folder,nullptr,"Node"+std::to_string(i)));
    CHECK(large.validate(error)); Scene roundtrip;
    CHECK(projectio::fromJson(roundtrip,projectio::toJson(large),error)); CHECK(roundtrip.objects().size()==1002);
    std::printf("1000-node hierarchy create/validate/round-trip: %.2f ms\n",
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
}
void ModelGizmoSession() {
    Scene scene;std::string error;
    auto* a=scene.createPart(PartShape::Block,{10,6,0},{2,2,2},"A");auto* b=scene.createPart(PartShape::Block,{14,6,0},{2,2,2},"B");
    auto* model=scene.group({a->id(),b->id()},"Movimento de conjunto",error);CHECK(model);if(!model)return;
    PhysicsWorld physics;physics.attach(scene);Gizmo gizmo;gizmo.setSnap(0,0,0);TransformSession base(gizmo,physics);EditHistory history;
    ModelTransformSession session(scene,physics,base,history);EditorCamera camera;camera.setOrbit({12,6,0},30,.65f,.43f);const Rectangle view{0,0,800,600};
    GizmoInput input;session.update(ToolMode::Move,model,camera,view,true,true,input);CHECK(session.proxy());
    const auto before=projectio::toJson(scene);const auto originA=a->position(),originB=b->position();
    auto layout=gizmolayout::Build(*session.proxy(),ToolMode::Move,camera,view,false,gizmo.visualSettings());
    input.mousePosition=layout.moveHandle[1];input.leftPressed=input.leftDown=true;
    CHECK(session.update(ToolMode::Move,model,camera,view,true,true,input).started);CHECK(physics.isPartEditing(a->id())&&physics.isPartEditing(b->id()));
    CHECK(physics.bodyCount()==2&&!physics.hasBody(model->id()));
    input.leftPressed=false;input.mousePosition=Vector2Add(input.mousePosition,Vector2Scale(layout.axisDirection[0],30));
    session.update(ToolMode::Move,model,camera,view,true,true,input);
    CHECK(Vector3Distance(a->position(),originA)>.01f);CHECK(NearlyEqual(Vector3Subtract(a->position(),originA),Vector3Subtract(b->position(),originB),.001f));
    input.leftDown=false;CHECK(session.update(ToolMode::Move,model,camera,view,true,true,input).ended);
    CHECK(!session.active()&&!physics.isPartEditing(a->id())&&!physics.isPartEditing(b->id()));CHECK(history.undoCount()==1);const auto after=projectio::toJson(scene);
    TransformEdit edit;CHECK(history.undo(edit));CHECK(edit.parts.size()==2);CHECK(session.applyEdit(edit,false,error));CHECK(projectio::toJson(scene)==before);
    CHECK(history.redo(edit));CHECK(session.applyEdit(edit,true,error));CHECK(projectio::toJson(scene)==after);
    input={};session.update(ToolMode::Move,model,camera,view,true,true,input);layout=gizmolayout::Build(*session.proxy(),ToolMode::Move,camera,view,false,gizmo.visualSettings());
    input.mousePosition=layout.moveHandle[1];input.leftPressed=input.leftDown=true;session.update(ToolMode::Move,model,camera,view,true,true,input);
    input.leftPressed=false;input.mousePosition=Vector2Add(input.mousePosition,Vector2Scale(layout.axisDirection[0],23));session.update(ToolMode::Move,model,camera,view,true,true,input);
    session.end(true,"cancel_test");CHECK(projectio::toJson(scene)==after);CHECK(history.undoCount()==1);CHECK(!physics.isPartEditing(a->id()));
    input={};session.update(ToolMode::Scale,model,camera,view,true,true,input);
    layout=gizmolayout::Build(*session.proxy(),ToolMode::Scale,camera,view,false,gizmo.visualSettings());
    input.mousePosition=layout.scaleHandle[1];input.leftPressed=input.leftDown=true;
    CHECK(session.update(ToolMode::Scale,model,camera,view,true,true,input).started);
    input.leftPressed=false;input.mousePosition=Vector2Add(input.mousePosition,Vector2Scale(layout.axisDirection[0],28));
    session.update(ToolMode::Scale,model,camera,view,true,true,input);
    CHECK(a->size().x>2);CHECK(NearlyEqual(a->size().x,a->size().y,.001f));
    session.end(true,"scale_cancel_test");CHECK(projectio::toJson(scene)==after);CHECK(physics.bodyCount()==2);
    ModelFrame frame;CHECK(scene.modelFrame(model->id(),frame,error));frame.scale*=1.25f;CHECK(session.applyProperty(model->id(),frame,false,error));CHECK(history.undoCount()==2);
    CHECK(history.undo(edit));CHECK(session.applyEdit(edit,false,error));CHECK(projectio::toJson(scene)==after);
    CHECK(scene.remove(b));const auto missing=projectio::toJson(scene);CHECK(!session.applyEdit(edit,true,error));CHECK(projectio::toJson(scene)==missing);
}
void StorageServices() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* storage=author.ensureService(InstanceClass::ServerStorage);
    auto* shared=author.ensureService(InstanceClass::ReplicatedStorage);
    auto* folder=author.createContainer(InstanceClass::Folder,storage,"Itens");
    auto* model=author.createContainer(InstanceClass::Model,nullptr,"Espada");
    auto* blade=author.createPart(PartShape::Block,{40,4,0},{2,4,2},"Lamina");blade->setAnchored(true);
    CHECK(author.reparent(blade,model,error));
    auto* behavior=author.createScript(InstanceClass::Script,model,"Item.lua");
    behavior->source="local r=linblux.scene.root;r:set_attribute('clone_started',(r:get_attribute('clone_started') or 0)+1)";
    PhysicsWorld physics;physics.attach(author);const auto bodies=physics.bodyCount();
    CHECK(physics.hasBody(blade->id()));CHECK(author.reparent(model,folder,error));
    CHECK(!physics.hasBody(blade->id()));CHECK(author.isStored(blade));CHECK(!author.isWorldObject(blade));
    CHECK(author.parts().size()==3&&author.worldParts().size()==2);
    ConstructionPlacement placement;CHECK(!placement.begin(author,model->id(),error));
    physics.applyChanges(blade);CHECK(!physics.hasBody(blade->id()));CHECK(physics.validate(author,error));
    CHECK(author.validate(error));CHECK(blade->anchored()&&blade->canCollide());
    Part* hit=nullptr;CHECK(physics.raycast({40,20,0},{0,-1,0},30,&hit));CHECK(hit!=blade);
    CHECK(author.pickPart({{40,20,0},{0,-1,0}},30)!=blade);
    CHECK(author.reparent(model,shared,error));CHECK(!physics.hasBody(blade->id()));
    CHECK(author.reparent(model,author.workspace(),error));CHECK(physics.hasBody(blade->id()));
    CHECK(physics.bodyCount()==bodies);CHECK(author.isWorldObject(blade));
    CHECK(placement.begin(author,model->id(),error));
    CHECK(author.reparent(model,folder,error));
    placement.update(author,{{40,20,0},{0,-1,0}},gizmomath::SnapFeature::Face,0,1,false);CHECK(!placement.active());CHECK(!physics.hasBody(blade->id()));
    model->setAttribute("_linblux_item_prototype",true);model->setAttribute("item_type",std::string("espada"));
    auto* module=author.createScript(InstanceClass::ModuleScript,shared,"Config");module->source="return {value=7}";
    author.createScript(InstanceClass::Script,storage,"Stored.lua")->source="error('stored script ran')";
    auto* service=author.ensureService(InstanceClass::ServerScriptService);
    author.createScript(InstanceClass::Script,service,"Service.lua")->source=R"(
local store=game:GetService('ServerStorage')
assert(store:FindFirstChild('Itens'):FindFirstChild('Espada'))
assert(require(game:GetService('ReplicatedStorage'):FindFirstChild('Config')).value==7)
linblux.scene.root:set_attribute('service_started',true)
)";
    auto* disabled=author.createScript(InstanceClass::Script,service,"Disabled.lua");disabled->enabled=false;disabled->source="error('disabled script ran')";
    author.createScript(InstanceClass::LocalScript,service,"Local.lua")->source="linblux.scene.root:set_attribute('local_started',true)";
    author.createScript(InstanceClass::ModuleScript,service,"LazyModule")->source="error('module started without require')";
    const auto saved=projectio::toJson(author);Scene loaded;
    CHECK(projectio::fromJson(loaded,saved,error));CHECK(projectio::toJson(loaded)==saved);
    CHECK(loaded.find(blade->id())->parent()->id()==model->id());CHECK(loaded.isStored(loaded.find(blade->id())));
    PhysicsWorld loadedPhysics;loadedPhysics.attach(loaded);CHECK(!loadedPhysics.hasBody(blade->id()));
    game::RuntimeSession runtime;CHECK(runtime.start(author,error));CHECK(runtime.scripts().errorCount()==0);
    CHECK(runtime.scene().workspace()->attributes().count("service_started"));CHECK(runtime.scene().workspace()->attributes().count("local_started"));
    CHECK(!runtime.scene().workspace()->attributes().count("clone_started"));
    auto& inventory=runtime.inventory();CHECK(inventory.prototype("espada")==model->id());
    const auto id=inventory.spawn("espada");auto* copy=runtime.mutableScene().find(id);
    CHECK(copy&&id!=model->id()&&copy->parent()==runtime.scene().workspace());
    CHECK(runtime.scene().isWorldObject(copy));CHECK(runtime.scene().isStored(runtime.scene().find(model->id())));
    CHECK(copy->children().size()==model->children().size());
    for(const auto* child:copy->children())CHECK(child->id()!=blade->id()&&child->id()!=behavior->id());
    runtime.update(1.f/60,{});CHECK(runtime.scripts().errorCount()==0);
    CHECK(runtime.scene().workspace()->attributes().count("clone_started"));
    inventory.collect(id);inventory.equip(id);CHECK(inventory.equipped()==id);
    inventory.drop(id);CHECK(inventory.info(id).state=="world");CHECK(runtime.validate(error));
    runtime.stop();CHECK(projectio::toJson(author)==saved);CHECK(behavior->enabled);
    CHECK(runtime.start(author,error));CHECK(runtime.scripts().errorCount()==0);
    CHECK(runtime.scene().workspace()->attributes().count("service_started"));
    CHECK(!runtime.scene().workspace()->attributes().count("clone_started"));runtime.stop();
    CHECK(projectio::toJson(author)==saved);
    // Destroying an attached PhysicsWorld must leave no live hierarchy callback.
    {Scene temporary;auto* p=temporary.createPart(PartShape::Block,{});{PhysicsWorld shortLived;shortLived.attach(temporary);}
        auto* destination=temporary.ensureService(InstanceClass::ServerStorage);CHECK(temporary.reparent(p,destination,error));}
}
void RootServices() {
    Scene scene;scene.buildDefaultScene();scene.installDefaultScripts();std::string error;
    for(auto type:{InstanceClass::ServerScriptService,InstanceClass::ServerStorage,InstanceClass::ReplicatedStorage}) {
        auto* service=scene.service(type);CHECK(service);if(!service)continue;
        const auto id=service->id();CHECK(scene.ensureService(type)==service);CHECK(service->id()==id);
        CHECK(!service->deletable());CHECK(!scene.remove(service));CHECK(!scene.canParent(service,scene.parts()[0]));
    }
    const auto saved=projectio::toJson(scene);scene.installDefaultScripts();CHECK(projectio::toJson(scene)==saved);
    Scene restored;CHECK(projectio::fromJson(restored,saved,error));CHECK(projectio::toJson(restored)==saved);
    for(auto type:{InstanceClass::ServerScriptService,InstanceClass::ServerStorage,InstanceClass::ReplicatedStorage}) {
        int count=0;for(const auto& n:restored.objects())count+=n->instanceClass()==type;
        CHECK(count==1);CHECK(restored.service(type)->id()==scene.service(type)->id());
    }
}
void PartNamesLua() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* legacy=author.createPart(PartShape::Block,{40,4,0},{1,1,1},"Part antiga",700);
    author.createScript(InstanceClass::Script,nullptr,"Names.lua")->source=R"(
local old=linblux.scene.find("Part antiga")
assert(old and old.name=="Part antiga")
local p=linblux.scene.create("part",{name="Nova Part"})
assert(p.name=="NovaPart")
p.name="NomeValido"
assert(p.name=="NomeValido" and old.name=="Part antiga")
)";
    const auto saved=projectio::toJson(author);game::RuntimeSession runtime;
    CHECK(runtime.start(author,error));CHECK(runtime.scripts().errorCount()==0);
    runtime.mutableScene().createScript(InstanceClass::Script,nullptr,"InvalidName.lua")->source=
        "linblux.scene.find('NomeValido').name='Nome com espaço'";
    runtime.update(1.f/60,{});CHECK(runtime.scripts().errorCount()==1);
    CHECK(runtime.scripts().messages().back().text.find("Nomes de peças não podem conter espaços")!=std::string::npos);
    CHECK(author.find(legacy->id())->name()=="Part antiga");runtime.stop();CHECK(projectio::toJson(author)==saved);
}
void EditorSelectionAndNames() {
    Scene scene;std::string error;
    auto* a=scene.createPart(PartShape::Block,{0,2,4},{2,2,2},"Peça grande");
    auto* b=scene.createPart(PartShape::Block,{0,2,0},{4,2,2},"Peça grande");
    auto* copy=scene.duplicatePart(*b);copy->setPosition({10,2,0});
    CHECK(b->name()=="PeçaGrande_002"&&copy->name()=="PeçaGrande_003");
    CHECK(a->name()=="PeçaGrande");
    auto* legacy=scene.createPart(PartShape::Block,{20,2,0},{1,1,1},"Plataforma inicial325",700);
    auto* duplicate=scene.duplicatePart(*legacy);CHECK(duplicate->name()=="PlataformaInicial325");
    CHECK(legacy->name()=="Plataforma inicial325");
    CHECK(!a->trySetName("Parede principal",error));CHECK(a->name()=="PeçaGrande");
    CHECK(!a->trySetName("Parede\tPrincipal",error));CHECK(!a->trySetName("Parede\nPrincipal",error));
    CHECK(!a->trySetName("Parede\xc2\xa0Principal",error));CHECK(!a->trySetName("",error));
    CHECK(!a->trySetName(b->name(),error));CHECK(a->trySetName("ParedePrincipal",error));
    CHECK(NewPartName("Piso da sala")=="PisoDaSala");CHECK(NewPartName("Nova Part")=="NovaPart");
    CHECK(!scene.createPart(PartShape::Block,{},{1,1,1},"Nova\tPart"));
    std::string unicode;for(int i=0;i<64;++i)unicode+="Á";
    auto* named=scene.createPart(PartShape::Block,{30,2,0},{1,1,1},unicode);
    auto* namedCopy=scene.duplicatePart(*named);CHECK(namedCopy->name().size()<=128);CHECK(scene.validate(error));
    const Ray ray{{0,2,10},{0,0,-2}};
    auto hits=scene.pickParts(ray,20);CHECK(hits.size()==2);CHECK(hits[0].part==a&&hits[1].part==b);
    CHECK(NearlyEqual(hits[0].distance,5));CHECK(scene.pickPart(ray,20)==a);
    auto color=a->color();color.a=0;a->setColor(color);
    hits=scene.pickParts(ray,20);CHECK(hits.size()==1&&hits[0].part==b);
    CHECK(scene.pickParts({{0,2,10},{0,0,0}},20).empty());
    CHECK(scene.pickParts(ray,-1).empty());
    Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));
    CHECK(loaded.find(legacy->id())->name()==legacy->name());CHECK(loaded.find(b->id())->name()==b->name());
    CHECK(loaded.find(namedCopy->id())->name()==namedCopy->name());
}
void CursorConstruction() {
    using namespace gizmomath;
    const auto box=[](Vector3 p,Vector3 size,Vector3 e=Vector3{}) {return TransformSnapshot{p,QuatFromEulerDegrees(e),size};};
    const auto origin=Vector3{.35f,.4f,.2f};
    CHECK(NearlyEqual(SnapGrid({1.66f,.4f,.2f},origin,{1,0,0},{},1,GridMode::Absolute),{2,.4f,.2f}));
    CHECK(NearlyEqual(SnapGrid({1.66f,.4f,.2f},origin,{1,0,0},{},1,GridMode::Relative),{1.35f,.4f,.2f}));
    for(float angle:{0.f,15.f,30.f,45.f,90.f})for(float width:{.2f,4.f,192.f}) {
        auto target=box({0,0,0},{width,6,4},{0,angle,0});
        auto normal=Vector3RotateByQuaternion({0,0,1},target.rotation);
        auto point=Vector3Scale(normal,2);
        auto source=box({0,10,10},{2,2,2});
        const Ray ray{Vector3Add(point,Vector3Scale(normal,20)),Vector3Negate(normal)};
        const auto result=SnapUnderCursor({{source,1}},{{target,2}},source.position,ray,SnapFeature::Face,true);
        CHECK(result.snapped&&result.target==2&&!result.blocked);
        source.position=Vector3Add(source.position,result.correction);
        source.rotation=QuaternionMultiply(result.rotationDelta,source.rotation);
        CHECK(std::fabs(BoxSeparation(source,target))<.0002f);
        CHECK(Vector3DotProduct(Vector3RotateByQuaternion({0,1,0},source.rotation),normal)>.9999f);
    }
    auto source=box({0,10,0},{2,2,2});auto floor=box({0,-.5f,0},{24,1,24});
    const Ray ray{{3,20,4},{0,-1,0}};
    auto face=SnapUnderCursor({{source,1}},{{floor,2},{box({0,-4,0},{30,1,30}),3}},source.position,ray,SnapFeature::Face);
    CHECK(face.snapped&&face.target==2&&NearlyEqual(face.point,{3,0,4}));
    auto edge=SnapUnderCursor({{source,1}},{{floor,2}},source.position,ray,SnapFeature::Edge);
    auto vertex=SnapUnderCursor({{source,1}},{{floor,2}},source.position,ray,SnapFeature::Vertex);
    CHECK(edge.snapped&&NearlyEqual(edge.point,{3,0,12}));
    CHECK(vertex.snapped&&NearlyEqual(vertex.point,{12,0,12}));
    auto obstacle=box({3,1,4},{4,2,4});
    auto blocked=SnapUnderCursor({{source,1}},{{floor,2},{obstacle,4}},source.position,{{3,5,14},{0,-.2f,-1}},SnapFeature::Face);
    // Explicit blocker: put another obstacle in the desired space beside the
    // face hit, outside the cursor ray, so nearest-hit picking still sees floor.
    blocked=SnapUnderCursor({{source,1}},{{floor,2},{box({4.4f,1,4},{1,2,2}),4}},source.position,ray,SnapFeature::Face);
    CHECK(blocked.blocked&&!blocked.snapped&&blocked.blocker==4&&blocked.reason);
    CHECK(!SnapUnderCursor({},{{floor,2}},source.position,ray,SnapFeature::Face).snapped);
    auto offset=source;offset.position={50,10,0};
    auto grouped=SnapUnderCursor({{source,1},{offset,5}},{{floor,2}}, {100,10,0},ray,SnapFeature::Face);
    CHECK(grouped.snapped); // Support computed from children, never a proxy cube.
    // Four walls on a floor, placed solely through cursor contact; persist it.
    Scene room;std::string error;std::vector<SurfaceBox> targets{{floor,1000}};
    auto* ground=room.createPart(PartShape::Block,floor.position,floor.size,"Floor");ground->setAnchored(true);
    for(int i=0;i<4;++i) {
        const Vector3 point=i==0?Vector3{0,0,-10}:i==1?Vector3{0,0,10}:i==2?Vector3{-10,0,0}:Vector3{10,0,0};
        auto wall=box({40,12,30},i<2?Vector3{21,8,1}:Vector3{1,8,19});
        auto placed=SnapUnderCursor({{wall,100+unsigned(i)}},targets,wall.position,{Vector3Add(point,{0,30,0}),{0,-1,0}},SnapFeature::Face);
        CHECK(placed.snapped&&!placed.blocked);wall.position=Vector3Add(wall.position,placed.correction);
        for(const auto& t:targets)CHECK(BoxSeparation(wall,t.pose)>=-.0001f);
        targets.push_back({wall,100+unsigned(i)});
        auto* part=room.createPart(PartShape::Block,wall.position,wall.size,"Wall");part->setAnchored(true);
    }
    Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(room),error));CHECK(loaded.parts().size()==5);
    for(auto* p:room.parts())CHECK(NearlyEqual(loaded.findPart(p->id())->position(),p->position()));
    for(int i=0;i<100;++i) {
        auto result=SnapUnderCursor({{source,1}},{{floor,2}},source.position,ray,SnapFeature::Face,true,source.rotation);
        CHECK(result.snapped);source.position=Vector3Add(source.position,result.correction);
        source.rotation=QuaternionMultiply(result.rotationDelta,source.rotation);
        CHECK(NearlyEqual(source.position,{3,1,4},.0001f));
    }
}

void ConstructionPreviewWorkflow() {
    using namespace gizmomath;
    for(float width:{2.f,191.f})for(float thickness:{.1f,1.f})for(float angle:{0.f,30.f}) {
        Scene scene;PhysicsWorld physics;physics.attach(scene);Gizmo gizmo;TransformSession transform(gizmo,physics);EditHistory history;ModelTransformSession models(scene,physics,transform,history);
        auto* floor=scene.createPart(PartShape::Block,{0,-thickness*.5f,0},{240,thickness,40},"Floor");floor->setAnchored(true);
        floor->setRotation(QuatFromEulerDegrees({0,angle,0}));
        auto* wall=scene.createPart(PartShape::Block,{0,20,50},{width,8,1},"Wall");wall->setAnchored(false);
        ConstructionPlacement proposal;std::string error;const auto original=projectio::toJson(scene);const auto before=wall->transform();
        CHECK(proposal.begin(scene,wall->id(),error));
        proposal.update(scene,{{0,30,0},{0,-1,0}},SnapFeature::Face,0,1,true);
        CHECK(proposal.contact().snapped&&!proposal.contact().blocked);CHECK(projectio::toJson(scene)==original);
        CHECK(proposal.preview().size()==1);CHECK(std::fabs(BoxSeparation(proposal.preview()[0].pose,{floor->position(),floor->rotation(),floor->size()}))<.001f);
        proposal.cancel();CHECK(projectio::toJson(scene)==original);
        CHECK(proposal.begin(scene,wall->id(),error));proposal.update(scene,{{0,30,0},{0,-1,0}},SnapFeature::Face,.25f,1,true);
        CHECK(proposal.commit(scene,physics,models,history,error));CHECK(!wall->anchored());CHECK(std::fabs(wall->position().y-(floor->position().y+floor->size().y*.5f+4.25f))<.001f);
        TransformEdit edit;CHECK(history.undo(edit));wall->setTransform(edit.before);CHECK(SameTransform(before,wall->transform()));
        CHECK(history.redo(edit));wall->setTransform(edit.after);Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));CHECK(SameTransform(loaded.findPart(wall->id())->transform(),wall->transform()));CHECK(!loaded.findPart(wall->id())->anchored());
    }
    // At an edge, a long wall stays centred tangentially and its outer face
    // is flush with the floor boundary; no jump to its 191-unit end corner.
    const TransformSnapshot wide{{0,30,40},QuaternionIdentity(),{191,8,1}},platform{{0,-.1f,0},QuaternionIdentity(),{200,.2f,20}};
    const auto edge=SnapUnderCursor({{wide,1}},{{platform,2}},wide.position,{{0,30,9},{0,-1,0}},SnapFeature::Edge,false,wide.rotation,0,0,1,true);
    CHECK(edge.snapped);CHECK(NearlyEqual(Vector3Add(wide.position,edge.correction),{0,4,9.5f},.001f));
    Scene scene;PhysicsWorld physics;physics.attach(scene);Gizmo gizmo;TransformSession transform(gizmo,physics);EditHistory history;ModelTransformSession models(scene,physics,transform,history);std::string error;
    auto* floor=scene.createPart(PartShape::Block,{0,-.5f,0},{40,1,40},"Floor");floor->setAnchored(true);
    auto* wall=scene.createPart(PartShape::Block,{0,10,30},{12,8,1},"Wall");
    auto* other=scene.createPart(PartShape::Block,{3,10,30},{1,2,5},"Child");
    auto* model=scene.group({wall->id(),other->id()},"Building",error);CHECK(model);ModelFrame frame;CHECK(scene.modelFrame(model->id(),frame,error));frame.position={80,12,30};CHECK(scene.setModelPivot(model->id(),frame,error));
    ConstructionPlacement proposal;CHECK(proposal.begin(scene,model->id(),error));const auto original=projectio::toJson(scene);
    CHECK(std::fabs(Vector3Length(proposal.duplicateStep(0,.5f))-12.5f)<.001f);
    proposal.update(scene,{{0,40,0},{0,-1,0}},SnapFeature::Face,0,1,false);CHECK(proposal.contact().snapped);CHECK(projectio::toJson(scene)==original);
    CHECK(proposal.commit(scene,physics,models,history,error));TransformEdit edit;CHECK(history.undo(edit));CHECK(models.applyEdit(edit,false,error));CHECK(projectio::toJson(scene)==original);CHECK(history.redo(edit));CHECK(models.applyEdit(edit,true,error));
    // Collision candidate stays a red proposal; even saving cannot mutate it.
    auto* obstacle=scene.createPart(PartShape::Block,{3,1,8},{3,2,3},"Conflict");
    CHECK(proposal.begin(scene,model->id(),error));const auto saved=projectio::toJson(scene);
    proposal.update(scene,{{0,30,8},{0,-1,0}},SnapFeature::Face,0,1,false);
    CHECK(proposal.contact().blocked&&proposal.contact().blocker==obstacle->id());CHECK(!proposal.commit(scene,physics,models,history,error));CHECK(projectio::toJson(scene)==saved);
    // A mutation during preview invalidates it instead of overwriting newer work.
    wall->setPosition({0,20,30});proposal.update(scene,{{0,30,8},{0,-1,0}},SnapFeature::Face,0,1,false);CHECK(!proposal.active());
}

void SurfaceSnapping() {
    using namespace gizmomath;
    const auto box=[](Vector3 p,Vector3 s,Vector3 e=Vector3{}) {return TransformSnapshot{p,QuatFromEulerDegrees(e),s};};
    const Vector3 x{1,0,0},y{0,1,0},z{0,0,1};
    auto platform=box({0,-.5f,0},{100,1,100});
    auto top=box({20,1.13f,4},{2,2,2});
    auto snap=SnapSurfaces({{top,1}},{{platform,2}},y,{},.2f);
    CHECK(snap.snapped&&snap.target==2);CHECK(NearlyEqual(snap.correction,{0,-.13f,0},.0001f));
    top.position=Vector3Add(top.position,snap.correction);CHECK(std::fabs(BoxSeparation(top,platform))<.0001f);
    CHECK(BoxSeparation(box({0,-.1f,0},{2,2,2}),platform)<0);
    for(float degrees:{0.f,15.f,30.f,45.f,90.f})for(float size:{.2f,2.f,2000.f}) {
        auto wall=box({0,0,0},{size*3,size*2,size*4},{0,degrees,0});
        const auto normal=Vector3RotateByQuaternion(x,wall.rotation);
        auto moving=box(Vector3Scale(normal,size*2+.07f),{size,size,size},{0,degrees,0});
        const auto result=SnapSurfaces({{moving,1}},{{wall,2}},normal,{},.1f);
        CHECK(result.snapped);CHECK(Vector3Distance(result.correction,Vector3Scale(normal,-.07f))<.001f);
        moving.position=Vector3Add(moving.position,result.correction);
        CHECK(std::fabs(BoxSeparation(moving,wall))<.001f);
        CHECK(Vector3Length(Vector3CrossProduct(result.correction,normal))<.001f);
    }
    for(float angle:{15.f,30.f,45.f}) {
        auto wall=box({0,0,0},{2,8,8});const float support=std::cos(angle*DEG2RAD)+std::sin(angle*DEG2RAD);
        auto moving=box({1+support+.08f,0,0},{2,2,2},{0,angle,0});
        const auto result=SnapSurfaces({{moving,1}},{{wall,2}},x,{},.15f);
        CHECK(result.snapped);moving.position=Vector3Add(moving.position,result.correction);
        CHECK(std::fabs(BoxSeparation(moving,wall))<.0001f);
    }
    auto a=box({2.1f,0,0},{2,2,2}),b=box({0,0,0},{2,2,2});
    CHECK(!SnapSurfaces({{a,1}},{{b,2}},y,{},.2f).snapped);
    CHECK(SnapSurfaces({{a,1}},{{b,2}},x,z,.2f).snapped);
    auto distant=b;distant.position.y=8;CHECK(!SnapSurfaces({{a,1}},{{distant,2}},x,{},.2f).snapped);
    CHECK(!SnapSurfaces({{a,1}},{{b,2}},x,{},.05f).snapped);
    auto obstacle=box({3.05f,0,0},{.2f,2,2});
    CHECK(!SnapSurfaces({{a,1}},{{b,2},{obstacle,3}},x,{},.2f).snapped);
    auto invalid=a;invalid.position.x=std::numeric_limits<float>::quiet_NaN();
    CHECK(!SnapSurfaces({{invalid,1}},{{b,2}},x,{},.2f).snapped);
    CHECK(!SnapSurfaces({{a,1}},{{b,2}},x,{},-1).snapped);

    // Modelo com pivô deslocado/filho rotacionado: gesto, contato, undo/redo e cancelamento.
    Scene scene;std::string error;
    auto* p=scene.createPart(PartShape::Block,{10,6,0},{2,2,2},"Principal");
    auto* child=scene.createPart(PartShape::Block,{14,6,1},{1,2,3},"Filho");child->setRotationEulerDegrees({0,30,0});
    auto* model=scene.group({p->id(),child->id()},"Modelo",error);CHECK(model);
    ModelFrame pivot;scene.modelFrame(model->id(),pivot,error);pivot.position={3,4,0};scene.setModelPivot(model->id(),pivot,error);
    PhysicsWorld physics;physics.attach(scene);Gizmo gizmo;gizmo.setSnap(1,0,0);TransformSession base(gizmo,physics);EditHistory history;
    ModelTransformSession session(scene,physics,base,history);EditorCamera camera;camera.setOrbit({10,6,0},30,.65f,.43f);
    const Rectangle view{0,0,800,600};GizmoInput input;session.update(ToolMode::Move,model,camera,view,true,true,input);
    const auto layout=gizmolayout::Build(*session.proxy(),ToolMode::Move,camera,view,false,gizmo.visualSettings());
    const auto original=p->transform(),originalChild=child->transform();const auto start=p->position();
    const auto restored=[&](const loblox::Transform& parent,const loblox::Transform& descendant,const ModelFrame& expected) {
        ModelFrame actual;CHECK(scene.modelFrame(model->id(),actual,error));
        CHECK(SameTransform(parent,p->transform())&&SameTransform(descendant,child->transform()));
        CHECK(NearlyEqual(actual.position,expected.position)&&NearlyEqual(actual.rotation,expected.rotation)&&NearlyEqual(actual.scale,expected.scale));
    };
    const auto begin=[&](){input={};input.mousePosition=layout.moveHandle[1];input.leftPressed=input.leftDown=true;
        CHECK(session.update(ToolMode::Move,model,camera,view,true,true,input).started);};
    const auto move=[&](){input.leftPressed=false;input.mousePosition=Vector2Add(layout.moveHandle[1],Vector2Scale(layout.axisDirection[0],30));
        session.update(ToolMode::Move,model,camera,view,true,true,input);};
    begin();move();const auto raw=p->position();session.end(true,"measure");restored(original,originalChild,pivot);
    auto target=box({raw.x-2.12f,6,0},{2,2,2});std::vector<SurfaceBox> targets{{target,9000}},moving;
    begin();moving={{{p->position(),p->rotation(),p->size()},p->id()},{{child->position(),child->rotation(),child->size()},child->id()}};
    input.surfaceSnap=true;input.movingBoxes=&moving;input.targetBoxes=&targets;move();
    CHECK(gizmo.surfaceContact().snapped);CHECK(std::fabs(BoxSeparation({p->position(),p->rotation(),p->size()},target))<.0001f);
    CHECK(NearlyEqual(child->position().x-14,p->position().x-10,.001f));
    input.leftDown=false;session.update(ToolMode::Move,model,camera,view,true,true,input);CHECK(history.undoCount()==1);
    TransformEdit edit;CHECK(history.undo(edit));CHECK(session.applyEdit(edit,false,error));restored(original,originalChild,pivot);
    CHECK(history.redo(edit));CHECK(session.applyEdit(edit,true,error));const auto committed=p->transform(),committedChild=child->transform();
    ModelFrame committedPivot;scene.modelFrame(model->id(),committedPivot,error);
    input={};session.update(ToolMode::Move,model,camera,view,true,true,input);
    auto active=gizmolayout::Build(*session.proxy(),ToolMode::Move,camera,view,false,gizmo.visualSettings());
    input.mousePosition=active.moveHandle[1];input.leftDown=input.leftPressed=true;
    session.update(ToolMode::Move,model,camera,view,true,true,input);
    input.leftPressed=false;input.shiftDown=true;input.mousePosition=Vector2Add(input.mousePosition,Vector2Scale(active.axisDirection[0],17));
    session.update(ToolMode::Move,model,camera,view,true,true,input);session.end(true,"escape");
    restored(committed,committedChild,committedPivot);CHECK(!gizmo.surfaceContact().snapped);
    CHECK(Vector3Distance(start,raw)>.1f);
}
void InteractionControls() {
    Scene scene;scene.buildDefaultScene();std::string error;PhysicsWorld physics;physics.attach(scene);game::InteractionSystem interactions(scene,physics);
    auto* door=scene.createPart(PartShape::Block,{3,3,0},{1,2,1},"Porta");door->setAnchored(true);physics.syncScene(scene);
    interactions.add(door->id(),{"Abrir porta",5,.5f,KEY_E});game::ScriptInput input;
    CHECK(interactions.update({0,3,0},input,.1f)==0&&interactions.prompt().object==door->id());input.keys.set(KEY_E);
    CHECK(interactions.update({0,3,0},input,.1f)==0&&interactions.prompt().progress>0);
    input.keys.reset();interactions.update({0,3,0},input,.1f);CHECK(interactions.prompt().progress==0);
    input.keys.set(KEY_E);interactions.update({0,3,0},input,.1f);interactions.update({30,3,0},input,.1f);CHECK(!interactions.prompt().object);
    InstanceId completed=0;for(int i=0;i<6;++i)completed=std::max(completed,interactions.update({0,3,0},input,.1f));CHECK(completed==door->id());
    CHECK(!interactions.update({0,3,0},input,.1f));input.keys.reset();interactions.update({0,3,0},input,.1f);
    auto* wall=scene.createPart(PartShape::Block,{1.5f,3,0},{1,5,5},"Obstáculo");wall->setAnchored(true);physics.syncScene(scene);
    CHECK(!interactions.update({0,3,0},input,.1f)&&!interactions.prompt().object);CHECK(scene.remove(wall));
    const auto doorId=door->id();CHECK(scene.remove(door));interactions.pruneItems({});CHECK(!interactions.update({0,3,0},input,.1f));(void)doorId;
    Scene author;author.buildDefaultScene();auto* setup=author.createScript(InstanceClass::Script,nullptr,"Controles.lua");
    setup->source=R"(local v=linblux.vec3;local p=linblux.player.position
local m=linblux.scene.create('model',{name='Coletável'})
linblux.scene.create('part',{parent=m,name='Item',position=v(p.x+3,p.y,p.z),size=v(1,1,1)})
linblux.items.register('controle',m);local item=linblux.items.spawn('controle');item:set_attribute('dados',37)
linblux.events.on('interaction_completed',function(event) linblux.scene.root:set_attribute('interagiu',true) end))";
    game::RuntimeSession runtime;CHECK(runtime.start(author,error));game::PlayerInput controls;runtime.update(.1f,controls);const auto world=runtime.inventory().worldItems();CHECK(world.size()==1);if(world.empty())return;const auto id=world.front();
    CHECK(runtime.interactions().prompt().object==id);controls.script.keys.set(KEY_E);for(int i=0;i<8;++i)runtime.update(.1f,controls);
    CHECK(runtime.inventory().list().size()==1);CHECK(runtime.scripts().errorCount()==0);CHECK(runtime.scene().workspace()->attributes().count("interagiu"));
    controls.script.keys.reset();runtime.update(.01f,controls);controls.script.keys.set(KEY_ONE);runtime.update(.02f,controls);CHECK(runtime.inventory().equipped()==id);
    controls.script.keys.reset();runtime.update(.01f,controls);controls.script.keys.set(KEY_DELETE);runtime.update(.02f,controls);CHECK(runtime.inventory().info(id).state=="world"&&runtime.inventory().list().empty());
    CHECK(std::get<double>(runtime.scene().find(id)->attributes().at("dados"))==37);
    controls.script.keys.reset();runtime.update(.02f,controls);controls.script.keys.set(KEY_E);for(int i=0;i<8;++i)runtime.update(.1f,controls);CHECK(runtime.inventory().list().size()==1);
    runtime.pause();runtime.update(.1f,controls);CHECK(!runtime.interactions().prompt().object);runtime.stop();
}
void InventoryLifecycle() {
    Scene scene;scene.buildDefaultScene();std::string error;
    auto* prototype=scene.createContainer(InstanceClass::Model,nullptr,"Chave original");
    auto* part=scene.createPart(PartShape::Block,{0,5,8},{1,1,2},"Corpo do item");part->setAnchored(true);part->setColor({220,40,60,200});CHECK(scene.reparent(part,prototype,error));
    auto* script=scene.createScript(InstanceClass::Script,prototype,"Comportamento.lua");script->source="linblux.self:set_attribute('script_da_instancia',true)";
    const auto original=projectio::toJson(scene);PhysicsWorld physics;physics.attach(scene);game::Inventory inventory(scene,physics);inventory.update({0,4,0},0);
    inventory.registerType("chave",prototype->id());CHECK(part->color().a==0&&!part->canCollide());CHECK(!script->enabled);
    ModelFrame pose;pose.position={4,3,5};pose.scale=2;const auto id=inventory.spawn("chave",&pose);CHECK(id!=prototype->id());
    auto* item=scene.find(id);CHECK(item);auto nodes=item->children();CHECK(nodes.size()==2);auto* itemPart=static_cast<Part*>(nodes[0]);
    CHECK(itemPart->color().a==200&&itemPart->anchored()&&itemPart->canCollide());CHECK(static_cast<Script*>(nodes[1])->enabled);
    CHECK(item->setAttribute("dados",std::string("carga:37")));inventory.collect(id);CHECK(inventory.list().size()==1&&inventory.info(id).state=="stored");CHECK(itemPart->color().a==0);
    bool duplicateRejected=false;try{inventory.collect(id);}catch(const std::exception&){duplicateRejected=true;}CHECK(duplicateRejected&&inventory.list().size()==1);
    inventory.equip(id);CHECK(inventory.equipped()==id&&itemPart->color().a==200&&!itemPart->canCollide());
    ModelFrame held;CHECK(scene.modelFrame(id,held,error));CHECK(NearlyEqual(held.scale,2));inventory.update({2,4,0},PI/2);CHECK(scene.modelFrame(id,held,error));CHECK(NearlyEqual(held.position,{0,4,-1},.001f));
    inventory.unequip();CHECK(!inventory.equipped()&&inventory.list().size()==1);inventory.equip(id);inventory.drop(id);
    CHECK(!inventory.equipped()&&inventory.list().empty()&&inventory.info(id).state=="world");CHECK(!itemPart->anchored()&&itemPart->canCollide());CHECK(std::get<std::string>(item->attributes().at("dados"))=="carga:37");
    CHECK(Vector3Distance(itemPart->position(),{2,4,0})>2);inventory.collect(id);inventory.equip(id);CHECK(inventory.equipped()==id);
    CHECK(scene.remove(item));inventory.update({2,4,0},0);CHECK(!inventory.equipped()&&inventory.list().empty());CHECK(!inventory.takeChanges().empty());
    bool invalidType=false;try{inventory.registerType("../invalido",prototype->id());}catch(const std::exception&){invalidType=true;}CHECK(invalidType);
    auto* sphereModel=scene.createContainer(InstanceClass::Model,nullptr,"Item esférico");
    auto* sphere=scene.createPart(PartShape::Sphere,{10,10,10},{1,10,1},"Esfera");sphere->setAnchored(true);CHECK(scene.reparent(sphere,sphereModel,error));
    inventory.registerType("esfera",sphereModel->id());const auto ball=inventory.spawn("esfera");inventory.collect(ball);inventory.equip(ball);inventory.drop(ball);
    const auto* ballPart=static_cast<const Part*>(scene.find(ball)->children().front());
    CHECK(Vector2Distance({ballPart->position().x,ballPart->position().z},{2,0})>ballPart->sphereRadius()+1);
    // A session operates on its own copy; prototype flags never alter saved authoring.
    Scene authored;CHECK(projectio::fromJson(authored,original,error));
    auto* authoredPrototype=authored.find(prototype->id());authoredPrototype->setAttribute("_linblux_item_prototype",true);authoredPrototype->setAttribute("item_type",std::string("chave"));
    static_cast<Script*>(authored.find(script->id()))->source="assert(linblux.items.info(linblux.self).name);linblux.self:set_attribute('rodou',true)";
    auto* prototypeCopy=authored.duplicateTree(*authoredPrototype);CHECK(prototypeCopy);
    CHECK(std::get<std::string>(prototypeCopy->attributes().at("item_type"))!="chave");
    const auto authorBefore=projectio::toJson(authored);game::RuntimeSession runtime;CHECK(runtime.start(authored,error));CHECK(runtime.scripts().errorCount()==0);
    const auto runtimeItem=runtime.inventory().spawn("chave");runtime.inventory().collect(runtimeItem);runtime.inventory().equip(runtimeItem);CHECK(!std::get<bool>(runtime.scene().find(runtimeItem)->attributes().at("_linblux_item_prototype")));runtime.update(1.f/60,{});
    CHECK(runtime.scripts().errorCount()==0);CHECK(runtime.scene().find(runtimeItem)->attributes().count("rodou"));runtime.stop();CHECK(projectio::toJson(authored)==authorBefore);
}
void AttachmentJoints() {
    Scene scene;std::string error;auto* model=scene.createContainer(InstanceClass::Model,nullptr,"Articulação");
    auto* base=scene.createPart(PartShape::Block,{0,8,0},{2,2,2},"Base da junta");base->setAnchored(true);base->setCanCollide(false);
    auto* arm=scene.createPart(PartShape::Block,{4,8,0},{2,2,2},"Peça móvel");arm->setCanCollide(false);
    CHECK(scene.reparent(base,model,error));CHECK(scene.reparent(arm,model,error));
    auto* a=scene.createAttachment(base,"Origem");auto* b=scene.createAttachment(arm,"Destino");CHECK(a&&b);if(!a||!b)return;
    const auto position=arm->position();CHECK(scene.connectAttachments(a->id(),b->id(),error));CHECK(NearlyEqual(position,arm->position()));CHECK(a->jointTarget()==b->id());
    CHECK(!scene.connectAttachments(a->id(),a->id(),error));CHECK(!scene.reparent(a,model,error));
    CHECK(scene.validate(error));PhysicsWorld physics;physics.attach(scene);CHECK(physics.jointCount()==1&&physics.bodyCount()==2);
    for(int i=0;i<120;++i)physics.simulate(scene,1.f/60);
    if(!NearlyEqual(arm->position(),position,.03f))std::fprintf(stderr,"Joint rest actual %.5f %.5f %.5f expected %.5f %.5f %.5f\n",arm->position().x,arm->position().y,arm->position().z,position.x,position.y,position.z);
    CHECK(NearlyEqual(arm->position(),position,.03f));
    auto pose=a->localFrame();pose.rotation=QuatFromEulerDegreesZYX({0,60,0});CHECK(a->setLocalFrame(pose));
    for(int i=0;i<120;++i)physics.simulate(scene,1.f/60);
    const auto expected=Vector3Add(base->position(),Vector3RotateByQuaternion({4,0,0},pose.rotation));if(!NearlyEqual(arm->position(),expected,.05f))std::fprintf(stderr,"Joint animated actual %.5f %.5f %.5f expected %.5f %.5f %.5f\n",arm->position().x,arm->position().y,arm->position().z,expected.x,expected.y,expected.z);CHECK(NearlyEqual(arm->position(),expected,.05f));
    auto* clone=scene.duplicateTree(*model);CHECK(clone);physics.syncScene(scene);CHECK(physics.jointCount()==2);
    int remapped=0;for(const auto& node:scene.objects())if(clone->isAncestorOf(node.get())&&node->instanceClass()==InstanceClass::Attachment){
        const auto target=static_cast<const Attachment&>(*node).jointTarget();if(target){++remapped;CHECK(clone->isAncestorOf(scene.find(target)));CHECK(target!=b->id());}}
    CHECK(remapped==1);const auto json=projectio::toJson(scene);CHECK(nlohmann::json::parse(json)["version"]==6);
    Scene loaded;CHECK(projectio::fromJson(loaded,json,error));CHECK(projectio::toJson(loaded)==json);
    PhysicsWorld restored;restored.attach(loaded);CHECK(restored.jointCount()==2);
    auto bad=nlohmann::json::parse(json);for(auto& node:bad["objects"])if(node["id"]==a->id())node["jointTarget"]=99999999u;
    CHECK(!projectio::fromJson(scene,bad.dump(),error));CHECK(projectio::toJson(scene)==json);
    CHECK(scene.remove(clone));CHECK(physics.jointCount()==1&&physics.bodyCount()==2);
    CHECK(scene.connectAttachments(b->id(),0,error));physics.syncScene(scene);CHECK(physics.jointCount()==0&&a->jointTarget()==0);
    CHECK(scene.connectAttachments(a->id(),b->id(),error));physics.syncScene(scene);CHECK(physics.jointCount()==1);
    CHECK(scene.remove(b));CHECK(a->jointTarget()==0&&physics.jointCount()==0);
    const auto height=arm->position().y;for(int i=0;i<60;++i)physics.simulate(scene,1.f/60);CHECK(arm->position().y<height-1);
    CHECK(scene.validate(error));CHECK(physics.validate(scene,error));
}
void Attachments() {
    Scene scene;std::string error;
    auto* model=scene.createContainer(InstanceClass::Model,nullptr,"Rifle fictício");
    auto* body=scene.createPart(PartShape::Block,{0,2,0},{2,2,2},"Corpo");
    auto* barrel=scene.createPart(PartShape::Block,{0,2,-3},{1,1,4},"Cano");
    CHECK(scene.reparent(body,model,error));CHECK(scene.reparent(barrel,model,error));
    ModelFrame pivot;CHECK(scene.setModelPivot(model->id(),pivot,error));
    auto* muzzle=scene.createAttachment(barrel,"BocaDoCano");CHECK(muzzle);if(!muzzle)return;
    ModelFrame local;local.position={0,0,-2};CHECK(muzzle->setLocalFrame(local));
    ModelFrame world;CHECK(scene.attachmentFrame(muzzle->id(),world,error));CHECK(NearlyEqual(world.position,{0,2,-5}));
    pivot.rotation=QuatFromEulerDegreesZYX({0,90,0});CHECK(scene.transformModel(model->id(),pivot,error));
    CHECK(scene.attachmentFrame(muzzle->id(),world,error));CHECK(NearlyEqual(world.position,{-5,2,0},.001f));
    CHECK(NearlyEqual(Vector3RotateByQuaternion({0,0,-1},world.rotation),{-1,0,0},.001f));
    pivot.scale=2;CHECK(scene.transformModel(model->id(),pivot,error));CHECK(scene.attachmentFrame(muzzle->id(),world,error));CHECK(NearlyEqual(world.position,{-10,4,0},.001f));
    CHECK(!scene.createAttachment(scene.workspace()));CHECK(!scene.createAttachment(muzzle));
    auto invalid=local;invalid.position.x=INFINITY;CHECK(!muzzle->setLocalFrame(invalid));
    auto* direct=scene.createAttachment(model,"PontoDeEfeito");CHECK(direct);CHECK(direct->setLocalFrame(local));
    auto* copy=scene.duplicateTree(*model);CHECK(copy);CHECK(scene.validate(error));
    int clonedAttachments=0;for(const auto& node:scene.objects())if(copy->isAncestorOf(node.get())&&node->instanceClass()==InstanceClass::Attachment){++clonedAttachments;CHECK(node->id()!=muzzle->id());}
    CHECK(clonedAttachments==2);
    const auto original=projectio::toJson(scene);CHECK(nlohmann::json::parse(original)["version"]==5);
    Scene loaded;CHECK(projectio::fromJson(loaded,original,error));CHECK(projectio::toJson(loaded)==original);
    ModelFrame recovered;CHECK(loaded.attachmentFrame(muzzle->id(),recovered,error));CHECK(NearlyEqual(recovered.position,world.position));
    bool rejectedLegacy=false;try{(void)scenelua::toLua(scene);}catch(const std::exception&){rejectedLegacy=true;}CHECK(rejectedLegacy);
    auto bad=nlohmann::json::parse(original);for(auto& o:bad["objects"])if(o["id"]==muzzle->id())o["attachment"]["rotation"]={0,0,0,0};
    CHECK(!projectio::fromJson(scene,bad.dump(),error));CHECK(projectio::toJson(scene)==original);
    bad=nlohmann::json::parse(original);bad["version"]=4;CHECK(!projectio::fromJson(scene,bad.dump(),error));CHECK(projectio::toJson(scene)==original);
    CHECK(scene.remove(direct));
    ModelFrame huge=local;huge.position={1000000,0,0};CHECK(muzzle->setLocalFrame(huge));
    const auto beforeFailure=projectio::toJson(scene);CHECK(!scene.ungroup(model->id(),error));CHECK(projectio::toJson(scene)==beforeFailure);
    CHECK(!scene.group({body->id(),barrel->id()},"Não deve perder peças",error));
    // A failed creation can consume an ID, but keeps every authored node and pose.
    CHECK(body->parent()==model&&barrel->parent()==model);CHECK(NearlyEqual(muzzle->localFrame().position,huge.position));
    CHECK(muzzle->setLocalFrame(local));CHECK(scene.attachmentFrame(muzzle->id(),world,error));
    CHECK(scene.ungroup(model->id(),error));CHECK(scene.attachmentFrame(muzzle->id(),recovered,error));CHECK(NearlyEqual(world.position,recovered.position));
    const auto muzzleId=muzzle->id();CHECK(scene.remove(barrel));CHECK(!scene.find(muzzleId));
}
void ModelTransforms() {
    Scene scene;std::string error;scene.buildDefaultScene();
    auto* a=scene.createPart(PartShape::Block,{10,4,0},{2,2,2},"A");a->setAnchored(true);
    auto* b=scene.createPart(PartShape::Block,{14,4,0},{2,2,2},"B");b->setAnchored(true);
    scene.select(a);scene.toggleSelection(b);CHECK(scene.selectedIds().size()==2);CHECK(scene.selected(a->id()));
    auto* model=scene.group(scene.selectedIds(),"Conjunto",error);CHECK(model);CHECK(model&&model->children().size()==2);
    if(!model)return;const auto id=model->id();ModelFrame frame;CHECK(scene.modelFrame(id,frame,error));CHECK(NearlyEqual(frame.position,{12,4,0}));
    a->setRotationEulerDegrees({15,0,17});
    frame.position={17,5,0};CHECK(scene.transformModel(id,frame,error));CHECK(NearlyEqual(a->position(),{15,5,0}));
    frame.rotation=QuatFromEulerDegrees({0,90,0});frame.scale=2;CHECK(scene.transformModel(id,frame,error));
    CHECK(NearlyEqual(a->position(),{17,5,4},.001f));CHECK(NearlyEqual(a->size(),{4,4,4}));
    PhysicsWorld physics;physics.attach(scene);Part* hit=nullptr;
    CHECK(physics.raycast({17,20,4},{0,-1,0},30,&hit)&&hit==a);
    const auto aid=a->id(),bid=b->id();const auto position=a->position();frame.position={20,5,1};CHECK(scene.setModelPivot(id,frame,error));CHECK(NearlyEqual(a->position(),position));
    frame.position.x+=1;CHECK(scene.transformModel(id,frame,error));CHECK(NearlyEqual(a->position(),Vector3Add(position,{1,0,0})));
    auto* clone=scene.duplicateTree(*model);CHECK(clone&&clone->id()!=id);ModelFrame cloned;CHECK(scene.modelFrame(clone->id(),cloned,error));CHECK(NearlyEqual(cloned.position,frame.position));
    CHECK(cloned.scale==2);CHECK(scene.remove(clone));
    const auto originalRotation=a->rotation();
    const auto before=projectio::toJson(scene);Scene loaded;CHECK(projectio::fromJson(loaded,before,error));ModelFrame recovered;CHECK(loaded.modelFrame(id,recovered,error));
    CHECK(NearlyEqual(recovered.position,frame.position));CHECK(NearlyEqual(recovered.rotation,frame.rotation));CHECK(recovered.scale==2);CHECK(NearlyEqual(loaded.findPart(aid)->rotation(),originalRotation));
    auto invalid=frame;invalid.scale=.0001f;CHECK(!scene.transformModel(id,invalid,error));CHECK(projectio::toJson(scene)==before);
    auto bad=nlohmann::json::parse(before);for(auto& object:bad["objects"])if(object["id"]==id)object["modelPivot"]["scale"]=-1;
    CHECK(!projectio::fromJson(scene,bad.dump(),error));CHECK(projectio::toJson(scene)==before);
    scene.select(a);scene.toggleSelection(b);CHECK(scene.remove(b));CHECK(!scene.selected(bid));CHECK(scene.selected(a->id()));
    // Keep selection coherent after removals and discard it when loading a scene.
    CHECK(projectio::fromJson(scene,before,error));CHECK(scene.selectedIds().empty());
    const auto kept=scene.findPart(aid)->position();CHECK(scene.ungroup(id,error));CHECK(scene.find(id)==nullptr);CHECK(NearlyEqual(scene.findPart(aid)->position(),kept));
    CHECK(scene.validate(error));
}
void BodyConfiguration() {
    Scene scene;std::string error;
    auto* floor=scene.createPart(PartShape::Block,{0,-.5f,0},{100,1,100},"Floor");floor->setAnchored(true);
    auto* part=scene.createPart(PartShape::Block,{-10,1,0},{2,2,2},"Body");
    BodyProperties body;body.density=2;body.friction=0;body.restitution=.6f;body.linearDamping=0;body.angularDamping=.3f;
    CHECK(part->setBodyProperties(body,error));
    PhysicsWorld world;world.attach(scene);CHECK(NearlyEqual(world.bodyMass(part->id()),16));CHECK(world.bodyMass(floor->id())==0);
    body.density=3;CHECK(part->setBodyProperties(body,error));world.syncScene(scene);CHECK(NearlyEqual(world.bodyMass(part->id()),24));
    part->setSize({4,2,2});world.syncScene(scene);CHECK(NearlyEqual(world.bodyMass(part->id()),48));
    auto* copy=scene.duplicateTree(*part);CHECK(copy&&copy->isPart());CHECK(static_cast<Part*>(copy)->bodyProperties().density==3);
    CHECK(scene.remove(copy));
    const auto json=projectio::toJson(scene);Scene loaded;CHECK(projectio::fromJson(loaded,json,error));CHECK(projectio::toJson(loaded)==json);
    CHECK(NearlyEqual(loaded.findPart(part->id())->bodyProperties().friction,0));
    Scene declarative;CHECK(scenelua::fromLua(declarative,scenelua::toLua(scene),error));CHECK(projectio::toJson(declarative)==json);
    auto invalid=nlohmann::json::parse(json);
    for(auto& object:invalid["objects"])if(object["id"]==part->id())object["body"]["density"]=-1;
    CHECK(!projectio::fromJson(loaded,invalid.dump(),error));CHECK(projectio::toJson(loaded)==json);
    body.density=std::numeric_limits<float>::quiet_NaN();CHECK(!part->setBodyProperties(body,error));CHECK(part->bodyProperties().density==3);
    auto legacy=nlohmann::json::parse(json);for(auto& object:legacy["objects"])object.erase("body");
    Scene old;CHECK(projectio::fromJson(old,legacy.dump(),error));CHECK(old.findPart(part->id())->bodyProperties().density==1);
    // Observe actual Bullet friction and damping instead of only comparing metadata.
    auto* rough=scene.createPart(PartShape::Block,{10,1,0},{2,2,2},"Rough");
    body={};body.friction=1;body.linearDamping=0;body.restitution=0;CHECK(rough->setBodyProperties(body,error));
    auto* damped=scene.createPart(PartShape::Block,{20,20,0},{2,2,2},"Damped");body.linearDamping=1;CHECK(damped->setBodyProperties(body,error));
    body={};body.restitution=1;CHECK(floor->setBodyProperties(body,error));
    auto* bounce=scene.createPart(PartShape::Block,{30,6,0},{2,2,2},"Bounce");CHECK(bounce->setBodyProperties(body,error));
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));
    bool bounced=false;for(int i=0;i<30;++i){runtime.update(1.f/60,{});if(runtime.partVelocity(bounce->id()).y>1)bounced=true;}
    CHECK(bounced);
    CHECK(runtime.partMotion(part->id(),{5,0,0},game::PartMotion::Velocity,error));
    CHECK(runtime.partMotion(rough->id(),{5,0,0},game::PartMotion::Velocity,error));
    CHECK(runtime.partMotion(damped->id(),{5,0,0},game::PartMotion::Velocity,error));
    for(int i=0;i<30;++i)runtime.update(1.f/60,{});
    CHECK(runtime.partVelocity(part->id()).x>4);CHECK(runtime.partVelocity(rough->id()).x<runtime.partVelocity(part->id()).x-1);
    CHECK(std::fabs(runtime.partVelocity(damped->id()).x)<.01f);CHECK(runtime.partMass(part->id())==48);runtime.stop();
    scene.createScript(InstanceClass::Script,nullptr,"InvalidBody.lua")->source="linblux.scene.find('Body').body={density=7,friction=-1}";
    CHECK(runtime.start(scene,error));CHECK(runtime.scripts().errorCount()==1);
    CHECK(runtime.scene().findPart(part->id())->bodyProperties().density==3);CHECK(runtime.partMass(part->id())==48);runtime.stop();
}
void Scripts() {
    Scene author;author.buildDefaultScene();
    auto* target=author.createPart(PartShape::Block,{20,4,0},{4,4,4},"Target");target->setAnchored(true);
    auto* storage=author.ensureService(InstanceClass::ReplicatedStorage);
    auto* module=author.createScript(InstanceClass::ModuleScript,storage,"Config");module->source="return { x = 31 }";
    auto* logic=author.createScript(InstanceClass::Script,nullptr,"Game.server.lua");
    logic->source=R"(
assert(os == nil and io == nil and debug == nil and package == nil)
assert(load == nil and dofile == nil and loadfile == nil and pcall == nil and coroutine == nil)
local config = require(game:GetService("ReplicatedStorage"):FindFirstChild("Config"))
workspace.Target.Position = Vector3.new(config.x, 4, 0)
workspace.Target.Color = Color3.fromRGB(25, 100, 240)
workspace.Target:SetAttribute("score", 42)
assert(workspace.Target.Parent == workspace)
assert(#workspace:GetDescendants() == 3)
local Players = game:GetService("Players")
Players.CharacterAdded:Connect(function(character)
    workspace:SetAttribute("spawns", (workspace:GetAttribute("spawns") or 0) + 1)
    character.Humanoid.WalkSpeed = 20
end)
local connection
connection = game:GetService("RunService").Heartbeat:Connect(function(dt)
    assert(dt >= 0)
    workspace:SetAttribute("heartbeat", true)
    connection:Disconnect()
end)
task.wait(0.05)
workspace.Target.Size = Vector3.new(3, 5, 2)
print("Game task completed")
)";
    auto* disabled=author.createScript(InstanceClass::Script,nullptr,"Disabled");disabled->enabled=false;disabled->source="error('must not execute')";
    auto* stored=author.createScript(InstanceClass::Script,storage,"Stored");stored->source="error('storage must not autoexecute')";
    const auto baseline=projectio::toJson(author); CHECK(target->position().x==20);CHECK(author.workspace()->attributes().empty());
    Scene reopened;std::string error;CHECK(projectio::fromJson(reopened,baseline,error));CHECK(projectio::toJson(reopened)==baseline);
    game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    CHECK(runtime.scripts().active());CHECK(runtime.scene().findPart(target->id())->position().x==31);
    CHECK(runtime.scene().workspace()->attributes().count("spawns"));CHECK(runtime.settings().walkSpeed==20);
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().findPart(target->id())->size().y==5);
    CHECK(runtime.scene().workspace()->attributes().count("heartbeat"));
    CHECK(runtime.scripts().memoryUsed()>0 && runtime.scripts().memoryUsed()<=8*1024*1024);
    CHECK(runtime.scripts().taskCount()==0);CHECK(runtime.validate(error));
    CHECK(runtime.respawn(error));runtime.update(1.f/60,{});
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("spawns"))==2);
    CHECK(projectio::toJson(author)==baseline);runtime.stop();
    CHECK(!runtime.scripts().active());CHECK(runtime.scripts().taskCount()==0);CHECK(runtime.scripts().memoryUsed()==0);
    for(int i=0;i<3;++i){CHECK(runtime.start(author,error));CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("spawns"))==1);runtime.stop();}
    auto* broken=author.createScript(InstanceClass::Script,nullptr,"Broken.lua");broken->source="local function bad()\n error('specific failure')\nend\nbad()";
    auto* loop=author.createScript(InstanceClass::Script,nullptr,"Loop.lua");loop->source="while true do end";
    auto* memory=author.createScript(InstanceClass::LocalScript,author.workspace(),"Memory.lua");memory->source="local big = string.rep('x', 32*1024*1024)";
    auto* stale=author.createScript(InstanceClass::Script,nullptr,"Stale.lua");stale->source="local p = Instance.new('Part'); p:Destroy(); print(p.Name)";
    auto* syntax=author.createScript(InstanceClass::Script,nullptr,"Syntax.lua");syntax->source="local = )";
    auto* wait=author.createScript(InstanceClass::Script,nullptr,"Waiting.lua");wait->source="task.wait(100);error('must be canceled on Stop')";
    CHECK(runtime.start(author,error));
    bool trace=false,quota=false,oom=false,destroyed=false,parse=false,forbiddenRun=false;
    for(const auto& msg:runtime.scripts().messages()) {
        trace|=msg.error&&msg.text.find("specific failure")!=std::string::npos&&msg.text.find("stack traceback")!=std::string::npos;
        quota|=msg.error&&msg.text.find("quota")!=std::string::npos;
        oom|=msg.error&&msg.text.find("memory")!=std::string::npos;
        destroyed|=msg.error&&msg.text.find("destroyed")!=std::string::npos;
        parse|=msg.error&&msg.script=="Syntax.lua";
        forbiddenRun|=msg.text.find("must not")!=std::string::npos;
    }
    CHECK(trace);CHECK(quota);CHECK(oom);CHECK(destroyed);CHECK(parse);CHECK(!forbiddenRun);
    CHECK(runtime.scripts().taskCount()>0);CHECK(runtime.validate(error));
    runtime.stop();CHECK(runtime.scripts().taskCount()==0);CHECK(runtime.validate(error));
}
void LinbluxScripts() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* platform=author.createPart(PartShape::Block,{20,8,0},{6,1,6},"Plataforma");platform->setAnchored(true);
    auto* bound=author.createScript(InstanceClass::Script,platform,"Plataforma.lua");
    bound->source=R"(
assert(linblux.api_version == 1 and linblux.self.name == "Plataforma")
assert(linblux.script.parent == linblux.self and linblux.script.type == "script")
local part = linblux.self
part.material = "metal"
part.color = linblux.vec3(210, 35, 65)
part.transparency = 0.25
part.size = linblux.vec3(8, 2, 6)
part.rotation = linblux.vec3(0, 45, 0)
linblux.events.on("start", function() part:set_attribute("started", true) end)
linblux.events.on("update", function(dt)
    local p = part.position
    part.position = linblux.vec3(p.x + dt * 3, p.y, p.z)
end)
linblux.time.after(0.03, function() part:set_attribute("timer", true) end)
)";
    auto* global=author.createScript(InstanceClass::Script,nullptr,"Mundo.lua");
    global->source=R"(
local root = linblux.scene.root
assert(linblux.self.type == "service")
local group = linblux.scene.create("group", {name="ObjetosLua"})
local part = linblux.scene.create("part", {name="CriadoLua", parent=group,
    position=linblux.vec3(30,9,0), size=linblux.vec3(2,3,4), material="emissive", shape="sphere"})
assert(part.type == "part" and part.anchored and part.collidable)
assert(part.parent == group and group:find_child("CriadoLua") == part)
assert(linblux.scene.find(part.id) == part and linblux.scene.find("CriadoLua") == part)
assert(#group:children() == 1 and #group:descendants() == 1)
local copy = part:clone()
assert(copy.id ~= part.id)
copy:destroy()
assert(not copy.alive)
local canceled = linblux.events.on("pontuar", function() error("listener cancelado") end)
canceled:disconnect()
assert(not canceled.connected)
local connection
connection = linblux.events.on("pontuar", function(payload)
    root:set_attribute("score", payload.value)
    connection:disconnect()
end)
linblux.events.emit("pontuar", {value=42})
linblux.time.spawn(function()
    linblux.time.wait(0.04)
    assert(linblux.time.now() >= 0.04)
    root:set_attribute("waited", true)
end)
linblux.events.on("stop", function() print("LINBLUX cleanup") end)
)";
    const auto baseline=projectio::toJson(author);Scene reopened;
    CHECK(projectio::fromJson(reopened,baseline,error));CHECK(projectio::toJson(reopened)==baseline);
    game::RuntimeSession runtime;
    scripting::Limits policy;policy.memoryBytes=2*1024*1024;
    CHECK(runtime.scripts().configure(policy,error));CHECK(runtime.start(reopened,error));
    CHECK(!runtime.scripts().configure(policy,error));
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    const auto* moved=runtime.scene().findPart(platform->id());
    CHECK(moved&&moved->position().x>20.4f&&moved->size().x==8);
    CHECK(moved->material()==PartMaterial::Metal&&moved->color().r==210&&moved->color().a==191);
    CHECK(moved->attributes().count("timer")&&moved->attributes().count("started"));
    CHECK(runtime.scene().workspace()->attributes().count("score"));
    CHECK(runtime.scene().workspace()->attributes().count("waited"));
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Lua error: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);
    runtime.stop();bool cleanup=false;for(const auto& message:runtime.scripts().messages())cleanup|=message.text=="LINBLUX cleanup";
    CHECK(cleanup);CHECK(runtime.scripts().memoryUsed()==0);CHECK(runtime.scripts().taskCount()==0);

    // Removing a bound script or an explicit scope cancels callbacks already queued.
    Scene scoped;scoped.buildDefaultScene();
    auto* owner=scoped.createPart(PartShape::Block,{15,5,0},{2,2,2},"Escopo");owner->setAnchored(true);
    auto* listener=scoped.createScript(InstanceClass::Script,owner,"Listener.lua");
    listener->source=R"(linblux.events.on("late", function() error("orphan listener") end)
linblux.time.after(0.1, function() error("orphan timer") end))";
    auto* controller=scoped.createScript(InstanceClass::Script,nullptr,"Controller.lua");
    controller->source=R"(
local scope = linblux.scene.find("Escopo")
local scoped = linblux.events.on("late", function() error("dead scope") end, scope)
linblux.events.emit("late", {})
scope:destroy()
assert(not scoped.connected)
linblux.time.after(0.02, function()
    for i=1,150 do
        local p = linblux.scene.create("part")
        linblux.events.on("late", function() error("scope leaked") end, p)
        p:destroy()
        linblux.time.wait(0)
    end
    linblux.events.emit("late", {})
    print("scopes cleaned")
end)
)";
    CHECK(runtime.start(scoped,error));for(int i=0;i<170;++i)runtime.update(1.f/60,{});
    bool cleaned=false;for(const auto& message:runtime.scripts().messages()){CHECK(!message.error);cleaned|=message.text=="scopes cleaned";}
    CHECK(cleaned);CHECK(runtime.scripts().taskCount()==0);runtime.stop();

    // Invalid creation is transactional; quotas and errors leave native gameplay valid.
    Scene hostile;hostile.buildDefaultScene();
    auto* invalid=hostile.createScript(InstanceClass::Script,nullptr,"Invalid.lua");invalid->source="linblux.scene.create('part', {size=linblux.vec3(-1,2,3)})";
    auto* loop=hostile.createScript(InstanceClass::Script,nullptr,"Quota.lua");loop->source="while true do end";
    auto* memory=hostile.createScript(InstanceClass::Script,nullptr,"Memory.lua");memory->source="string.rep('x', 4*1024*1024)";
    auto* blocked=hostile.createScript(InstanceClass::Script,nullptr,"Sandbox.lua");blocked->source=R"(
assert(os==nil and io==nil and package==nil and debug==nil)
assert(load==nil and loadfile==nil and dofile==nil and string.dump==nil)
assert(pcall==nil and xpcall==nil)
require("/tmp/unsafe.lua")
)";
    CHECK(runtime.start(hostile,error));CHECK(runtime.scene().parts().size()==hostile.parts().size());
    bool quota=false,oom=false,invalidOption=false,denied=false;
    for(const auto& message:runtime.scripts().messages()){
        quota|=message.error&&message.text.find("quota")!=std::string::npos;
        oom|=message.error&&message.text.find("memory")!=std::string::npos;
        invalidOption|=message.error&&message.script=="Invalid.lua"&&message.line==1;
        denied|=message.error&&message.script=="Sandbox.lua";
    }
    CHECK(quota&&oom&&invalidOption&&denied);CHECK(runtime.validate(error));runtime.stop();
    policy.memoryBytes=1;CHECK(!runtime.scripts().configure(policy,error));
}
void LinbluxExamples() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto source=[&](const char* name){std::ifstream file(root/"assets/examples/lua"/name);CHECK(file.good());return std::string(std::istreambuf_iterator<char>(file),{});};
    Scene author;author.buildDefaultScene();std::string error;
    auto* platform=author.createPart(PartShape::Block,{20,8,0},{6,1,6},"PlataformaExemplo");
    auto* door=author.createPart(PartShape::Block,{30,6,0},{3,8,1},"PortaExemplo");
    author.createScript(InstanceClass::Script,platform,"Plataforma.lua")->source=source("plataforma.lua");
    author.createScript(InstanceClass::Script,door,"Porta.lua")->source=source("porta.lua");
    author.createScript(InstanceClass::Script,nullptr,"Mundo.lua")->source=source("mundo.lua");
    author.createScript(InstanceClass::Script,nullptr,"Temporizadores.lua")->source=source("temporizadores.lua");
    author.createScript(InstanceClass::Script,nullptr,"Camera.lua")->source=source("camera.lua");
    author.createScript(InstanceClass::Script,nullptr,"Transformacoes.lua")->source=source("transformacoes.lua");
    author.createScript(InstanceClass::Script,nullptr,"ConsultasControle.lua")->source=source("consultas_controle.lua");
    author.createScript(InstanceClass::Script,nullptr,"CorposRigidos.lua")->source=source("corpos_rigidos.lua");
    author.createScript(InstanceClass::Script,nullptr,"Modelos.lua")->source=source("modelos.lua");
    author.gameAssets().player.cameraFov=80;
    const auto baseline=projectio::toJson(author);
    const auto testDir=root/".turbo"/"lua-examples";std::filesystem::create_directories(testDir);
    const auto path=(testDir/(std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json")).string();
    CHECK(projectio::save(author,path,error));Scene reopened;CHECK(projectio::load(reopened,path,error));
    CHECK(projectio::toJson(reopened)==baseline);
    CHECK(reopened.gameAssets().player.cameraFov==80);
    game::RuntimeSession runtime;CHECK(runtime.start(reopened,error));
    for(int i=0;i<90;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().findPart(platform->id())->position().x>25);
    CHECK(runtime.scene().findPart(door->id())->position().y==13);
    CHECK(std::get<bool>(runtime.scene().findPart(door->id())->attributes().at("aberta")));
    CHECK(runtime.scene().workspace()->attributes().count("timer_concluido"));
    CHECK(runtime.scene().workspace()->attributes().count("camera_concluida"));
    CHECK(runtime.scene().workspace()->attributes().count("transformacoes_concluidas"));
    CHECK(runtime.scene().workspace()->attributes().count("consultas_controle_concluido"));
    CHECK(runtime.scene().workspace()->attributes().count("corpos_rigidos_concluido"));
    CHECK(runtime.scene().workspace()->attributes().count("modelos_concluido"));
    CHECK(runtime.playerCamera().camera().fovy==75);
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Example: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(projectio::toJson(reopened)==baseline);CHECK(runtime.validate(error));runtime.stop();std::filesystem::remove(path);
}
void LinbluxPhysics() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* script=author.createScript(InstanceClass::Script,nullptr,"Fisica.lua");
    script->source=R"(
linblux.physics.set_gravity(10)
assert(linblux.physics.gravity() == 10)
local p = linblux.scene.create("part", {name="FisicaLua", position=linblux.vec3(40,20,0),
    size=linblux.vec3(1,1,1), anchored=false})
p.velocity = linblux.vec3(0,2,0)
p:apply_impulse(linblux.vec3(1,0,0))
p:apply_force(linblux.vec3(0,60,0))
assert(math.abs(p.velocity.x - 1) < 0.01)
local hit = linblux.physics.raycast(linblux.vec3(40,30,0), linblux.vec3(0,-1,0), 15)
assert(hit and hit.object == p and hit.normal.y > 0.9)
assert(math.abs(hit.point.y - 20.5) < 0.1 and math.abs(hit.distance - 9.5) < 0.1)
assert(linblux.physics.raycast(linblux.vec3(40,30,0), linblux.vec3(0,-1,0), 15, p) == nil)
local nearby = linblux.physics.nearby(p.position, 2)
assert(#nearby == 1 and nearby[1] == p)
linblux.scene.root:set_attribute("physics_ready", true)
)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    CHECK(runtime.scene().workspace()->attributes().count("physics_ready"));
    const Part* dynamic=nullptr;for(const auto* part:runtime.scene().parts())if(part->name()=="FisicaLua")dynamic=part;
    CHECK(dynamic!=nullptr);
    if(dynamic) {
        const auto id=dynamic->id();for(int i=0;i<10;++i)runtime.update(1.f/60,{});
        CHECK(dynamic->position().x>40.1f&&dynamic->position().y>20);
        CHECK(runtime.partVelocity(id).x>.9f);
        CHECK(!runtime.partMotion(id,{NAN,0,0},game::PartMotion::Force,error));
    }
    CHECK(!runtime.partMotion(author.parts().front()->id(),{1,0,0},game::PartMotion::Impulse,error));
    CHECK(!runtime.setGravity(0,error));CHECK(runtime.settings().gravity==10);
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Physics Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
}
void LinbluxFixedContacts() {
    std::string error;
    const auto driven=[&](int fps){
        Scene author;author.buildDefaultScene();
        auto* part=author.createPart(PartShape::Block,{80,100,0},{1,1,1},"Propulsor");part->setAnchored(false);
        author.createScript(InstanceClass::Script,part,"PassoFixo.lua")->source=R"(
linblux.physics.set_gravity(1)
local ticks = 0
linblux.events.on("physics_update", function(dt)
    assert(math.abs(dt - 1/60) < 0.000001)
    ticks = ticks + 1
    linblux.self:apply_force(linblux.vec3(20,0,0))
    linblux.self:set_attribute("steps", ticks)
end)
)";
        game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<fps;++i)runtime.update(1.f/fps,{});
        CHECK(std::get<double>(runtime.scene().findPart(part->id())->attributes().at("steps"))==60);
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Fixed Lua: %s\n",message.text.c_str());CHECK(!message.error);}
        const auto speed=runtime.partVelocity(part->id()).x;CHECK(speed>1);CHECK(runtime.validate(error));return speed;
    };
    CHECK(std::fabs(driven(30)-driven(120))<.001f);
    for(const bool destroy:{false,true}){
        Scene author;author.buildDefaultScene();author.workspace()->setAttribute("destroy_on_enter",destroy);
        auto* trigger=author.createPart(PartShape::Block,{80,10,0},{4,4,4},"Sensor");trigger->setAnchored(true);trigger->setCanCollide(false);
        auto* target=author.createPart(PartShape::Block,{80,10,0},{1,1,1},"Alvo");target->setAnchored(false);
        author.createScript(InstanceClass::Script,trigger,"Contato.lua")->source=R"(
local root, sensor = linblux.scene.root, linblux.self
linblux.events.on("collision_enter", function(hit)
    if hit.first_id == 0 or hit.second_id == 0 then root:set_attribute("avatar_contact", (root:get_attribute("avatar_contact") or 0)+1) end
end)
linblux.events.on("collision_enter", function(hit)
    local other = hit.first_id == sensor.id and hit.second or hit.first
    assert(other and other.name == "Alvo")
    sensor:set_attribute("enters", (sensor:get_attribute("enters") or 0)+1)
    if root:get_attribute("destroy_on_enter") then other:destroy()
    else other.position = linblux.vec3(100,10,0) end
end, sensor)
linblux.events.on("collision_exit", function(hit)
    assert(hit.first_id == sensor.id or hit.second_id == sensor.id)
    sensor:set_attribute("exits", (sensor:get_attribute("exits") or 0)+1)
end, sensor)
)";
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        const auto& attrs=runtime.scene().findPart(trigger->id())->attributes();
        CHECK(attrs.count("enters")&&attrs.count("exits"));
        if(attrs.count("enters"))CHECK(std::get<double>(attrs.at("enters"))==1);
        if(attrs.count("exits"))CHECK(std::get<double>(attrs.at("exits"))==1);
        CHECK(runtime.scene().workspace()->attributes().count("avatar_contact"));
        const auto previous=std::get<double>(runtime.scene().workspace()->attributes().at("avatar_contact"));
        CHECK(runtime.respawn(error));for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("avatar_contact"))>previous);
        if(destroy)CHECK(!runtime.scene().find(target->id()));
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Contact Lua: %s\n",message.text.c_str());CHECK(!message.error);}
        CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);
    }
}
void NpcHeadingRegression() {
    // atan2 em heading deve cobrir o círculo inteiro, não o intervalo asin de Euler Y.
    for(float angle:{0.f,45.f,90.f,135.f,180.f,-135.f,-90.f,-45.f}) {
        Scene author;author.buildDefaultScene();std::string error;
        auto* source=author.createPart(PartShape::Block,{30,2.5f,20},{2,5,2},"Teste de giro");
        source->setAnchored(false);NpcProperties config;config.speed=2;config.turnSpeed=8;
        CHECK(source->setNpcProperties(config,error));const auto id=source->id();
        game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int frame=0;frame<30;++frame)runtime.update(1.f/60,{});
        const Vector3 direction{-std::sin(angle*DEG2RAD),0,-std::cos(angle*DEG2RAD)};
        const auto start=runtime.scene().findPart(id)->position();
        CHECK(runtime.npcs().moveTo(runtime,id,Vector3Add(start,Vector3Scale(direction,25)),error));
        for(int frame=0;frame<120;++frame) {
            const auto previous=Vector3RotateByQuaternion({0,0,-1},runtime.scene().findPart(id)->rotation());
            runtime.update(1.f/60,{});
            const auto current=Vector3RotateByQuaternion({0,0,-1},runtime.scene().findPart(id)->rotation());
            CHECK(std::acos(std::clamp(Vector3DotProduct(previous,current),-1.f,1.f))<=config.turnSpeed/60+.002f);
        }
        const auto* part=runtime.scene().findPart(id);
        const auto forward=Vector3RotateByQuaternion({0,0,-1},part->rotation());
        const float alignment=Vector3DotProduct(forward,direction);
        if(alignment<.98f)std::printf("NPC heading %.0f: alignment %.4f, forward %.3f %.3f\n",angle,alignment,forward.x,forward.z);
        CHECK(alignment>.98f);CHECK(std::fabs(forward.y)<.001f);
        const float displacement=Vector3Distance(start,part->position());
        if(displacement<=2||displacement>=4.3f)std::printf("NPC heading %.0f: displacement %.4f in 2s (speed2)\n",angle,displacement);
        CHECK(displacement>3.8f&&displacement<4.1f); // velocidade de controlador, não bloco arrastado
        CHECK(NearlyEqual(part->bodyProperties().friction,.8f)); // material autoral não alterado
        CHECK(runtime.npcs().stopMoving(id));for(int frame=0;frame<30;++frame)runtime.update(1.f/60,{});
        const auto speed=runtime.partVelocity(id);CHECK(std::hypot(speed.x,speed.z)<.02f);
        CHECK(runtime.validate(error));runtime.stop();
    }
}
void LinbluxNpc() {
    Scene author;author.buildDefaultScene();std::string error;
    // Muro estatico entre o NPC e o destino; pilar alto isola o topo.
    auto* wall=author.createPart(PartShape::Block,{10,3,0},{1,6,10},"Muro");wall->setAnchored(true);
    auto* pillar=author.createPart(PartShape::Block,{30,4,30},{6,8,6},"Pilar");pillar->setAnchored(true);
    const auto wallId=wall->id();
    // NPC autorado: dados persistidos, script proprio e controlador no Play.
    auto* sentinel=author.createPart(PartShape::Block,{-40,2.5f,40},{2,5,2},"Sentinela");
    NpcProperties authored;authored.speed=6;authored.maxHealth=50;
    CHECK(sentinel->setNpcProperties(authored,error));
    author.createScript(InstanceClass::Script,sentinel,"Sentinela.lua")->source=
        "linblux.scene.root:set_attribute(\"sentinela_viva\", true)";
    const auto sentinelId=sentinel->id();
    author.createScript(InstanceClass::Script,nullptr,"Npc.lua")->source=R"(
local root = linblux.scene.root
local nav = linblux.npc.navigation()
assert(nav.ready and nav.cells > 0 and nav.walkable > 0 and nav.cell_size > 0)
local guarda = linblux.npc.create{name="Guarda", position=linblux.vec3(0,2.6,0), size=linblux.vec3(2,5,2), speed=14}
local patrulha = linblux.npc.create{name="Patrulha", position=linblux.vec3(-20,2.6,20), size=linblux.vec3(1.5,4,1.5), speed=12}
local alvo = linblux.npc.create{name="Alvo", position=linblux.vec3(-10,2.6,0), size=linblux.vec3(2,5,2), max_health=20, respawn_delay=0.5}
assert(linblux.npc.state(guarda) == "idle")
assert(linblux.npc.health(guarda) == 100 and linblux.npc.max_health(guarda) == 100)
assert(linblux.npc.speed(guarda) == 14)
linblux.npc.set_speed(guarda, 12)
assert(linblux.npc.speed(guarda) == 12)
linblux.npc.attach(patrulha, {speed = 12})
assert(linblux.npc.speed(patrulha) == 12)
root:set_attribute("guarda", guarda.id)
root:set_attribute("patrulha", patrulha.id)
root:set_attribute("alvo", alvo.id)
linblux.time.after(0.05, function()
    assert(linblux.npc.move_to(guarda, linblux.vec3(22, 2.6, 0)) == true)
    local rota = linblux.npc.path(guarda)
    assert(#rota >= 3)
    root:set_attribute("rota", #rota)
    assert(linblux.npc.target(guarda).x == 22)
    assert(linblux.npc.state(guarda) == "moving")
    assert(linblux.npc.move_to(patrulha, linblux.vec3(-20, 2.6, -20)) == true)
    assert(linblux.npc.move_to(alvo, linblux.vec3(30, 8, 30)) == false)
    assert(linblux.npc.state(alvo) == "unreachable")
    local pontos, ok = linblux.npc.find_path(linblux.vec3(0,2.6,0), linblux.vec3(22,2.6,0))
    assert(ok and #pontos >= 3)
    linblux.npc.damage(alvo, 5)
    assert(linblux.npc.health(alvo) == 15)
    linblux.npc.heal(alvo, 2)
    assert(linblux.npc.health(alvo) == 17)
    linblux.npc.set_health(alvo, 0)
    assert(linblux.npc.state(alvo) == "dead")
end)
linblux.time.after(1.2, function()
    assert(linblux.npc.state(alvo) ~= "dead")
    assert(linblux.npc.health(alvo) == 20)
    root:set_attribute("respawn_ok", true)
end)
linblux.time.after(2.0, function()
    local efemero = linblux.npc.create{name="Efemero", position=linblux.vec3(-20,2.6,34)}
    root:set_attribute("efemero", efemero.id)
    efemero:destroy()
    root:set_attribute("efemero_destruido", true)
end)
linblux.time.after(2.2, function()
    local desanexado = linblux.npc.create{name="Desanexa", position=linblux.vec3(-30,2.6,34)}
    linblux.npc.detach(desanexado)
    root:set_attribute("detach_ok", true)
end)
)";
    // Serializacao: o componente viaja no projeto e volta com os mesmos valores.
    const auto json=projectio::toJson(author);Scene reopened;
    CHECK(projectio::fromJson(reopened,json,error));CHECK(projectio::toJson(reopened)==json);
    const auto* restored=reopened.findPart(sentinelId);
    CHECK(restored!=nullptr&&restored->hasNpc());
    if(restored!=nullptr&&restored->hasNpc())CHECK(restored->npcProperties()->speed==6&&restored->npcProperties()->maxHealth==50);
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;
    CHECK(runtime.start(author,error));
    CHECK(runtime.npcs().attached(sentinelId));
    // O script de autoria roda no Play e cria tres NPCs independentes.
    CHECK(runtime.npcs().count()==4);
    for(int i=0;i<300;++i){
        runtime.update(1.f/60,{});
        for(auto it=runtime.scene().parts().begin();it!=runtime.scene().parts().end();++it){
            const auto* part=*it;if(!part||part->name()!="Guarda")continue;
            const auto p=part->position();
            if(p.x>9.4f&&p.x<10.6f&&p.z>-5.f&&p.z<5.f)CHECK(false&&"NPC atravessou o muro");
        }
    }
    const auto attribute=[&](const char* name)->bool{return runtime.scene().workspace()->attributes().count(name)>0;};
    CHECK(attribute("sentinela_viva"));CHECK(attribute("respawn_ok"));CHECK(attribute("efemero_destruido"));CHECK(attribute("detach_ok"));
    CHECK(attribute("rota")&&std::get<double>(runtime.scene().workspace()->attributes().at("rota"))>=3);
    const auto id=[&](const char* name){return InstanceId(std::get<double>(runtime.scene().workspace()->attributes().at(name)));};
    const auto* guarda=runtime.scene().findPart(id("guarda"));
    const auto* patrulha=runtime.scene().findPart(id("patrulha"));
    CHECK(guarda!=nullptr&&patrulha!=nullptr);
    if(guarda){CHECK(std::fabs(guarda->position().x-22)<2.f&&std::fabs(guarda->position().z)<2.5f);CHECK(runtime.npcs().state(guarda->id())==game::NpcState::Arrived);}
    if(patrulha){CHECK(std::fabs(patrulha->position().x+20)<2.f&&std::fabs(patrulha->position().z+20)<2.5f);CHECK(runtime.npcs().state(patrulha->id())==game::NpcState::Arrived);}
    CHECK(!runtime.npcs().attached(id("efemero")));
    CHECK(runtime.npcs().count()==4);
    CHECK(runtime.validate(error));
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("NPC Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(projectio::toJson(author)==baseline);
    runtime.stop();CHECK(runtime.npcs().count()==0);CHECK(runtime.validate(error));
}
void NpcExamples() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto source=[&](const char* name){std::ifstream file(root/"assets/examples/lua"/name);CHECK(file.good());return std::string(std::istreambuf_iterator<char>(file),{});};
    const auto find=[&](const game::RuntimeSession& runtime,const char* name)->const Part*{
        for(const auto* part:runtime.scene().parts())if(part->name()==name)return part;return nullptr;};
    const auto noErrors=[&](const game::RuntimeSession& runtime,const char* label){
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("NPC %s: %s\n",label,message.text.c_str());CHECK(!message.error);}};

    // Patrulha: percorre o ciclo e para em cada waypoint.
    {
        Scene author;author.buildDefaultScene();std::string error;
        author.createScript(InstanceClass::Script,nullptr,"npc_patrulha.lua")->source=source("npc_patrulha.lua");
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        float farthest=0;
        for(int i=0;i<600;++i){
            runtime.update(1.f/60,{});
            const auto* guard=find(runtime,"GuardaPatrulha");
            if(guard)farthest=std::max(farthest,Vector3Distance(guard->position(),{-30,2.6f,-30}));
        }
        const auto& attrs=runtime.scene().workspace()->attributes();
        CHECK(attrs.count("patrulha_pronta"));
        CHECK(attrs.count("patrulha_pontos")&&std::get<double>(attrs.at("patrulha_pontos"))>=2);
        CHECK(!attrs.count("patrulha_inacessivel"));
        const auto* guard=find(runtime,"GuardaPatrulha");CHECK(guard!=nullptr);
        CHECK(farthest>10); // percorreu a rota, nao ficou parado
        noErrors(runtime,"patrulha");CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    }

    // Perseguidor: atualiza o destino quando o alvo se move e para ao perde-lo.
    {
        Scene author;author.buildDefaultScene();std::string error;
        author.createScript(InstanceClass::Script,nullptr,"npc_perseguidor.lua")->source=source("npc_perseguidor.lua");
        author.createScript(InstanceClass::Script,nullptr,"MovimentoJogador.lua")->source=R"(
linblux.time.after(1.5, function() linblux.player:teleport(linblux.vec3(-24, 6, 0)) end)
linblux.time.after(3.0, function() linblux.player:teleport(linblux.vec3(60, 6, 60)) end)
)";
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        const auto* monster=find(runtime,"Perseguidor");CHECK(monster!=nullptr);
        const Vector3 spawn=monster?monster->position():Vector3{};
        for(int i=0;i<220;++i)runtime.update(1.f/60,{});
        const auto& attrs=runtime.scene().workspace()->attributes();
        CHECK(attrs.count("perseguindo"));
        CHECK(attrs.count("replanejamentos")&&std::get<double>(attrs.at("replanejamentos"))>=2);
        CHECK(attrs.count("alvo_perdido"));
        if(monster)CHECK(Vector3Distance(monster->position(),spawn)>3&&runtime.npcs().state(monster->id())!=game::NpcState::Moving);
        noErrors(runtime,"perseguidor");CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    }

    // Monstro com dano: persegue, ataca pela vida real e morre ao zerar a vida.
    {
        Scene author;author.buildDefaultScene();std::string error;
        author.createScript(InstanceClass::Script,nullptr,"npc_monstro.lua")->source=source("npc_monstro.lua");
        author.createScript(InstanceClass::Script,nullptr,"Ataque.lua")->source=R"(
linblux.time.after(6.0, function() linblux.events.emit("atacar_monstro", {dano = 100}) end)
)";
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        const float maxHealth=runtime.settings().maxHealth;
        for(int i=0;i<420;++i)runtime.update(1.f/60,{});
        const auto& attrs=runtime.scene().workspace()->attributes();
        CHECK(attrs.count("ataques")&&std::get<double>(attrs.at("ataques"))>=1);
        CHECK(runtime.health()<maxHealth);
        CHECK(attrs.count("monstro_morto"));
        const auto* monster=find(runtime,"Monstro");CHECK(monster!=nullptr);
        if(monster)CHECK(runtime.npcs().state(monster->id())==game::NpcState::Dead);
        noErrors(runtime,"monstro");CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    }
}
void NpcAnimatedExamples() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto source=[&](const char* name){std::ifstream file(root/"assets/examples/lua"/name);CHECK(file.good());return std::string(std::istreambuf_iterator<char>(file),{});};

    // Patrulha animada: rig aplicado e clipes reais vistos em movimento.
    {
        Scene author;author.buildDefaultScene();std::string error;
        auto& rig=author.gameAssets().createLinbluxCharacter();author.gameAssets().defaultRig=rig.id;
        author.createScript(InstanceClass::Script,nullptr,"npc_patrulha_animado.lua")->source=source("npc_patrulha_animado.lua");
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<420;++i)runtime.update(1.f/60,{});
        const auto& attrs=runtime.scene().workspace()->attributes();
        CHECK(attrs.count("patrulha_animada_pronta"));
        CHECK(attrs.count("patrulha_rig")&&!std::get<std::string>(attrs.at("patrulha_rig")).empty());
        CHECK(attrs.count("patrulha_run"));
        CHECK(attrs.count("patrulha_animada_pontos")&&std::get<double>(attrs.at("patrulha_animada_pontos"))>=1);
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Patrulha animada: %s\n",message.text.c_str());CHECK(!message.error);}
        CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    }

    // Perseguidor animado: run/perseguição/ataque sem erro.
    {
        Scene author;author.buildDefaultScene();std::string error;
        auto& rig=author.gameAssets().createLinbluxCharacter();author.gameAssets().defaultRig=rig.id;
        author.createScript(InstanceClass::Script,nullptr,"npc_perseguidor_animado.lua")->source=source("npc_perseguidor_animado.lua");
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<420;++i)runtime.update(1.f/60,{});
        const auto& attrs=runtime.scene().workspace()->attributes();
        CHECK(attrs.count("perseguidor_animado_pronto"));
        CHECK(attrs.count("perseguidor_perseguindo"));
        CHECK(attrs.count("perseguidor_ataques")&&std::get<double>(attrs.at("perseguidor_ataques"))>=1);
        CHECK(attrs.count("perseguidor_clipe"));
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Perseguidor animado: %s\n",message.text.c_str());CHECK(!message.error);}
        CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    }

    // Monstro animado: ataque, hurt, morte com clipe e respawn.
    {
        Scene author;author.buildDefaultScene();std::string error;
        auto& rig=author.gameAssets().createLinbluxCharacter();author.gameAssets().defaultRig=rig.id;
        author.createScript(InstanceClass::Script,nullptr,"npc_monstro_animado.lua")->source=source("npc_monstro_animado.lua");
        author.createScript(InstanceClass::Script,nullptr,"FerirMonstro.lua")->source=R"(
linblux.time.after(3.0, function() linblux.events.emit("atacar_monstro", {dano = 20}) end)
linblux.time.after(4.0, function() linblux.events.emit("atacar_monstro", {dano = 100}) end)
)";
        const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<540;++i)runtime.update(1.f/60,{});
        const auto& attrs=runtime.scene().workspace()->attributes();
        CHECK(attrs.count("monstro_animado_pronto"));
        CHECK(attrs.count("monstro_ataques")&&std::get<double>(attrs.at("monstro_ataques"))>=1);
        CHECK(attrs.count("monstro_ferido"));
        CHECK(attrs.count("monstro_morto"));
        CHECK(attrs.count("monstro_morte_animada"));
        CHECK(attrs.count("monstro_respawn"));
        for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Monstro animado: %s\n",message.text.c_str());CHECK(!message.error);}
        CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    }
}

void NpcAnimation() {
    Scene author;author.buildDefaultScene();std::string error;
    auto& rig=author.gameAssets().createLinbluxCharacter();const std::string rigId=rig.id;
    author.gameAssets().defaultRig=rigId;
    auto* npc=author.createPart(PartShape::Block,{12,3,0},{2,5,2},"NpcAnimado");
    NpcProperties properties;properties.speed=14;properties.maxHealth=40;properties.respawnDelay=1;properties.rig="player";
    CHECK(npc->setNpcProperties(properties,error));
    const auto npcId=npc->id();
    const auto baseline=projectio::toJson(author);
    game::RuntimeSession runtime;
    if(!runtime.start(author,error))std::printf("NPC animação: start falhou: %s\n",error.c_str());
    CHECK(runtime.npcs().attached(npcId));
    CHECK(runtime.npcs().rig(npcId)==rigId);
    runtime.update(1.f/60,{});
    CHECK(runtime.scene().npcVisuals().size()==1);
    CHECK(runtime.scene().npcVisuals()[0].rigId==rigId);
    CHECK(runtime.scene().npcVisuals()[0].part==npcId);
    CHECK(runtime.npcs().clip(npcId)==game::NpcClip::Idle);

    // Correr: velocidade alta seleciona o clipe run e a pose move os membros.
    CHECK(runtime.npcs().moveTo(runtime,npcId,{28,3,0},error));
    for(int i=0;i<20;++i)runtime.update(1.f/60,{});
    CHECK(runtime.npcs().clip(npcId)==game::NpcClip::Run);
    const auto& running=runtime.scene().npcVisuals();
    CHECK(running.size()==1);
    if(!running.empty()) {
        const auto& hip=running[0].pose[game::LeftHip].rotation;
        CHECK(std::fabs(hip.z)>0.01f||std::fabs(hip.x)>0.01f);
        const auto* live=runtime.scene().findPart(npcId);
        if(live)CHECK(std::fabs(running[0].root.position.y-(live->position().y-2.5f+3.f))<0.3f);
    }

    // Ataque, dano e morte trocam o clipe e alimentam o mesmo visual.
    CHECK(runtime.npcs().attack(runtime,npcId,error));
    runtime.update(1.f/60,{});
    CHECK(runtime.npcs().clip(npcId)==game::NpcClip::Attack);
    CHECK(std::fabs(runtime.scene().npcVisuals()[0].pose[game::RightShoulder].rotation.z)>0.01f);
    CHECK(runtime.npcs().damage(runtime,npcId,5,error));
    runtime.update(1.f/60,{});
    CHECK(runtime.npcs().clip(npcId)==game::NpcClip::Hurt);
    for(int i=0;i<25;++i)runtime.update(1.f/60,{});
    CHECK(runtime.npcs().clip(npcId)!=game::NpcClip::Hurt);
    CHECK(runtime.npcs().damage(runtime,npcId,1000,error));
    runtime.update(1.f/60,{});
    CHECK(runtime.npcs().state(npcId)==game::NpcState::Dead);
    CHECK(runtime.npcs().clip(npcId)==game::NpcClip::Death);
    bool refused=false;
    try{refused=!runtime.npcs().attack(runtime,npcId,error);}catch(...){refused=true;}
    CHECK(refused);
    for(int i=0;i<35;++i)runtime.update(1.f/60,{});
    CHECK(!runtime.scene().npcVisuals().empty());
    if(!runtime.scene().npcVisuals().empty())
        CHECK(std::fabs(runtime.scene().npcVisuals()[0].pose[game::RootJoint].rotation.z)>0.2f);

    // Respawn coerente: vida cheia, clipe fora de death e visual recriado.
    for(int i=0;i<90;++i)runtime.update(1.f/60,{});
    CHECK(runtime.npcs().state(npcId)!=game::NpcState::Dead);
    CHECK(runtime.npcs().health(npcId)==runtime.npcs().maxHealth(npcId));
    CHECK(runtime.npcs().clip(npcId)!=game::NpcClip::Death);
    CHECK(!runtime.scene().npcVisuals().empty());

    // Sem rig o NPC volta a ser apenas a Part dinâmica.
    CHECK(runtime.npcs().setRig(runtime,npcId,"",error));
    runtime.update(1.f/60,{});
    CHECK(runtime.scene().npcVisuals().empty());
    CHECK(!runtime.npcs().setRig(runtime,npcId,"rig-inexistente",error));
    CHECK(!error.empty());
    CHECK(runtime.npcs().setRig(runtime,npcId,rigId,error));
    runtime.update(1.f/60,{});
    CHECK(runtime.scene().npcVisuals().size()==1);

    // Rig removido da cena é detectado, nunca desenhado como ponteiro órfão.
    runtime.mutableScene().gameAssets().removeRig(rigId);
    CHECK(!runtime.npcs().validate(runtime,error));
    CHECK(projectio::toJson(author)==baseline);
    runtime.stop();

    // O id do rig é serializado como texto estável (nunca ponteiro).
    properties.rig=rigId;CHECK(npc->setNpcProperties(properties,error));
    Scene roundTrip;CHECK(projectio::fromJson(roundTrip,projectio::toJson(author),error));
    CHECK(roundTrip.findPart(npcId)!=nullptr);
    if(roundTrip.findPart(npcId)){
        CHECK(roundTrip.findPart(npcId)->npcProperties()->rig==rigId);
        game::RuntimeSession replay;CHECK(replay.start(roundTrip,error));
        replay.update(1.f/60,{});
        CHECK(replay.scene().npcVisuals().size()==1);
        replay.stop();
    }
}

void LinbluxProgress() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto directory=root/".turbo"/("progress-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    Scene author;author.buildDefaultScene();std::string error;
    std::ifstream file(root/"assets/examples/lua/persistencia.lua");CHECK(file.good());
    author.createScript(InstanceClass::Script,nullptr,"Persistencia.lua")->source=std::string(std::istreambuf_iterator<char>(file),{});
    const game::ProgressOptions options{"test-experience",directory.string(),true};
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;
    const auto visits=[&]{return std::get<double>(runtime.scene().workspace()->attributes().at("visitas_persistidas"));};
    CHECK(runtime.start(author,error,options));CHECK(visits()==1);CHECK(runtime.storage().persistent()&&runtime.storage().pending());
    runtime.stop(false); // canceled startup must not save its writes or stop callback
    CHECK(runtime.start(author,error,options));CHECK(visits()==1);
    runtime.update(1.f/60,{});CHECK(!runtime.storage().pending());
    game::RuntimeSession competing;CHECK(!competing.start(author,error,options));
    runtime.stop();CHECK(runtime.start(author,error,options));CHECK(visits()==2);CHECK(runtime.storage().get("encerrado")=="true");
    runtime.pause();CHECK(!runtime.storage().pending());runtime.stop();
    CHECK(runtime.start(author,error,{options.identity,options.directory,false}));CHECK(visits()==1);CHECK(!runtime.storage().persistent());runtime.stop();
    CHECK(runtime.start(author,error,options));CHECK(visits()==3);runtime.stop();
    CHECK(projectio::toJson(author)==baseline);
    std::filesystem::path data;
    for(const auto& item:std::filesystem::recursive_directory_iterator(directory))if(item.path().filename()=="progress.json")data=item.path();
    CHECK(!data.empty());
    if(!data.empty()){
        const auto original=[&]{std::ifstream input(data);return std::string(std::istreambuf_iterator<char>(input),{});}();
        CHECK((std::filesystem::status(data).permissions()&std::filesystem::perms::others_read)==std::filesystem::perms::none);
        {std::ofstream out(data);out<<"corrupted progress";}
        CHECK(!runtime.start(author,error,options));
        {std::ifstream input(data);CHECK(std::string(std::istreambuf_iterator<char>(input),{})=="corrupted progress");}
        {std::ofstream out(data);out<<original;}
        const auto external=directory/"outside.json";{std::ofstream out(external);out<<original;}
        std::filesystem::remove(data);std::filesystem::create_symlink(external,data);
        CHECK(!runtime.start(author,error,options));
        runtime.stop(false);
        {std::ifstream input(external);CHECK(std::string(std::istreambuf_iterator<char>(input),{})==original);}
    }
    game::ProgressStore store;store.open({});bool rejected=false;
    try{store.set("oversized",std::string(65537,'x'));}catch(const std::exception&){rejected=true;}CHECK(rejected);
    rejected=false;try{store.set("bad","{invalid");}catch(const std::exception&){rejected=true;}CHECK(rejected);
    for(int i=0;i<62;++i)store.set("limited",std::to_string(i));
    rejected=false;try{store.set("limited","99");}catch(const std::exception&){rejected=true;}CHECK(rejected);
    store.tick(0);CHECK(store.get("limited")=="61");store.close();std::filesystem::remove_all(directory);
}
void LinbluxTransformValidation() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* target=author.createPart(PartShape::Block,{70,10,0},{3,3,3},"AlvoValidacao");target->setAnchored(true);
    const char* sources[]{
        "local p=linblux.scene.find('AlvoValidacao'); p.transform={position=linblux.vec3(100,100,100),size=linblux.vec3(0,2,2)}",
        "linblux.scene.find('AlvoValidacao'):get_transform(linblux.scene.root)",
        "linblux.scene.find('AlvoValidacao'):scale_by(-2)",
        "local other=linblux.scene.create('part'); local c=other:add_script(\"linblux.scene.root:set_attribute('component_survived',true)\"); linblux.scene.find('AlvoValidacao'):remove_component(c)",
        R"(linblux.scene.find('AlvoValidacao'):add_script('', 'a\0b'))",
        "linblux.scene.find('AlvoValidacao'):add_script(string.rep('x',262145))"
    };
    int index=0;for(const auto* source:sources)author.createScript(InstanceClass::Script,nullptr,"ValidacaoTransform"+std::to_string(++index))->source=source;
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<20;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scripts().errorCount()==6);
    const auto* actual=runtime.scene().findPart(target->id());CHECK(NearlyEqual(actual->position(),target->position()));CHECK(NearlyEqual(actual->size(),target->size()));
    CHECK(actual->children().empty());CHECK(runtime.scene().workspace()->attributes().count("component_survived"));
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
}
void LinbluxCameraControl() {
    Scene author;author.buildDefaultScene();std::string error;
    author.gameAssets().player.cameraFov=85;
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    auto& camera=runtime.playerCamera();CHECK(camera.camera().fovy==85);
    Camera3D pose{{10,8,10},{0,3,0},{0,1,0},90,CAMERA_PERSPECTIVE};
    CHECK(camera.setPose(pose,.2f,error));CHECK(camera.scripted());
    runtime.update(.1f,{});CHECK(camera.transitioning());CHECK(camera.camera().position.x>0&&camera.camera().position.x<10);
    runtime.pause();const auto paused=camera.camera();runtime.update(.1f,{});CHECK(NearlyEqual(paused.position,camera.camera().position));runtime.resume();
    runtime.update(.1f,{});CHECK(!camera.transitioning());CHECK(NearlyEqual(pose.position,camera.camera().position));
    CHECK(NearlyEqual(pose.target,camera.camera().target));CHECK(camera.camera().fovy==90);
    const auto original=camera.camera();pose.up={0,0,0};CHECK(!camera.setPose(pose,0,error));CHECK(NearlyEqual(original.position,camera.camera().position));
    pose=original;pose.fovy=NAN;CHECK(!camera.setPose(pose,0,error));pose=original;
    // Opposite orientations must never interpolate to a zero look/up vector.
    pose.target=Vector3Subtract(pose.position,Vector3Subtract(original.target,original.position));pose.up={0,-1,0};
    CHECK(camera.setPose(pose,.2f,error));
    for(int i=0;i<12;++i){runtime.update(1.f/60,{});const auto c=camera.camera();CHECK(Vector3Length(Vector3CrossProduct(Vector3Subtract(c.target,c.position),c.up))>.01f);}
    camera.release(runtime.characterRoot().position,runtime.settings());runtime.update(1.f/60,{});CHECK(!camera.scripted());CHECK(camera.camera().fovy==85);
    CHECK(projectio::toJson(author)==baseline);CHECK(runtime.validate(error));runtime.stop();
    auto old=nlohmann::json::parse(baseline);old["game"]["player"].erase("cameraFov");Scene loaded;
    CHECK(projectio::fromJson(loaded,old.dump(),error));CHECK(loaded.gameAssets().player.cameraFov==60);
    old["game"]["player"]["cameraFov"]=180;CHECK(!projectio::fromJson(loaded,old.dump(),error));CHECK(loaded.gameAssets().player.cameraFov==60);
}
void LinbluxTaskCancellation() {
    Scene author;author.buildDefaultScene();std::string error;
    author.createScript(InstanceClass::Script,nullptr,"Tarefas.lua")->source=R"(
local root = linblux.scene.root
local never = linblux.time.spawn(function() error("canceled spawn executed") end)
assert(never.active and never:cancel() and not never.active)
assert(not linblux.time.cancel(never))
local finished = linblux.time.after(0.01, function(value) root:set_attribute("finished", value) end, 7)
assert(finished.active)
linblux.time.after(0.05, function()
    assert(root:get_attribute("finished") == 7 and not finished.active)
    local next = linblux.time.spawn(function() root:set_attribute("next", true) end)
    -- Registry slots may be reused; an old task handle must not cancel a new task.
    assert(not linblux.time.cancel(finished) and next.active)
end)
)";
    author.createScript(InstanceClass::Script,nullptr,"IntervaloZero.lua")->source="linblux.time.every(0, function() end)";
    author.createScript(InstanceClass::Script,nullptr,"CancelInvalido.lua")->source="linblux.time.cancel(linblux.player)";
    author.createScript(InstanceClass::Script,nullptr,"IntervaloComErro.lua")->source="linblux.time.every(0.01, function() error('interval failure') end)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<30;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().workspace()->attributes().count("next"));
    bool zero=false,invalid=false;int intervalErrors=0;
    for(const auto& message:runtime.scripts().messages())if(message.error){
        if(message.script=="IntervaloZero.lua")zero=true;
        else if(message.script=="CancelInvalido.lua")invalid=true;
        else if(message.script=="IntervaloComErro.lua")++intervalErrors;
        else {std::printf("Task Lua: %s\n",message.text.c_str());CHECK(false);}
    }
    CHECK(zero&&invalid&&intervalErrors==1);CHECK(runtime.scripts().errorCount()==3);CHECK(runtime.scripts().taskCount()==0);
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    CHECK(runtime.scripts().errorCount()==3);
    Scene clean;clean.buildDefaultScene();CHECK(runtime.start(clean,error));CHECK(runtime.scripts().errorCount()==0);runtime.stop();
}
void LinbluxInput() {
    Scene author;author.buildDefaultScene();std::string error;
    author.createScript(InstanceClass::Script,nullptr,"Entrada.lua")->source=R"(
local root = linblux.scene.root
local function count(name) root:set_attribute(name, (root:get_attribute(name) or 0)+1) end
linblux.events.on("key_pressed", function(key)
    assert(key == "e" and linblux.input.key_down("e") and linblux.input.key_pressed("e"))
    count("key_downs")
end)
linblux.events.on("key_released", function(key)
    assert(key == "e" and not linblux.input.key_down("e") and linblux.input.key_released("e"))
    count("key_ups")
end)
linblux.events.on("mouse_pressed", function(button)
    assert(button == "left" and linblux.input.mouse_pressed("left"))
    local p = linblux.input.pointer()
    assert(p.x == 123 and p.y == 45 and p.dx == 2 and p.wheel == -1)
    count("clicks")
end)
linblux.events.on("gamepad_pressed", function(button)
    assert(button == "south" and linblux.input.gamepad_down("south") and linblux.input.gamepad_connected())
    assert(linblux.input.gamepad_axis("left_x") == 0.5)
    count("pad_downs")
end)
linblux.events.on("gamepad_released", function(button)
    assert(button == "south" and linblux.input.gamepad_released("south"))
    count("pad_ups")
end)
linblux.events.on("porta.alterar", function(open) root:set_attribute("porta_aberta", open) end)
)";
    const auto examplePath=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/examples/lua/interacao.lua";
    std::ifstream example(examplePath);CHECK(example.good());
    author.createScript(InstanceClass::Script,nullptr,"Interacao.lua")->source=std::string(std::istreambuf_iterator<char>(example),{});
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    game::PlayerInput input;input.script.keys.set(KEY_E);input.script.mouse.set(MOUSE_BUTTON_LEFT);
    input.script.pointer={123,45};input.script.delta={2,0};input.script.wheel=-1;
    input.script.gamepad=true;input.script.gamepadButtons.set(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);input.script.gamepadAxes[GAMEPAD_AXIS_LEFT_X]=.5f;
    runtime.update(1.f/60,input);runtime.update(1.f/60,input);
    const auto count=[&](const char* key){const auto& attrs=runtime.scene().workspace()->attributes();CHECK(attrs.count(key));return attrs.count(key)?std::get<double>(attrs.at(key)):0.;};
    CHECK(count("key_downs")==1);CHECK(count("clicks")==1);CHECK(count("pad_downs")==1);
    // Both E and the south button were pressed: two toggles, initially closed.
    runtime.update(1.f/60,input);CHECK(!std::get<bool>(runtime.scene().workspace()->attributes().at("porta_aberta")));
    runtime.pause();runtime.update(1.f/60,{});CHECK(count("key_downs")==1);CHECK(!runtime.scene().workspace()->attributes().count("key_ups"));
    runtime.resume();runtime.update(1.f/60,{});CHECK(count("key_ups")==1);CHECK(count("pad_ups")==1);
    input.script.gamepad=false;input.script.gamepadButtons.reset();input.script.mouse.reset();runtime.update(1.f/60,input);runtime.update(1.f/60,input);
    CHECK(count("key_downs")==2);CHECK(std::get<bool>(runtime.scene().workspace()->attributes().at("porta_aberta")));
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Input Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();CHECK(!runtime.scriptInput().keys.any());
}
void LinbluxPlayer() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* script=author.createScript(InstanceClass::Script,nullptr,"Jogador.lua");
    script->source=R"(
local player, root = linblux.player, linblux.scene.root
linblux.events.on("spawn", function(current)
    assert(current == player)
    root:set_attribute("spawns", (root:get_attribute("spawns") or 0) + 1)
end)
linblux.events.on("death", function() root:set_attribute("died", true) end)
linblux.events.on("health_changed", function(value) root:set_attribute("health", value) end)
player.max_health = 150
player.walk_speed = 22
player.jump_height = 8
player.auto_rotate = false
player:damage(25)
player:heal(10)
assert(player.health == 85)
local clips = linblux.animations.list()
assert(#clips > 0 and clips[1].duration > 0)
player:set_animation("walk", clips[1].id)
linblux.time.after(0.02, function()
    local previous = player.generation
    player:teleport(linblux.vec3(30,12,0))
    player:set_checkpoint()
    player:damage(200)
    assert(player.state == "dead")
    player:respawn()
    assert(player.generation == previous + 1 and player.position.x == 30)
    linblux.camera.configure({mode="first_person", distance=0.3, sensitivity=0.004, allow_shift_lock=true})
    linblux.time.after(0.03, function()
        assert(linblux.camera.state().first_person)
        root:set_attribute("player_ready", true)
    end)
end)
)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    const auto& attributes=runtime.scene().workspace()->attributes();
    CHECK(attributes.count("player_ready"));CHECK(attributes.count("died"));
    CHECK(std::get<double>(attributes.at("spawns"))==2);
    CHECK(runtime.health()==150&&runtime.settings().maxHealth==150&&runtime.settings().walkSpeed==22);
    CHECK(runtime.playerCamera().firstPerson()&&runtime.settings().enableShiftLock);
    CHECK(runtime.position().x==30);
    const auto previous=runtime.position();CHECK(!runtime.teleportPlayer({30,0,0},error));CHECK(NearlyEqual(runtime.position(),previous));
    CHECK(!runtime.setCheckpoint({NAN,0,0},error));
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Player Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();
    CHECK(runtime.start(author,error));CHECK(std::fabs(runtime.position().x)<.01f);runtime.stop();
}
void LuaFinalizerSafety() {
    Scene scene;scene.buildDefaultScene();std::string error;
    scene.createScript(InstanceClass::Script,nullptr,"Finalizer.lua")->source=R"(setmetatable({}, {__gc=function() while true do end end}))";
    scene.createScript(InstanceClass::Script,nullptr,"FalseFinalizer.lua")->source=R"(setmetatable({}, {__gc=false}))";
    scene.createScript(InstanceClass::Script,nullptr,"Metatable.lua")->source=R"(
local object = setmetatable({}, {__index={value=7}})
assert(object.value == 7)
local meta, retained = {}, {}
setmetatable(retained, meta)
meta.__gc = function() while true do end end
_G.retained = retained
print("metatable works")
)";
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));
    int denied=0;bool valid=false;for(const auto& message:runtime.scripts().messages()) {
        denied+=message.error&&message.text.find("__gc")!=std::string::npos;
        valid|=message.text=="metatable works";
    }
    CHECK(denied==2&&valid);
    const auto before=std::chrono::steady_clock::now();runtime.stop();
    CHECK(std::chrono::duration<double>(std::chrono::steady_clock::now()-before).count()<2);
    CHECK(runtime.scripts().memoryUsed()==0&&runtime.scripts().taskCount()==0);CHECK(runtime.validate(error));
}
void DynamicScriptLifecycle() {
    Scene author;author.buildDefaultScene();std::string error;
    auto* part=author.createPart(PartShape::Block,{25,8,0},{2,2,2},"Original");part->setAnchored(true);
    author.createScript(InstanceClass::Script,part,"Componente.lua")->source=R"(
local owner = linblux.self
linblux.events.on("start", function() owner:set_attribute("starts", (owner:get_attribute("starts") or 0)+1) end)
linblux.events.on("update", function() owner:set_attribute("ticks", (owner:get_attribute("ticks") or 0)+1) end)
)";
    author.createScript(InstanceClass::Script,nullptr,"Controlador.lua")->source=R"(
linblux.time.after(0.02, function()
    local canceled = linblux.scene.create("part")
    local connection = linblux.events.on("destroy", function() error("canceled destroy") end, canceled)
    canceled:destroy()
    local flush = linblux.events.on("nothing", function() end)
    connection:disconnect()
    flush:disconnect()
    local copy = linblux.scene.find("Original"):clone()
    copy.name = "Clone"
    copy:set_attribute("starts", 0)
    copy:set_attribute("ticks", 0)
    local component
    for _, child in ipairs(copy:children()) do if child.type == "script" then component = child end end
    assert(component and component.enabled)
    local root = linblux.scene.root
    root:set_attribute("clone", copy.id)
    linblux.events.on("destroy", function(id) root:set_attribute("removed", id) end, copy)
    linblux.time.after(0.06, function()
        assert(copy:get_attribute("starts") == 1 and copy:get_attribute("ticks") > 0)
        local ticks = copy:get_attribute("ticks")
        component.enabled = false
        linblux.time.after(0.04, function()
            assert(copy:get_attribute("ticks") == ticks)
            component.enabled = true
            linblux.time.after(0.05, function()
                assert(copy:get_attribute("starts") == 2)
                component.enabled = false
                component.enabled = true
                local ticks = copy:get_attribute("ticks")
                linblux.time.after(0.05, function()
                    assert(copy:get_attribute("starts") == 3)
                    -- A restart must leave precisely one update subscription.
                    local before = copy:get_attribute("ticks")
                    linblux.time.after(0, function()
                        assert(copy:get_attribute("ticks") == before + 1)
                        copy:destroy()
                    end)
                end)
            end)
        end)
    end)
end)
)";
    const auto baseline=projectio::toJson(author);game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<40;++i)runtime.update(1.f/60,{});
    const auto& attributes=runtime.scene().workspace()->attributes();CHECK(attributes.count("clone")&&attributes.count("removed"));
    if(attributes.count("removed")) {
        CHECK(std::get<double>(attributes.at("clone"))==std::get<double>(attributes.at("removed")));
        CHECK(runtime.scene().find(InstanceId(std::get<double>(attributes.at("clone"))))==nullptr);
    }
    CHECK(std::get<double>(runtime.scene().findPart(part->id())->attributes().at("starts"))==1);
    for(const auto& message:runtime.scripts().messages()){if(message.error)std::printf("Lifecycle Lua: %s\n",message.text.c_str());CHECK(!message.error);}
    CHECK(runtime.validate(error));CHECK(projectio::toJson(author)==baseline);runtime.stop();CHECK(runtime.scripts().taskCount()==0);
}
void AnimationIds() {
    Scene scene;scene.buildDefaultScene();scene.installDefaultScripts();std::string error;
    auto* service=scene.service(InstanceClass::ServerScriptService);CHECK(service && service->children().size()==1);
    auto* animations=scene.createScript(InstanceClass::Script,service,"CustomCharacterAnimations.lua");
    CHECK(animations);if(!animations)return; // custom scripts remain supported; no automatic animation script
    auto edited=game::MakeWalk(65);edited.id="custom-walk";edited.name="My Walk";
    scene.gameAssets().animations.push_back(edited);
    animations->source=R"(game:GetService("Players").CharacterAdded:Connect(function(character)
character.Animator:SetAnimation("Walk", "anim://custom-walk")
character.Animator:SetAnimation("Jump", "anim://r6-jump")
local info = game:GetService("AnimationService"):GetAnimation("anim://custom-walk")
assert(info.Id == "anim://custom-walk" and info.RigType == "R6" and math.abs(info.Duration - 0.8) < 0.0001)
end))";
    const auto source=animations->source;scene.installDefaultScripts();CHECK(animations->source==source);CHECK(service->children().size()==2);
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));CHECK(runtime.settings().walkAnimation=="anim://custom-walk");
    CHECK(runtime.settings().jumpAnimation=="anim://r6-jump");
    for(const auto& message:runtime.scripts().messages())CHECK(!message.error);
    CHECK(!runtime.setAnimation("Walk","anim://missing",error));CHECK(runtime.settings().walkAnimation=="anim://custom-walk");
    CHECK(!runtime.setAnimation("Dance","anim://custom-walk",error));
    CHECK(runtime.scene().gameAssets().findAnimation(runtime.settings().walkAnimation)->sample(0)[game::LeftHip].rotation.x==edited.sample(0)[game::LeftHip].rotation.x);
    runtime.stop();
    Scene reopened;CHECK(projectio::fromJson(reopened,projectio::toJson(scene),error));
    auto* restored=static_cast<Script*>(reopened.find(animations->id()));CHECK(restored && restored->source==source);
    CHECK(runtime.start(reopened,error));CHECK(runtime.settings().walkAnimation=="anim://custom-walk");runtime.stop();
    auto legacy=scene.gameAssets();legacy.player.jumpAnimation.clear();
    CHECK(legacy.findAnimation("r6-jump")!=nullptr);CHECK(scene.validate(error));
}
void HealthAndVoid() {
    Scene scene;scene.buildDefaultScene();scene.gameAssets().createR6();std::string error;
    auto* script=scene.createScript(InstanceClass::Script,nullptr,"Health.server.lua");
    script->source=R"(game:GetService("Players").CharacterAdded:Connect(function(character)
workspace:SetAttribute("spawns", (workspace:GetAttribute("spawns") or 0) + 1)
local h=character.Humanoid
h.HealthChanged:Connect(function(value) workspace:SetAttribute("health",value) end)
h.Died:Connect(function() workspace:SetAttribute("deaths",(workspace:GetAttribute("deaths") or 0)+1) end)
end))";
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));CHECK(runtime.health()==100);
    CHECK(runtime.takeDamage(25,error));CHECK(runtime.health()==75);runtime.update(0,{});
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("health"))==75);
    CHECK(runtime.takeDamage(50,error));CHECK(runtime.health()==25);CHECK(runtime.heal(10,error));CHECK(runtime.health()==35);
    CHECK(!runtime.takeDamage(std::numeric_limits<float>::quiet_NaN(),error));
    CHECK(!runtime.heal(-1,error));CHECK(runtime.health()==35);
    CHECK(runtime.takeDamage(100,error));CHECK(runtime.health()==0);CHECK(runtime.humanoidState()==game::HumanoidState::Dead);
    CHECK(runtime.takeDamage(1,error));runtime.update(0,{});
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("deaths"))==1);
    CHECK(!runtime.heal(100,error));
    auto generation=runtime.characterGeneration();runtime.pause();
    for(int i=0;i<180;++i)runtime.update(1.f/60,{});CHECK(runtime.characterGeneration()==generation);
    runtime.resume();game::PlayerInput input;input.movement={1,0};const float x=runtime.position().x;
    runtime.update(.1f,input);CHECK(std::fabs(runtime.position().x-x)<.01f);
    for(int i=0;i<130;++i)runtime.update(1.f/60,{});
    CHECK(runtime.characterGeneration()==generation+1);CHECK(runtime.health()==100);
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("spawns"))==2);
    for(int i=0;i<70;++i){CHECK(runtime.resetCharacter(error));runtime.update(0,{});CHECK(runtime.health()==100);CHECK(runtime.validate(error));}
    for(const auto& message:runtime.scripts().messages())CHECK(!message.error);
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("deaths"))==71);
    CHECK(runtime.bodyCount()==scene.parts().size()+1);runtime.stop();
    Scene hazard;hazard.buildDefaultScene();hazard.gameAssets().createR6();
    hazard.findPart(4)->setAttribute("Damage",25.);
    CHECK(runtime.start(hazard,error));for(int i=0;i<30;++i)runtime.update(1.f/60,{});
    CHECK(runtime.health()==75);runtime.stop();
    hazard.findPart(4)->setCanCollide(false);
    CHECK(runtime.start(hazard,error));for(int i=0;i<30;++i)runtime.update(1.f/60,{});
    CHECK(runtime.health()==100);runtime.stop();
    Scene voidScene;voidScene.createPart(PartShape::Block,{10,-30,0},{4,4,4},"Falling");
    voidScene.gameAssets().createR6();voidScene.workspace()->setFallenPartsDestroyHeight(-20);
    const auto author=projectio::toJson(voidScene);CHECK(runtime.start(voidScene,error));
    for(int i=0;i<70;++i)runtime.update(1.f/60,{});
    CHECK(runtime.scene().parts().empty());CHECK(runtime.health()==0);
    for(int i=0;i<130;++i)runtime.update(1.f/60,{});
    CHECK(runtime.characterGeneration()>=2);CHECK(projectio::toJson(voidScene)==author);
    CHECK(runtime.validate(error));runtime.stop();
    Scene reloaded;CHECK(projectio::fromJson(reloaded,author,error));CHECK(reloaded.workspace()->fallenPartsDestroyHeight()==-20);
    CHECK(!reloaded.workspace()->setFallenPartsDestroyHeight(std::numeric_limits<float>::infinity()));
    Scene scripted;scripted.buildDefaultScene();
    auto* damageScript=scripted.createScript(InstanceClass::Script,nullptr,"Damage.server.lua");
    damageScript->source=R"(game:GetService("Players").CharacterAdded:Connect(function(character)
local h=character.Humanoid
h.Died:Connect(function()workspace:SetAttribute("deaths",(workspace:GetAttribute("deaths") or 0)+1)end)
assert(h.Health==100 and h.MaxHealth==100)
h:TakeDamage(25);assert(h.Health==75)
h:TakeDamage(50);assert(h.Health==25)
h:Heal(5);assert(h.Health==30)
h.Health=0;h:TakeDamage(1)
assert(h.State=="Dead")
end))";
    CHECK(runtime.start(scripted,error));runtime.update(0,{});
    CHECK(runtime.health()==0);
    CHECK(std::get<double>(runtime.scene().workspace()->attributes().at("deaths"))==1);
    for(const auto& message:runtime.scripts().messages())CHECK(!message.error);
    runtime.stop();
}
void PartChildScripts() {
    Scene scene;scene.buildDefaultScene();
    auto* part=scene.createPart(PartShape::Block,{20,4,0});part->setAnchored(true);
    auto* child=scene.createScript(InstanceClass::Script,part,"PartChild.lua");
    child->source="script.Parent:SetAttribute('childRan', true)";
    std::string error;Scene loaded;
    CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));
    CHECK(loaded.find(child->id())!=nullptr);
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));
    CHECK(runtime.scene().findPart(part->id())->attributes().count("childRan")==1);
    CHECK(part->attributes().empty());runtime.stop();
}
void SignificantFallPresentation() {
    Scene author;author.buildDefaultScene();author.gameAssets().createLinbluxCharacter();std::string error;
    auto* fall=author.gameAssets().findAnimation(author.gameAssets().player.fallAnimation);CHECK(fall);if(!fall)return;
    const auto marker=QuaternionFromEuler(.9f,0,0);fall->tracks[game::LeftShoulder].clear();fall->setKey(game::LeftShoulder,{0,{{0,0,0},marker}});
    for(float fps:{30.f,60.f,144.f}) {
        game::RuntimeSession runtime;CHECK(runtime.start(author,error));
        for(int i=0;i<int(fps);++i)runtime.update(1/fps,{});CHECK(runtime.grounded());
        for(float height:{2.f,7.f,20.f}) {
            runtime.settings().jumpHeight=height;game::PlayerInput jump;jump.jump=true;runtime.update(1/fps,jump);
            bool sawFall=false,sawDescent=false,landed=false;int fallingFrames=0;
            for(int frame=0;frame<int(fps*2);++frame) {
                runtime.update(1/fps,{});sawDescent|=runtime.humanoidState()==game::HumanoidState::Freefall;
                sawFall|=runtime.significantFall();if(runtime.significantFall())++fallingFrames;
                if(fallingFrames>fps*.15f&&runtime.significantFall())CHECK(NearlyEqual(runtime.pose()[game::LeftShoulder].rotation,marker,.03f));
                if(frame>fps*.2f&&runtime.grounded()){landed=true;break;}
            }
            CHECK(sawDescent&&landed);CHECK(sawFall==(height>10));CHECK(!runtime.significantFall());
            for(int i=0;i<5;++i)runtime.update(1/fps,{});
        }
        auto position=runtime.position();position.y+=2;CHECK(runtime.teleportPlayer(position,error));
        for(int i=0;i<int(fps);++i){runtime.update(1/fps,{});CHECK(!runtime.significantFall());}
        position=runtime.position();position.y+=30;CHECK(runtime.teleportPlayer(position,error));
        bool saw=false;for(int i=0;i<int(fps)&&!saw;++i){runtime.update(1/fps,{});saw=runtime.significantFall();}
        CHECK(saw);CHECK(runtime.respawn(error));CHECK(!runtime.significantFall());CHECK(runtime.validate(error));runtime.stop();
    }
}
void IndependentFall() {
    Scene scene;scene.buildDefaultScene();scene.gameAssets().createR6();std::string error;
    auto& assets=scene.gameAssets();auto* fall=assets.findAnimation(assets.player.fallAnimation);
    CHECK(fall && fall->id!="r6-jump");if(!fall)return;
    for(const auto& track:fall->tracks)CHECK(track.empty());
    CHECK(!assets.removeAnimation(fall->id,error));
    auto unused=game::MakeFall();unused.id="unused-fall";assets.animations.push_back(unused);
    CHECK(assets.removeAnimation("unused-fall",error));CHECK(assets.validate(error));
    game::RuntimeSession runtime;CHECK(runtime.start(scene,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    CHECK(runtime.grounded());game::PlayerInput input;input.jump=true;runtime.update(1.f/60,input);
    bool sawJump=false,sawFall=false;int fallingFrames=0;
    for(int i=0;i<90;++i) {
        runtime.update(1.f/60,{});
        if(runtime.humanoidState()==game::HumanoidState::Jumping)sawJump=true;
        if(runtime.humanoidState()==game::HumanoidState::Freefall) {
            sawFall=true;
            ++fallingFrames;CHECK(!runtime.significantFall()); // pulo normal mantém a apresentação de pulo
        }
    }
    CHECK(sawJump && sawFall && fallingFrames>=14);runtime.stop();
    assets.findAnimation(assets.player.fallAnimation)->setKey(game::LeftShoulder,{0,{{0,0,0},QuaternionFromEuler(.4f,0,0)}});
    Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));
    CHECK(loaded.gameAssets().player.fallAnimation==assets.player.fallAnimation);
    CHECK(runtime.start(loaded,error));CHECK(runtime.setAnimation("Fall","anim://r6-fall",error));runtime.stop();
    assets.player.fallAnimation="anim://missing";CHECK(!assets.validate(error));
}
void JumpTiming() {
    Scene author;author.buildDefaultScene();author.gameAssets().createR6();std::string error;
    game::RuntimeSession runtime;CHECK(runtime.start(author,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    CHECK(runtime.grounded());const float floorY=runtime.position().y;
    for(auto* part:runtime.mutableScene().parts())part->setPosition(Vector3Add(part->position(),{0,-1000,0}));
    runtime.update(1.f/60,{});CHECK(!runtime.grounded());
    game::PlayerInput jump;jump.jump=true;runtime.update(1.f/60,jump);
    CHECK(runtime.velocity().y>1); // press after support was lost, inside 100 ms
    runtime.stop();CHECK(runtime.start(author,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    for(auto* part:runtime.mutableScene().parts())part->setPosition(Vector3Add(part->position(),{0,-1000,0}));
    for(int i=0;i<12;++i)runtime.update(1.f/60,{});
    runtime.update(1.f/60,jump);CHECK(runtime.velocity().y<0); // grace expired
    runtime.stop();CHECK(runtime.start(author,error));
    for(int i=0;i<120;++i)runtime.update(1.f/60,{});
    runtime.update(1.f/60,jump);bool queued=false,bounced=false;
    for(int i=0;i<120;++i) {
        if(!queued && runtime.velocity().y< -1 && runtime.position().y<floorY+1.2f && runtime.position().y>floorY+.2f) {
            CHECK(!runtime.grounded());runtime.update(1.f/60,jump);queued=true;
        } else runtime.update(1.f/60,{});
        if(queued && runtime.velocity().y>1){bounced=true;break;}
    }
    CHECK(queued && bounced); // a single press before contact executes after landing
    CHECK(runtime.validate(error));runtime.stop();
}
void ObbyLanding() {
    Scene scene;auto* platform=scene.createSpawnLocation({0,.5f,0},{.4f,1,6});
    scene.gameAssets().createR6();std::string error;game::RuntimeSession runtime;
    CHECK(runtime.start(scene,error));for(int i=0;i<90;++i)runtime.update(1.f/60,{});
    const float rest=runtime.position().y;
    game::PlayerInput jump;jump.jump=true;
    // A key queued in the exact fixed step which makes contact must launch
    // without spending another physics tick standing on the small platform.
    for(float frame:{1.f/60,1.f/30}) {
        runtime.stop();CHECK(runtime.start(scene,error));
        for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        runtime.update(frame,jump);bool pressed=false;
        for(int i=0;i<120;++i) {
            const float gap=runtime.position().y-rest,vy=runtime.velocity().y;
            if(vy< -5 && gap>.13f && gap< -vy*frame+.08f) {
                runtime.update(frame,jump);pressed=true;
                CHECK(runtime.velocity().y>1);break;
            }
            runtime.update(frame,{});
        }
        CHECK(pressed);
        for(int i=0;i<120 && !runtime.grounded();++i)runtime.update(1.f/60,{});
        CHECK(runtime.grounded());
    }
    // Capsule supported at an edge, with no surface under its centre ray.
    auto* support=runtime.mutableScene().findPart(platform->id());
    support->setSize({1.4f,1,6});support->setPosition({.75f,.5f,0});
    for(int i=0;i<6;++i)runtime.update(1.f/60,{});
    CHECK(runtime.grounded());runtime.update(1.f/60,jump);CHECK(runtime.velocity().y>1);
    const float first=runtime.velocity().y;runtime.update(1.f/60,jump);
    CHECK(runtime.velocity().y<first); // no second impulse from old contacts
    CHECK(runtime.validate(error));runtime.stop();
}
void ConsecutivePlatforms() {
    for(float dt:{1.f/60,1.f/30}) {
        Scene scene;scene.createSpawnLocation({0,.5f,0},{1.4f,1,3});
        for(float x:{2.4f,4.8f,7.2f})scene.createPart(PartShape::Block,{x,.5f,0},{1.4f,1,3})->setAnchored(true);
        scene.gameAssets().createR6();scene.gameAssets().player.walkSpeed=6;scene.gameAssets().player.jumpHeight=2;
        game::RuntimeSession runtime;std::string error;CHECK(runtime.start(scene,error));
        for(int i=0;i<90;++i)runtime.update(1.f/60,{});
        const float floor=runtime.position().y;game::PlayerInput input;input.movement={1,0};input.jump=true;
        runtime.update(dt,input);int launches=0;bool queued=false;
        for(int frame=0;frame<150 && launches<3;++frame) {
            const float vy=runtime.velocity().y;
            input.jump=!queued && vy< -1 && runtime.position().y<floor+.5f;
            queued|=input.jump;runtime.update(dt,input);
            if(queued && runtime.velocity().y>1) {++launches;queued=false;CHECK(runtime.position().x>launches*1.7f);}
        }
        CHECK(launches==3);CHECK(runtime.validate(error));runtime.stop();
    }
}
void AnimationDeletion() {
    Scene scene;scene.gameAssets().createR6();auto& assets=scene.gameAssets();std::string error;
    auto clip=game::MakeIdle();clip.id="delete-me";clip.name="Renamed animation";assets.animations.push_back(clip);
    CHECK(assets.removeAnimation(clip.id,error));CHECK(!assets.findAnimation(clip.id));
    const auto jump=assets.player.jumpAnimation,fall=assets.player.fallAnimation;
    CHECK(!assets.removeAnimation(assets.player.walkAnimation,error));
    const auto walk=assets.player.walkAnimation;
    CHECK(assets.removeAnimation(walk,error,true));CHECK(assets.player.walkAnimation.empty());
    CHECK(assets.player.jumpAnimation==jump && assets.player.fallAnimation==fall);
    for(const auto id:{assets.player.idleAnimation,jump,fall})CHECK(assets.removeAnimation(id,error,true));
    CHECK(assets.animations.empty());CHECK(assets.validate(error));
    Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));
    CHECK(loaded.gameAssets().animations.empty());game::RuntimeSession runtime;
    CHECK(runtime.start(loaded,error));runtime.update(.1f,{});CHECK(runtime.validate(error));runtime.stop();
}
void NativeCloneBudget() {
    Scene scene;scene.buildDefaultScene();auto* folder=scene.createContainer(InstanceClass::Folder,nullptr,"Payload");
    for(int i=0;i<17;++i) {
        auto* node=scene.createScript(InstanceClass::ModuleScript,folder,"Data"+std::to_string(i));
        node->source="--"+std::string(262140,'x');
    }
    auto* script=scene.createScript(InstanceClass::Script,nullptr,"CloneBudget.lua");
    script->source="workspace.Payload:Clone()";
    game::RuntimeSession runtime;std::string error;CHECK(runtime.start(scene,error));
    bool rejected=false;for(const auto& message:runtime.scripts().messages())
        rejected|=message.error&&message.text.find("native content budget")!=std::string::npos;
    CHECK(rejected);CHECK(runtime.scene().objects().size()==scene.objects().size());
    CHECK(runtime.validate(error));runtime.stop();
}

// Ferramentas de transformacao: medidas em studs/graus, aplicacao exata a
// partir da pose inicial, formatacao e Undo/Redo de uma operacao por arraste.
void TransformTools() {
    using namespace transformmetrics;
    const gizmomath::TransformSnapshot origin{{0,0,0},{0,0,0,1},{4,4,4}};
    Context move; move.kind=Kind::Move; move.axis=0; move.space=Space::World; move.start=origin;
    gizmomath::TransformSnapshot current=origin;
    current.position={4,0,0};
    Measurement measured=Measure(move,current);
    CHECK(measured.valid);CHECK(NearlyEqual(measured.value,4.0f));
    char text[64];
    CHECK(Format(measured,text,sizeof(text)));
    CHECK(std::string(text)=="X  +4.00 studs");
    // Aplicar 4 studs a partir da pose inicial e reaplicar 2 nao acumula 6.
    CHECK(NearlyEqual(Apply(move,4.0f).position,{4,0,0}));
    CHECK(NearlyEqual(Apply(move,2.0f).position,{2,0,0}));
    CHECK(NearlyEqual(Apply(move,-3.5f).position,{-3.5f,0,0}));
    measured=Measure(move,Apply(move,-3.5f));
    CHECK(NearlyEqual(measured.value,-3.5f));
    CHECK(std::string(text)=="X  +4.00 studs");
    CHECK(Format(measured,text,sizeof(text)));
    CHECK(std::string(text)=="X  -3.50 studs");

    // Local: eixo X da peca girada 90 graus em Y aponta para -Z do mundo.
    Context local=move; local.space=Space::Local;
    local.start.rotation=QuaternionFromAxisAngle({0,1,0},90.0f*DEG2RAD);
    const Vector3 localAxis=AxisVector(local);
    CHECK(NearlyEqual(localAxis,{0,0,-1},0.001f));
    const auto localMoved=Apply(local,2.5f);
    CHECK(NearlyEqual(localMoved.position,{0,0,-2.5f},0.001f));
    CHECK(NearlyEqual(Measure(local,localMoved).value,2.5f,0.001f));
    CHECK(localMoved.rotation.x==local.start.rotation.x&&
          localMoved.rotation.w==local.start.rotation.w);

    // Escala a partir de uma face de uma Part rotacionada: tamanho exato,
    // face oposta estavel e minimo respeitado.
    Context scale; scale.kind=Kind::Size; scale.axis=0; scale.side=1;
    scale.space=Space::Local;
    scale.start=origin;
    scale.start.rotation=QuaternionFromAxisAngle({0,1,0},30.0f*DEG2RAD);
    auto scaled=Apply(scale,8.5f);
    CHECK(NearlyEqual(scaled.size.x,8.5f));CHECK(NearlyEqual(scaled.size.y,4.0f));
    CHECK(NearlyEqual(scaled.rotation,scale.start.rotation));
    CHECK(NearlyEqual(Measure(scale,scaled).value,8.5f));
    // (centro novo - centro velho) . eixoX local == metade do crescimento
    const Vector3 grow=Vector3Subtract(scaled.position,scale.start.position);
    CHECK(NearlyEqual(Vector3DotProduct(grow,gizmomath::Axis(0,scale.start.rotation,true)),2.25f,0.001f));
    // Face oposta: o ponto no lado negativo continua no mesmo lugar.
    const Vector3 negativeFace=Vector3Add(scale.start.position,
        Vector3Scale(gizmomath::Axis(0,scale.start.rotation,true),-2.0f));
    const Vector3 negativeFaceAfter=Vector3Add(scaled.position,
        Vector3Scale(gizmomath::Axis(0,scale.start.rotation,true),-4.25f));
    CHECK(NearlyEqual(negativeFace,negativeFaceAfter,0.001f));
    // Alt: escala centrada deixa o centro parado e move as duas faces.
    Context centered=scale; centered.centered=true;
    auto centeredPose=Apply(centered,10.0f);
    CHECK(NearlyEqual(centeredPose.position,scale.start.position));
    CHECK(NearlyEqual(centeredPose.size.x,10.0f));
    const auto centeredGizmo=gizmomath::ScaleCenteredFace(
        scale.start,0,1,true,3.0f,0.0f);
    CHECK(NearlyEqual(centeredGizmo.position,scale.start.position));
    CHECK(NearlyEqual(centeredGizmo.size.x,10.0f));
    CHECK(NearlyEqual(centeredGizmo.size.y,scale.start.size.y));
    const auto centeredMin=gizmomath::ScaleCenteredFace(
        scale.start,0,1,true,-100.0f,0.0f);
    CHECK(NearlyEqual(centeredMin.size.x,kMinPartSize));
    auto minimum=Apply(scale,0.0f);
    CHECK(NearlyEqual(minimum.size.x,kMinPartSize));
    auto below=Apply(scale,-5.0f);
    CHECK(NearlyEqual(below.size.x,kMinPartSize));CHECK(NearlyEqual(below.size.x,minimum.size.x));
    // Em World o eixo positivo pode apontar para a face local negativa.
    Context worldScale=scale; worldScale.space=Space::World; worldScale.side=1;
    worldScale.start.rotation=QuaternionFromAxisAngle({0,1,0},90.0f*DEG2RAD);
    CHECK(SizeComponent(worldScale)==2);
    const auto worldScaled=Apply(worldScale,6.0f);
    CHECK(NearlyEqual(worldScaled.size.z,6.0f));

    // Rotacao: angulo com sinal, 90 graus, wrap de +190 para -170.
    Context angle; angle.kind=Kind::Angle; angle.axis=1; angle.space=Space::World;
    angle.start=origin;
    const auto turned=Apply(angle,45.0f);
    CHECK(NearlyEqual(Measure(angle,turned).value,45.0f,0.01f));
    CHECK(std::string(text)=="X  -3.50 studs");
    CHECK(Format(Measure(angle,turned),text,sizeof(text)));
    CHECK(std::string(text)=="Y  +45.00 deg");
    CHECK(NearlyEqual(Measure(angle,Apply(angle,-45.0f)).value,-45.0f,0.01f));
    CHECK(NearlyEqual(Measure(angle,Apply(angle,90.0f)).value,90.0f,0.01f));
    CHECK(NearlyEqual(Measure(angle,Apply(angle,190.0f)).value,-170.0f,0.01f));
    // q e -q descrevem a mesma pose: medir -45 nao pode virar +315.
    auto negated=Apply(angle,-45.0f);
    negated.rotation={-negated.rotation.x,-negated.rotation.y,-negated.rotation.z,-negated.rotation.w};
    CHECK(NearlyEqual(Measure(angle,negated).value,-45.0f,0.01f));
    CHECK(NearlyEqual(Apply(angle,90.0f).position,origin.position));
    CHECK(NearlyEqual(Apply(angle,90.0f).size,origin.size));

    // Plano XY: mede os dois eixos e nao inventa valor digitado.
    Context plane; plane.kind=Kind::MovePlane; plane.axis=0; plane.space=Space::World;
    plane.start=origin;
    auto planePose=origin; planePose.position={3,2,0};
    const auto planeMeasure=Measure(plane,planePose);
    CHECK(planeMeasure.valid);CHECK(!planeMeasure.editable());
    CHECK(NearlyEqual(planeMeasure.value,3.0f));CHECK(NearlyEqual(planeMeasure.valueB,2.0f));
    CHECK(planeMeasure.axis==0&&planeMeasure.axisB==1);
    CHECK(Format(planeMeasure,text,sizeof(text)));
    CHECK(std::string(text)=="X +3.00 | Y +2.00 studs");
    CHECK(NearlyEqual(Apply(plane,5.0f).position,origin.position));

    // Formatacao do Properties: sem notacao cientifica e sem -0.
    CHECK(numfmt::Text(0.0f)=="0");
    CHECK(numfmt::Text(-9.7066e-12f)=="0");
    CHECK(numfmt::Text(-0.0f)=="0");
    CHECK(numfmt::Text(1.5f)=="1.5");
    CHECK(numfmt::Text(-2.25f)=="-2.25");
    CHECK(numfmt::Text(8.5f,2)=="8.5");
    CHECK(numfmt::Text(123456.0f)=="123456");
    float parsed=0.0f;
    CHECK(numfmt::Parse("4",parsed));CHECK(NearlyEqual(parsed,4.0f));
    CHECK(numfmt::Parse(" -8.5 ",parsed));CHECK(NearlyEqual(parsed,-8.5f));
    CHECK(!numfmt::Parse("4studs",parsed));
    CHECK(!numfmt::Parse("",parsed));
    CHECK(!numfmt::Parse("nan",parsed));
    CHECK(!numfmt::Parse("1e999",parsed));

    // Undo/Redo: um arraste = uma operacao.
    EditHistory history;
    loblox::Transform before{{0,0,0},{0,0,0,1},{4,4,4}};
    loblox::Transform after=before; after.position={4,0,0};
    CHECK(!history.canUndo());
    CHECK(!history.record(7,before,before)); // clique sem arrastar
    CHECK(history.record(7,before,after));
    CHECK(history.undoCount()==1);CHECK(history.canUndo());CHECK(!history.canRedo());
    TransformEdit edit;
    CHECK(history.undo(edit));CHECK(edit.id==7);CHECK(NearlyEqual(edit.before.position,{0,0,0}));
    CHECK(NearlyEqual(edit.after.position,{4,0,0}));CHECK(history.canRedo());
    CHECK(history.redo(edit));CHECK(NearlyEqual(edit.after.position,{4,0,0}));
    CHECK(history.undo(edit));
    loblox::Transform other=before; other.position={0,5,0};
    CHECK(history.record(7,after,other)); // ramo novo descarta o redo
    CHECK(!history.canRedo());
    // q e -q sao a mesma orientacao: nao geram operacao fantasma.
    loblox::Transform flipped=before;
    flipped.rotation={-before.rotation.x,-before.rotation.y,-before.rotation.z,-before.rotation.w};
    CHECK(!history.record(9,before,flipped));
    EditHistory limited;
    for(int i=0;i<200;++i) {
        loblox::Transform a=before,b=before;a.position.x=float(i);b.position.x=float(i+1);
        limited.record(1,a,b);
    }
    CHECK(limited.undoCount()==EditHistory::kLimit);
    int undos=0;while(limited.undo(edit))++undos;
    CHECK(undos==static_cast<int>(EditHistory::kLimit));
    // O mais antigo preservado e o 72 (128 entradas mais recentes).
    CHECK(NearlyEqual(edit.before.position.x,float(200-EditHistory::kLimit)));
}
}
int main() {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    char temporary[]="/tmp/loblox-studio-XXXXXX";const char* directory=mkdtemp(temporary);
    if(!directory)return 1;diag::configure(std::string(directory)+"/studio.jsonl");
    SignificantFallPresentation();NpcHeadingRegression();StorageServices();RootServices();PartNamesLua();EditorSelectionAndNames();CursorConstruction();ConstructionPreviewWorkflow();SurfaceSnapping();Hierarchy();InteractionControls();InventoryLifecycle();AttachmentJoints();Attachments();ModelGizmoSession();ModelTransforms();BodyConfiguration();Scripts();LinbluxScripts();LinbluxExamples();LinbluxPhysics();LinbluxFixedContacts();LinbluxNpc();NpcExamples();NpcAnimation();NpcAnimatedExamples();LinbluxProgress();LinbluxTransformValidation();LinbluxCameraControl();LinbluxTaskCancellation();LinbluxInput();LinbluxPlayer();LuaFinalizerSafety();DynamicScriptLifecycle();AnimationIds();HealthAndVoid();PartChildScripts();IndependentFall();JumpTiming();ObbyLanding();ConsecutivePlatforms();AnimationDeletion();NativeCloneBudget();TransformTools();
    std::printf("Studio: %d checks, %d failures; artifacts: %s\n",checks,failures,directory);return failures?1:0;
}

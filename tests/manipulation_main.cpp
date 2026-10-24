#include <cstdio>
#include <cmath>
#include "ManipulationFixture.h"
#include "editor/GizmoLayout.h"
#include "editor/TransformSession.h"
#include "physics/PhysicsWorld.h"
#include "rendering/EditorCamera.h"
#include "serialization/ProjectIO.h"

using namespace loblox;
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}}while(0)
float CheckCorners(const Part& part,const PhysicsWorld& physics) {
    CollisionGeometry collider;CHECK(physics.collisionGeometry(part.id(),collider));
    const auto a=part.bounds().corners(),b=collider.box.corners();float error=0;
    for(int i=0;i<8;++i){const float d=Vector3Distance(a[i],b[i]);CHECK(d<.0001f);error=std::max(error,d);}
    return error;
}
int main(int argc,char** argv) {
    Scene scene;BuildManipulationFixture(scene);std::string error;
    if(argc==3&&std::string(argv[1])=="--fixture")return projectio::save(scene,argv[2],error)?0:1;
    PhysicsWorld physics;physics.attach(scene);float maxError=0;
    for(auto* p:scene.parts())maxError=std::max(maxError,CheckCorners(*p,physics));
    auto* part=scene.createPart(PartShape::Block,{20,10,0},{4,2,6},"ScaleProbe");physics.syncScene(scene);
    const Rectangle view{0,0,1000,700};
    for(const Vector3 rotation:{Vector3{},Vector3{0,90,0},Vector3{37,21,37}})for(int axis=0;axis<3;++axis)for(int side:{-1,1}) {
        const loblox::Transform start{{20,10,0},QuatFromEulerDegrees(rotation),{4,2,6}};part->setTransform(start);physics.applyChanges(part);
        const auto localAxis=gizmomath::Axis(axis,start.rotation,true);
        const auto fixedBefore=Vector3Subtract(start.position,Vector3Scale(localAxis,side*(&start.size.x)[axis]*.5f));
        EditorCamera camera;camera.setOrbit(start.position,22,.65f,.43f);
        Gizmo gizmo;gizmo.setSnap(1,1,0);gizmo.setLocalSpace(false);TransformSession session(gizmo,physics);
        const auto layout=gizmolayout::Build(*part,ToolMode::Scale,camera,view,false);
        GizmoInput input;input.mousePosition=layout.scaleHandle[axis*2+(side>0)];input.leftPressed=input.leftDown=true;
        const auto begin=session.update(ToolMode::Scale,part,camera,view,true,true,input);CHECK(begin.started);
        input.leftPressed=false;
        session.update(ToolMode::Scale,part,camera,view,true,true,input);CHECK(NearlyEqual(part->position(),start.position));
        Vector2 origin{},goal{};const auto face=Vector3Add(start.position,Vector3Scale(localAxis,side*(&start.size.x)[axis]*.5f));
        CHECK(camera.projectChecked(face,{view.width,view.height},origin));
        CHECK(camera.projectChecked(Vector3Add(face,Vector3Scale(localAxis,side*2.f)),{view.width,view.height},goal));
        input.mousePosition=Vector2Add(input.mousePosition,Vector2Subtract(goal,origin));
        session.update(ToolMode::Scale,part,camera,view,true,true,input);
        const auto size=part->size();
        CHECK(std::fabs((&size.x)[axis]-((&start.size.x)[axis]+2))<.0001f);
        const auto fixedAfter=Vector3Subtract(part->position(),Vector3Scale(localAxis,side*(&size.x)[axis]*.5f));
        CHECK(Vector3Distance(fixedBefore,fixedAfter)<.0001f);
        maxError=std::max(maxError,CheckCorners(*part,physics)); // BEFORE mouse-up
        session.cancel(part,true,"regression");CHECK(NearlyEqual(part->position(),start.position));CHECK(NearlyEqual(part->size(),start.size));
        CHECK(!physics.isPartEditing(part->id()));
    }
    // Geometry picking is independent from the world's AABB/collision margin.
    part->setTransform({{20,10,0},QuatFromEulerDegrees({0,37,0}),{8,.2f,2}});physics.applyChanges(part);
    Ray ray{{20,20,0},{0,-1,0}};CHECK(scene.pickPart(ray,100)==part);maxError=std::max(maxError,CheckCorners(*part,physics));
    // A fresh thin box and a scaled box must have the same margin and bounds.
    part->setSize({.2f,.2f,.2f});physics.applyChanges(part);
    auto* fresh=scene.createPart(PartShape::Block,{25,10,0},{.2f,.2f,.2f},"FreshThin");physics.syncScene(scene);
    CollisionGeometry a,b;CHECK(physics.collisionGeometry(part->id(),a));CHECK(physics.collisionGeometry(fresh->id(),b));
    CHECK(NearlyEqual(a.margin,b.margin));CHECK(NearlyEqual(a.box.half,b.box.half));
    // Changing shape used to cast a btBoxShape into a btSphereShape.
    part->setShape(PartShape::Sphere);physics.applyChanges(part);CHECK(physics.collisionGeometry(part->id(),a));CHECK(a.shape==PartShape::Sphere);
    part->setShape(PartShape::Block);physics.applyChanges(part);CHECK(physics.collisionGeometry(part->id(),a));CHECK(a.shape==PartShape::Block);
    Scene loaded;CHECK(projectio::fromJson(loaded,projectio::toJson(scene),error));PhysicsWorld loadedPhysics;loadedPhysics.attach(loaded);
    for(auto* p:loaded.parts())maxError=std::max(maxError,CheckCorners(*p,loadedPhysics));
    std::printf("Manipulation: %d checks, %d failures; max OBB/Bullet corner error %.9f\n",checks,failures,maxError);
    return failures?1:0;
}

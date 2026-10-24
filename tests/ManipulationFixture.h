#pragma once
#include "scene/Scene.h"
inline void BuildManipulationFixture(loblox::Scene& scene) {
    using namespace loblox;
    scene.createBaseplate({0,-.5f,0},{100,1,100});
    struct Box{const char* name;Vector3 position,size,rotation;};
    const Box boxes[]={{"Cube",{-9,2,0},{4,4,4},{}},{"Long",{0,2,0},{12,2,2},{}},
        {"Thin",{0,4,-8},{8,.2f,6},{}},{"Rotated90",{9,3,0},{4,2,6},{0,90,0}},
        {"Rotated37",{-8,4,-9},{4,2,6},{37,21,37}},
        {"AdjacentA",{0,2,9},{4,4,4},{}},{"AdjacentB",{4,2,9},{4,4,4},{}}};
    for(const auto& box:boxes){auto* p=scene.createPart(PartShape::Block,box.position,box.size,box.name);p->setAnchored(true);p->setRotationEulerDegrees(box.rotation);}
    auto* model=scene.createContainer(InstanceClass::Model,nullptr,"Assembly");
    auto* nested=scene.createContainer(InstanceClass::Folder,model,"Nested");std::string error;
    auto* a=scene.createPart(PartShape::Block,{10,3,-10},{2,6,2},"ModelColumn");a->setAnchored(true);scene.reparent(a,nested,error);
    auto* b=scene.createPart(PartShape::Block,{13,5,-10},{4,2,2},"ModelBeam");b->setAnchored(true);b->setRotationEulerDegrees({0,0,37});scene.reparent(b,model,error);
}

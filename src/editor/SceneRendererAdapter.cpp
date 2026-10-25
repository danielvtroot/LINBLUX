#include "rendering/SceneRenderer.h"
#include "rendering/EditorCamera.h"
namespace loblox {
void SceneRenderer::drawScene(const Scene& scene, const EditorCamera& camera, int width,
                              int height, InstanceId selectedId, const RigRenderState* rig) {
    drawScene(scene, camera.camera(), width, height, selectedId, rig);
}

void SceneRenderer::drawCollisionShapes(const Scene& scene,const PhysicsWorld& physics,const EditorCamera& camera,InstanceId selection) const {
    drawCollisionShapes(scene,physics,camera.camera(),selection);
}
}

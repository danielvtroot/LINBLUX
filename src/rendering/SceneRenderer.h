#pragma once

#include <cstdint>
#include <unordered_map>

#include "core/Math.h"
#include "objects/Part.h"
#include "game/Rig.h"

namespace loblox {

class Scene;
class EditorCamera;
class PhysicsWorld;
struct RigRenderState {
    const game::Rig* rig = nullptr;
    game::Pose pose{};
    game::RigidTransform root;
    int selectedBody = -1;
    bool pivots = false;
};

// Desenha a cena com recursos minimos (sem sombras/post-processamento).
// Recursos graficos compartilhados sao liberados com o ultimo renderer.
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void drawScene(const Scene& scene, const EditorCamera& camera, int width, int height,
                   InstanceId selectedId, const RigRenderState* rig = nullptr);
    void drawScene(const Scene& scene, Camera3D camera, int width, int height,
                   InstanceId selectedId, const RigRenderState* rig = nullptr,
                   bool authoredRigs = true);

    // Cor final da Part considerando o material escolhido.
    static Color MaterialColor(const Part& part);
    static Color MaterialColor(Color color, PartMaterial material);
    void drawCollisionShapes(const Scene& scene,const PhysicsWorld& physics,
                             const EditorCamera& camera,InstanceId selection) const;

private:
    void drawBaseplate(const Part& baseplate, Vector3 lightDirection, float sunIntensity,
                       float brightness);
    void drawSelection(const Part& part) const;
    void drawRig(const RigRenderState& state, Vector3 light, float intensity, float brightness);

};

}  // namespace loblox

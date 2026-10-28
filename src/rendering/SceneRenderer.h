#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>
#include "rendering/RenderSettings.h"

#include "core/Math.h"
#include "objects/Part.h"
#include "game/Rig.h"

namespace loblox {

class Scene;
class EditorCamera;
class PhysicsWorld;
class ModernMaterials;
class SceneTarget;
struct RigRenderState {
    const game::Rig* rig = nullptr;
    game::Pose pose{};
    game::RigidTransform root;
    int selectedBody = -1;
    bool pivots = false;
};
struct EditorGeometryView {
    bool edges=false;
    bool bounds=false;
    bool normals=false;
};

// Cena compartilhada pelo Editor/Player, com qualidade escalonável.
// Recursos graficos compartilhados sao liberados com o ultimo renderer.
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    void setQuality(RenderQuality quality) {quality_=quality;}
    RenderQuality quality() const {return quality_;}
    bool modernReady() const;
    bool shadowsActive() const;
    void setAntialiasing(Antialiasing value) {antialiasing_=value;}
    Antialiasing antialiasing() const {return antialiasing_;}
    Antialiasing actualAntialiasing() const;

    void drawScene(const Scene& scene, const EditorCamera& camera, int width, int height,
                   InstanceId selectedId, const RigRenderState* rig = nullptr);
    void drawScene(const Scene& scene, Camera3D camera, int width, int height,
                   InstanceId selectedId, const RigRenderState* rig = nullptr,
                   bool authoredRigs = true);

    // Cor final da Part considerando o material escolhido.
    static Color MaterialColor(const Part& part);
    static Color MaterialColor(Color color, PartMaterial material);
    // Passe opcional de autoria. O Player não o chama.
    void drawEditorGuides(const Scene& scene,Camera3D camera,const EditorGeometryView& options,
                          InstanceId hovered=0) const;
    void drawCollisionShapes(const Scene& scene,const PhysicsWorld& physics,
                             const EditorCamera& camera,InstanceId selection) const;

    void drawCollisionShapes(const Scene& scene,const PhysicsWorld& physics,
                             Camera3D camera,InstanceId selection) const;

private:
    void drawScenePass(const Scene&,Camera3D,int,int,InstanceId,const RigRenderState*,bool);
    Antialiasing antialiasing_=Antialiasing::Auto;
    std::unique_ptr<SceneTarget> target_;
    RenderQuality quality_=RenderQuality::Medium;
    std::unique_ptr<ModernMaterials> modern_;
    void drawBaseplate(const Part& baseplate, Vector3 lightDirection, float sunIntensity,
                       float brightness);
    void drawSelection(const Part& part) const;
    void drawRig(const RigRenderState& state, Vector3 light, float intensity, float brightness);

};

}  // namespace loblox

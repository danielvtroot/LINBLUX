#pragma once

#include <cstdint>
#include <unordered_map>

#include "core/Math.h"
#include "objects/Part.h"

namespace loblox {

class Scene;
class EditorCamera;

// Desenha a cena com recursos minimos (sem sombras/post-processamento).
// Recursos graficos sao criados uma vez e cacheados (studs da Baseplate).
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void drawScene(const Scene& scene, const EditorCamera& camera, int width, int height,
                   InstanceId selectedId);

    // Cor final da Part considerando o material escolhido.
    static Color MaterialColor(const Part& part);

private:
    struct StudMeshEntry {
        Vector3 size{0.0f, 0.0f, 0.0f};
        Mesh mesh{};
        bool valid = false;
    };

    void ensureStudMesh(const Part& baseplate);
    void unloadStudMeshes();
    void drawBaseplate(const Part& baseplate, Vector3 lightDirection, float sunIntensity,
                       float brightness);
    void drawSelection(const Part& part) const;

    std::unordered_map<InstanceId, StudMeshEntry> studMeshes_;
    Material studMaterial_{};
};

}  // namespace loblox

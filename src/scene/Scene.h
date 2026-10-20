#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "objects/Baseplate.h"
#include "objects/Instance.h"
#include "objects/Lighting.h"
#include "objects/Part.h"
#include "objects/SpawnLocation.h"
#include "objects/Workspace.h"
#include "game/Rig.h"

namespace loblox {

// Dona de todos os objetos da cena e da hierarquia. Nao conhece ImGui, OpenGL
// nem Bullet: a fisica se registra como listener de remocao.
class Scene {
public:
    using RemovalListener = std::function<void(Instance*)>;

    Scene();
    ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Cria a cena inicial: Baseplate + SpawnLocation dentro do Workspace.
    void buildDefaultScene();

    Workspace* workspace() const { return workspace_; }
    Lighting* lighting() const { return lighting_; }

    Part* createPart(PartShape shape, Vector3 position,
                     Vector3 size = Vector3{4.0f, 4.0f, 4.0f},
                     const std::string& name = "",
                     InstanceId forcedId = kInvalidInstanceId);
    Baseplate* createBaseplate(Vector3 position = Vector3{0.0f, -0.5f, 0.0f},
                               Vector3 size = Vector3{160.0f, 1.0f, 160.0f},
                               const std::string& name = "",
                               InstanceId forcedId = kInvalidInstanceId);
    SpawnLocation* createSpawnLocation(
        Vector3 position = Vector3{0.0f, 0.5f, 0.0f},
        Vector3 size = Vector3{12.0f, 1.0f, 12.0f},
        const std::string& name = "",
        InstanceId forcedId = kInvalidInstanceId);

    // Copy properties in place, with a fresh ID and unique name. Selection is unchanged.
    Part* duplicatePart(const Part& source);

    // Remove a subarvore. Recusa objetos protegidos (Baseplate/servicos).
    bool remove(Instance* object);
    bool removeById(InstanceId id);

    // Remove tudo que esta dentro dos servicos, inclusive a Baseplate.
    // Usado apenas ao carregar um projeto.
    void clearObjects();

    // Troca o conteudo por uma cena temporaria ja construida e validada.
    // Nunca falha: a cena atual so e substituida depois que o candidato esta
    // completo. Os listeners de remocao permanecem neste Scene e sao avisados
    // sobre as Parts antigas para que a fisica nao guarde corpos orfaos.
    void replaceContentsFrom(Scene& source);

    Instance* find(InstanceId id) const;
    Part* findPart(InstanceId id) const;

    const std::vector<std::unique_ptr<Instance>>& objects() const { return objects_; }
    const std::vector<Part*>& parts() const { return parts_; }
    game::GameAssets& gameAssets() { return gameAssets_; }
    const game::GameAssets& gameAssets() const { return gameAssets_; }

    Instance* selection() const { return selection_; }
    void select(Instance* object);

    InstanceId nextId() const { return nextId_; }
    void setNextId(InstanceId id);
    InstanceId allocateId();

    // Invariantes de cena. Retorna false com uma descricao sem alterar nada.
    bool validate(std::string& error) const;

    std::string makeUniqueName(const std::string& base) const;

    void addRemovalListener(RemovalListener listener);

private:
    bool adopt(std::unique_ptr<Instance> object, Instance* parent);
    void notifyRemoval(Instance* object);
    bool isService(const Instance* object) const;
    bool canDeleteSubtree(const Instance* object) const;
    void removeSubtree(Instance* object);
    void rebuildParts();

    std::vector<std::unique_ptr<Instance>> objects_;
    std::vector<Part*> parts_;
    std::unordered_map<InstanceId, Instance*> byId_;
    std::vector<RemovalListener> removalListeners_;
    Workspace* workspace_ = nullptr;
    Lighting* lighting_ = nullptr;
    Instance* selection_ = nullptr;
    InstanceId nextId_ = 1;
    game::GameAssets gameAssets_;
};

}  // namespace loblox

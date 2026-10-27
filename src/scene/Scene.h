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

// Apresentação visual de um NPC com rig, reconstruída a cada passo fixo pelo
// NpcSystem. Vive apenas na cena de execução: nunca é serializada nem chega à
// cena autoral, por isso o Play continua sem tocar no documento.
struct NpcVisual {
    InstanceId part = 0;
    std::string rigId;
    game::Pose pose{};
    game::RigidTransform root{};
};

// Dona de todos os objetos da cena e da hierarquia. Nao conhece ImGui, OpenGL
// nem Bullet: a fisica se registra como listener de remocao.
class Scene {
public:
    using RemovalListener = std::function<void(Instance*)>;

    explicit Scene(InstanceId workspaceId = 1, InstanceId lightingId = 2);
    ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Cria a cena inicial: Baseplate + SpawnLocation dentro do Workspace.
    void buildDefaultScene();
    void installDefaultScripts(); // explicit opt-in for old maps; never overwrites existing names

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
    Attachment* createAttachment(Instance* parent,const std::string& name="Attachment",InstanceId forcedId=0);
    bool connectAttachments(InstanceId source,InstanceId target,std::string& error,bool preserve=true);
    bool attachmentParentFrame(const Instance* owner,ModelFrame& frame,std::string& error) const;
    bool attachmentFrame(InstanceId attachment,ModelFrame& world,std::string& error) const;
    Surface* createSurface(InstanceClass type,Instance* parent,const std::string& name="",InstanceId forcedId=0);
    Container* createContainer(InstanceClass type, Instance* parent = nullptr,
                              const std::string& name = "", InstanceId forcedId = 0);
    // World transforms and object addresses remain unchanged. Depth is capped at 128.
    bool canParent(const Instance* object, const Instance* parent) const;
    bool reparent(Instance* object, Instance* parent, std::string& error);
    bool modelFrame(InstanceId model,ModelFrame& frame,std::string& error) const;
    bool setModelPivot(InstanceId model,const ModelFrame& frame,std::string& error);
    bool clearModelPivot(InstanceId model,std::string& error);
    bool transformModel(InstanceId model,const ModelFrame& frame,std::string& error);
    Container* group(const std::vector<InstanceId>& objects,const std::string& name,std::string& error);
    bool ungroup(InstanceId model,std::string& error);
    Instance* duplicateTree(const Instance& source, Instance* parent = nullptr);
    Script* createScript(InstanceClass type, Instance* parent = nullptr,
                         const std::string& name = "", InstanceId forcedId = 0);
    Instance* ensureService(InstanceClass type, InstanceId forcedId = 0);
    Instance* service(InstanceClass type) const;

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
    std::vector<NpcVisual>& npcVisuals() { return npcVisuals_; }
    const std::vector<NpcVisual>& npcVisuals() const { return npcVisuals_; }

    Instance* selection() const { return selection_; }
    void select(Instance* object);
    void toggleSelection(Instance* object);
    bool selected(InstanceId id) const;
    const std::vector<InstanceId>& selectedIds() const { return selectionIds_; }

    InstanceId nextId() const { return nextId_; }
    void setNextId(InstanceId id);
    InstanceId allocateId();

    // Invariantes de cena. Retorna false com uma descricao sem alterar nada.
    bool validate(std::string& error) const;
    Part* pickPart(Ray ray, float maxDistance, Vector3* point=nullptr) const;
    struct PartPick {Part* part;float distance;Vector3 point;};
    std::vector<PartPick> pickParts(Ray ray,float maxDistance) const;

    std::string makeUniqueName(const std::string& base) const;

    void addRemovalListener(RemovalListener listener);

private:
    bool adopt(std::unique_ptr<Instance> object, Instance* parent);
    void notifyRemoval(Instance* object);
    bool isService(const Instance* object) const;
    bool accepts(InstanceClass type, const Instance* parent) const;
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
    std::vector<InstanceId> selectionIds_;
    InstanceId nextId_ = 1;
    game::GameAssets gameAssets_;
    std::vector<NpcVisual> npcVisuals_;
};

}  // namespace loblox

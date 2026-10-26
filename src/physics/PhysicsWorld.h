#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include "core/Math.h"
#include "objects/Part.h"

class btFixedConstraint;
class btBroadphaseInterface;
class btCollisionConfiguration;
class btCollisionDispatcher;
class btCollisionShape;
class btDiscreteDynamicsWorld;
class btRigidBody;
class btSequentialImpulseConstraintSolver;
class btDefaultCollisionConfiguration;

namespace loblox {
namespace game { class RuntimeSession; }

class Scene;
struct CollisionGeometry {
    PartShape shape=PartShape::Block;
    OrientedBox box;
    float margin=0;
};

// Ponte entre o modelo de objetos e o Bullet. Existe exatamente um corpo por
// Part; a criacao/destruicao e controlada por addPart/removePart.
// Nao depende de ImGui/OpenGL. Nao cria corpos por frame.
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Registra o listener de remocao e cria corpos para todas as Parts.
    // A Scene deve viver mais que este PhysicsWorld.
    void attach(Scene& scene);
    void rebuildAll(Scene& scene);
    void clearBodies();

    void addPart(Part* part);
    void removePart(InstanceId id);
    bool hasBody(InstanceId id) const;
    std::size_t bodyCount() const { return bodies_.size(); }
    std::size_t jointCount() const { return joints_.size(); }
    // Correspondencia exata Part <-> corpo e editingParts_ contido nos corpos.
    bool validate(const Scene& scene, std::string& error) const;

    // Aplica as flags dirty de uma Part ao corpo correspondente.
    void applyChanges(Part* part);
    void setPartEditing(InstanceId id, bool editing);
    bool isPartEditing(InstanceId id) const;
    void syncScene(Scene& scene);

    // Passo fixo (1/60) com limite de subpassos; independente do FPS.
    void simulate(Scene& scene, float deltaSeconds);

    // Copia transformacoes dos corpos dinamicos para as Parts.
    void syncFromBodies(Scene& scene);

    void setGravity(Vector3 gravity);
    Vector3 gravity() const;
    void setFixedStep(float seconds, int maxSubSteps);

    // Consultas usadas pelos testes e pela UI.
    bool isDynamic(InstanceId id) const;
    Vector3 bodyPosition(InstanceId id) const;
    Vector3 linearVelocity(InstanceId id) const;
    float bodyMass(InstanceId id) const;
    Quaternion bodyRotation(InstanceId id) const;
    // Read the ACTUAL Bullet shape, never reconstruct it from the Part.
    bool collisionGeometry(InstanceId id, CollisionGeometry& out) const;

    // Raycast contra todos os corpos. Retorna a Part atingida mais proxima.
    // solidOnly ignora corpos sem resposta de contato (CanCollide=false) e
    // staticOnly restringe aos corpos estaticos, que e o que a navegacao pode
    // tratar como chao ou parede sem envelhecer quando um corpo se move.
    bool raycast(Vector3 origin, Vector3 direction, float maxDistance, Part** hitPart,
                 Vector3* hitPoint = nullptr, Vector3* hitNormal = nullptr,
                 bool solidOnly = false, InstanceId ignore = 0,
                 bool staticOnly = false) const;

    // Gira somente a orientacao do corpo, preservando posicao e velocidade.
    // Usado pelo controlador de NPC; a Part recebe o valor no proximo sync.
    bool setBodyYaw(InstanceId id, float yawDegrees);
    bool lockAngularMotion(InstanceId id, bool locked);

    // Verdadeiro quando nenhuma Part solida ocupa a caixa indicada. Usado pelo
    // degrau do NPC: a subida so e aplicada onde o corpo cabe de fato.
    bool isSpaceClear(Vector3 center, Vector3 halfExtents, InstanceId ignore) const;

private:
    friend class game::RuntimeSession; // single explicitly-owned runtime capsule
    struct BodyEntry {
        btRigidBody* body = nullptr;
        btCollisionShape* shape = nullptr;
    };

    btCollisionShape* createShape(const Part& part) const;
    btRigidBody* createBody(Part& part, btCollisionShape* shape);
    void destroyEntry(InstanceId id);
    void removeJointsFor(InstanceId id);
    void syncJoints(Scene& scene);
    struct JointEntry {btFixedConstraint* constraint=nullptr;InstanceId target=0,a=0,b=0;};
    std::unordered_map<InstanceId,JointEntry> joints_;
    void updateShape(const Part& part, BodyEntry& entry);
    void setBodyTransform(const Part& part, BodyEntry& entry, bool resetVelocity);
    void applyCollisionFlag(const Part& part, btRigidBody* body);

    std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration_;
    std::unique_ptr<btCollisionDispatcher> dispatcher_;
    std::unique_ptr<btBroadphaseInterface> broadphase_;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;
    std::unique_ptr<btDiscreteDynamicsWorld> world_;
    std::unordered_map<InstanceId, BodyEntry> bodies_;
    std::unordered_set<InstanceId> editingParts_;
    float fixedStep_ = 1.0f / 60.0f;
    int maxSubSteps_ = 5;
};

}  // namespace loblox

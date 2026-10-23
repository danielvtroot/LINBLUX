#include "physics/PhysicsWorld.h"
#include "core/Diagnostics.h"

#include <algorithm>
#include <cmath>

#include <btBulletDynamicsCommon.h>

#include "scene/Scene.h"

namespace loblox {

namespace {

constexpr float kMinMass = 0.1f;
constexpr float kDensity = 1.0f;

float PartVolume(const Part& part) {
    const Vector3 size = part.size();
    if (part.shape() == PartShape::Sphere) {
        const float radius = std::max(0.05f, part.sphereRadius());
        return (4.0f / 3.0f) * PI * radius * radius * radius;
    }
    return size.x * size.y * size.z;
}

float PartMass(const Part& part) {
    return part.anchored() ? 0.0f : std::max(kMinMass, PartVolume(part) * kDensity);
}

btVector3 ToBullet(Vector3 value) {
    return btVector3(value.x, value.y, value.z);
}

Vector3 FromBullet(const btVector3& value) {
    return Vector3{value.x(), value.y(), value.z()};
}

btQuaternion ToBullet(Quaternion value) {
    return btQuaternion(value.x, value.y, value.z, value.w);
}

Quaternion FromBullet(const btQuaternion& value) {
    return Quaternion{value.x(), value.y(), value.z(), value.w()};
}

}  // namespace

PhysicsWorld::PhysicsWorld() {
    collisionConfiguration_ = std::make_unique<btDefaultCollisionConfiguration>();
    dispatcher_ = std::make_unique<btCollisionDispatcher>(collisionConfiguration_.get());
    broadphase_ = std::make_unique<btDbvtBroadphase>();
    solver_ = std::make_unique<btSequentialImpulseConstraintSolver>();
    world_ = std::make_unique<btDiscreteDynamicsWorld>(
        dispatcher_.get(), broadphase_.get(), solver_.get(), collisionConfiguration_.get());
    world_->setGravity(btVector3(0.0f, -98.1f, 0.0f));
    // More solver iterations noticeably reduce jitter for boxes resting on
    // thin or rotated platforms without changing the fixed-step contract.
    world_->getSolverInfo().m_numIterations = 16;
    world_->getSolverInfo().m_splitImpulse = 1;
    world_->getSolverInfo().m_splitImpulsePenetrationThreshold = -0.02f;
}

PhysicsWorld::~PhysicsWorld() {
    clearBodies();
}

void PhysicsWorld::attach(Scene& scene) {
    scene.addRemovalListener([this](Instance* object) {
        if (object != nullptr && object->isPart()) {
            removePart(object->id());
        }
    });
    rebuildAll(scene);
}

void PhysicsWorld::rebuildAll(Scene& scene) {
    clearBodies();
    for (Part* part : scene.parts()) {
        addPart(part);
        if (part != nullptr) {
            part->clearDirty();
        }
    }
}

void PhysicsWorld::clearBodies() {
    for (auto& pair : bodies_) {
        BodyEntry& entry = pair.second;
        world_->removeRigidBody(entry.body);
        delete entry.body->getMotionState();
        delete entry.body;
        delete entry.shape;
    }
    bodies_.clear();
    editingParts_.clear();
}

btCollisionShape* PhysicsWorld::createShape(const Part& part) const {
    if (part.shape() == PartShape::Sphere) {
        return new btSphereShape(std::max(0.05f, part.sphereRadius()));
    }
    const Vector3 half = part.halfExtents();
    return new btBoxShape(btVector3(std::max(0.05f, half.x), std::max(0.05f, half.y),
                                    std::max(0.05f, half.z)));
}

btRigidBody* PhysicsWorld::createBody(Part& part, btCollisionShape* shape) {
    const Vector3 size = part.size();
    const float mass = PartMass(part);

    btVector3 inertia(0.0f, 0.0f, 0.0f);
    if (mass > 0.0f) {
        shape->calculateLocalInertia(mass, inertia);
    }

    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(ToBullet(part.position()));
    transform.setRotation(ToBullet(part.rotation()));

    auto* motionState = new btDefaultMotionState(transform);

    btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, shape, inertia);
    info.m_friction = 0.8f;
    info.m_restitution = 0.0f;
    info.m_linearDamping = 0.02f;
    info.m_angularDamping = 0.1f;

    auto* body = new btRigidBody(info);
    body->setUserPointer(&part);
    body->setSleepingThresholds(0.2f, 0.25f);

    if (part.anchored()) {
        body->setCollisionFlags(body->getCollisionFlags() |
                                btCollisionObject::CF_STATIC_OBJECT);
        body->setActivationState(DISABLE_DEACTIVATION);
    } else {
        // CCD evita que pecas rapidas atravessem a Baseplate.
        const float minSize = std::min(size.x, std::min(size.y, size.z));
        body->setCcdMotionThreshold(std::max(0.2f, minSize * 0.5f));
        body->setCcdSweptSphereRadius(std::max(0.1f, minSize * 0.2f));
    }

    applyCollisionFlag(part, body);
    world_->addRigidBody(body);
    return body;
}

void PhysicsWorld::applyCollisionFlag(const Part& part, btRigidBody* body) {
    int flags = body->getCollisionFlags();
    if (part.canCollide()) {
        flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
    } else {
        flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
    }
    body->setCollisionFlags(flags);
}

void PhysicsWorld::addPart(Part* part) {
    if (part == nullptr) {
        return;
    }
    // Garante um unico corpo por Part.
    destroyEntry(part->id());
    btCollisionShape* shape = createShape(*part);
    btRigidBody* body = createBody(*part, shape);
    bodies_[part->id()] = BodyEntry{body, shape};
}

void PhysicsWorld::destroyEntry(InstanceId id) {
    editingParts_.erase(id);
    const auto it = bodies_.find(id);
    if (it == bodies_.end()) {
        return;
    }
    BodyEntry entry = it->second;
    bodies_.erase(it);
    if (entry.body != nullptr) {
        world_->removeRigidBody(entry.body);
        delete entry.body->getMotionState();
        delete entry.body;
    }
    delete entry.shape;
}

void PhysicsWorld::removePart(InstanceId id) {
    destroyEntry(id);
}

bool PhysicsWorld::hasBody(InstanceId id) const {
    return bodies_.find(id) != bodies_.end();
}

bool PhysicsWorld::validate(const Scene& scene, std::string& error) const {
    error.clear();
    std::size_t partCount = 0;
    for (Part* part : scene.parts()) {
        if (part == nullptr) {
            error = "Part nula na cena";
            return false;
        }
        ++partCount;
        const auto it = bodies_.find(part->id());
        if (it == bodies_.end()) {
            error = "Part sem corpo fisico: " + std::to_string(part->id());
            return false;
        }
        if (it->second.body == nullptr || it->second.shape == nullptr) {
            error = "corpo ou shape nulo para a Part " + std::to_string(part->id());
            return false;
        }
        if (it->second.body->getUserPointer() != part) {
            error = "corpo aponta para outra Part: " + std::to_string(part->id());
            return false;
        }
    }
    if (bodies_.size() != partCount) {
        error = "quantidade de corpos difere da quantidade de Parts";
        return false;
    }
    for (const auto& pair : bodies_) {
        if (scene.findPart(pair.first) == nullptr) {
            error = "corpo orfao sem Part: " + std::to_string(pair.first);
            return false;
        }
    }
    for (InstanceId id : editingParts_) {
        if (bodies_.find(id) == bodies_.end()) {
            error = "Part em edicao sem corpo: " + std::to_string(id);
            return false;
        }
    }
    return true;
}

void PhysicsWorld::setBodyTransform(const Part& part, BodyEntry& entry, bool resetVelocity) {
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(ToBullet(part.position()));
    transform.setRotation(ToBullet(part.rotation()));

    btRigidBody* body = entry.body;
    body->setWorldTransform(transform);
    body->setInterpolationWorldTransform(transform);
    if (body->getMotionState() != nullptr) {
        body->getMotionState()->setWorldTransform(transform);
    }
    if (resetVelocity) {
        body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
        body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
        body->clearForces();
    }
    if (!isPartEditing(part.id())) body->activate(true);
    world_->updateSingleAabb(body);
}

void PhysicsWorld::applyChanges(Part* part) {
    if (part == nullptr) {
        return;
    }
    const auto it = bodies_.find(part->id());
    if (it == bodies_.end()) {
        addPart(part);
        part->clearDirty();
        return;
    }

    PartDirty& dirty = part->dirty();
    const PartDirty pending = dirty;

    if (pending.anchored) {
        // Alternar estatico/dinamico exige mudanca de massa: reconstroi o corpo.
        addPart(part);
        part->clearDirty();
        return;
    }

    BodyEntry& entry = it->second;
    if (pending.shape) {
        // Redimensiona o collider existente (nao cria corpo novo por frame).
        updateShape(*part, entry);
    }
    if (pending.transform) {
        setBodyTransform(*part, entry, true);
    }
    if (pending.collide) {
        applyCollisionFlag(*part, entry.body);
        entry.body->activate(true);
    }
    dirty.clear();
}

void PhysicsWorld::updateShape(const Part& part, BodyEntry& entry) {
    if (part.shape() == PartShape::Sphere) {
        static_cast<btSphereShape*>(entry.shape)->setUnscaledRadius(
            std::max(0.05f, part.sphereRadius()));
    } else {
        const Vector3 half = part.halfExtents();
        const btScalar margin = entry.shape->getMargin();
        static_cast<btBoxShape*>(entry.shape)->setImplicitShapeDimensions(
            btVector3(std::max(0.05f, half.x) - margin, std::max(0.05f, half.y) - margin,
                      std::max(0.05f, half.z) - margin));
    }

    const float mass = PartMass(part);
    btVector3 inertia(0.0f, 0.0f, 0.0f);
    if (mass > 0.0f) {
        entry.shape->calculateLocalInertia(mass, inertia);
    }
    entry.body->setMassProps(mass, inertia);
    entry.body->updateInertiaTensor();
    world_->updateSingleAabb(entry.body);
    if (!isPartEditing(part.id())) entry.body->activate(true);
}

void PhysicsWorld::setPartEditing(InstanceId id, bool editing) {
    const auto bodyIt = bodies_.find(id);
    if (bodyIt == bodies_.end() || bodyIt->second.body->getInvMass() <= 0.0f) return;
    btRigidBody* body = bodyIt->second.body;
    if (editing) {
        if (!editingParts_.insert(id).second) return;
        body->setLinearVelocity(btVector3(0, 0, 0));
        body->setAngularVelocity(btVector3(0, 0, 0));
        body->clearForces();
        body->forceActivationState(DISABLE_SIMULATION);
        diag::Event event;
        event.subsystem = "physics";
        event.name = "edit_begin";
        event.entityId = id;
        diag::log(event);
        return;
    }
    if (editingParts_.erase(id) == 0) return;
    body->forceActivationState(ACTIVE_TAG);
    body->setLinearVelocity(btVector3(0, 0, 0));
    body->setAngularVelocity(btVector3(0, 0, 0));
    body->clearForces();
    body->activate(true);
    diag::Event event;
    event.subsystem = "physics";
    event.name = "edit_end";
    event.entityId = id;
    diag::log(event);
}

bool PhysicsWorld::isPartEditing(InstanceId id) const {
    return editingParts_.find(id) != editingParts_.end();
}

void PhysicsWorld::syncScene(Scene& scene) {
    for (Part* part : scene.parts()) {
        if (part == nullptr) {
            continue;
        }
        // Part criada pela UI/loader e ainda sem corpo: cria no proximo frame.
        if (bodies_.find(part->id()) == bodies_.end()) {
            addPart(part);
            part->clearDirty();
            continue;
        }
        if (part->dirty().any()) {
            applyChanges(part);
        }
    }
}

void PhysicsWorld::simulate(Scene& scene, float deltaSeconds) {
    const float clamped = std::clamp(deltaSeconds, 0.0f, 0.1f);
    syncScene(scene);
    if (clamped > 0.0f) {
        world_->stepSimulation(clamped, maxSubSteps_, fixedStep_);
    }
    syncFromBodies(scene);
}

void PhysicsWorld::syncFromBodies(Scene& scene) {
    for (Part* part : scene.parts()) {
        if (part == nullptr || part->anchored() || isPartEditing(part->id())) {
            continue;
        }
        const auto it = bodies_.find(part->id());
        if (it == bodies_.end()) {
            continue;
        }
        const btTransform& transform = it->second.body->getWorldTransform();
        part->syncFromPhysics(FromBullet(transform.getOrigin()),
                              FromBullet(transform.getRotation()));
    }
}

void PhysicsWorld::setGravity(Vector3 gravity) {
    if (world_ != nullptr) {
        world_->setGravity(ToBullet(gravity));
    }
}

Vector3 PhysicsWorld::gravity() const {
    return world_ != nullptr ? FromBullet(world_->getGravity()) : Vector3{0, 0, 0};
}

void PhysicsWorld::setFixedStep(float seconds, int maxSubSteps) {
    fixedStep_ = std::max(1.0f / 240.0f, seconds);
    maxSubSteps_ = std::max(1, maxSubSteps);
}

bool PhysicsWorld::isDynamic(InstanceId id) const {
    const auto it = bodies_.find(id);
    return it != bodies_.end() && it->second.body->getInvMass() > 0.0f;
}

Vector3 PhysicsWorld::bodyPosition(InstanceId id) const {
    const auto it = bodies_.find(id);
    if (it == bodies_.end()) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }
    return FromBullet(it->second.body->getWorldTransform().getOrigin());
}

Quaternion PhysicsWorld::bodyRotation(InstanceId id) const {
    const auto it = bodies_.find(id);
    if (it == bodies_.end()) {
        return Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
    }
    return FromBullet(it->second.body->getWorldTransform().getRotation());
}

Vector3 PhysicsWorld::linearVelocity(InstanceId id) const {
    const auto it = bodies_.find(id);
    if (it == bodies_.end()) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }
    return FromBullet(it->second.body->getLinearVelocity());
}

bool PhysicsWorld::raycast(Vector3 origin, Vector3 direction, float maxDistance, Part** hitPart,
                           Vector3* hitPoint, Vector3* hitNormal) const {
    if (hitPart != nullptr) {
        *hitPart = nullptr;
    }
    const float length = Vector3Length(direction);
    if (length < 1e-6f) {
        return false;
    }
    const Vector3 normalized = Vector3Scale(direction, 1.0f / length);
    const btVector3 from = ToBullet(origin);
    const btVector3 to = ToBullet(Vector3Add(origin, Vector3Scale(normalized, maxDistance)));

    btCollisionWorld::ClosestRayResultCallback callback(from, to);
    world_->rayTest(from, to, callback);
    if (!callback.hasHit()) {
        return false;
    }

    auto* part = static_cast<Part*>(const_cast<btCollisionObject*>(callback.m_collisionObject)
                                        ->getUserPointer());
    if (part == nullptr) {
        return false;
    }
    if (hitPart != nullptr) {
        *hitPart = part;
    }
    if (hitPoint != nullptr) {
        *hitPoint = FromBullet(callback.m_hitPointWorld);
    }
    if (hitNormal != nullptr) {
        *hitNormal = FromBullet(callback.m_hitNormalWorld);
    }
    return true;
}

}  // namespace loblox

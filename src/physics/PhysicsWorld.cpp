#include "physics/PhysicsWorld.h"
#include "core/Diagnostics.h"

#include <algorithm>
#include <cmath>

#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btFixedConstraint.h>

#include "scene/Scene.h"

namespace loblox {

namespace {

constexpr float kMinMass = 0.1f;

float PartVolume(const Part& part) {
    const Vector3 size = part.size();
    if (part.shape() == PartShape::Sphere) {
        const float radius = std::max(0.05f, part.sphereRadius());
        return (4.0f / 3.0f) * PI * radius * radius * radius;
    }
    return size.x * size.y * size.z;
}

float PartMass(const Part& part) {
    return part.anchored() ? 0.0f : std::max(kMinMass, PartVolume(part) * part.bodyProperties().density);
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
        if(object)removeJointsFor(object->id());
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
    syncJoints(scene);
}

void PhysicsWorld::clearBodies() {
    for(auto& pair:joints_){world_->removeConstraint(pair.second.constraint);delete pair.second.constraint;}joints_.clear();
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
    const auto& properties=part.bodyProperties();
    info.m_friction = properties.friction;
    info.m_restitution = properties.restitution;
    info.m_linearDamping = properties.linearDamping;
    info.m_angularDamping = properties.angularDamping;

    auto* body = new btRigidBody(info);
    body->setUserPointer(&part);
    body->setSleepingThresholds(0.2f, 0.25f);

    if (part.anchored()) {
        body->setCollisionFlags(body->getCollisionFlags() |
                                btCollisionObject::CF_STATIC_OBJECT);
        body->setActivationState(DISABLE_DEACTIVATION);
    } else {
        // Um NPC e um corpo dinamico conduzido pelo controlador: fica em pe
        // (sem tombamento) e sempre ativo, porque a velocidade e reescrita a
        // cada passo fixo.
        if (part.hasNpc()) {
            body->setAngularFactor(btVector3(0, 0, 0));
            body->setActivationState(DISABLE_DEACTIVATION);
        }
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
    removeJointsFor(id);
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
    if(world_->getNumConstraints()!=static_cast<int>(joints_.size())){error="Índice de juntas Bullet inconsistente.";return false;}
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
    if(pending.body) {
        const auto& properties=part->bodyProperties();
        const float mass=PartMass(*part);btVector3 inertia(0,0,0);
        if(mass>0)entry.shape->calculateLocalInertia(mass,inertia);
        entry.body->setMassProps(mass,inertia);entry.body->updateInertiaTensor();
        // Liga/desliga o travamento de rotacao do NPC sem recriar o corpo.
        entry.body->setAngularFactor(part->hasNpc()?btVector3(0,0,0):btVector3(1,1,1));
        entry.body->setFriction(properties.friction);entry.body->setRestitution(properties.restitution);
        entry.body->setDamping(properties.linearDamping,properties.angularDamping);
        if(!isPartEditing(part->id()))entry.body->activate(true);
    }
    dirty.clear();
}

void PhysicsWorld::updateShape(const Part& part, BodyEntry& entry) {
    // A resize invalidates cached contacts as well as the broadphase bounds.
    world_->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
        entry.body->getBroadphaseHandle(),dispatcher_.get());
    const int wanted=part.shape()==PartShape::Sphere?SPHERE_SHAPE_PROXYTYPE:BOX_SHAPE_PROXYTYPE;
    if(entry.shape->getShapeType()!=wanted) {
        auto* previous=entry.shape;entry.shape=createShape(part);
        entry.body->setCollisionShape(entry.shape);delete previous;
    }
    if (part.shape() == PartShape::Sphere) {
        static_cast<btSphereShape*>(entry.shape)->setUnscaledRadius(
            std::max(0.05f, part.sphereRadius()));
    } else {
        const Vector3 half = part.halfExtents();
        // Match btBoxShape's safe-margin policy after EVERY resize, not only
        // construction. A thin resized box must match a fresh identical box.
        entry.shape->setMargin(std::min(.04f,.1f*std::min({half.x,half.y,half.z})));
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

void PhysicsWorld::removeJointsFor(InstanceId id) {
    for(auto it=joints_.begin();it!=joints_.end();)if(it->first==id||it->second.target==id||it->second.a==id||it->second.b==id){
        world_->removeConstraint(it->second.constraint);delete it->second.constraint;it=joints_.erase(it);
    }else ++it;
}
void PhysicsWorld::syncJoints(Scene& scene) {
    std::unordered_set<InstanceId> live;
    for(const auto& node:scene.objects())if(node->instanceClass()==InstanceClass::Attachment){
        const auto& a=static_cast<const Attachment&>(*node);const auto* other=scene.find(a.jointTarget());
        if(!other||other->instanceClass()!=InstanceClass::Attachment||!a.parent()->isPart()||!other->parent()->isPart()||a.parent()==other->parent())continue;
        const auto aid=a.parent()->id(),bid=other->parent()->id();if(!hasBody(aid)||!hasBody(bid))continue;
        ModelFrame ownerA,ownerB;std::string error;
        if(!scene.attachmentParentFrame(a.parent(),ownerA,error)||!scene.attachmentParentFrame(other->parent(),ownerB,error))continue;
        auto local=[](const Attachment& anchor,float scale){btTransform frame;frame.setIdentity();frame.setOrigin(ToBullet(Vector3Scale(anchor.localFrame().position,scale)));frame.setRotation(ToBullet(anchor.localFrame().rotation));return frame;};
        const auto frameA=local(a,ownerA.scale),frameB=local(static_cast<const Attachment&>(*other),ownerB.scale);
        auto it=joints_.find(a.id());
        if(it!=joints_.end()&&(it->second.target!=other->id()||it->second.a!=aid||it->second.b!=bid)){removeJointsFor(a.id());it=joints_.end();}
        if(it==joints_.end()){
            auto* constraint=new btFixedConstraint(*bodies_.at(aid).body,*bodies_.at(bid).body,frameA,frameB);
            // Long lever arms under LINBLUX gravity need a tighter local solve.
            // Keep the global solver budget unchanged for unconnected bodies.
            constraint->setOverrideNumSolverIterations(64);
            world_->addConstraint(constraint,true);joints_[a.id()]={constraint,other->id(),aid,bid};it=joints_.find(a.id());
            bodies_.at(aid).body->activate(true);bodies_.at(bid).body->activate(true);
        }else if(!(it->second.constraint->getFrameOffsetA()==frameA)||!(it->second.constraint->getFrameOffsetB()==frameB)){
            it->second.constraint->setFrames(frameA,frameB);bodies_.at(aid).body->activate(true);bodies_.at(bid).body->activate(true);
        }
        it->second.constraint->setEnabled(!isPartEditing(aid)&&!isPartEditing(bid));live.insert(a.id());
    }
    for(auto it=joints_.begin();it!=joints_.end();)if(!live.count(it->first)){
        world_->removeConstraint(it->second.constraint);delete it->second.constraint;it=joints_.erase(it);
    }else ++it;
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
    syncJoints(scene);
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
float PhysicsWorld::bodyMass(InstanceId id) const {
    const auto found=bodies_.find(id);if(found==bodies_.end())return 0;
    const float inverse=found->second.body->getInvMass();return inverse>0?1.f/inverse:0;
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
bool PhysicsWorld::collisionGeometry(InstanceId id,CollisionGeometry& out) const {
    const auto found=bodies_.find(id);if(found==bodies_.end())return false;
    const auto& entry=found->second;
    out.box.center=FromBullet(entry.body->getWorldTransform().getOrigin());
    out.box.rotation=FromBullet(entry.body->getWorldTransform().getRotation());
    out.margin=entry.shape->getMargin();
    if(entry.shape->getShapeType()==BOX_SHAPE_PROXYTYPE) {
        out.shape=PartShape::Block;out.box.half=FromBullet(static_cast<const btBoxShape*>(entry.shape)->getHalfExtentsWithMargin());
    }else if(entry.shape->getShapeType()==SPHERE_SHAPE_PROXYTYPE) {
        out.shape=PartShape::Sphere;const float r=static_cast<const btSphereShape*>(entry.shape)->getRadius();out.box.half={r,r,r};
    }else return false;
    return true;
}

Vector3 PhysicsWorld::linearVelocity(InstanceId id) const {
    const auto it = bodies_.find(id);
    if (it == bodies_.end()) {
        return Vector3{0.0f, 0.0f, 0.0f};
    }
    return FromBullet(it->second.body->getLinearVelocity());
}

bool PhysicsWorld::raycast(Vector3 origin, Vector3 direction, float maxDistance, Part** hitPart,
                           Vector3* hitPoint, Vector3* hitNormal, bool solidOnly,
                           InstanceId ignore, bool staticOnly) const {
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

    // Filtro por corpo: usado pela navegacao para nao considerar volumes sem
    // colisao como chao ou parede. Ignora tambem o proprio agente.
    struct FilteredRay final : btCollisionWorld::ClosestRayResultCallback {
        bool solidOnly, staticOnly;
        InstanceId ignore;
        FilteredRay(btVector3 a, btVector3 b, bool solid, InstanceId skip, bool statics)
            : ClosestRayResultCallback(a, b), solidOnly(solid), staticOnly(statics), ignore(skip) {}
        bool needsCollision(btBroadphaseProxy* proxy) const override {
            const auto* body = static_cast<const btCollisionObject*>(proxy->m_clientObject);
            const auto* part = static_cast<const Part*>(body->getUserPointer());
            if (!part || part->id() == ignore) return false;
            if (solidOnly && !body->hasContactResponse()) return false;
            if (staticOnly && !body->isStaticObject()) return false;
            return ClosestRayResultCallback::needsCollision(proxy);
        }
    } callback(from, to, solidOnly, ignore, staticOnly);
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

bool PhysicsWorld::setBodyYaw(InstanceId id, float yawDegrees) {
    if (!std::isfinite(yawDegrees)) return false;
    const auto it = bodies_.find(id);
    if (it == bodies_.end() || it->second.body->isStaticOrKinematicObject()) return false;
    btTransform transform = it->second.body->getWorldTransform();
    const btQuaternion yaw(btVector3(0, 1, 0), yawDegrees * DEG2RAD);
    transform.setRotation(yaw);
    it->second.body->setWorldTransform(transform);
    it->second.body->getMotionState()->setWorldTransform(transform);
    it->second.body->activate(true);
    return true;
}

bool PhysicsWorld::isSpaceClear(Vector3 center, Vector3 halfExtents, InstanceId ignore) const {
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z) ||
        !(halfExtents.x > 0) || !(halfExtents.y > 0) || !(halfExtents.z > 0)) {
        return false;
    }
    btBoxShape box(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
    btCollisionObject probe;
    probe.setCollisionShape(&box);
    probe.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(ToBullet(center));
    probe.setWorldTransform(transform);
    struct Clear final : btCollisionWorld::ContactResultCallback {
        InstanceId ignore;
        mutable bool blocked = false;
        bool needsCollision(btBroadphaseProxy* proxy) const override {
            const auto* body = static_cast<const btCollisionObject*>(proxy->m_clientObject);
            const auto* part = static_cast<const Part*>(body->getUserPointer());
            if (!part || part->id() == ignore || !body->hasContactResponse()) return false;
            return ContactResultCallback::needsCollision(proxy);
        }
        btScalar addSingleResult(btManifoldPoint& point, const btCollisionObjectWrapper*, int, int,
                                 const btCollisionObjectWrapper*, int, int) override {
            if (point.getDistance() <= .001f) blocked = true;
            return 0;
        }
    } callback;
    callback.ignore = ignore;
    world_->contactTest(&probe, callback);
    return !callback.blocked;
}

bool PhysicsWorld::lockAngularMotion(InstanceId id, bool locked) {
    const auto it = bodies_.find(id);
    if (it == bodies_.end()) return false;
    it->second.body->setAngularFactor(locked ? btVector3(0, 0, 0) : btVector3(1, 1, 1));
    it->second.body->activate(true);
    return true;
}

}  // namespace loblox

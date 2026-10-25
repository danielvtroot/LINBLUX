#include "game/Runtime.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <btBulletDynamicsCommon.h>
#include "core/Diagnostics.h"
#include "game/Spawn.h"
#include "serialization/ProjectIO.h"

namespace loblox::game {
namespace {
btVector3 Bt(Vector3 v) { return {v.x, v.y, v.z}; }
Vector3 Vec(const btVector3& v) { return {v.x(), v.y(), v.z()}; }
float Response(float rate, float dt) { return 1 - std::exp(-rate * dt); }
bool InsideBox(Vector3 p, Vector3 half, float margin = 0) {
    return std::fabs(p.x) <= half.x+margin && std::fabs(p.y) <= half.y+margin &&
           std::fabs(p.z) <= half.z+margin;
}
// Exit of a ray starting inside a visual body's local box. Same rig transforms
// and dimensions as rendering; no independently-maintained avatar collision rig.
float BoxExit(Vector3 origin, Vector3 direction, Vector3 half) {
    float exit = 1e30f;
    const float o[]{origin.x,origin.y,origin.z}, d[]{direction.x,direction.y,direction.z};
    const float h[]{half.x,half.y,half.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) > 1e-6f) exit = std::min(exit, ((d[i] > 0 ? h[i] : -h[i])-o[i])/d[i]);
    }
    return std::max(0.f,exit);
}
void Event(const char* subsystem, const char* name, const std::string& correlation,
           nlohmann::json context = nlohmann::json::object()) {
    diag::Event e; e.subsystem = subsystem; e.name = name; e.correlationId = correlation;
    e.context = std::move(context); e.result = "ok"; diag::log(e);
}
struct SolidRay : btCollisionWorld::ClosestRayResultCallback {
    const btCollisionObject* ignored;
    SolidRay(btVector3 from, btVector3 to, const btCollisionObject* ignore)
        : ClosestRayResultCallback(from, to), ignored(ignore) {}
    bool needsCollision(btBroadphaseProxy* proxy) const override {
        const auto* object = static_cast<const btCollisionObject*>(proxy->m_clientObject);
        return object != ignored && object->hasContactResponse() && ClosestRayResultCallback::needsCollision(proxy);
    }
};
struct SolidSweep : btCollisionWorld::ClosestConvexResultCallback {
    const btCollisionObject* ignored;
    SolidSweep(btVector3 from, btVector3 to, const btCollisionObject* ignore)
        : ClosestConvexResultCallback(from, to), ignored(ignore) {}
    bool needsCollision(btBroadphaseProxy* proxy) const override {
        const auto* object = static_cast<const btCollisionObject*>(proxy->m_clientObject);
        return object != ignored && object->hasContactResponse() && ClosestConvexResultCallback::needsCollision(proxy);
    }
};
// A downward shape sweep for the player capsule. Only up-facing surfaces stop
// the fall; brushing a vertical wall must not freeze vertical velocity.
struct DownSweep : btCollisionWorld::ClosestConvexResultCallback {
    const btCollisionObject* ignored;
    DownSweep(btVector3 from, btVector3 to, const btCollisionObject* ignore)
        : ClosestConvexResultCallback(from, to), ignored(ignore) {}
    bool needsCollision(btBroadphaseProxy* proxy) const override {
        const auto* object = static_cast<const btCollisionObject*>(proxy->m_clientObject);
        return object != ignored && object->hasContactResponse() &&
               ClosestConvexResultCallback::needsCollision(proxy);
    }
    btScalar addSingleResult(btCollisionWorld::LocalConvexResult& result,
                             bool normalInWorldSpace) override {
        const btVector3 normal = normalInWorldSpace
            ? result.m_hitNormalLocal
            : result.m_hitCollisionObject->getWorldTransform().getBasis() *
                  result.m_hitNormalLocal;
        if (normal.y() < .2f) return 1.0f;
        return ClosestConvexResultCallback::addSingleResult(result, normalInWorldSpace);
    }
};
struct Overlap : btCollisionWorld::ContactResultCallback {
    const btCollisionObject* ignored = nullptr;
    bool overlapping = false;
    bool needsCollision(btBroadphaseProxy* proxy) const override {
        const auto* object = static_cast<const btCollisionObject*>(proxy->m_clientObject);
        return object != ignored && object->hasContactResponse() && ContactResultCallback::needsCollision(proxy);
    }
    btScalar addSingleResult(btManifoldPoint& point, const btCollisionObjectWrapper* a, int, int,
                              const btCollisionObjectWrapper* b, int, int) override {
        if (a->getCollisionObject()->hasContactResponse() && b->getCollisionObject()->hasContactResponse() &&
            point.getDistance() < -.02f) overlapping = true;
        return 0;
    }
};
}
const char* StateName(HumanoidState state) {
    switch (state) {
        case HumanoidState::Idle: return "Idle";
        case HumanoidState::Walking: return "Walking";
        case HumanoidState::Jumping: return "Jumping";
        case HumanoidState::Freefall: return "Freefall";
        case HumanoidState::Landing: return "Landing";
        case HumanoidState::Dead: return "Dead";
    }
    return "Unknown";
}
void PlayerCamera::reset(Vector3 root, const PlayerSettings& settings) {
    yaw_ = 0; pitch_ = .25f; followDelay_ = 0; shoulderOffset_ = 0;
    wasMouseLocked_=false;lockEntryOffset_={};
    requestedDistance_ = distance_ = settings.cameraDistance;
    target_ = Vector3Add(root, {0, 1.2f, 0});
    previousMode_ = settings.cameraMode;
    firstPerson_ = settings.cameraMode == CameraMode::FirstPerson;
    avatarOccluded_ = false;
    camera_ = {Vector3Add(target_, {0, 3, distance_}), target_, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
}
bool PlayerCamera::setMouseLockShoulderOffset(float value) {
    if(!std::isfinite(value)||std::fabs(value)>5)return false;
    mouseLockShoulderOffset_=value;return true;
}
Vector3 PlayerCamera::moveDirection(Vector2 input) const {
    const float length = Vector2Length(input);
    if (length > 1) input = Vector2Scale(input, 1/length);
    return {std::cos(yaw_) * input.x - std::sin(yaw_) * input.y, 0,
            -std::sin(yaw_) * input.x - std::cos(yaw_) * input.y};
}
void PlayerCamera::processInput(float dt, const PlayerInput& input,const PlayerSettings& settings) {
    if (settings.cameraMode != previousMode_) {
        Event("camera", "mode_change", "", {{"mode", static_cast<int>(settings.cameraMode)}});
        previousMode_ = settings.cameraMode;
    }
    if (input.look.x != 0 || input.look.y != 0) followDelay_ = 1.5f;
    yaw_ -= input.look.x * settings.sensitivity;
    yaw_ = std::remainder(yaw_, 2*PI);
    pitch_ = std::clamp(pitch_ + input.look.y * settings.sensitivity, -1.35f, 1.35f);
    requestedDistance_ = std::clamp(requestedDistance_ - input.zoom * 1.5f, settings.minZoom, settings.maxZoom);
    firstPerson_ = settings.cameraMode == CameraMode::FirstPerson ||
        (settings.allowFirstPerson && requestedDistance_ <= .7f);
    followDelay_ = std::max(0.f, followDelay_ - dt);
}
void PlayerCamera::update(float dt, Vector3 root, Vector3 velocity, const PlayerInput&,
                         const PlayerSettings& settings, const RuntimeSession& runtime) {
    const float horizontal = std::hypot(velocity.x, velocity.z);
    if (settings.cameraMode == CameraMode::Follow && !firstPerson_ && !settings.cameraLock &&
        !runtime.shiftLockActive() && followDelay_ == 0 && horizontal > .5f) {
        const float desiredYaw = std::atan2(-velocity.x, -velocity.z);
        yaw_ += std::remainder(desiredYaw - yaw_, 2*PI) * Response(2, dt);
    }
    const auto bodies = runtime.rig().evaluate(runtime.pose(), {root,runtime.characterRoot().rotation});
    Vector3 desiredTarget = Compose(bodies[Head], {{0,.08f,0}}).position;
    const bool mouseLocked=runtime.shiftLockActive();
    if(mouseLocked) {
        const auto neutral=runtime.rig().evaluate(Pose{},RigidTransform{});
        desiredTarget=Vector3Add(root,{0,neutral[Head].position.y+.08f,0});
    }
    if(mouseLocked) {
        // Position was already interpolated between Bullet ticks. Following
        // another filtered sample makes the avatar bob against its own camera.
        // Only blend the entry discrepancy from the previous free camera.
        if(!wasMouseLocked_)lockEntryOffset_=Vector3Subtract(target_,desiredTarget);
        lockEntryOffset_=Vector3Scale(lockEntryOffset_,std::exp(-14*dt));
        target_=Vector3Add(desiredTarget,lockEntryOffset_);
    }else target_=firstPerson_?desiredTarget:Vector3Lerp(target_,desiredTarget,Response(14,dt));
    wasMouseLocked_=mouseLocked;
    shoulderOffset_=firstPerson_?0.f:Lerp(shoulderOffset_,mouseLocked?mouseLockShoulderOffset_:0.f,Response(12,dt));
    const Vector3 lateral=Vector3Scale(Vector3{std::cos(yaw_),0,-std::sin(yaw_)},shoulderOffset_);
    // The shoulder itself may be across a wall. Sweep from the subject before
    // the usual backwards camera sweep, rather than starting inside/beyond it.
    const float lateralLength=Vector3Length(lateral);
    const float lateralHit=lateralLength>.001f?runtime.cameraClearance(target_,Vector3Add(target_,lateral),.25f):1.f;
    const float lateralFraction=lateralHit<1?std::max(0.f,lateralHit-.02f/lateralLength):1.f;
    const Vector3 aimTarget=Vector3Add(target_,Vector3Scale(lateral,lateralFraction));
    const Vector3 back{std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_),
                       std::cos(yaw_) * std::cos(pitch_)};
    if (!settings.allowFirstPerson && !firstPerson_) {
        float visualMinimum = settings.minZoom;
        for (int i = 0; i < kBodyCount; ++i) {
            const auto inverse = Inverse(bodies[i]);
            const auto local = Compose(inverse,{aimTarget}).position;
            const auto half = Vector3Scale(runtime.rig().bodies[i].size,.5f);
            if (InsideBox(local,half)) visualMinimum = std::max(visualMinimum,
                BoxExit(local,Vector3RotateByQuaternion(back,inverse.rotation),half)+.15f);
        }
        requestedDistance_ = std::max(requestedDistance_,std::min(settings.maxZoom,visualMinimum));
    }
    const float wanted = firstPerson_ ? .05f : requestedDistance_;
    const Vector3 endpoint = Vector3Add(aimTarget, Vector3Scale(back, wanted));
    const float clearance = runtime.cameraClearance(aimTarget, endpoint, .25f);
    const float safe = std::max(.05f, wanted * clearance - (clearance < 1 ? .1f : 0));
    distance_ = safe < distance_ ? safe : Lerp(distance_, safe, Response(8, dt));
    if (firstPerson_) distance_ = .05f;
    camera_.position = Vector3Add(aimTarget, Vector3Scale(back, distance_));
    avatarOccluded_ = false;
    for (int i = 0; i < kBodyCount; ++i) {
        const auto local = Compose(Inverse(bodies[i]),{camera_.position}).position;
        if (InsideBox(local,Vector3Scale(runtime.rig().bodies[i].size,.5f),.08f)) avatarOccluded_ = true;
    }
    camera_.target = Vector3Subtract(camera_.position, back);
    camera_.up = {0, 1, 0}; camera_.fovy = 60; camera_.projection = CAMERA_PERSPECTIVE;
}
RuntimeSession::RuntimeSession() = default;
RuntimeSession::~RuntimeSession() { stop(); }
bool RuntimeSession::start(const Scene& authoring, std::string& error) {
    if (active()) { error = "Play already active"; return false; }
    if (!authoring.validate(error)) return false;
    state_ = RuntimeState::StartingPlay;
    correlation_ = diag::newCorrelationId();
    try {
        scene_ = std::make_unique<Scene>();
        if (!projectio::fromJson(*scene_, projectio::toJson(authoring), error)) { stop(); return false; }
        // Old maps receive a runtime-only R6; authoring is never changed by F5.
        if (scene_->gameAssets().rigs.empty()) scene_->gameAssets().createLinbluxCharacter();
        if (!scene_->gameAssets().validate(error)) { stop(); return false; }
        const auto& assets = scene_->gameAssets();
        const Rig* selected = assets.findRig(assets.defaultRig);
        if (!selected) { error = "Choose a valid default character in Rig Editor"; stop(); return false; }
        rig_ = *selected; settings_ = assets.player;
        // Older whole-rig editing stored translation on RootJoint. Canonicalize
        // only the runtime copy so the visible torso follows the capsule/spawn.
        if(!rig_.recenterAuthoringRoot(error)){stop();return false;}
        Event("animation", "clip_load", correlation_, {{"walk", settings_.walkAnimation}, {"idle", settings_.idleAnimation}});
        physics_ = std::make_unique<PhysicsWorld>(); physics_->attach(*scene_);
        physics_->setGravity({0, -settings_.gravity, 0});
        const auto neutral = rig_.evaluate({}, {});
        float sole = 0;
        for (int b = 0; b < kBodyCount; ++b) {
            for (int corner = 0; corner < 8; ++corner) {
                const Vector3 size = rig_.bodies[b].size;
                Vector3 p{(corner&1 ? 1.f : -1.f)*size.x*.5f, (corner&2 ? 1.f : -1.f)*size.y*.5f,
                          (corner&4 ? 1.f : -1.f)*size.z*.5f};
                sole = std::min(sole, Compose(neutral[b], {p}).position.y);
            }
        }
        rootHeightOffset_ = -settings_.collisionHeight*.5f - sole;
        respawns_ = 0;
        if (!respawn(error)) { stop(); return false; }
        state_ = RuntimeState::Playing;
        const bool spawnedDead=health_<=0;
        scripts_.start(*this); // errors are reported; safe native gameplay remains available
        if(spawnedDead && health_<=0)scripts_.healthChanged(health_,true);
        Event("runtime", "start", correlation_, {{"bodies", bodyCount()}, {"rig", rig_.id}});
        return true;
    } catch (const std::exception& e) { error = std::string("Play startup: ") + e.what(); stop(); return false; }
}
void RuntimeSession::destroyCharacter() {
    if (body_ && physics_) physics_->world_->removeRigidBody(body_.get());
    body_.reset(); motion_.reset(); capsule_.reset();
}
void RuntimeSession::stop() {
    if (!active()) return;
    state_ = RuntimeState::StoppingPlay;
    if (!activeAnimation_.empty()) Event("animation", "stop", correlation_, {{"clip", activeAnimation_}});
    activeAnimation_.clear();
    scripts_.stop();
    destroyCharacter(); physics_.reset(); scene_.reset();
    accumulator_ = 0; jumpPending_ = false; coyoteTime_=jumpBufferTime_=0; pose_ = {};
    blendFrom_ = {}; poseSource_.clear(); poseWalking_ = false; wasAirborne_ = false; blendTime_ = 0;
    shiftLockRequested_ = false; lastShiftKey_ = 0; // Stop nunca deixa o lock preso
    state_ = RuntimeState::Editing;
    Event("runtime", "stop", correlation_);
}
void RuntimeSession::pause() {
    if (state_ != RuntimeState::Playing) return;
    state_ = RuntimeState::Paused; jumpPending_ = false;jumpBufferTime_=0;
    Event("runtime", "pause", correlation_);
}
void RuntimeSession::resume() {
    if (state_ != RuntimeState::Paused) return;
    state_ = RuntimeState::Playing;
    lastShiftKey_=1; // keys held in a menu/another app need a release first
    Event("runtime", "resume", correlation_);
}
bool RuntimeSession::respawn(std::string& error) {
    if (!physics_) { error = "No runtime physics"; return false; }
    if (!settings_.validate(error)) return false;
    const float halfHeight=settings_.collisionHeight*.5f;
    const float spawnMargin=.08f;
    const float voidHeight=scene_->workspace()->fallenPartsDestroyHeight();
    const auto spawnPoints=game::OrderedSpawnPoints(*scene_);
    std::vector<Vector3> starts;
    if(spawnPoints.empty()) {
        // Legacy maps without any spawn marker keep the original deterministic
        // fallback: find the first solid surface on the vertical axis at origin.
        Vector3 start{};
        const btVector3 from(start.x, start.y + 1000, start.z), to(start.x, start.y - 1000, start.z);
        SolidRay ray(from, to, body_.get()); physics_->world_->rayTest(from, to, ray);
        start.y = ray.hasHit() ? ray.m_hitPointWorld.y() + halfHeight + spawnMargin : 20;
        starts.push_back(start);
    } else {
        for(const auto& spawn:spawnPoints) {
            // SpawnTopCenter is the upper surface (not the centre) and accounts
            // for rotation. The capsule starts fully above it with a small gap.
            starts.push_back({spawn.position.x,spawn.position.y+halfHeight+spawnMargin,spawn.position.z});
        }
    }
    auto capsule = std::make_unique<btCapsuleShape>(settings_.collisionRadius,
                           settings_.collisionHeight - 2*settings_.collisionRadius);
    btTransform transform; transform.setIdentity();
    auto motion = std::make_unique<btDefaultMotionState>(transform);
    btVector3 inertia; capsule->calculateLocalInertia(1, inertia);
    btRigidBody::btRigidBodyConstructionInfo info(1, motion.get(), capsule.get(), inertia);
    // Friction is what keeps an idle capsule from sliding down a rotated
    // platform; the controller still owns horizontal velocity while walking.
    info.m_friction = 1.0f; info.m_restitution = 0;
    auto body = std::make_unique<btRigidBody>(info);
    body->setAngularFactor({0, 0, 0}); body->setActivationState(DISABLE_DEACTIVATION);
    body->setCcdMotionThreshold(.2f); body->setCcdSweptSphereRadius(settings_.collisionRadius * .8f);
    // A spawn is accepted only when the capsule is fully clear at the computed
    // surface offset. Lifting an obstructed capsule can tunnel it through a
    // ceiling, so try the next candidate instead.
    bool clear = false;
    std::size_t belowFloor = 0;
    for(const Vector3& start:starts) {
        if(start.y-halfHeight<=voidHeight) { ++belowFloor; continue; }
        transform.setOrigin(Bt(start)); body->setWorldTransform(transform);
        Overlap overlap; overlap.ignored=body_.get(); physics_->world_->contactTest(body.get(),overlap);
        if(!overlap.overlapping) { clear=true; break; }
    }
    if(!clear) {
        error = belowFloor==starts.size()
            ? "Spawn is below FallenPartsDestroyHeight; adjust Workspace or spawn"
            : spawnPoints.empty() ? "Spawn is obstructed: cannot place character capsule"
                                  : "All spawn points are obstructed; cannot place character capsule";
        return false;
    }
    // Only replace the old avatar after the candidate is collision-safe.
    motion->setWorldTransform(body->getWorldTransform());
    destroyCharacter();
    capsule_ = std::move(capsule); motion_ = std::move(motion); body_ = std::move(body);
    physics_->world_->addRigidBody(body_.get());
    pose_ = {}; walkTime_ = 0; idleTime_ = 0; airTime_=0; walkWeight_ = 0; accumulator_ = 0; yaw_ = 0; jumpPending_ = false;
    blendFrom_ = {}; poseSource_.clear(); poseWalking_ = false; wasAirborne_ = false; blendTime_ = 0;
    previousCharacterPosition_=position();
    coyoteTime_=jumpBufferTime_=0;
    // Shift Lock is a player choice, not a property of the old character.
    // Keep it across respawns, but swallow a key already held during respawn.
    lastShiftKey_ = 1;
    health_=respawns_==0?settings_.health:settings_.maxHealth;deadTime_=damageCooldown_=0;
    humanoidState_ = health_>0?HumanoidState::Idle:HumanoidState::Dead;
    camera_.reset(characterRoot().position, settings_);
    camera_.update(1, characterRoot().position, {}, {}, settings_, *this);
    Event("player", respawns_++ ? "respawn" : "spawn", correlation_, {{"x", position().x}, {"y", position().y}, {"z", position().z}});
    if (scripts_.active()) scripts_.characterAdded();
    error.clear(); return true;
}
Vector3 RuntimeSession::position() const { return body_ ? Vec(body_->getWorldTransform().getOrigin()) : Vector3{}; }
bool RuntimeSession::shiftLockActive() const {
    return settings_.enableShiftLock && shiftLockRequested_ && state_ == RuntimeState::Playing;
}
Vector3 RuntimeSession::velocity() const { return body_ ? Vec(body_->getLinearVelocity()) : Vector3{}; }
RigidTransform RuntimeSession::characterRoot() const {
    return {Vector3Add(position(), {0, rootHeightOffset_, 0}), QuaternionFromEuler(0, yaw_, 0)};
}
RigidTransform RuntimeSession::renderCharacterRoot() const {
    auto root=characterRoot();
    root.position=Vector3Add(Vector3Lerp(previousCharacterPosition_,position(),
        std::clamp(accumulator_*60.f,0.f,1.f)),{0,rootHeightOffset_,0});
    return root;
}
bool RuntimeSession::grounded() const {
    if (!body_ || velocity().y > 1) return false;
    // A capsule can stand on a platform edge with no ground under its centre
    // ray. Use Bullet's actual supporting contacts before the proximity ray.
    for(int i=0;i<physics_->dispatcher_->getNumManifolds();++i) {
        const auto* manifold=physics_->dispatcher_->getManifoldByIndexInternal(i);
        const bool first=manifold->getBody0()==body_.get();
        if(!first && manifold->getBody1()!=body_.get())continue;
        const auto* other=first?manifold->getBody1():manifold->getBody0();
        if(!other->hasContactResponse())continue;
        for(int j=0;j<manifold->getNumContacts();++j) {
            const auto& contact=manifold->getContactPoint(j);
            const auto normal=first?contact.m_normalWorldOnB:-contact.m_normalWorldOnB;
            const auto point=first?contact.getPositionWorldOnA():contact.getPositionWorldOnB();
            if(contact.getDistance()<=.04f && normal.y()>=.65f &&
               point.y()<body_->getWorldTransform().getOrigin().y()-settings_.collisionHeight*.25f)return true;
        }
    }
    const Vector3 p = position();
    const btVector3 from = Bt(p), to = Bt(Vector3Add(p, {0, -settings_.collisionHeight*.5f - .12f, 0}));
    SolidRay ray(from, to, body_.get()); physics_->world_->rayTest(from, to, ray);
    return ray.hasHit() && ray.m_hitNormalWorld.y() >= .65f;
}
void RuntimeSession::transition(HumanoidState state) {
    if (state == humanoidState_) return;
    Event("player", "move_state", correlation_, {{"previous", StateName(humanoidState_)}, {"state", StateName(state)}});
    humanoidState_ = state;
}
void RuntimeSession::preventFallThrough(float dt) {
    if (!body_ || !capsule_ || !physics_ || dt <= 0) return;
    const btVector3 velocity = body_->getLinearVelocity();
    const float travel = velocity.y() * dt;
    if (travel >= -.0001f) return;
    const btTransform from = body_->getWorldTransform();
    btTransform to = from;
    to.getOrigin().setY(from.getOrigin().y() + travel);
    DownSweep hit(from.getOrigin(), to.getOrigin(), body_.get());
    physics_->world_->convexSweepTest(capsule_.get(), from, to, hit);
    if (!hit.hasHit() || hit.m_closestHitFraction >= 1.0f) return;
    const float fraction = std::clamp(hit.m_closestHitFraction, 0.0f, 1.0f);
    const float allowed = travel * fraction;
    const float nudge = fraction > 0 ? .001f : 0;
    btTransform stopped = from;
    stopped.getOrigin().setY(from.getOrigin().y() + allowed + nudge);
    body_->setWorldTransform(stopped);
    body_->setInterpolationWorldTransform(stopped);
    if (motion_) motion_->setWorldTransform(stopped);
    btVector3 clamped = velocity;
    clamped.setY(0);
    body_->setLinearVelocity(clamped);
    physics_->world_->updateSingleAabb(body_.get());
}
void RuntimeSession::step(float dt, Vector3 desired, bool jump) {
    if (health_<=0) {
        auto v=body_->getLinearVelocity();v.setX(0);v.setZ(0);body_->setLinearVelocity(v);
        physics_->world_->stepSimulation(dt,0);physics_->syncFromBodies(*scene_);return;
    }
    const bool wasGrounded = grounded();
    const Vector3 before = position();
    Vector3 speed = velocity();
    const Vector3 wanted = Vector3Scale(desired, settings_.walkSpeed);
    Vector3 difference{wanted.x - speed.x, 0, wanted.z - speed.z};
    const float change = (Vector3LengthSqr(desired) > 0 ? settings_.acceleration : settings_.deceleration) * dt;
    if (Vector3Length(difference) > change) difference = Vector3Scale(Vector3Normalize(difference), change);
    speed.x += difference.x; speed.z += difference.z;

    // Coyote time: tempo de graça após sair do chão
    if (wasGrounded) {
        coyoteTime_ = 0.1f; // 100ms de graça após sair do chão
    } else {
        coyoteTime_ = std::max(0.f, coyoteTime_ - dt);
    }

    // Jump buffer: armazena intenção de pular enquanto no ar
    if (jump) {
        jumpBufferTime_ = 0.15f; // 150ms de buffer
    } else {
        jumpBufferTime_ = std::max(0.f, jumpBufferTime_ - dt);
    }

    // Pulo: permite pular se estava no chão (coyote time) ou se há buffer de pulo
    const bool canJump = coyoteTime_ > 0;
    bool jumped = false;
    const auto launch = [&] {
        speed.y = std::sqrt(2 * settings_.gravity * settings_.jumpHeight);
        body_->setLinearVelocity(Bt(speed));
        Event("player", "jump", correlation_);
        coyoteTime_ = 0;
        jumpBufferTime_ = 0;
        jumped = true;
    };
    if (jumpBufferTime_ > 0 && canJump) launch();
    body_->setLinearVelocity(Bt(speed));
    // Full walking speed while the controller has input; friction only holds
    // an idle capsule in place on slopes.
    body_->setFriction(std::hypot(desired.x, desired.z) > .01f ? 0.0f : 1.0f);
    physics_->syncScene(*scene_);
    if (!jumped) preventFallThrough(dt);
    physics_->world_->stepSimulation(dt, 0); // exactly one fixed step; no second accumulator
    physics_->syncFromBodies(*scene_);
    const Vector3 actual = Vector3Scale(Vector3Subtract(position(), before), 1/dt);
    const float horizontal = std::hypot(actual.x, actual.z);
    bool onGround = grounded();
    // Landing can happen inside this very physics step. Consume the buffered
    // edge now, not one tick later; never apply a second impulse in this step.
    if(onGround && !jumped && jumpBufferTime_>0) {
        speed=velocity();launch();onGround=false;
    }
    if (!onGround) transition(velocity().y > 1 ? HumanoidState::Jumping : HumanoidState::Freefall);
    else if (!wasGrounded) transition(HumanoidState::Landing);
    else transition(horizontal > .2f ? HumanoidState::Walking : HumanoidState::Idle);
    if (!(settings_.cameraLock || shiftLockActive() || camera_.firstPerson()) && settings_.autoRotate && horizontal > .2f) {
        const float target = std::atan2(-actual.x, -actual.z);
        yaw_ += std::remainder(target-yaw_, 2*PI) * Response(settings_.turnSpeed, dt);
    }
    animate(dt, horizontal);
    if (position().y < scene_->workspace()->fallenPartsDestroyHeight()) {
        std::string error;setHealth(0,error);
    }
    if (damageCooldown_<=0 && health_>0) {
        float damage=0;
        for(int i=0;i<physics_->dispatcher_->getNumManifolds();++i) {
            const auto* contact=physics_->dispatcher_->getManifoldByIndexInternal(i);
            const auto* other=contact->getBody0()==body_.get()?contact->getBody1():
                contact->getBody1()==body_.get()?contact->getBody0():nullptr;
            if(!other || !other->hasContactResponse())continue;
            const auto* part=static_cast<const Part*>(other->getUserPointer());
            if(!part || scene_->findPart(part->id())!=part)continue;
            bool touching=false;for(int j=0;j<contact->getNumContacts();++j)touching|=contact->getContactPoint(j).getDistance()<=.02f;
            if(!touching)continue;
            auto it=part->attributes().find("Damage");
            if(it!=part->attributes().end()) if(const auto* amount=std::get_if<double>(&it->second))
                damage=std::max(damage,float(std::clamp(*amount,0.,100000.)));
        }
        if(damage>0) { std::string error;takeDamage(damage,error);damageCooldown_=.75f; }
    }
}
bool RuntimeSession::setHealth(float value,std::string& error) {
    if(!active() || !std::isfinite(value)){error="Health must be finite during Play";return false;}
    value=std::clamp(value,0.f,settings_.maxHealth);
    if(health_<=0 && value>0){error="Dead character must respawn before healing";return false;}
    if(value==health_){error.clear();return true;}
    const bool died=value==0 && health_>0;health_=value;
    if(died){
        transition(HumanoidState::Dead);pose_={};blendFrom_={};poseSource_.clear();poseWalking_=false;wasAirborne_=false;blendTime_=0;deadTime_=0;jumpPending_=false;
        if(!activeAnimation_.empty())Event("animation","stop",correlation_,{{"clip",activeAnimation_}});
        activeAnimation_.clear();
    }
    Event("humanoid",died?"died":"health_changed",correlation_,{{"health",health_},{"maxHealth",settings_.maxHealth}});
    scripts_.healthChanged(health_,died);error.clear();return true;
}
bool RuntimeSession::takeDamage(float amount,std::string& error) {
    if(!std::isfinite(amount)||amount<0){error="Damage must be finite and nonnegative";return false;}
    return setHealth(health_-amount,error);
}
bool RuntimeSession::heal(float amount,std::string& error) {
    if(!std::isfinite(amount)||amount<0){error="Healing must be finite and nonnegative";return false;}
    return setHealth(health_+amount,error);
}
bool RuntimeSession::resetCharacter(std::string& error) {
    if(!setHealth(0,error))return false;
    // Menu reset is immediate; the same health/death transition is used.
    return respawn(error);
}
void RuntimeSession::animate(float dt, float horizontal) {
    const auto& assets = scene_->gameAssets();
    const auto* idle = assets.findAnimation(settings_.idleAnimation);
    const auto* walk = assets.findAnimation(settings_.walkAnimation);
    const bool walking = humanoidState_ == HumanoidState::Walking;
    const bool jumping = humanoidState_ == HumanoidState::Jumping;
    const bool falling = humanoidState_ == HumanoidState::Freefall;
    const bool airborne = jumping || falling;
    static const std::string airPose = "R6_AirPose";
    // A new take-off always starts its clip at the entry pose. A landing jump
    // can go from grounded to Jumping inside one fixed step, so waiting for a
    // clip change is not enough: the previous take-off's time would leak into
    // the new one and every buffered jump would replay a later slice.
    if (airborne && !wasAirborne_) airTime_ = 0;
    wasAirborne_ = airborne;
    std::string desiredClip;
    if (falling) {
        desiredClip = settings_.fallAnimation.empty()?"R6_NeutralFall":settings_.fallAnimation;
    } else if (jumping) {
        desiredClip = settings_.jumpAnimation.empty() ? airPose : settings_.jumpAnimation;
    } else if (walking) {
        desiredClip = settings_.walkAnimation;
    } else {
        desiredClip = settings_.idleAnimation;
    }
    if (activeAnimation_ != desiredClip) {
        if (airborne) airTime_ = 0;
        if (!activeAnimation_.empty()) Event("animation", "stop", correlation_, {{"clip", activeAnimation_}});
        activeAnimation_ = desiredClip;
        Event("animation", "play", correlation_, {{"clip", activeAnimation_}});
    }
    walkWeight_ = Lerp(walkWeight_, walking ? 1.f : 0.f, Response(14, dt));
    if (walking) walkTime_ += dt * std::clamp(horizontal / 16.f, 0.f, 4.f);
    if (!airborne) idleTime_ += dt;
    else airTime_ += dt;
    Pose target = BlendPose(idle ? idle->sample(idleTime_) : Pose{}, walk ? walk->sample(walkTime_) : Pose{}, walkWeight_);
    if (airborne) {
        target = {};
        if (falling) {
            if (const auto* fall = assets.findAnimation(settings_.fallAnimation)) target = fall->sample(airTime_);
        } else if (jumping) {
            if (const auto* jump = assets.findAnimation(settings_.jumpAnimation)) target = jump->sample(airTime_);
            else {
                target[LeftShoulder].rotation=QuaternionFromEuler(0,0,-.35f);
                target[RightShoulder].rotation=QuaternionFromEuler(0,0,.35f);
                target[LeftHip].rotation=QuaternionFromEuler(-.15f,0,0);
                target[RightHip].rotation=QuaternionFromEuler(.15f,0,0);
            }
        }
    }
    // Deterministic playback: the pose is always rebuilt from a fixed entry
    // pose and the clip's own sample. After the short crossfade the visible
    // pose equals the sampled keyframes exactly, so replaying Jump uses the
    // same path every time and never accumulates the previous jump's error.
    constexpr float kPoseBlendSeconds = .12f;
    if (desiredClip != poseSource_ || walking != poseWalking_) {
        blendFrom_ = pose_;
        poseSource_ = desiredClip;
        poseWalking_ = walking;
        blendTime_ = 0;
    }
    blendTime_ = std::isfinite(blendTime_) ? std::min(blendTime_ + std::max(dt, 0.f), kPoseBlendSeconds)
                                           : kPoseBlendSeconds;
    const float t = std::clamp(blendTime_ / kPoseBlendSeconds, 0.f, 1.f);
    pose_ = BlendPose(blendFrom_, target, t * t * (3 - 2 * t));
}
bool RuntimeSession::setAnimation(const std::string& state,const std::string& id,std::string& error) {
    const auto* clip=scene_?scene_->gameAssets().findAnimation(id):nullptr;
    if (!clip || clip->rigType!="R6" || !clip->validate(error)) {
        error="Animation asset missing, invalid or incompatible with R6: "+id;return false;
    }
    const std::string uri="anim://"+clip->id;
    if (state=="Idle") settings_.idleAnimation=uri;
    else if (state=="Walk") settings_.walkAnimation=uri;
    else if (state=="Jump") settings_.jumpAnimation=uri;
    else if (state=="Fall") settings_.fallAnimation=uri;
    else {error="Animation state must be Idle, Walk, Jump or Fall";return false;}
    error.clear();return true;
}
void RuntimeSession::update(float dt, const PlayerInput& input) {
    if (state_ != RuntimeState::Playing || !body_ || !std::isfinite(dt) || dt < 0) return;
    std::string settingsError;
    if (!settings_.validate(settingsError)) {
        diag::Event event; event.level = diag::Level::Error;
        event.subsystem = "runtime"; event.name = "validation_error";
        event.correlationId = correlation_; event.error = settingsError; event.result = "rejected";
        diag::log(event);
        pause(); return;
    }
    if (!std::isfinite(input.movement.x) || !std::isfinite(input.movement.y) ||
        !std::isfinite(input.look.x) || !std::isfinite(input.look.y) || !std::isfinite(input.zoom)) return;
    dt = std::min(dt, .1f);
    // Toggle do Shift Lock (Left Shift) detectado por borda; ignorado sem a
    // opção habilitada e nunca dispara no editor (route só roteia gameplay).
    const bool shiftDown = input.shiftKey;
    if (settings_.enableShiftLock && shiftDown && !lastShiftKey_)
        shiftLockRequested_ = !shiftLockRequested_;
    lastShiftKey_ = shiftDown ? 1 : 0;
    if (suppressJump_ && !input.jump) suppressJump_ = false;
    const bool jumpPressed = input.jump && !lastJumpDown_ && !suppressJump_;
    lastJumpDown_ = input.jump;
    scripts_.update(dt);
    physics_->syncScene(*scene_);
    damageCooldown_=std::max(0.f,damageCooldown_-dt);
    if(health_<=0) {
        deadTime_+=dt;
        if(deadTime_>=settings_.respawnDelay) { std::string error;if(!respawn(error))pause(); }
    }
    camera_.processInput(dt,input,settings_);
    const Vector3 desired = camera_.moveDirection(input.movement);
    jumpPending_ = jumpPending_ || jumpPressed;
    accumulator_ += dt;
    constexpr float fixed = 1.f / 60;
    int steps = 0;
    while (accumulator_ + .000001f >= fixed && steps++ < 6) {
        previousCharacterPosition_=position();
        step(fixed, desired, jumpPending_); jumpPending_ = false;
        accumulator_ = std::max(0.f, accumulator_ - fixed);
        if (state_ != RuntimeState::Playing) break;
    }
    // One facing owner in locked modes, at display frequency. A 144/240 Hz
    // camera must not wait for the next 60 Hz physics tick to turn the body.
    if(settings_.cameraLock || shiftLockActive() || camera_.firstPerson())yaw_=camera_.yaw();
    if(state_==RuntimeState::Playing)camera_.update(dt,renderCharacterRoot().position,velocity(),input,settings_,*this);
    std::vector<InstanceId> fallen;
    for(auto* part:scene_->parts()) if(!part->anchored() && part->deletable() && part->position().y<scene_->workspace()->fallenPartsDestroyHeight())fallen.push_back(part->id());
    for(auto id:fallen)scene_->removeById(id);
}
float RuntimeSession::cameraClearance(Vector3 from, Vector3 to, float radius) const {
    if (!physics_ || Vector3Distance(from, to) < .001f) return 1;
    btSphereShape sphere(radius);
    btTransform a, b; a.setIdentity(); b.setIdentity(); a.setOrigin(Bt(from)); b.setOrigin(Bt(to));
    SolidSweep hit(a.getOrigin(), b.getOrigin(), body_.get());
    physics_->world_->convexSweepTest(&sphere, a, b, hit);
    return hit.hasHit() ? hit.m_closestHitFraction : 1.f;
}
bool RuntimeSession::partMotion(InstanceId id,Vector3 value,PartMotion operation,std::string& error) {
    auto* part=scene_?scene_->findPart(id):nullptr;
    if(!physics_||!part||part->anchored()||!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)||
       std::fabs(value.x)>1000000||std::fabs(value.y)>1000000||std::fabs(value.z)>1000000) {
        error="Movimento físico requer uma peça dinâmica e um vetor finito no intervalo suportado.";return false;
    }
    physics_->syncScene(*scene_);
    const auto found=physics_->bodies_.find(id);
    if(found==physics_->bodies_.end()||found->second.body->isStaticOrKinematicObject()){error="Corpo dinâmico indisponível.";return false;}
    auto* body=found->second.body;
    switch(operation) {
        case PartMotion::Velocity:body->setLinearVelocity(Bt(value));break;
        case PartMotion::Force:body->applyCentralForce(Bt(value));break;
        case PartMotion::Impulse:body->applyCentralImpulse(Bt(value));break;
        default:error="Operação física desconhecida.";return false;
    }
    body->activate(true);error.clear();return true;
}
Vector3 RuntimeSession::partVelocity(InstanceId id) {
    if(!physics_||!scene_)return {};
    physics_->syncScene(*scene_);return physics_->linearVelocity(id);
}
bool RuntimeSession::setGravity(float magnitude,std::string& error) {
    if(!physics_){error="Física da sessão indisponível.";return false;}
    auto candidate=settings_;candidate.gravity=magnitude;
    if(!candidate.validate(error))return false;
    settings_=candidate;physics_->setGravity({0,-magnitude,0});error.clear();return true;
}
bool RuntimeSession::raycastParts(Vector3 origin,Vector3 direction,float distance,Part*& object,
                                 Vector3& point,Vector3& normal,InstanceId ignored) {
    object=nullptr;
    if(!physics_||!scene_||!std::isfinite(distance)||distance<=0||distance>1000000||
       !std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z)||
       !std::isfinite(direction.x)||!std::isfinite(direction.y)||!std::isfinite(direction.z)||Vector3LengthSqr(direction)<1e-12f)return false;
    physics_->syncScene(*scene_);
    const auto from=Bt(origin),to=Bt(Vector3Add(origin,Vector3Scale(Vector3Normalize(direction),distance)));
    struct Ray final:btCollisionWorld::ClosestRayResultCallback {
        InstanceId ignored;
        Ray(btVector3 from,btVector3 to,InstanceId skip):ClosestRayResultCallback(from,to),ignored(skip){}
        bool needsCollision(btBroadphaseProxy* proxy) const override {
            const auto* body=static_cast<const btCollisionObject*>(proxy->m_clientObject);
            const auto* part=static_cast<const Part*>(body->getUserPointer());
            return part&&part->id()!=ignored&&ClosestRayResultCallback::needsCollision(proxy);
        }
    } result(from,to,ignored);
    physics_->world_->rayTest(from,to,result);if(!result.hasHit())return false;
    object=static_cast<Part*>(result.m_collisionObject->getUserPointer());
    point=Vec(result.m_hitPointWorld);normal=Vec(result.m_hitNormalWorld);return object!=nullptr;
}
std::size_t RuntimeSession::bodyCount() const {
    return physics_ ? static_cast<std::size_t>(physics_->world_->getNumCollisionObjects()) : 0;
}
bool RuntimeSession::validate(std::string& error) const {
    if (!active()) {
        if (body_ || physics_ || scene_) { error = "runtime resources survived Stop"; return false; }
        error.clear(); return true;
    }
    if (!scene_ || !physics_ || !body_ || bodyCount() != scene_->parts().size()+1 ||
        !Valid(characterRoot()) || !std::isfinite(health_) || health_<0 || health_>settings_.maxHealth) { error = "runtime: missing/invalid character, health or orphan body"; return false; }
    return scene_->validate(error) && physics_->validate(*scene_, error);
}
} // namespace loblox::game

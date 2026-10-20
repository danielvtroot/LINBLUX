#include "game/Runtime.h"

#include <algorithm>
#include <cmath>
#include <btBulletDynamicsCommon.h>
#include "core/Diagnostics.h"
#include "serialization/ProjectIO.h"

namespace loblox::game {
namespace {
btVector3 Bt(Vector3 v) { return {v.x, v.y, v.z}; }
Vector3 Vec(const btVector3& v) { return {v.x(), v.y(), v.z()}; }
float Response(float rate, float dt) { return 1 - std::exp(-rate * dt); }
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
struct Overlap : btCollisionWorld::ContactResultCallback {
    bool overlapping = false;
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
    }
    return "Unknown";
}
void PlayerCamera::reset(Vector3 root, const PlayerSettings& settings) {
    yaw_ = 0; pitch_ = .25f; followDelay_ = 0;
    requestedDistance_ = distance_ = settings.cameraDistance;
    target_ = Vector3Add(root, {0, 1.2f, 0});
    previousMode_ = settings.cameraMode;
    firstPerson_ = settings.cameraMode == CameraMode::FirstPerson;
    camera_ = {Vector3Add(target_, {0, 3, distance_}), target_, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
}
Vector3 PlayerCamera::moveDirection(Vector2 input) const {
    const float length = Vector2Length(input);
    if (length > 1) input = Vector2Scale(input, 1/length);
    return {std::cos(yaw_) * input.x - std::sin(yaw_) * input.y, 0,
            -std::sin(yaw_) * input.x - std::cos(yaw_) * input.y};
}
void PlayerCamera::update(float dt, Vector3 root, Vector3 velocity, const PlayerInput& input,
                         const PlayerSettings& settings, const RuntimeSession& runtime) {
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
    const float horizontal = std::hypot(velocity.x, velocity.z);
    if (settings.cameraMode == CameraMode::Follow && !firstPerson_ && !settings.cameraLock &&
        followDelay_ == 0 && horizontal > .5f) {
        const float desiredYaw = std::atan2(-velocity.x, -velocity.z);
        yaw_ += std::remainder(desiredYaw - yaw_, 2*PI) * Response(2, dt);
    }
    const Vector3 desiredTarget = Vector3Add(root, {0, 1.2f, 0});
    target_ = firstPerson_ ? desiredTarget : Vector3Lerp(target_, desiredTarget, Response(14, dt));
    const Vector3 back{std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_),
                       std::cos(yaw_) * std::cos(pitch_)};
    const float wanted = firstPerson_ ? .05f : requestedDistance_;
    const Vector3 endpoint = Vector3Add(target_, Vector3Scale(back, wanted));
    const float clearance = runtime.cameraClearance(target_, endpoint, .25f);
    const float safe = std::max(.05f, wanted * clearance - (clearance < 1 ? .1f : 0));
    distance_ = safe < distance_ ? safe : Lerp(distance_, safe, Response(8, dt));
    if (firstPerson_) distance_ = .05f;
    camera_.position = Vector3Add(target_, Vector3Scale(back, distance_));
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
        if (scene_->gameAssets().rigs.empty()) scene_->gameAssets().createR6();
        if (!scene_->gameAssets().validate(error)) { stop(); return false; }
        const auto& assets = scene_->gameAssets();
        const Rig* selected = assets.findRig(assets.defaultRig);
        if (!selected) { error = "Choose a valid default character in Rig Editor"; stop(); return false; }
        rig_ = *selected; settings_ = assets.player;
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
    destroyCharacter(); physics_.reset(); scene_.reset();
    accumulator_ = 0; jumpPending_ = false; pose_ = {};
    state_ = RuntimeState::Editing;
    Event("runtime", "stop", correlation_);
}
void RuntimeSession::pause() {
    if (state_ != RuntimeState::Playing) return;
    state_ = RuntimeState::Paused; accumulator_ = 0; jumpPending_ = false;
    Event("runtime", "pause", correlation_);
}
void RuntimeSession::resume() {
    if (state_ != RuntimeState::Paused) return;
    state_ = RuntimeState::Playing;
    Event("runtime", "resume", correlation_);
}
bool RuntimeSession::respawn(std::string& error) {
    if (!physics_) { error = "No runtime physics"; return false; }
    destroyCharacter();
    Part* spawn = nullptr;
    for (Part* part : scene_->parts()) if (part->instanceClass() == InstanceClass::SpawnLocation &&
        part->canCollide() && (!spawn || part->id() < spawn->id())) spawn = part;
    Vector3 start = spawn ? spawn->position() : Vector3{};
    // Vertical query chooses a solid surface, including obstacles above a spawn.
    const btVector3 from(start.x, start.y + 1000, start.z), to(start.x, start.y - 1000, start.z);
    SolidRay ray(from, to, nullptr); physics_->world_->rayTest(from, to, ray);
    start.y = ray.hasHit() ? ray.m_hitPointWorld.y() + settings_.collisionHeight*.5f + .08f : 20;
    capsule_ = std::make_unique<btCapsuleShape>(settings_.collisionRadius,
                           settings_.collisionHeight - 2*settings_.collisionRadius);
    btTransform transform; transform.setIdentity(); transform.setOrigin(Bt(start));
    motion_ = std::make_unique<btDefaultMotionState>(transform);
    btVector3 inertia; capsule_->calculateLocalInertia(1, inertia);
    btRigidBody::btRigidBodyConstructionInfo info(1, motion_.get(), capsule_.get(), inertia);
    info.m_friction = 0; info.m_restitution = 0;
    body_ = std::make_unique<btRigidBody>(info);
    body_->setAngularFactor({0, 0, 0}); body_->setActivationState(DISABLE_DEACTIVATION);
    body_->setCcdMotionThreshold(.2f); body_->setCcdSweptSphereRadius(settings_.collisionRadius * .8f);
    // Resolve a cramped spawn deterministically, never teleport a running body each frame.
    bool clear = false;
    for (int attempt = 0; attempt < 64; ++attempt) {
        Overlap overlap; physics_->world_->contactTest(body_.get(), overlap);
        if (!overlap.overlapping) { clear = true; break; }
        transform.getOrigin().setY(transform.getOrigin().y()+.5f); body_->setWorldTransform(transform);
    }
    if (!clear) { destroyCharacter(); error = "Spawn is obstructed: cannot place character capsule"; return false; }
    motion_->setWorldTransform(body_->getWorldTransform());
    physics_->world_->addRigidBody(body_.get());
    pose_ = {}; walkTime_ = 0; idleTime_ = 0; walkWeight_ = 0; accumulator_ = 0; yaw_ = 0; jumpPending_ = false;
    humanoidState_ = HumanoidState::Idle;
    camera_.reset(characterRoot().position, settings_);
    camera_.update(1, characterRoot().position, {}, {}, settings_, *this);
    Event("player", respawns_++ ? "respawn" : "spawn", correlation_, {{"x", position().x}, {"y", position().y}, {"z", position().z}});
    error.clear(); return true;
}
Vector3 RuntimeSession::position() const { return body_ ? Vec(body_->getWorldTransform().getOrigin()) : Vector3{}; }
Vector3 RuntimeSession::velocity() const { return body_ ? Vec(body_->getLinearVelocity()) : Vector3{}; }
RigidTransform RuntimeSession::characterRoot() const {
    return {Vector3Add(position(), {0, rootHeightOffset_, 0}), QuaternionFromEuler(0, yaw_, 0)};
}
bool RuntimeSession::grounded() const {
    if (!body_ || velocity().y > 1) return false;
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
void RuntimeSession::step(float dt, Vector3 desired, bool jump) {
    const bool wasGrounded = grounded();
    const Vector3 before = position();
    Vector3 speed = velocity();
    const Vector3 wanted = Vector3Scale(desired, settings_.walkSpeed);
    Vector3 difference{wanted.x - speed.x, 0, wanted.z - speed.z};
    const float change = (Vector3LengthSqr(desired) > 0 ? settings_.acceleration : settings_.deceleration) * dt;
    if (Vector3Length(difference) > change) difference = Vector3Scale(Vector3Normalize(difference), change);
    speed.x += difference.x; speed.z += difference.z;
    if (jump && wasGrounded) {
        speed.y = std::sqrt(2 * settings_.gravity * settings_.jumpHeight);
        Event("player", "jump", correlation_);
    }
    body_->setLinearVelocity(Bt(speed));
    physics_->syncScene(*scene_);
    physics_->world_->stepSimulation(dt, 0); // exactly one fixed step; no second accumulator
    physics_->syncFromBodies(*scene_);
    const Vector3 actual = Vector3Scale(Vector3Subtract(position(), before), 1/dt);
    const float horizontal = std::hypot(actual.x, actual.z);
    const bool onGround = grounded();
    if (!onGround) transition(velocity().y > 1 ? HumanoidState::Jumping : HumanoidState::Freefall);
    else if (!wasGrounded) transition(HumanoidState::Landing);
    else transition(horizontal > .2f ? HumanoidState::Walking : HumanoidState::Idle);
    if (settings_.cameraLock || camera_.firstPerson()) yaw_ = camera_.yaw();
    else if (settings_.autoRotate && horizontal > .2f) {
        const float target = std::atan2(-actual.x, -actual.z);
        yaw_ += std::remainder(target-yaw_, 2*PI) * Response(settings_.turnSpeed, dt);
    }
    animate(dt, horizontal);
    if (position().y < -500) { std::string error; if (!respawn(error)) pause(); }
}
void RuntimeSession::animate(float dt, float horizontal) {
    const auto& assets = scene_->gameAssets();
    const auto* idle = assets.findAnimation(settings_.idleAnimation);
    const auto* walk = assets.findAnimation(settings_.walkAnimation);
    const bool walking = humanoidState_ == HumanoidState::Walking;
    const bool airborne = humanoidState_ == HumanoidState::Jumping || humanoidState_ == HumanoidState::Freefall;
    static const std::string airPose = "R6_AirPose";
    const std::string& desiredClip = airborne ? airPose : walking ? settings_.walkAnimation : settings_.idleAnimation;
    if (activeAnimation_ != desiredClip) {
        if (!activeAnimation_.empty()) Event("animation", "stop", correlation_, {{"clip", activeAnimation_}});
        activeAnimation_ = desiredClip;
        Event("animation", "play", correlation_, {{"clip", activeAnimation_}});
    }
    walkWeight_ = Lerp(walkWeight_, walking ? 1.f : 0.f, Response(14, dt));
    if (walking) walkTime_ += dt * std::clamp(horizontal / 16.f, 0.f, 4.f);
    if (!airborne) idleTime_ += dt;
    Pose target = BlendPose(idle ? idle->sample(idleTime_) : Pose{}, walk ? walk->sample(walkTime_) : Pose{}, walkWeight_);
    if (airborne) {
        target = {};
        target[LeftShoulder].rotation = QuaternionFromEuler(0, 0, -.35f);
        target[RightShoulder].rotation = QuaternionFromEuler(0, 0, .35f);
        target[LeftHip].rotation = QuaternionFromEuler(-.15f, 0, 0);
        target[RightHip].rotation = QuaternionFromEuler(.15f, 0, 0);
    }
    pose_ = BlendPose(pose_, target, Response(20, dt));
}
void RuntimeSession::update(float dt, const PlayerInput& input) {
    if (state_ != RuntimeState::Playing || !body_ || !std::isfinite(dt) || dt < 0) return;
    dt = std::min(dt, .1f);
    camera_.update(dt, characterRoot().position, velocity(), input, settings_, *this);
    const Vector3 desired = camera_.moveDirection(input.movement);
    jumpPending_ = jumpPending_ || input.jump;
    accumulator_ += dt;
    constexpr float fixed = 1.f / 60;
    int steps = 0;
    while (accumulator_ + .000001f >= fixed && steps++ < 6) {
        step(fixed, desired, jumpPending_); jumpPending_ = false;
        accumulator_ = std::max(0.f, accumulator_ - fixed);
        if (state_ != RuntimeState::Playing) break;
    }
}
float RuntimeSession::cameraClearance(Vector3 from, Vector3 to, float radius) const {
    if (!physics_ || Vector3Distance(from, to) < .001f) return 1;
    btSphereShape sphere(radius);
    btTransform a, b; a.setIdentity(); b.setIdentity(); a.setOrigin(Bt(from)); b.setOrigin(Bt(to));
    SolidSweep hit(a.getOrigin(), b.getOrigin(), body_.get());
    physics_->world_->convexSweepTest(&sphere, a, b, hit);
    return hit.hasHit() ? hit.m_closestHitFraction : 1.f;
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
        !Valid(characterRoot())) { error = "runtime: missing/invalid character or orphan body"; return false; }
    return scene_->validate(error) && physics_->validate(*scene_, error);
}
} // namespace loblox::game

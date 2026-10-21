#pragma once

#include <memory>
#include "game/Rig.h"
#include "scripting/LuaRuntime.h"
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"

class btCapsuleShape;
class btRigidBody;
class btDefaultMotionState;

namespace loblox::game {
enum class RuntimeState { Editing, StartingPlay, Playing, Paused, StoppingPlay };
enum class HumanoidState { Idle, Walking, Jumping, Freefall, Landing, Dead };
const char* StateName(HumanoidState state);
struct PlayerInput {
    Vector2 movement{}; // X=right, Y=forward, relative to horizontal camera
    Vector2 look{};     // mouse pixels; apply once per display frame
    float zoom = 0;
    bool jump = false;
};

class RuntimeSession;
class PlayerCamera {
public:
    void reset(Vector3 root, const PlayerSettings& settings);
    void update(float dt, Vector3 root, Vector3 velocity, const PlayerInput& input,
                const PlayerSettings& settings, const RuntimeSession& runtime);
    Camera3D camera() const { return camera_; }
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    float distance() const { return requestedDistance_; }
    bool firstPerson() const { return firstPerson_; }
    bool avatarHidden() const { return firstPerson_ || avatarOccluded_; }
    void setDistance(float value) { if (std::isfinite(value)) requestedDistance_ = value; }
    Vector3 moveDirection(Vector2 input) const;
private:
    Camera3D camera_{};
    Vector3 target_{};
    float yaw_ = 0, pitch_ = .25f;
    float requestedDistance_ = 12, distance_ = 12;
    float followDelay_ = 0;
    CameraMode previousMode_ = CameraMode::Classic;
    bool firstPerson_ = false;
    bool avatarOccluded_ = false; // geometry-derived visibility, not a control mode
};

// Owns its Scene, PhysicsWorld and one capsule. Lifetime: capsule -> physics -> scene.
// No pointers refer to authoring objects. Construction and start are headless-safe.
class RuntimeSession {
public:
    RuntimeSession();
    ~RuntimeSession();
    RuntimeSession(const RuntimeSession&) = delete;
    RuntimeSession& operator=(const RuntimeSession&) = delete;
    bool start(const Scene& authoring, std::string& error);
    void stop();
    void pause();
    void resume();
    bool respawn(std::string& error);
    void update(float dt, const PlayerInput& input);
    RuntimeState state() const { return state_; }
    bool active() const { return state_ != RuntimeState::Editing; }
    const Scene& scene() const { return *scene_; }
    Scene& mutableScene() { return *scene_; } // validated native bindings, runtime only
    scripting::LuaRuntime& scripts() { return scripts_; }
    const scripting::LuaRuntime& scripts() const { return scripts_; }
    unsigned characterGeneration() const { return respawns_; }
    bool setAnimation(const std::string& state,const std::string& assetId,std::string& error);
    float health() const { return health_; }
    bool setHealth(float value,std::string& error);
    bool takeDamage(float amount,std::string& error);
    bool heal(float amount,std::string& error);
    bool resetCharacter(std::string& error);
    const Rig& rig() const { return rig_; }
    RigidTransform characterRoot() const;
    const Pose& pose() const { return pose_; }
    Vector3 position() const;
    Vector3 velocity() const;
    HumanoidState humanoidState() const { return humanoidState_; }
    bool grounded() const;
    std::size_t bodyCount() const;
    bool validate(std::string& error) const;
    // Sphere sweep for camera, ignores avatar and non-collidable Parts.
    float cameraClearance(Vector3 from, Vector3 to, float radius) const;
    PlayerSettings& settings() { return settings_; } // session preferences, not project writes
    const PlayerSettings& settings() const { return settings_; }
    PlayerCamera& playerCamera() { return camera_; }
    const PlayerCamera& playerCamera() const { return camera_; }
private:
    void destroyCharacter();
    void step(float dt, Vector3 desired, bool jump);
    void transition(HumanoidState state);
    void animate(float dt, float horizontalSpeed);

    std::unique_ptr<Scene> scene_;
    std::unique_ptr<PhysicsWorld> physics_;
    std::unique_ptr<btCapsuleShape> capsule_;
    std::unique_ptr<btDefaultMotionState> motion_;
    std::unique_ptr<btRigidBody> body_;
    RuntimeState state_ = RuntimeState::Editing;
    HumanoidState humanoidState_ = HumanoidState::Idle;
    Rig rig_;
    PlayerSettings settings_;
    PlayerCamera camera_;
    Pose pose_{};
    float accumulator_ = 0, walkTime_ = 0, idleTime_ = 0, airTime_ = 0, walkWeight_ = 0, yaw_ = 0;
    float rootHeightOffset_ = .1f;
    bool jumpPending_ = false;
    unsigned respawns_ = 0;
    std::string correlation_;
    std::string activeAnimation_;
    scripting::LuaRuntime scripts_;
    float health_=100, deadTime_=0, damageCooldown_=0;
};
} // namespace loblox::game

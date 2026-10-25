#pragma once

#include <memory>
#include <optional>
#include <set>
#include "game/Npc.h"
#include "game/Rig.h"
#include "game/ScriptInput.h"
#include "game/ProgressStore.h"
#include "scripting/LuaRuntime.h"
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"

class btCapsuleShape;
class btRigidBody;
class btDefaultMotionState;

namespace loblox::game {
enum class RuntimeState { Editing, StartingPlay, Playing, Paused, StoppingPlay };
enum class HumanoidState { Idle, Walking, Jumping, Freefall, Landing, Dead };
enum class PartMotion { Velocity, Force, Impulse, AngularVelocity, Torque, AngularImpulse };
struct SpatialQuery {
    Vector3 center{},size{1,1,1},rotation{};
    float radius=0; // zero selects an oriented box, positive selects sphere
    InstanceId ignore=0;
    bool includePlayer=true,solidOnly=false;
    unsigned maxResults=256;
};
const char* StateName(HumanoidState state);
struct PlayerInput {
    bool shiftKey = false; // roteado ao gameplay; edge detect do Shift Lock
    Vector2 movement{}; // X=right, Y=forward, relative to horizontal camera
    Vector2 look{};     // mouse pixels; apply once per display frame
    float zoom = 0;
    bool jump = false;
    ScriptInput script;
};

class RuntimeSession;
class PlayerCamera {
public:
    void reset(Vector3 root, const PlayerSettings& settings);
    void processInput(float dt, const PlayerInput& input, const PlayerSettings& settings);
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
    Vector3 pivot() const { return target_; }
    float shoulderOffset() const { return shoulderOffset_; }
    float followDistance() const { return distance_; }
    // Camera-local X, in scene units. Independent of Humanoid/head offsets.
    bool setMouseLockShoulderOffset(float value);
    bool setPose(Camera3D pose,float seconds,std::string& error);
    void release(Vector3 root,const PlayerSettings& settings);
    bool scripted() const { return scripted_; }
    bool transitioning() const { return scripted_&&transitionElapsed_<transitionDuration_; }
    Quaternion orientation() const;
private:
    Camera3D camera_{};
    Vector3 target_{};
    float yaw_ = 0, pitch_ = .25f;
    float requestedDistance_ = 12, distance_ = 12;
    float followDelay_ = 0;
    float shoulderOffset_ = 0;
    float mouseLockShoulderOffset_ = 1.75f;
    Vector3 lockEntryOffset_{};
    bool wasMouseLocked_ = false;
    CameraMode previousMode_ = CameraMode::Classic;
    bool firstPerson_ = false;
    bool avatarOccluded_ = false; // geometry-derived visibility, not a control mode
    bool scripted_=false;
    Camera3D transitionSource_{},transitionTarget_{};
    float transitionElapsed_=0,transitionDuration_=0;
    float orbitYaw_=0,orbitPitch_=0,orbitDistance_=12;
};

// Owns its Scene, PhysicsWorld and one capsule. Lifetime: capsule -> physics -> scene.
// No pointers refer to authoring objects. Construction and start are headless-safe.
class RuntimeSession {
public:
    RuntimeSession();
    ~RuntimeSession();
    RuntimeSession(const RuntimeSession&) = delete;
    RuntimeSession& operator=(const RuntimeSession&) = delete;
    bool start(const Scene& authoring, std::string& error,const ProgressOptions& progress={});
    void stop(bool saveProgress=true);
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
    NpcSystem& npcs() { return npcs_; }
    const NpcSystem& npcs() const { return npcs_; }
    ProgressStore& storage(){return progress_;}
    const ScriptInput& scriptInput() const { return scriptInput_; }
    const ScriptInput& previousScriptInput() const { return previousScriptInput_; }
    unsigned characterGeneration() const { return respawns_; }
    bool setAnimation(const std::string& state,const std::string& assetId,std::string& error);
    float health() const { return health_; }
    bool setHealth(float value,std::string& error);
    bool takeDamage(float amount,std::string& error);
    bool heal(float amount,std::string& error);
    bool resetCharacter(std::string& error);
    const Rig& rig() const { return rig_; }
    RigidTransform characterRoot() const;
    // Physics remains authoritative; camera and avatar share this presentation
    // sample between fixed ticks. Rotation controlled by mouse is not delayed.
    RigidTransform renderCharacterRoot() const;
    const Pose& pose() const { return pose_; }
    Vector3 position() const;
    Vector3 velocity() const;
    HumanoidState humanoidState() const { return humanoidState_; }
    bool grounded() const;
    std::size_t bodyCount() const;
    bool validate(std::string& error) const;
    // Validated script operations; called only outside Bullet's simulation step.
    bool partMotion(InstanceId id,Vector3 value,PartMotion operation,std::string& error);
    Vector3 partVelocity(InstanceId id,bool angular=false);
    float partMass(InstanceId id);
    bool setGravity(float magnitude,std::string& error);
    bool teleportPlayer(Vector3 position,std::string& error);
    bool setCheckpoint(Vector3 position,std::string& error);
    std::vector<InstanceId> overlap(const SpatialQuery& query,bool& truncated,std::string& error);
    bool movePlayer(Vector3 direction,bool cameraRelative,std::string& error);
    bool jumpPlayer(std::string& error);
    bool playerMotion(Vector3 value,PartMotion operation,std::string& error);
    bool raycastParts(Vector3 origin,Vector3 direction,float distance,Part*& object,
                      Vector3& point,Vector3& normal,InstanceId ignored=0);
    // Sphere sweep for camera, ignores avatar and non-collidable Parts.
    float cameraClearance(Vector3 from, Vector3 to, float radius) const;
    PlayerSettings& settings() { return settings_; } // session preferences, not project writes
    const PlayerSettings& settings() const { return settings_; }

    // Shift Lock: estado de sessão (não persistido); toggle pelo Left Shift.
    bool shiftLockActive() const;
    bool shiftLockRequested() const { return shiftLockRequested_; }
    void setShiftLockRequested(bool requested) { shiftLockRequested_ = requested; }
    PlayerCamera& playerCamera() { return camera_; }
    const PlayerCamera& playerCamera() const { return camera_; }
private:
    friend class NpcSystem; // controlador de NPC conduz corpos e navegacao
    void destroyCharacter();
    void step(float dt, Vector3 desired, bool jump);
    void updateContacts();
    void transition(HumanoidState state);
    void animate(float dt, float horizontalSpeed);
    // Sweeps the full capsule down before a fixed step so a fast fall cannot
    // skip a thin platform. Bullet's CCD swept sphere only covers the body
    // centre, while the capsule ends are what normally touch the ground.
    void preventFallThrough(float dt);

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
    Vector3 previousCharacterPosition_{};
    Pose pose_{};
    // Pose evaluation is replayable: the visible pose is a short, time-based
    // crossfade from the pose captured when the active clip changed. It is
    // never a repeated increment over the previous frame (which would drift,
    // depend on the display rate and inherit the last jump).
    Pose blendFrom_{};
    std::string poseSource_;
    bool poseWalking_ = false;
    bool wasAirborne_ = false;
    float blendTime_ = 0;
    float accumulator_ = 0, walkTime_ = 0, idleTime_ = 0, airTime_ = 0, walkWeight_ = 0, yaw_ = 0;
    float rootHeightOffset_ = .1f;
    float coyoteTime_ = 0;        // tempo restante para pular após sair do chão
    float jumpBufferTime_ = 0;    // buffer de pulo pressionado no ar
    bool jumpPending_ = false;
    std::optional<Vector3> scriptMovement_;
    unsigned spatialQueries_=0;
    bool lastJumpDown_ = false;   // edge detection: holding jump must not bunny-hop
    bool suppressJump_ = false;   // a key held through respawn/pause needs a release
    unsigned respawns_ = 0;
    std::string correlation_;
    std::string activeAnimation_;
    scripting::LuaRuntime scripts_;
    NpcSystem npcs_;
    ProgressStore progress_;
    std::set<std::pair<InstanceId,InstanceId>> activeContacts_;
    ScriptInput scriptInput_,previousScriptInput_;
    float health_=100, deadTime_=0, damageCooldown_=0;
    std::optional<Vector3> scriptCheckpoint_;
    bool shiftLockRequested_ = false;
    unsigned char lastShiftKey_ = 0;
};
} // namespace loblox::game

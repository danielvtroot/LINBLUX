#pragma once

#include <array>
#include <string>
#include <vector>

#include "objects/Part.h"

namespace loblox::game {

// Original block rig. Y up, front = -Z. Root is at the torso centre;
// the default sole is root.y - 3. No limb is an independent rigid body.
constexpr int kJointCount = 6;
constexpr int kBodyCount = 6;
enum BodyIndex { Torso, Head, LeftArm, RightArm, LeftLeg, RightLeg };
enum JointIndex { RootJoint, Neck, LeftShoulder, RightShoulder, LeftHip, RightHip };

struct RigidTransform {
    Vector3 position{};
    Quaternion rotation{0, 0, 0, 1};
};
RigidTransform Compose(const RigidTransform& parent, const RigidTransform& local);
RigidTransform Inverse(const RigidTransform& value);
bool Valid(const RigidTransform& value);

using Pose = std::array<RigidTransform, kJointCount>;
struct RigDecal {
    std::string id, name="Decal";
    SurfaceProperties surface;
};
struct Body {
    std::string name;
    Vector3 size{1, 2, 1};
    Color color{240, 190, 80, 255};
    PartMaterial material = PartMaterial::SmoothPlastic;
    // Legacy single front image; kept for JSON compatibility. New decals are
    // independent children. Front=-Z, Back=+Z, Left=-X, Right=+X, Top=+Y.
    std::string image;
    std::vector<RigDecal> decals{};
};
struct Joint {
    std::string name;
    int parent = -1; // -1 is HumanoidRootPart, otherwise a body index
    int child = 0;
    RigidTransform parentAnchor;
    RigidTransform childAnchor;
};
struct Rig {
    std::string id = "r6-default";
    std::string name = "R6Character";
    RigidTransform root{{8, 3, 0}, {0, 0, 0, 1}};
    std::array<Body, kBodyCount> bodies;
    // Topological order is part of the contract. No cycles or multiple parents.
    std::array<Joint, kJointCount> joints;
    Pose restPose{};
    unsigned long long nextDecalId=1;
    RigDecal& addDecal(int body);
    RigDecal* findDecal(const std::string& id);
    bool eraseDecal(const std::string& id);

    bool validate(std::string& error) const;
    std::array<RigidTransform, kBodyCount> evaluate(const Pose& pose,
                                                   const RigidTransform& rootWorld) const;
    RigidTransform pivot(int joint, const Pose& pose) const;
    void resizeBody(int body, Vector3 size); // preserves connections at face anchors
};
Rig MakeR6();

enum class Interpolation { Linear, Smooth, Constant };
enum class AnimationPriority { Idle, Movement, Action };
struct Keyframe {
    float time = 0;
    RigidTransform pose;
    Interpolation interpolation = Interpolation::Smooth;
    // Signed sweep, in degrees, of the segment that starts at this key. Zero
    // keeps the shortest arc (legacy clips). A value such as 250 preserves a
    // rotation that deliberately goes the long way round, which a pair of
    // quaternions cannot express on its own. The engine only honours it when
    // it ends on the next key's stored orientation; otherwise it falls back to
    // the shortest arc, so older or hand-edited data stays safe.
    float arcDegrees = 0;
};
struct AnimationClip {
    int version = 1;
    std::string id;
    std::string name;
    std::string rigType = "R6";
    float duration = 0.8f;
    int fps = 30;
    bool loop = true;
    AnimationPriority priority = AnimationPriority::Movement;
    std::array<std::vector<Keyframe>, kJointCount> tracks;

    bool validate(std::string& error) const;
    Pose sample(float seconds) const;
    void setKey(int joint, Keyframe key);
    // Deterministic rotation between two keys: hemisphere-continuous and, when
    // the starting key carries a valid authored arc, faithful to that path.
    static Quaternion interpolate(const Keyframe& from, const Keyframe& to, float alpha);
    // Converts an authoring rotation (axis + signed sweep of any size) into the
    // arcDegrees convention used by interpolation, which is measured along the
    // engine's deterministic shortest-rotation axis.
    static float authoredArcDegrees(const Keyframe& from, const Keyframe& to,
                                    Vector3 authoredAxis, float sweepDegrees);
    bool eraseKey(int joint, float time);
};
AnimationClip MakeWalk(float amplitudeDegrees = 30, float duration = 0.8f);
AnimationClip MakeIdle();
AnimationClip MakeJump();
AnimationClip MakeFall(); // independent asset with no authored keys
Pose BlendPose(const Pose& from, const Pose& to, float alpha);

class Animator {
public:
    void play(const AnimationClip& clip, bool restart = true);
    void pause(bool value) { paused_ = value; }
    void stop();
    void seek(float time);
    void advance(float dt);
    Pose pose() const;
    float time() const { return time_; }
    bool playing() const { return clip_ != nullptr && !paused_; }
    float speed = 1;
private:
    const AnimationClip* clip_ = nullptr; // caller owns clip; stop before replacing it
    float time_ = 0;
    bool paused_ = false;
};

enum class CameraMode { Classic, Follow, FirstPerson };
struct PlayerSettings {
    float walkSpeed = 16;
    float turnSpeed = 12; // radians/second exponential response
    float acceleration = 80;
    float deceleration = 100;
    float jumpHeight = 7; // world units; impulse = sqrt(2 * gravity * height)
    float gravity = 98.1f;
    float maxHealth = 100;
    float health = 100;
    bool autoRotate = true;
    float collisionHeight = 5.8f; // total capsule height, including hemispheres
    float collisionRadius = 0.9f;
    CameraMode cameraMode = CameraMode::Classic;
    float minZoom = 0.3f;
    float maxZoom = 30;
    float cameraDistance = 12;
    float sensitivity = 0.003f;
    bool allowFirstPerson = true;
    bool cameraLock = false;
    bool enableShiftLock = false; // Left Shift liga/desliga durante Play
    std::string walkAnimation = "r6-walk";
    std::string idleAnimation = "r6-idle";
    std::string jumpAnimation; // empty retains the legacy native air-pose fallback
    std::string fallAnimation; // empty uses fallback pose during fall
    float respawnDelay=2;
    int healthDisplayType=2; // 0=AlwaysOn, 1=WhenDamaged, 2=AlwaysOff
    float healthDisplayDistance=45;
    float nameDisplayDistance=0; // local HUD is the default, overhead opt-in
    bool validate(std::string& error) const;
};

// Project-owned value data, copied once on Play. Preview/runtime never write it.
struct GameAssets {
    std::vector<Rig> rigs;
    std::vector<AnimationClip> animations;
    std::string defaultRig;
    PlayerSettings player;
    Rig& createR6();
    bool removeAnimation(const std::string& id,std::string& error,bool clearAssignments=false);
    Rig* findRig(const std::string& id);
    const Rig* findRig(const std::string& id) const;
    bool removeRig(const std::string& id);
    AnimationClip* findAnimation(const std::string& id);
    const AnimationClip* findAnimation(const std::string& id) const;
    bool validate(std::string& error) const;
};

} // namespace loblox::game

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
struct Body {
    std::string name;
    Vector3 size{1, 2, 1};
    Color color{240, 190, 80, 255};
    PartMaterial material = PartMaterial::SmoothPlastic;
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
    bool eraseKey(int joint, float time);
};
AnimationClip MakeWalk(float amplitudeDegrees = 30, float duration = 0.8f);
AnimationClip MakeIdle();
AnimationClip MakeJump();
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
    std::string walkAnimation = "r6-walk";
    std::string idleAnimation = "r6-idle";
    std::string jumpAnimation; // empty retains the legacy native air-pose fallback
    bool validate(std::string& error) const;
};

// Project-owned value data, copied once on Play. Preview/runtime never write it.
struct GameAssets {
    std::vector<Rig> rigs;
    std::vector<AnimationClip> animations;
    std::string defaultRig;
    PlayerSettings player;
    Rig& createR6();
    Rig* findRig(const std::string& id);
    const Rig* findRig(const std::string& id) const;
    AnimationClip* findAnimation(const std::string& id);
    const AnimationClip* findAnimation(const std::string& id) const;
    bool validate(std::string& error) const;
};

} // namespace loblox::game

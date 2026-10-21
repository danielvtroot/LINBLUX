#include "game/Rig.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "core/Diagnostics.h"

namespace loblox::game {
namespace {
bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool Range(float v, float low, float high) { return std::isfinite(v) && v >= low && v <= high; }
RigidTransform Mix(const RigidTransform& a, const RigidTransform& b, float t) {
    return {Vector3Lerp(a.position, b.position, t),
            QuaternionNormalize(QuaternionSlerp(a.rotation, b.rotation, t))};
}
}

RigidTransform Compose(const RigidTransform& a, const RigidTransform& b) {
    return {Vector3Add(a.position, Vector3RotateByQuaternion(b.position, a.rotation)),
            QuaternionNormalize(QuaternionMultiply(a.rotation, b.rotation))};
}
RigidTransform Inverse(const RigidTransform& t) {
    const Quaternion q = QuaternionInvert(t.rotation);
    return {Vector3RotateByQuaternion(Vector3Negate(t.position), q), q};
}
bool Valid(const RigidTransform& t) {
    const auto q = t.rotation;
    return Finite(t.position) && Vector3Length(t.position) <= 1000000 &&
        std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
        std::fabs(QuaternionLength(q) - 1) < 0.001f;
}

Rig MakeR6() {
    Rig r;
    r.bodies = {{{"Torso", {2, 2, 1}, {55, 120, 205, 255}},
                 {"Head", {2, 1, 1}, {245, 201, 91, 255}},
                 {"Left Arm", {1, 2, 1}, {245, 201, 91, 255}},
                 {"Right Arm", {1, 2, 1}, {245, 201, 91, 255}},
                 {"Left Leg", {1, 2, 1}, {83, 150, 85, 255}},
                 {"Right Leg", {1, 2, 1}, {83, 150, 85, 255}}}};
    r.joints = {{{"RootJoint", -1, Torso, {}, {}},
                 {"Neck", Torso, Head, {{0, 1, 0}}, {{0, -0.5f, 0}}},
                 {"Left Shoulder", Torso, LeftArm, {{-1, 1, 0}}, {{0.5f, 1, 0}}},
                 {"Right Shoulder", Torso, RightArm, {{1, 1, 0}}, {{-0.5f, 1, 0}}},
                 {"Left Hip", Torso, LeftLeg, {{-0.5f, -1, 0}}, {{0, 1, 0}}},
                 {"Right Hip", Torso, RightLeg, {{0.5f, -1, 0}}, {{0, 1, 0}}}}};
    return r;
}

bool Rig::validate(std::string& error) const {
    if (id.empty() || id.size() > 128 || name.empty() || name.size() > 128 || !Valid(root)) {
        error = "rig: invalid id, name or root transform"; return false;
    }
    std::array<bool, kBodyCount> visited{};
    std::set<std::string> names;
    for (int i = 0; i < kJointCount; ++i) {
        const auto& j = joints[i];
        if (j.name.empty() || !names.insert(j.name).second || j.child < 0 || j.child >= kBodyCount ||
            visited[j.child] || (i == 0 ? j.parent != -1 :
                (j.parent < 0 || j.parent >= kBodyCount || !visited[j.parent])) ||
            !Valid(j.parentAnchor) || !Valid(j.childAnchor) || !Valid(restPose[i])) {
            error = "rig: invalid hierarchy, joint or quaternion at " + std::to_string(i); return false;
        }
        visited[j.child] = true;
    }
    names.clear();
    for (const auto& body : bodies) {
        if (body.name.empty() || !names.insert(body.name).second || !Finite(body.size) ||
            !Range(body.size.x, 0.2f, 10) || !Range(body.size.y, 0.2f, 10) ||
            !Range(body.size.z, 0.2f, 10) || static_cast<int>(body.material) < 0 ||
            static_cast<int>(body.material) >= PartMaterialCount()) {
            error = "rig: invalid body dimensions, material or name"; return false;
        }
    }
    error.clear(); return true;
}

std::array<RigidTransform, kBodyCount> Rig::evaluate(const Pose& pose,
                                                  const RigidTransform& rootWorld) const {
    std::array<RigidTransform, kBodyCount> result{};
    for (int i = 0; i < kJointCount; ++i) {
        const auto& j = joints[i];
        const auto& parent = j.parent < 0 ? rootWorld : result[j.parent];
        result[j.child] = Compose(Compose(Compose(Compose(parent, j.parentAnchor), restPose[i]),
                                           pose[i]), Inverse(j.childAnchor));
    }
    return result;
}
RigidTransform Rig::pivot(int index, const Pose& pose) const {
    const auto world = evaluate(pose, root);
    const auto& j = joints[index];
    return Compose(Compose(j.parent < 0 ? root : world[j.parent], j.parentAnchor), restPose[index]);
}
void Rig::resizeBody(int index, Vector3 size) {
    if (index < 0 || index >= kBodyCount || !Finite(size)) return;
    size = {std::clamp(size.x, .2f, 10.f), std::clamp(size.y, .2f, 10.f), std::clamp(size.z, .2f, 10.f)};
    const auto old = bodies[index].size;
    const Vector3 ratio{size.x / old.x, size.y / old.y, size.z / old.z};
    for (auto& joint : joints) {
        if (joint.parent == index) joint.parentAnchor.position = Vector3Multiply(joint.parentAnchor.position, ratio);
        if (joint.child == index) joint.childAnchor.position = Vector3Multiply(joint.childAnchor.position, ratio);
    }
    bodies[index].size = size;
}

bool AnimationClip::validate(std::string& error) const {
    if (version != 1 || id.empty() || id.size() > 128 || name.empty() || name.size() > 128 ||
        rigType != "R6" || !Range(duration, .05f, 120) || fps < 1 || fps > 240 ||
        static_cast<int>(priority) < 0 || static_cast<int>(priority) > 2) {
        error = "animation: invalid header"; return false;
    }
    for (const auto& track : tracks) {
        if (track.size() > 10000) { error = "animation: too many keys"; return false; }
        float previous = -1;
        for (const auto& key : track) {
            if (!Range(key.time, 0, duration) || key.time <= previous || !Valid(key.pose) ||
                static_cast<int>(key.interpolation) < 0 || static_cast<int>(key.interpolation) > 2) {
                error = "animation: invalid/unsorted keyframe"; return false;
            }
            previous = key.time;
        }
    }
    error.clear(); return true;
}
Pose AnimationClip::sample(float seconds) const {
    Pose result{};
    if (!std::isfinite(seconds) || duration <= 0) return result;
    float t = loop ? std::fmod(seconds, duration) : std::clamp(seconds, 0.f, duration);
    if (t < 0) t += duration;
    for (int j = 0; j < kJointCount; ++j) {
        const auto& track = tracks[j];
        if (track.empty()) continue;
        if (track.size() == 1) { result[j] = track.front().pose; continue; }
        auto next = std::upper_bound(track.begin(), track.end(), t,
            [](float time, const Keyframe& k) { return time < k.time; });
        const Keyframe* a;
        const Keyframe* b;
        float ta, tb, local = t;
        if (next == track.begin() || next == track.end()) {
            if (!loop) { result[j] = next == track.begin() ? track.front().pose : track.back().pose; continue; }
            a = &track.back(); b = &track.front();
            ta = a->time; tb = b->time + duration;
            if (next == track.begin()) local += duration;
        } else { b = &*next; a = &*(next - 1); ta = a->time; tb = b->time; }
        float alpha = tb > ta ? std::clamp((local - ta) / (tb - ta), 0.f, 1.f) : 0;
        if (a->interpolation == Interpolation::Constant) alpha = 0;
        else if (a->interpolation == Interpolation::Smooth) alpha = alpha * alpha * (3 - 2 * alpha);
        result[j] = Mix(a->pose, b->pose, alpha);
    }
    return result;
}
void AnimationClip::setKey(int joint, Keyframe key) {
    if (joint < 0 || joint >= kJointCount || !Range(key.time, 0, duration) || !Valid(key.pose)) return;
    auto& track = tracks[joint];
    auto it = std::lower_bound(track.begin(), track.end(), key.time,
        [](const Keyframe& k, float time) { return k.time < time; });
    if (it != track.end() && std::fabs(it->time - key.time) < .0001f) *it = key;
    else if (it != track.begin() && std::fabs((it - 1)->time - key.time) < .0001f) *(it - 1) = key;
    else track.insert(it, key);
}
bool AnimationClip::eraseKey(int joint, float time) {
    if (joint < 0 || joint >= kJointCount) return false;
    auto& track = tracks[joint];
    const auto it = std::find_if(track.begin(), track.end(), [time](const Keyframe& k) { return std::fabs(k.time-time) < .0001f; });
    if (it == track.end()) return false;
    track.erase(it); return true;
}
AnimationClip MakeWalk(float amplitude, float duration) {
    AnimationClip clip;
    clip.id = "r6-walk"; clip.name = "R6_Walk";
    clip.duration = std::clamp(duration, .1f, 10.f);
    amplitude = std::clamp(amplitude, 0.f, 80.f);
    for (int i = 0; i <= 4; ++i) {
        const float phase = i * PI * .5f;
        const float angle = std::cos(phase) * amplitude * DEG2RAD;
        Pose pose{};
        // +X rotates a downward limb towards -Z (the rig's front).
        pose[LeftHip].rotation = QuaternionFromEuler(angle, 0, 0);
        pose[RightHip].rotation = QuaternionFromEuler(-angle, 0, 0);
        pose[LeftShoulder].rotation = QuaternionFromEuler(-angle * .8f, 0, 0);
        pose[RightShoulder].rotation = QuaternionFromEuler(angle * .8f, 0, 0);
        pose[RootJoint].position.y = .05f * (1 - std::cos(phase * 2));
        pose[RootJoint].rotation = QuaternionFromEuler(-3 * DEG2RAD, std::sin(phase) * .035f, 0);
        pose[Neck].rotation = QuaternionFromEuler(3 * DEG2RAD, 0, 0);
        for (int j = 0; j < kJointCount; ++j) clip.setKey(j, {clip.duration * i / 4, pose[j]});
    }
    return clip;
}
AnimationClip MakeIdle() {
    AnimationClip clip;
    clip.id = "r6-idle"; clip.name = "R6_Idle"; clip.priority = AnimationPriority::Idle;
    for (int i = 0; i < kJointCount; ++i) clip.setKey(i, {});
    return clip;
}
Pose BlendPose(const Pose& a, const Pose& b, float alpha) {
    Pose out;
    for (int i = 0; i < kJointCount; ++i) out[i] = Mix(a[i], b[i], std::clamp(alpha, 0.f, 1.f));
    return out;
}
AnimationClip MakeJump() {
    AnimationClip clip=MakeIdle();clip.id="r6-jump";clip.name="R6_Jump";
    clip.duration=.5f;clip.loop=false;clip.priority=AnimationPriority::Movement;
    // Preserve the approved native air pose, now as an editable asset.
    Pose pose{};pose[LeftShoulder].rotation=QuaternionFromEuler(0,0,-.35f);
    pose[RightShoulder].rotation=QuaternionFromEuler(0,0,.35f);
    pose[LeftHip].rotation=QuaternionFromEuler(-.15f,0,0);
    pose[RightHip].rotation=QuaternionFromEuler(.15f,0,0);
    for(int j=0;j<kJointCount;++j) {clip.setKey(j,{0,pose[j]});clip.setKey(j,{clip.duration,pose[j]});}
    return clip;
}
void Animator::play(const AnimationClip& clip, bool restart) {
    if (restart || clip_ != &clip) time_ = 0;
    clip_ = &clip; paused_ = false;
}
void Animator::stop() { clip_ = nullptr; time_ = 0; paused_ = false; }
void Animator::seek(float time) { if (std::isfinite(time)) time_ = std::max(0.f, time); }
void Animator::advance(float dt) {
    if (!playing() || !std::isfinite(dt) || !std::isfinite(speed)) return;
    time_ = std::max(0.f, time_ + std::max(0.f, dt) * speed);
    if (clip_->loop) time_ = std::fmod(time_, clip_->duration);
    else if (time_ >= clip_->duration) { time_ = clip_->duration; paused_ = true; }
}
Pose Animator::pose() const { return clip_ ? clip_->sample(time_) : Pose{}; }

bool PlayerSettings::validate(std::string& error) const {
    if (!Range(walkSpeed, .1f, 100) || !Range(turnSpeed, .1f, 50) ||
        !Range(acceleration, 1, 500) || !Range(deceleration, 1, 500) ||
        !Range(jumpHeight, .1f, 50) || !Range(gravity, 1, 500) ||
        !Range(maxHealth, 1, 100000) || !Range(health, 0, maxHealth) ||
        !Range(respawnDelay,.1f,30) || healthDisplayType<0 || healthDisplayType>2 ||
        !Range(healthDisplayDistance,0,500) || !Range(nameDisplayDistance,0,500) ||
        !Range(collisionRadius, .2f, 3) || !Range(collisionHeight, 2 * collisionRadius, 15) ||
        !Range(minZoom, .1f, 10) || !Range(maxZoom, minZoom, 100) ||
        !Range(cameraDistance, minZoom, maxZoom) || !Range(sensitivity, .0001f, .03f) ||
        static_cast<int>(cameraMode) < 0 || static_cast<int>(cameraMode) > 2) {
        error = "player: settings out of range"; return false;
    }
    error.clear(); return true;
}
Rig& GameAssets::createR6() {
    Rig rig = MakeR6();
    for (unsigned i = 1; findRig(rig.id); ++i) rig.id = "r6-" + std::to_string(i);
    rig.name = rigs.empty() ? "R6Character" : "R6Character " + std::to_string(rigs.size()+1);
    rig.root.position.x += rigs.size() * 5.f;
    rigs.push_back(std::move(rig));
    if (defaultRig.empty()) defaultRig = rigs.back().id;
    if (!findAnimation("r6-walk")) animations.push_back(MakeWalk());
    if (!findAnimation("r6-idle")) animations.push_back(MakeIdle());
    if (!findAnimation("r6-jump")) animations.push_back(MakeJump());
    if (player.jumpAnimation.empty()) player.jumpAnimation="r6-jump";
    diag::Event event; event.subsystem = "rig"; event.name = "create";
    event.context = {{"rig", rigs.back().id}}; event.result = "ok"; diag::log(event);
    return rigs.back();
}
Rig* GameAssets::findRig(const std::string& id) {
    for (auto& r : rigs) if (r.id == id) return &r;
    return nullptr;
}
const Rig* GameAssets::findRig(const std::string& id) const {
    for (const auto& r : rigs) if (r.id == id) return &r;
    return nullptr;
}
AnimationClip* GameAssets::findAnimation(const std::string& id) {
    const auto key=id.rfind("anim://",0)==0?id.substr(7):id;
    for (auto& c : animations) if (c.id == key) return &c;
    return nullptr;
}
const AnimationClip* GameAssets::findAnimation(const std::string& id) const {
    const auto key=id.rfind("anim://",0)==0?id.substr(7):id;
    for (const auto& c : animations) if (c.id == key) return &c;
    return nullptr;
}
bool GameAssets::validate(std::string& error) const {
    if (rigs.size() > 128 || animations.size() > 256) { error = "game: asset limit exceeded"; return false; }
    std::set<std::string> ids;
    for (const auto& r : rigs) if (!r.validate(error) || !ids.insert(r.id).second) { if (error.empty()) error = "duplicate rig id"; return false; }
    ids.clear();
    std::size_t keys = 0;
    for (const auto& clip : animations) for (const auto& track : clip.tracks) keys += track.size();
    if (keys > 100000) { error = "game: project keyframe budget exceeded"; return false; }
    for (const auto& c : animations) if (!c.validate(error) || !ids.insert(c.id).second) { if (error.empty()) error = "duplicate animation id"; return false; }
    if (!player.validate(error)) return false;
    const bool reservedDefaults = rigs.empty() && animations.empty() &&
        player.walkAnimation == "r6-walk" && player.idleAnimation == "r6-idle";
    if ((!player.jumpAnimation.empty() && !findAnimation(player.jumpAnimation)) ||
        ((!defaultRig.empty() || !rigs.empty()) && !findRig(defaultRig)) || (!reservedDefaults &&
        (!findAnimation(player.walkAnimation) || !findAnimation(player.idleAnimation)))) {
        error = "game: missing default rig or animation reference"; return false;
    }
    error.clear(); return true;
}
} // namespace loblox::game

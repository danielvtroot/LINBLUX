#include "game/Rig.h"
#include "core/Geometry.h"
#include "game/Audio.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <stdexcept>

#include "core/Diagnostics.h"

namespace loblox::game {
namespace {
bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool Range(float v, float low, float high) { return std::isfinite(v) && v >= low && v <= high; }
RigidTransform Mix(const RigidTransform& a, const RigidTransform& b, float t) {
    return {Vector3Lerp(a.position, b.position, t),
            QuaternionSlerpShortest(a.rotation, b.rotation, t)};
}
constexpr float kMaxArcDegrees = 36000.0f;

// Deterministic shortest-rotation data between two stored orientations. The
// target is aligned to the source hemisphere (q and -q are the same pose) and
// the exact-180 tie is resolved from the relative axis itself, never from the
// stored sign or float noise.
void RelativeRotation(Quaternion from, Quaternion to, Vector3& axis, float& shortest) {
    Quaternion a = UnitQuaternion(from);
    Quaternion b = UnitQuaternion(to);
    if (QuaternionDotProduct(a, b) < 0.0f) b = NegateQuaternion(b);
    Quaternion relative = UnitQuaternion(QuaternionMultiply(QuaternionInvert(a), b));
    axis = Vector3{relative.x, relative.y, relative.z};
    if (relative.w < 0.0f) { relative = NegateQuaternion(relative); axis = Vector3Negate(axis); }
    if (std::fabs(relative.w) < 1e-5f) {
        const float ax = std::fabs(axis.x), ay = std::fabs(axis.y), az = std::fabs(axis.z);
        const int largest = ax >= ay && ax >= az ? 0 : (ay >= az ? 1 : 2);
        const float component = largest == 0 ? axis.x : largest == 1 ? axis.y : axis.z;
        if (component < 0.0f) { relative = NegateQuaternion(relative); axis = Vector3Negate(axis); }
    }
    shortest = 2.0f * std::acos(std::clamp(relative.w, -1.0f, 1.0f));
    const float length = Vector3Length(axis);
    axis = length > 1e-6f ? Vector3Scale(axis, 1.0f / length) : Vector3{1, 0, 0};
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
    r.bodies = {{{"Torso", {2, 2, 1}, {55, 120, 205, 255}, PartMaterial::SmoothPlastic, ""},
                 {"Head", {2, 1, 1}, {245, 201, 91, 255}, PartMaterial::SmoothPlastic, ""},
                 {"Left Arm", {1, 2, 1}, {245, 201, 91, 255}, PartMaterial::SmoothPlastic, ""},
                 {"Right Arm", {1, 2, 1}, {245, 201, 91, 255}, PartMaterial::SmoothPlastic, ""},
                 {"Left Leg", {1, 2, 1}, {83, 150, 85, 255}, PartMaterial::SmoothPlastic, ""},
                 {"Right Leg", {1, 2, 1}, {83, 150, 85, 255}, PartMaterial::SmoothPlastic, ""}}};
    r.joints = {{{"RootJoint", -1, Torso, {}, {}},
                 {"Neck", Torso, Head, {{0, 1, 0}}, {{0, -0.5f, 0}}},
                 {"Left Shoulder", Torso, LeftArm, {{-1, 1, 0}}, {{0.5f, 1, 0}}},
                 {"Right Shoulder", Torso, RightArm, {{1, 1, 0}}, {{-0.5f, 1, 0}}},
                 {"Left Hip", Torso, LeftLeg, {{-0.5f, -1, 0}}, {{0, 1, 0}}},
                 {"Right Hip", Torso, RightLeg, {{0.5f, -1, 0}}, {{0, 1, 0}}}}};
    return r;
}

bool Rig::validate(std::string& error) const {
    if(nextDecalId==0||nextDecalId>1000000000000ull){error="rig: invalid decal counter";return false;}
    std::set<std::string> decalIds;
    for(const auto& body:bodies) {
        if(body.decals.size()>64){error="rig: at most 64 decals per body";return false;}
        for(const auto& decal:body.decals)if(decal.id.empty()||decal.id.size()>128||decal.id.find('\0')!=std::string::npos||!decalIds.insert(decal.id).second||
            decal.name.empty()||decal.name.size()>128||decal.name.find('\0')!=std::string::npos||
            !decal.surface.valid()||decal.surface.image.find('\0')!=std::string::npos) {
            error="rig: invalid/duplicate decal id, name, image or face";return false;
        }
    }
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
            static_cast<int>(body.material) >= PartMaterialCount() || body.image.size() > 1024 ||
            body.image.find('\0') != std::string::npos) {
            error = "rig: invalid body dimensions, material, name or image"; return false;
        }
    }
    error.clear(); return true;
}

RigDecal* Rig::findDecal(const std::string& id) {
    for(auto& body:bodies)for(auto& decal:body.decals)if(decal.id==id)return &decal;
    return nullptr;
}
RigDecal& Rig::addDecal(int body) {
    if(body<0||body>=kBodyCount||bodies[body].decals.size()>=64||nextDecalId>=1000000000000ull)
        throw std::runtime_error("Invalid body or decal limit reached");
    std::string id;do{id="decal-"+std::to_string(nextDecalId++);}while(findDecal(id));
    bodies[body].decals.push_back({id,"Decal",{}});
    return bodies[body].decals.back();
}
bool Rig::eraseDecal(const std::string& id) {
    for(auto& body:bodies)for(auto it=body.decals.begin();it!=body.decals.end();++it)if(it->id==id){body.decals.erase(it);return true;}
    return false;
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
    return Compose(world[j.child],j.childAnchor);
}
RigidTransform Rig::authoringPivot() const {return {evaluate({},root)[Torso].position,root.rotation};}
Vector3 Rig::authoringSize() const {
    const auto pivot=authoringPivot();const auto world=evaluate({},root);Vector3 half{};
    for(int b=0;b<kBodyCount;++b)for(const auto& corner:OrientedBox{world[b].position,world[b].rotation,Vector3Scale(bodies[b].size,.5f)}.corners()) {
        const auto local=Vector3RotateByQuaternion(Vector3Subtract(corner,pivot.position),QuaternionInvert(pivot.rotation));
        half.x=std::max(half.x,std::fabs(local.x));half.y=std::max(half.y,std::fabs(local.y));half.z=std::max(half.z,std::fabs(local.z));
    }
    return Vector3Scale(half,2);
}
bool Rig::transformAuthoring(RigidTransform targetPivot,float scale,std::string& error) {
    if(!Valid(targetPivot)||!std::isfinite(scale)||scale<=0){error="Invalid rig transform";return false;}
    const auto before=authoringPivot();auto transformed=*this;
    const auto rotation=QuaternionNormalize(QuaternionMultiply(targetPivot.rotation,QuaternionInvert(before.rotation)));
    transformed.root.rotation=QuaternionNormalize(QuaternionMultiply(rotation,root.rotation));
    transformed.root.position=Vector3Add(targetPivot.position,Vector3RotateByQuaternion(Vector3Scale(Vector3Subtract(root.position,before.position),scale),rotation));
    for(auto& body:transformed.bodies)body.size=Vector3Scale(body.size,scale);
    for(auto& joint:transformed.joints){joint.parentAnchor.position=Vector3Scale(joint.parentAnchor.position,scale);joint.childAnchor.position=Vector3Scale(joint.childAnchor.position,scale);}
    for(auto& pose:transformed.restPose)pose.position=Vector3Scale(pose.position,scale);
    if(!transformed.validate(error))return false;
    *this=std::move(transformed);return true;
}
bool Rig::recenterAuthoringRoot(std::string& error) {
    if(joints[RootJoint].parent!=-1){error="RootJoint must be attached to the internal root";return false;}
    auto result=*this;const auto pivot=authoringPivot();const auto delta=Vector3Subtract(pivot.position,root.position);
    const auto frame=Compose(root,joints[RootJoint].parentAnchor);
    result.root.position=pivot.position;
    result.restPose[RootJoint].position=Vector3Subtract(restPose[RootJoint].position,Vector3RotateByQuaternion(delta,QuaternionInvert(frame.rotation)));
    if(!result.validate(error))return false;
    *this=std::move(result);return true;
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
                static_cast<int>(key.interpolation) < 0 || static_cast<int>(key.interpolation) > 2 ||
                !std::isfinite(key.arcDegrees) || std::fabs(key.arcDegrees) > kMaxArcDegrees) {
                error = "animation: invalid/unsorted keyframe"; return false;
            }
            previous = key.time;
        }
    }
    error.clear(); return true;
}
Quaternion AnimationClip::interpolate(const Keyframe& from, const Keyframe& to, float alpha) {
    const Quaternion a = UnitQuaternion(from.pose.rotation);
    Quaternion b = UnitQuaternion(to.pose.rotation);
    // q and -q are the same orientation. Aligning the target to the source
    // hemisphere is what keeps the arc independent from how the key was saved.
    if (QuaternionDotProduct(a, b) < 0.0f) b = NegateQuaternion(b);
    Vector3 axis;
    float shortest = 0;
    RelativeRotation(a, b, axis, shortest);
    const float arc = std::isfinite(from.arcDegrees) ? from.arcDegrees : 0.0f;
    if (std::fabs(arc) > 1e-4f && std::fabs(arc) <= kMaxArcDegrees) {
        const Quaternion end = UnitQuaternion(
            QuaternionMultiply(a, QuaternionFromAxisAngle(axis, arc * DEG2RAD)));
        // The authored sweep only applies when it really ends on the next key
        // (q and -q are the same orientation). Retimed keys and hand-edited
        // files fall back to the shortest arc.
        if (std::fabs(QuaternionDotProduct(end, b)) > 0.9999f) {
            return UnitQuaternion(QuaternionMultiply(
                a, QuaternionFromAxisAngle(axis, arc * std::clamp(alpha, 0.0f, 1.0f) * DEG2RAD)));
        }
    }
    return QuaternionSlerpShortest(a, b, alpha);
}
float AnimationClip::authoredArcDegrees(const Keyframe& from, const Keyframe& to,
                                      Vector3 authoredAxis, float sweepDegrees) {
    if (!std::isfinite(sweepDegrees) || std::fabs(sweepDegrees) > kMaxArcDegrees ||
        Vector3Length(authoredAxis) < 1e-6f) return 0.0f;
    Vector3 axis;
    float shortest = 0;
    RelativeRotation(from.pose.rotation, to.pose.rotation, axis, shortest);
    return Vector3DotProduct(Vector3Normalize(authoredAxis), axis) >= 0.0f
        ? sweepDegrees : -sweepDegrees;
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
        result[j] = {Vector3Lerp(a->pose.position, b->pose.position, alpha),
                     interpolate(*a, *b, alpha)};
    }
    return result;
}
void AnimationClip::setKey(int joint, Keyframe key) {
    if (joint < 0 || joint >= kJointCount || !Range(key.time, 0, duration) || !Valid(key.pose)) return;
    // Canonical hemisphere keeps the stored bytes stable across editor edits.
    key.pose.rotation = CanonicalQuaternion(UnitQuaternion(key.pose.rotation));
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
AnimationClip MakeFall() {
    AnimationClip clip;clip.id="r6-fall";clip.name="R6_Fall";
    clip.duration=.8f;clip.loop=true;clip.priority=AnimationPriority::Movement;
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
        !Range(cameraDistance, minZoom, maxZoom) || !Range(cameraFov,1,170) || !Range(sensitivity, .0001f, .03f) ||
        static_cast<int>(cameraMode) < 0 || static_cast<int>(cameraMode) > 2) {
        error = "player: settings out of range"; return false;
    }
    error.clear(); return true;
}
Rig& GameAssets::createLinbluxCharacter() {
    auto& rig=createR6();const auto previous=rig.id;
    std::string next;for(unsigned i=1;;++i){next="linblux-character-"+std::to_string(i);if(!findRig(next))break;}
    rig.id=next;rig.name="Sentinela LINBLUX";
    const std::array<Vector3,kBodyCount> sizes{{{1.8f,1.8f,1.1f},{1.35f,1.2f,1.15f},{.65f,1.8f,.75f},{.65f,1.8f,.75f},{.7f,1.9f,.85f},{.7f,1.9f,.85f}}};
    const std::array<Color,kBodyCount> colors{{{158,34,48,255},{190,193,203,255},{76,79,89,255},{76,79,89,255},{52,54,64,255},{52,54,64,255}}};
    for(int i=0;i<kBodyCount;++i){rig.resizeBody(i,sizes[i]);rig.bodies[i].color=colors[i];}
    if(defaultRig==previous)defaultRig=rig.id;
    return rig;
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
    if (!findAnimation("r6-fall")) animations.push_back(MakeFall());
    if (player.jumpAnimation.empty()) player.jumpAnimation="r6-jump";
    if (player.fallAnimation.empty()) player.fallAnimation="r6-fall";
    diag::Event event; event.subsystem = "rig"; event.name = "create";
    event.context = {{"rig", rigs.back().id}}; event.result = "ok"; diag::log(event);
    return rigs.back();
}
bool GameAssets::removeAnimation(const std::string& id,std::string& error,bool clearAssignments) {
    const auto* clip=findAnimation(id);
    if(!clip) {error="Animation no longer exists";return false;}
    for(const auto* reference:{&player.idleAnimation,&player.walkAnimation,&player.jumpAnimation,&player.fallAnimation})
        if(findAnimation(*reference)==clip && !clearAssignments) {error="Animation is assigned in Game Config; confirm deletion to clear its assignments.";return false;}
    const std::string key=clip->id;
    for(auto* reference:{&player.idleAnimation,&player.walkAnimation,&player.jumpAnimation,&player.fallAnimation})
        if(findAnimation(*reference)==clip)reference->clear();
    animations.erase(std::remove_if(animations.begin(),animations.end(),[&](const auto& value){return value.id==key;}),animations.end());
    error.clear();return true;
}
bool GameAssets::removeRig(const std::string& id) {
    for (auto it = rigs.begin(); it != rigs.end(); ++it) {
        if (it->id == id) {
            rigs.erase(it);
            if (defaultRig == id) {
                defaultRig = rigs.empty() ? "" : rigs.front().id;
            }
            return true;
        }
    }
    return false;
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
SoundAsset* GameAssets::findSound(const std::string& id) {
    for (auto& sound : sounds) if (sound.id == id) return &sound;
    return nullptr;
}
const SoundAsset* GameAssets::findSound(const std::string& id) const {
    for (const auto& sound : sounds) if (sound.id == id) return &sound;
    return nullptr;
}
SoundAsset& GameAssets::addSound(const std::string& reference, const std::string& name) {
    SoundAsset sound;
    sound.reference = reference;
    sound.filename = std::filesystem::path(reference).filename().string();
    sound.id = std::filesystem::path(reference).stem().string();
    if (sound.id.empty()) sound.id = "sound-" + std::to_string(sounds.size() + 1);
    for (int suffix = 2; findSound(sound.id); ++suffix) sound.id = std::filesystem::path(reference).stem().string() + "-" + std::to_string(suffix);
    sound.name = name.empty() ? sound.id : name;
    sounds.push_back(std::move(sound));
    return sounds.back();
}
bool GameAssets::removeSound(const std::string& id) {
    const auto before = sounds.size();
    sounds.erase(std::remove_if(sounds.begin(), sounds.end(), [&](const SoundAsset& sound) { return sound.id == id; }), sounds.end());
    return sounds.size() != before;
}
bool GameAssets::validate(std::string& error) const {
    if (rigs.size() > 128 || animations.size() > 256) { error = "game: asset limit exceeded"; return false; }
    if (sounds.size() > 256) { error = "game: sound limit exceeded"; return false; }
    std::set<std::string> soundIds;
    for (const auto& sound : sounds) {
        if (sound.id.empty() || sound.id.size() > 128 || sound.reference.size() > 512 || !soundIds.insert(sound.id).second) {
            error = "game: invalid or duplicate sound id"; return false;
        }
        if (!std::isfinite(sound.volume) || sound.volume < 0 || sound.volume > 1) { error = "game: sound volume out of range"; return false; }
        if (sound.reference.rfind("assets/", 0) != 0) { error = "game: sound reference must live in assets/"; return false; }
    }
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
    if ((!player.fallAnimation.empty() && !findAnimation(player.fallAnimation)) ||
        (!player.jumpAnimation.empty() && !findAnimation(player.jumpAnimation)) ||
        ((!defaultRig.empty() || !rigs.empty()) && !findRig(defaultRig)) || (!reservedDefaults &&
        ((!player.walkAnimation.empty() && !findAnimation(player.walkAnimation)) ||
         (!player.idleAnimation.empty() && !findAnimation(player.idleAnimation))))) {
        error = "game: missing default rig or animation reference"; return false;
    }
    error.clear(); return true;
}
} // namespace loblox::game

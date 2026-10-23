#include <cmath>
#include <cstdio>
#include <limits>
#include <cstdlib>
#include "core/Diagnostics.h"
#include "editor/InputRouting.h"

#include "game/Rig.h"
#include "game/GameAssetsIO.h"
#include "game/Runtime.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char* expression, int line) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %d: %s\n", line, expression); }
}
#define CHECK(x) Check((x), #x, __LINE__)
using namespace loblox;
using namespace loblox::game;
void RigTests() {
    std::string error;
    Rig rig = MakeR6();
    CHECK(rig.validate(error));
    CHECK(rig.bodies.size() == 6);
    CHECK(rig.joints.size() == 6);
    Pose pose{};
    auto neutral = rig.evaluate(pose, rig.root);
    CHECK(NearlyEqual(neutral[Torso].position, rig.root.position));
    CHECK(NearlyEqual(neutral[Head].position, Vector3{8, 4.5f, 0}));
    CHECK(NearlyEqual(neutral[LeftLeg].position, Vector3{7.5f, 1, 0}));
    pose[LeftShoulder].rotation = QuaternionFromEuler(PI/2, 0, 0);
    auto raised = rig.evaluate(pose, rig.root);
    CHECK(NearlyEqual(raised[Torso].position, neutral[Torso].position));
    CHECK(NearlyEqual(raised[RightArm].position, neutral[RightArm].position));
    CHECK(raised[LeftArm].position.z < neutral[LeftArm].position.z - .9f);
    auto pivot = rig.pivot(LeftShoulder, pose);
    auto anchor = Compose(raised[LeftArm], rig.joints[LeftShoulder].childAnchor);
    CHECK(NearlyEqual(pivot.position, anchor.position));
    pose[LeftHip].rotation = QuaternionFromEuler(-PI/4, 0, 0);
    CHECK(rig.evaluate(pose, rig.root)[LeftLeg].position.z > .6f);
    auto movedRoot = rig.root;
    movedRoot.position = Vector3Add(movedRoot.position, {4, 5, 6});
    movedRoot.rotation = QuaternionFromEuler(0, PI/2, 0);
    auto moved = rig.evaluate(pose, movedRoot);
    for (const auto& body : moved) CHECK(Valid(body));
    rig.resizeBody(Torso, {3, 4, 2});
    rig.resizeBody(LeftArm, {2, 3, 1});
    auto resized = rig.evaluate(pose, rig.root);
    CHECK(NearlyEqual(rig.pivot(LeftShoulder, pose).position,
                      Compose(resized[LeftArm], rig.joints[LeftShoulder].childAnchor).position));
    CHECK(rig.validate(error));
    pose = {};
    CHECK(NearlyEqual(rig.evaluate(pose, rig.root)[Torso].position, rig.root.position));
    rig.joints[0].parent = Torso;
    CHECK(!rig.validate(error));
    rig = MakeR6(); rig.joints[2].child = Torso;
    CHECK(!rig.validate(error));
    rig = MakeR6(); rig.joints[1].parentAnchor.rotation.w = 0;
    CHECK(!rig.validate(error));
}
void InputTests() {
    EditorInput input;
    InputContext ctx; ctx.windowFocused = ctx.viewportVisible = ctx.focused = ctx.hovered = true;
    ctx.mode = InputMode::Playing;
    CHECK(RouteInput(input,ctx).owner == InputOwner::Gameplay);
    ctx.popup = true; CHECK(RouteInput(input,ctx).owner == InputOwner::UI);
    ctx.popup = false; ctx.widget = true; CHECK(RouteInput(input,ctx).owner == InputOwner::UI);
    ctx.widget = false; ctx.text = true; input.escape = true;
    CHECK(RouteInput(input,ctx).owner == InputOwner::UI);
    ctx.text = false; CHECK(RouteInput(input,ctx).owner == InputOwner::Pause);
    ctx.mode = InputMode::Paused; ctx.popup = true;
    CHECK(RouteInput(input,ctx).owner == InputOwner::Resume);
    ctx.menuDepth = 1; CHECK(RouteInput(input,ctx).owner == InputOwner::Resume);
    ctx.text = ctx.widget = true; CHECK(RouteInput(input,ctx).owner == InputOwner::UI);
    input.stop = true; CHECK(RouteInput(input,ctx).owner == InputOwner::Stop);
    input = {}; ctx = {}; ctx.windowFocused = true; ctx.mode = InputMode::Playing;
    CHECK(RouteInput(input,ctx).owner == InputOwner::Pause);
    ctx.viewportVisible = true; CHECK(RouteInput(input,ctx).owner == InputOwner::UI);
    ctx.focused = true; ctx.windowFocused = false;
    CHECK(RouteInput(input,ctx).owner == InputOwner::Pause);
}
void AnimationTests() {
    auto walk = MakeWalk();
    std::string error;
    CHECK(walk.validate(error));
    CHECK(walk.tracks[LeftHip].size() == 5);
    for (int quarter = 0; quarter <= 4; ++quarter) {
        auto pose = walk.sample(walk.duration * quarter / 4);
        for (const auto& joint : pose) CHECK(Valid(joint));
        const auto leg = EulerDegreesFromQuat(pose[LeftHip].rotation).x;
        const auto arm = EulerDegreesFromQuat(pose[RightShoulder].rotation).x;
        CHECK(std::fabs(arm - leg * .8f) < .001f);
        CHECK(std::fabs(leg + EulerDegreesFromQuat(pose[RightHip].rotation).x) < .001f);
    }
    CHECK(NearlyEqual(walk.sample(0)[LeftHip].rotation, walk.sample(walk.duration)[LeftHip].rotation));
    CHECK(NearlyEqual(walk.sample(.0001f)[LeftHip].rotation,
                      walk.sample(walk.duration - .0001f)[LeftHip].rotation, .0001f));
    AnimationClip clip = MakeIdle(); clip.loop = false; clip.duration = 1;
    clip.setKey(Neck, {0, {{}, QuatFromEulerDegrees({0, 170, 0})}, Interpolation::Linear});
    clip.setKey(Neck, {1, {{}, QuatFromEulerDegrees({0, -170, 0})}});
    auto half = clip.sample(.5f)[Neck].rotation;
    CHECK(std::fabs(half.y) > .999f); // short arc through 180, not through zero
    CHECK(clip.validate(error));
    clip.setKey(Neck, {.5f, {{0, .2f, 0}}});
    CHECK(clip.tracks[Neck].size() == 3);
    CHECK(NearlyEqual(clip.sample(.5f)[Neck].position.y, .2f));
    CHECK(clip.eraseKey(Neck, .5f));
    CHECK(!clip.eraseKey(Neck, .5f));
    clip.tracks[Neck][0].interpolation = Interpolation::Constant;
    CHECK(NearlyEqual(clip.sample(.5f)[Neck].rotation, clip.tracks[Neck][0].pose.rotation));
    // Sparse looping tracks interpolate across the boundary.
    clip.loop = true; clip.tracks[Neck].clear();
    clip.setKey(Neck, {.25f, {{0, 0, 0}}, Interpolation::Linear});
    clip.setKey(Neck, {.75f, {{0, 2, 0}}, Interpolation::Linear});
    CHECK(NearlyEqual(clip.sample(0)[Neck].position.y, 1));
    CHECK(NearlyEqual(clip.sample(-1)[Neck].position.y, 1));
    Animator animator; animator.play(walk); animator.advance(.2f);
    CHECK(NearlyEqual(animator.time(), .2f));
    animator.pause(true); animator.advance(.2f);
    CHECK(NearlyEqual(animator.time(), .2f));
    animator.seek(.4f);
    CHECK(NearlyEqual(animator.pose()[LeftHip].rotation, walk.sample(.4f)[LeftHip].rotation));
    animator.stop(); CHECK(!animator.playing());
    CHECK(NearlyEqual(animator.pose()[Neck].rotation, QuaternionIdentity()));
    walk.tracks[0][1].time = 0;
    CHECK(!walk.validate(error));
    GameAssets assets; auto id = assets.createR6().id;
    CHECK(assets.animations.size() == 4);
    assets.animations[0].name = "My edited walk";
    assets.createR6();
    CHECK(assets.animations[0].name == "My edited walk");
    CHECK(assets.defaultRig == id);
    CHECK(assets.validate(error));
    assets.player.gravity = std::numeric_limits<float>::quiet_NaN();
    CHECK(!assets.validate(error));
}
void PersistenceTests() {
    Scene scene; scene.buildDefaultScene();
    auto& rig = scene.gameAssets().createR6();
    rig.bodies[LeftArm].color = RED;
    rig.resizeBody(LeftLeg, {1.2f, 2.3f, 1});
    rig.joints[LeftShoulder].parentAnchor.position.x -= .2f;
    auto* walk = scene.gameAssets().findAnimation("r6-walk");
    walk->setKey(RightShoulder, {.25f, {{}, QuatFromEulerDegrees({42, 5, 0})}});
    scene.gameAssets().player.walkSpeed = 18;
    std::string error;
    CHECK(scene.validate(error));
    const auto saved = projectio::toJson(scene);
    CHECK(nlohmann::json::parse(saved)["version"] == 2);
    Scene loaded;
    CHECK(projectio::fromJson(loaded, saved, error));
    CHECK(projectio::toJson(loaded) == saved);
    auto malformed = nlohmann::json::parse(saved); malformed["format"] = 42;
    CHECK(!projectio::fromJson(loaded, malformed.dump(), error));
    CHECK(projectio::toJson(loaded) == saved);
    malformed = nlohmann::json::parse(saved); malformed["version"] = 4294967298ULL;
    CHECK(!projectio::fromJson(loaded, malformed.dump(), error));
    CHECK(projectio::toJson(loaded) == saved);
    CHECK(loaded.gameAssets().rigs[0].bodies[LeftArm].color.r == RED.r);
    CHECK(NearlyEqual(loaded.gameAssets().player.walkSpeed, 18));
    auto invalid = nlohmann::json::parse(saved);
    invalid["game"]["rigs"][0]["joints"][0]["parent"] = 0;
    CHECK(!projectio::fromJson(loaded, invalid.dump(), error));
    CHECK(projectio::toJson(loaded) == saved);
    invalid = nlohmann::json::parse(saved);
    invalid["game"]["animations"][0]["tracks"][0][1]["time"] = 0;
    CHECK(!projectio::fromJson(loaded, invalid.dump(), error));
    CHECK(projectio::toJson(loaded) == saved);
    invalid = nlohmann::json::parse(saved);
    invalid["game"]["player"]["walkAnimation"] = "missing";
    CHECK(!projectio::fromJson(loaded, invalid.dump(), error));
    CHECK(projectio::toJson(loaded) == saved);
    invalid = nlohmann::json::parse(saved);
    invalid["game"]["animations"][0]["fps"] = 30.5;
    CHECK(!projectio::fromJson(loaded, invalid.dump(), error));
    CHECK(projectio::toJson(loaded) == saved);
    invalid["game"]["animations"][0]["fps"] = 4294967326ULL;
    CHECK(!projectio::fromJson(loaded, invalid.dump(), error));
    CHECK(projectio::toJson(loaded) == saved);
    Scene legacy; legacy.buildDefaultScene();
    CHECK(projectio::fromJson(loaded, projectio::toJson(legacy), error));
    CHECK(loaded.gameAssets().rigs.empty());
    CHECK(loaded.gameAssets().player.validate(error));
    loaded.gameAssets().player.walkAnimation = "missing-without-rig";
    CHECK(!loaded.validate(error));
}
void Advance(RuntimeSession& runtime, float seconds, PlayerInput input = {}, float step = 1.f/60) {
    for (int i = 0; i < static_cast<int>(seconds/step+.5f); ++i) {
        runtime.update(step, input); input.jump = false;
    }
}
void RuntimeTests() {
    Scene author; author.buildDefaultScene();
    author.gameAssets().createR6();
    auto* wall = author.createPart(PartShape::Block, {0, 5, -15}, {30, 10, 1}, "Wall");
    wall->setAnchored(true);
    const std::string original = projectio::toJson(author);
    RuntimeSession runtime; std::string error;
    CHECK(runtime.start(author, error));
    CHECK(runtime.validate(error));
    CHECK(runtime.bodyCount() == author.parts().size()+1);
    Advance(runtime, 1);
    CHECK(runtime.grounded());
    CHECK(runtime.position().y > 3.7f && runtime.position().y < 4.1f); // spawn top 1 + half capsule
    CHECK(runtime.humanoidState() == HumanoidState::Idle);
    const auto spawn = runtime.position();
    PlayerInput forward; forward.movement = {0, 1};
    Advance(runtime, .4f, forward);
    CHECK(runtime.position().z < spawn.z - 3);
    CHECK(runtime.humanoidState() == HumanoidState::Walking);
    CHECK(std::fabs(runtime.pose()[LeftHip].rotation.x) > .01f);
    const auto walkPose = runtime.pose()[LeftHip].rotation;
    Advance(runtime, .13f, forward);
    CHECK(!NearlyEqual(walkPose, runtime.pose()[LeftHip].rotation, .0001f));
    Advance(runtime, 2, forward);
    CHECK(runtime.position().z > -13.7f); // wall face -14.5 + capsule radius
    Advance(runtime, .5f);
    CHECK(runtime.humanoidState() == HumanoidState::Idle);
    CHECK(std::hypot(runtime.velocity().x, runtime.velocity().z) < .01f);
    CHECK(runtime.respawn(error));
    Advance(runtime, .5f);
    const float groundedY = runtime.position().y;
    PlayerInput jump; jump.jump = true;
    runtime.update(1.f/60, jump);
    CHECK(runtime.velocity().y > 10);
    CHECK(runtime.humanoidState() == HumanoidState::Jumping);
    const auto airPose = runtime.pose()[LeftShoulder].rotation;
    const float firstJump = runtime.velocity().y;
    runtime.update(1.f/60, jump); // a repeated press in midair cannot jump again
    CHECK(runtime.velocity().y < firstJump);
    Advance(runtime, .2f);
    CHECK(runtime.position().y > groundedY+3);
    Advance(runtime, 1.5f);
    CHECK(runtime.grounded());
    CHECK(std::fabs(runtime.position().y-groundedY) < .1f);
    CHECK(!NearlyEqual(airPose, runtime.pose()[LeftShoulder].rotation, .001f));
    runtime.pause(); const auto paused = runtime.position(); const auto pose = runtime.pose();
    Advance(runtime, 1, forward);
    CHECK(NearlyEqual(runtime.position(), paused));
    CHECK(NearlyEqual(runtime.pose()[LeftHip].rotation, pose[LeftHip].rotation));
    runtime.resume(); Advance(runtime, .3f, forward);
    CHECK(runtime.position().z < paused.z - 1);
    for (int i = 0; i < 20; ++i) { CHECK(runtime.respawn(error)); CHECK(runtime.validate(error)); }
    CHECK(runtime.bodyCount() == author.parts().size()+1);
    CHECK(projectio::toJson(author) == original);
    const auto resetPosition = runtime.position();
    runtime.settings().collisionHeight = -1;
    CHECK(!runtime.respawn(error));
    CHECK(NearlyEqual(runtime.position(), resetPosition));
    CHECK(runtime.bodyCount() == author.parts().size()+1);
    runtime.settings().collisionHeight = 5.8f;
    PlayerInput invalidInput; invalidInput.movement.x = std::numeric_limits<float>::quiet_NaN();
    runtime.update(.1f, invalidInput);
    CHECK(NearlyEqual(runtime.position(),resetPosition));
    CHECK(runtime.validate(error));
    runtime.stop(); CHECK(runtime.bodyCount() == 0); CHECK(runtime.validate(error));
    for (int i = 0; i < 10; ++i) {
        CHECK(runtime.start(author, error)); Advance(runtime, .1f); runtime.stop();
        CHECK(runtime.validate(error)); CHECK(projectio::toJson(author) == original);
    }
    // Frame-rate independence, normalized diagonals and camera-relative movement.
    Scene flat; flat.createBaseplate({0, -.5f, 0}, {200, 1, 200});
    flat.gameAssets().createR6();
    CHECK(runtime.start(flat, error)); Advance(runtime, .5f);
    auto begin = runtime.position(); Advance(runtime, 1, forward);
    const float distance = Vector3Distance(begin, runtime.position()); runtime.stop();
    CHECK(runtime.start(flat, error)); Advance(runtime, .5f);
    begin = runtime.position(); PlayerInput diagonal; diagonal.movement = {1, 1};
    Advance(runtime, 1, diagonal);
    CHECK(std::fabs(Vector3Distance(begin, runtime.position())-distance) < .1f);
    runtime.stop(); CHECK(runtime.start(flat, error)); Advance(runtime, .5f);
    begin = runtime.position(); Advance(runtime, 1, forward, 1.f/15);
    CHECK(std::fabs(Vector3Distance(begin, runtime.position())-distance) < .1f);
    runtime.stop(); CHECK(runtime.start(flat, error)); Advance(runtime, .5f);
    PlayerInput look; look.look = {-PI/.003f/2, 100000};
    runtime.update(0, look);
    CHECK(runtime.playerCamera().pitch() <= 1.35f);
    auto direction = runtime.playerCamera().moveDirection({0, 1});
    CHECK(direction.x < -.99f && std::fabs(direction.z) < .01f);
    runtime.settings().cameraMode = game::CameraMode::FirstPerson; runtime.update(.016f, {});
    CHECK(runtime.playerCamera().firstPerson());
    runtime.settings().cameraMode = game::CameraMode::Classic;
    PlayerInput zoom; zoom.zoom = 100; runtime.update(.016f, zoom);
    CHECK(runtime.playerCamera().firstPerson());
    zoom.zoom = -100; runtime.update(.016f, zoom);
    CHECK(!runtime.playerCamera().firstPerson());
    CHECK(runtime.playerCamera().distance() <= runtime.settings().maxZoom);
    runtime.settings().allowFirstPerson = false;
    zoom.zoom = 100; runtime.update(0,zoom);
    const auto head = runtime.rig().evaluate(runtime.pose(),runtime.characterRoot())[Head];
    const auto cameraInHead = Compose(Inverse(head),{runtime.playerCamera().camera().position}).position;
    const auto headHalf = Vector3Scale(runtime.rig().bodies[Head].size,.5f);
    CHECK(!runtime.playerCamera().firstPerson());
    CHECK(std::fabs(cameraInHead.x) > headHalf.x || std::fabs(cameraInHead.y) > headHalf.y ||
          std::fabs(cameraInHead.z) > headHalf.z);
    runtime.settings().allowFirstPerson = true;
    zoom.zoom = -100; runtime.update(.016f,zoom);
    runtime.settings().cameraMode = game::CameraMode::Follow;
    const float followYaw = runtime.playerCamera().yaw();
    PlayerInput strafe; strafe.movement = {1,0}; Advance(runtime,3,strafe);
    CHECK(std::fabs(std::remainder(runtime.playerCamera().yaw()-followYaw,2*PI)) > .1f);
    CHECK(Valid({runtime.playerCamera().camera().position}));
    runtime.stop();
    // CanCollide=false does not block the capsule or the camera sweep.
    auto* ghost = flat.createPart(PartShape::Block, {0, 4, -8}, {30, 8, 1}, "Ghost wall");
    ghost->setAnchored(true); ghost->setCanCollide(false);
    CHECK(runtime.start(flat, error)); Advance(runtime, .5f); Advance(runtime, 1, forward);
    CHECK(runtime.position().z < -10);
    CHECK(NearlyEqual(runtime.cameraClearance({0, 4, 0}, {0, 4, -10}, .25f), 1));
    runtime.stop(); ghost->setCanCollide(true);
    CHECK(runtime.start(flat, error));
    CHECK(runtime.cameraClearance({0, 4, 0}, {0, 4, -10}, .25f) < .8f);
    runtime.stop();
    // Eye height follows edited proportions, not a hard-coded torso offset.
    flat.gameAssets().rigs[0].resizeBody(Torso,{2,4,1});
    flat.gameAssets().rigs[0].resizeBody(Head,{2,2,1});
    flat.gameAssets().player.collisionHeight = 9;
    flat.gameAssets().player.cameraMode = game::CameraMode::FirstPerson;
    CHECK(runtime.start(flat,error)); Advance(runtime,.5f);
    const auto tallHead = runtime.rig().evaluate(runtime.pose(),runtime.characterRoot())[Head];
    CHECK(Vector3Distance(runtime.playerCamera().camera().position,tallHead.position) < .2f);
    CHECK(runtime.playerCamera().avatarHidden());
    runtime.stop();
    // A side wall can force the camera into the wide head, even though the
    // narrow collision capsule itself fits. Hide the avatar, not the wall,
    // without forcing first-person controls or rotating the character.
    Scene side; side.createBaseplate({0,-.5f,0},{200,1,200}); side.gameAssets().createR6();
    auto* sideWall = side.createPart(PartShape::Block,{1.3f,5,0},{.2f,10,30}); sideWall->setAnchored(true);
    CHECK(runtime.start(side,error)); Advance(runtime,.5f);
    PlayerInput sideLook; sideLook.look.x = -PI/(2*runtime.settings().sensitivity);
    runtime.update(.016f,sideLook);
    CHECK(!runtime.playerCamera().firstPerson());
    CHECK(runtime.playerCamera().avatarHidden());
    CHECK(NearlyEqual(runtime.characterRoot().rotation,QuaternionIdentity()));
    runtime.stop();
}
void PhysicsPlayTests() {
    std::string error;
    // A very high spawn above a 0.2-thick platform exercises the full-capsule
    // fall sweep; Bullet's centre-sphere CCD alone can miss the capsule foot.
    {
        Scene scene;
        auto* thin = scene.createPart(PartShape::Block, {0, 2, 0}, {8, .2f, 8}, "Thin");
        thin->setAnchored(true);
        auto* marker = scene.createPart(PartShape::Block, {0, 100, 0}, {.2f, .2f, .2f}, "HighSpawn");
        marker->setAnchored(true); marker->setCanCollide(false);
        marker->setAttribute("spawn", true);
        RuntimeSession runtime;
        CHECK(runtime.start(scene, error));
        Advance(runtime, 2.0f);
        CHECK(runtime.state() == RuntimeState::Playing);
        CHECK(runtime.position().y > 4.6f && runtime.position().y < 5.5f);
        CHECK(runtime.grounded());
        const float landed = runtime.position().y;
        Advance(runtime, 1.0f);
        CHECK(runtime.grounded());
        CHECK(std::fabs(runtime.position().y - landed) < .05f);
        runtime.stop();
    }
    // A thin rotated platform must still be a valid support, not a surface the
    // capsule visually clips through or slides off while standing still.
    {
        Scene scene;
        auto* ramp = scene.createPart(PartShape::Block, {0, 2, 0}, {10, .2f, 10}, "Ramp");
        ramp->setAnchored(true); ramp->setRotationEulerDegrees({0, 0, 22});
        auto* marker = scene.createPart(PartShape::Block, {0, 15, 0}, {.2f, .2f, .2f}, "RampSpawn");
        marker->setAnchored(true); marker->setCanCollide(false);
        marker->setAttribute("spawn", true);
        RuntimeSession runtime;
        CHECK(runtime.start(scene, error));
        Advance(runtime, 2.0f);
        CHECK(runtime.state() == RuntimeState::Playing);
        CHECK(runtime.position().y > 3.0f);
        CHECK(runtime.grounded());
        const float landed = runtime.position().y;
        Advance(runtime, 1.0f);
        CHECK(runtime.grounded());
        CHECK(std::fabs(runtime.position().y - landed) < .08f);
        runtime.stop();
    }
    // Coyote time: leave a ledge, then press jump inside the grace window.
    {
        Scene scene;
        auto* ledge = scene.createPart(PartShape::Block, {0, 2, 0}, {6, .2f, 6}, "Ledge");
        ledge->setAnchored(true);
        auto* marker = scene.createPart(PartShape::Block, {0, 2, 0}, {.2f, .2f, .2f}, "LedgeSpawn");
        marker->setAnchored(true); marker->setCanCollide(false);
        marker->setAttribute("spawn", true);
        RuntimeSession runtime;
        CHECK(runtime.start(scene, error));
        Advance(runtime, .5f);
        PlayerInput forward; forward.movement = {0, 1};
        int steps = 0;
        while (runtime.grounded() && steps++ < 120) runtime.update(1.f/60, forward);
        CHECK(!runtime.grounded());
        PlayerInput jump; jump.jump = true;
        runtime.update(1.f/60, jump);
        CHECK(runtime.velocity().y > 5);
        CHECK(runtime.humanoidState() == HumanoidState::Jumping);
        runtime.stop();
    }
    // Jump buffer: press in the air just before landing; the landing step must
    // consume the edge and leave the ground immediately.
    {
        Scene scene;
        scene.createBaseplate({0, -.5f, 0}, {200, 1, 200});
        auto* marker = scene.createPart(PartShape::Block, {0, 10, 0}, {.2f, .2f, .2f}, "AirSpawn");
        marker->setAnchored(true); marker->setCanCollide(false);
        marker->setAttribute("spawn", true);
        RuntimeSession runtime;
        CHECK(runtime.start(scene, error));
        int steps = 0;
        while (runtime.position().y > 5.0f && steps++ < 240)
            runtime.update(1.f/60, {});
        CHECK(!runtime.grounded());
        CHECK(runtime.position().y > 4.0f);
        PlayerInput jump; jump.jump = true;
        runtime.update(1.f/60, jump);
        Advance(runtime, .2f);
        CHECK(runtime.velocity().y > 5 || runtime.position().y > 3.7f);
        runtime.stop();
    }
    // Holding jump through a landing must not create an infinite bunny hop.
    {
        Scene scene;
        scene.createBaseplate({0, -.5f, 0}, {200, 1, 200});
        RuntimeSession runtime;
        CHECK(runtime.start(scene, error));
        Advance(runtime, .5f);
        PlayerInput held; held.jump = true;
        runtime.update(1.f/60, held);
        for (int i = 0; i < 180; ++i) runtime.update(1.f/60, held);
        CHECK(runtime.grounded());
        CHECK(runtime.velocity().y < 1);
        CHECK(runtime.humanoidState() != HumanoidState::Jumping);
        runtime.stop();
    }
    // A small obby gap: jump from the start platform to a lower/far platform.
    {
        Scene scene;
        auto* start = scene.createPart(PartShape::Block, {0, 2, 0}, {6, .2f, 6}, "Start");
        start->setAnchored(true);
        auto* landing = scene.createPart(PartShape::Block, {0, 2, -10}, {10, .2f, 8}, "Landing");
        landing->setAnchored(true);
        auto* marker = scene.createPart(PartShape::Block, {0, 2, 0}, {.2f, .2f, .2f}, "StartSpawn");
        marker->setAnchored(true); marker->setCanCollide(false);
        marker->setAttribute("spawn", true);
        RuntimeSession runtime;
        CHECK(runtime.start(scene, error));
        Advance(runtime, .5f);
        PlayerInput move; move.movement = {0, 1}; move.jump = true;
        runtime.update(1.f/60, move);
        move.jump = false;
        int steps = 0;
        while (steps++ < 120) {
            runtime.update(1.f/60, move);
            if (runtime.grounded() && runtime.position().z < -5.5f) break;
        }
        CHECK(runtime.position().z < -5.5f);
        CHECK(runtime.grounded());
        CHECK(runtime.humanoidState() != HumanoidState::Dead);
        runtime.stop();
    }
    // Movement and jump/landing are stable across common display rates.
    {
        Scene flat; flat.createBaseplate({0, -.5f, 0}, {200, 1, 200});
        flat.gameAssets().createR6();
        RuntimeSession runtime;
        float referenceDistance = 0;
        for (float step : {1.f/60, 1.f/144, 1.f/30, 1.f/15}) {
            CHECK(runtime.start(flat, error));
            Advance(runtime, .5f, {}, step);
            const auto begin = runtime.position();
            PlayerInput forward; forward.movement = {0, 1};
            Advance(runtime, 1.0f, forward, step);
            const float moved = Vector3Distance(begin, runtime.position());
            if (referenceDistance == 0) referenceDistance = moved;
            else CHECK(std::fabs(moved - referenceDistance) < .12f);
            runtime.stop();
        }
        for (float step : {1.f/60, 1.f/144, 1.f/30, 1.f/15}) {
            CHECK(runtime.start(flat, error));
            Advance(runtime, .5f, {}, step);
            const float ground = runtime.position().y;
            PlayerInput jump; jump.jump = true;
            runtime.update(step, jump);
            Advance(runtime, .3f, {}, step);
            CHECK(runtime.position().y > ground + 2);
            Advance(runtime, 1.5f, {}, step);
            CHECK(runtime.grounded());
            CHECK(std::fabs(runtime.position().y - ground) < .12f);
            runtime.stop();
        }
    }
}
}  // namespace
int main(int argc, char** argv) {
    char temporary[] = "/tmp/loblox-game-headless-XXXXXX";
    if (const char* path = mkdtemp(temporary)) loblox::diag::configure(std::string(path)+"/game.jsonl");
    if (argc == 3 && std::string(argv[1]) == "--write-fixture") {
        Scene scene; scene.buildDefaultScene();
        auto& rig = scene.gameAssets().createR6();
        rig.bodies[LeftArm].color = {210,50,140,255}; rig.resizeBody(LeftLeg, {1.2f,2.3f,1});
        scene.gameAssets().findAnimation("r6-walk")->setKey(RightShoulder, {.25f, {{}, QuatFromEulerDegrees({55,5,0})}});
        scene.gameAssets().player.walkSpeed = 18;
        std::string error;
        if (!projectio::save(scene, argv[2], error)) { std::fprintf(stderr,"%s\n",error.c_str()); return 1; }
        return 0;
    }
    RigTests(); InputTests(); AnimationTests(); PersistenceTests(); RuntimeTests(); PhysicsPlayTests();
    {
        Scene scene;scene.buildDefaultScene();scene.gameAssets().createR6();
        scene.gameAssets().player.enableShiftLock=true;
        scene.gameAssets().player.cameraMode=game::CameraMode::Follow;
        RuntimeSession runtime;std::string error;CHECK(runtime.start(scene,error));
        Advance(runtime,.5f);runtime.setShiftLockRequested(true);
        PlayerInput look;look.look={30,-12};runtime.update(1.f/60,look);
        const float yaw=runtime.playerCamera().yaw();
        CHECK(std::fabs(yaw+30*runtime.settings().sensitivity)<.0001f);
        PlayerInput strafe;strafe.movement={1,0};Advance(runtime,2,strafe);
        CHECK(std::fabs(runtime.playerCamera().yaw()-yaw)<.0001f); // Follow must not fight lock.
        // An arbitrary animated head offset must never change locked framing.
        auto& pose=const_cast<Pose&>(runtime.pose());const auto saved=pose;
        PlayerCamera a,b;a.reset(runtime.position(),runtime.settings());b.reset(runtime.position(),runtime.settings());
        a.update(.016f,runtime.position(),{}, {},runtime.settings(),runtime);
        pose[Neck].position={0,8,0};
        b.update(.016f,runtime.position(),{}, {},runtime.settings(),runtime);
        CHECK(NearlyEqual(a.camera().position,b.camera().position,.0001f));pose=saved;
        for(int jump=0;jump<3;++jump) {
            Advance(runtime,1);PlayerInput input;input.jump=true;
            auto before=runtime.playerCamera().camera().position;runtime.update(1.f/60,input);
            CHECK(Vector3Distance(before,runtime.playerCamera().camera().position)<.4f);
        }
        runtime.stop();
    }
    std::printf("Game: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

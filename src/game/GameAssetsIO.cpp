#include "game/GameAssetsIO.h"

#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace loblox::game {
namespace {
using json = nlohmann::json;
json Vec(Vector3 v) { return json::array({v.x, v.y, v.z}); }
json Transform(const RigidTransform& t) {
    return {{"position", Vec(t.position)}, {"rotation", {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w}}};
}
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int ReadInt(const json& v, int low, int high) {
    Require(v.is_number_integer(), "expected integer");
    const double n = v.get<double>();
    Require(n >= low && n <= high, "integer out of range");
    return v.get<int>();
}
Vector3 ReadVec(const json& v) {
    Require(v.is_array() && v.size() == 3, "expected vector[3]");
    return {v.at(0).get<float>(), v.at(1).get<float>(), v.at(2).get<float>()};
}
RigidTransform ReadTransform(const json& v) {
    const auto& q = v.at("rotation");
    Require(q.is_array() && q.size() == 4, "expected quaternion[4]");
    RigidTransform t{ReadVec(v.at("position")), {q.at(0).get<float>(), q.at(1).get<float>(), q.at(2).get<float>(), q.at(3).get<float>()}};
    Require(Valid(t), "nonfinite transform or nonunit quaternion");
    return t;
}
}
nlohmann::json EncodeAssets(const GameAssets& a) {
    json j{{"version", 1}, {"defaultRig", a.defaultRig}, {"rigs", json::array()}, {"animations", json::array()}};
    if(!a.sounds.empty()) {
        j["sounds"]=json::array();
        for(const auto& sound:a.sounds)j["sounds"].push_back({{"id",sound.id},{"name",sound.name},
            {"reference",sound.reference},{"volume",sound.volume},{"loop",sound.loop},{"kind",sound.kind}});
    }
    for (const auto& r : a.rigs) {
        json rig{{"id", r.id}, {"name", r.name}, {"root", Transform(r.root)},
                 {"bodies", json::array()}, {"joints", json::array()}};
        if(r.nextDecalId>1)rig["nextDecalId"]=r.nextDecalId;
        for (const auto& b : r.bodies) {
            json body{{"name", b.name}, {"size", Vec(b.size)},
                      {"color", {b.color.r, b.color.g, b.color.b, b.color.a}},
                      {"material", PartMaterialName(b.material)}};
            if (!b.image.empty()) body["image"] = b.image;
            if(!b.decals.empty()) {
                body["decals"]=json::array();
                for(const auto& d:b.decals)body["decals"].push_back({{"id",d.id},{"name",d.name},
                    {"image",d.surface.image},{"face",d.surface.face},{"transparency",d.surface.transparency}});
            }
            rig["bodies"].push_back(std::move(body));
        }
        for (int i = 0; i < kJointCount; ++i) {
            const auto& joint = r.joints[i];
            rig["joints"].push_back({{"name", joint.name}, {"parent", joint.parent}, {"child", joint.child},
                {"parentAnchor", Transform(joint.parentAnchor)}, {"childAnchor", Transform(joint.childAnchor)},
                {"restPose", Transform(r.restPose[i])}});
        }
        j["rigs"].push_back(std::move(rig));
    }
    for (const auto& clip : a.animations) {
        json c{{"version", clip.version}, {"id", clip.id}, {"name", clip.name}, {"rigType", clip.rigType},
            {"duration", clip.duration}, {"fps", clip.fps}, {"loop", clip.loop},
            {"priority", static_cast<int>(clip.priority)}, {"tracks", json::array()}};
        for (const auto& track : clip.tracks) {
            json t = json::array();
            for (const auto& key : track) {
                json k{{"time", key.time}, {"pose", Transform(key.pose)},
                       {"interpolation", static_cast<int>(key.interpolation)}};
                // Only authored long-way sweeps add the field; legacy clips keep
                // their previous JSON byte for byte.
                if (std::fabs(key.arcDegrees) > 1e-4f) k["arc"] = key.arcDegrees;
                t.push_back(std::move(k));
            }
            c["tracks"].push_back(std::move(t));
        }
        j["animations"].push_back(std::move(c));
    }
    const auto& p = a.player;
    j["player"] = {{"walkSpeed", p.walkSpeed}, {"turnSpeed", p.turnSpeed}, {"acceleration", p.acceleration},
        {"deceleration", p.deceleration}, {"jumpHeight", p.jumpHeight}, {"gravity", p.gravity},
        {"maxHealth", p.maxHealth}, {"health", p.health}, {"autoRotate", p.autoRotate},
        {"collisionHeight", p.collisionHeight}, {"collisionRadius", p.collisionRadius},
        {"cameraMode", static_cast<int>(p.cameraMode)}, {"minZoom", p.minZoom}, {"maxZoom", p.maxZoom},
        {"cameraDistance", p.cameraDistance}, {"cameraFov",p.cameraFov}, {"sensitivity", p.sensitivity},
        {"allowFirstPerson", p.allowFirstPerson}, {"cameraLock", p.cameraLock},
        {"walkAnimation", p.walkAnimation}, {"idleAnimation", p.idleAnimation}};
    if (!p.jumpAnimation.empty()) j["player"]["jumpAnimation"]=p.jumpAnimation;
    if (!p.fallAnimation.empty()) j["player"]["fallAnimation"]=p.fallAnimation;
    if (p.respawnDelay!=2) j["player"]["respawnDelay"]=p.respawnDelay;
    if (p.healthDisplayType!=2) j["player"]["healthDisplayType"]=p.healthDisplayType;
    if (p.healthDisplayDistance!=45) j["player"]["healthDisplayDistance"]=p.healthDisplayDistance;
    if (p.nameDisplayDistance!=0) j["player"]["nameDisplayDistance"]=p.nameDisplayDistance;
    if (p.enableShiftLock) j["player"]["enableShiftLock"]=true;
    return j;
}
bool DecodeAssets(const nlohmann::json& j, GameAssets& assets, std::string& error) {
    try {
        GameAssets a;
        ReadInt(j.at("version"), 1, 1);
        a.defaultRig = j.at("defaultRig").get<std::string>();
        const auto& rigs = j.at("rigs");
        const auto& clips = j.at("animations");
        Require(rigs.is_array() && rigs.size() <= 128 && clips.is_array() && clips.size() <= 256, "asset limit or invalid array");
        for (const auto& value : rigs) {
            Rig r;
            r.id = value.at("id").get<std::string>(); r.name = value.at("name").get<std::string>();
            r.root = ReadTransform(value.at("root"));
            r.nextDecalId=value.value("nextDecalId",1ull);
            const auto& bodies = value.at("bodies"); const auto& joints = value.at("joints");
            Require(bodies.is_array() && bodies.size() == kBodyCount && joints.is_array() && joints.size() == kJointCount, "R6 needs six bodies and six joints");
            for (int i = 0; i < kBodyCount; ++i) {
                const auto& b = bodies.at(i); auto& body = r.bodies[i];
                body.name = b.at("name").get<std::string>(); body.size = ReadVec(b.at("size"));
                const auto& c = b.at("color"); Require(c.is_array() && c.size() == 4, "invalid body color");
                unsigned char rgba[4];
                for (int ch = 0; ch < 4; ++ch) rgba[ch] = static_cast<unsigned char>(ReadInt(c.at(ch), 0, 255));
                body.color = {rgba[0], rgba[1], rgba[2], rgba[3]};
                Require(PartMaterialFromName(b.at("material").get<std::string>(), body.material), "unknown body material");
                body.image = b.value("image", std::string());
                if(b.contains("decals")) {
                    Require(b["decals"].is_array()&&b["decals"].size()<=64,"invalid rig decals array");
                    for(const auto& d:b["decals"]) {
                        RigDecal decal;decal.id=d.at("id").get<std::string>();decal.name=d.at("name").get<std::string>();
                        decal.surface.image=d.at("image").get<std::string>();decal.surface.face=ReadInt(d.at("face"),0,5);
                        decal.surface.transparency=d.value("transparency",0.f);body.decals.push_back(std::move(decal));
                    }
                }
            }
            for (int i = 0; i < kJointCount; ++i) {
                const auto& v = joints.at(i); auto& joint = r.joints[i];
                joint.name = v.at("name").get<std::string>();
                joint.parent = ReadInt(v.at("parent"), -1, 5); joint.child = ReadInt(v.at("child"), 0, 5);
                joint.parentAnchor = ReadTransform(v.at("parentAnchor")); joint.childAnchor = ReadTransform(v.at("childAnchor"));
                r.restPose[i] = ReadTransform(v.at("restPose"));
            }
            a.rigs.push_back(std::move(r));
        }
        std::size_t totalKeys = 0;
        for (const auto& v : clips) {
            AnimationClip c;
            c.version = ReadInt(v.at("version"), 1, 1); c.id = v.at("id").get<std::string>();
            c.name = v.at("name").get<std::string>(); c.rigType = v.at("rigType").get<std::string>();
            c.duration = v.at("duration").get<float>(); c.fps = ReadInt(v.at("fps"), 1, 240); c.loop = v.at("loop").get<bool>();
            c.priority = static_cast<AnimationPriority>(ReadInt(v.at("priority"), 0, 2));
            const auto& tracks = v.at("tracks");
            Require(tracks.is_array() && tracks.size() == kJointCount, "invalid animation tracks");
            for (int i = 0; i < kJointCount; ++i) {
                Require(tracks.at(i).is_array() && tracks.at(i).size() <= 10000, "keyframe limit or invalid track");
                totalKeys += tracks.at(i).size(); Require(totalKeys <= 100000, "project keyframe budget exceeded");
                for (const auto& key : tracks.at(i)) {
                    const float arc = key.contains("arc") ? key.at("arc").get<float>() : 0.0f;
                    Require(std::isfinite(arc) && std::fabs(arc) <= 36000.0f, "invalid keyframe arc");
                    c.tracks[i].push_back({key.at("time").get<float>(), ReadTransform(key.at("pose")),
                        static_cast<Interpolation>(ReadInt(key.at("interpolation"), 0, 2)), arc});
                }
            }
            a.animations.push_back(std::move(c));
        }
        if(j.contains("sounds")) {
            const auto& sounds=j.at("sounds");
            Require(sounds.is_array()&&sounds.size()<=256,"invalid sound library");
            for(const auto& value:sounds) {
                SoundAsset sound;
                sound.id=value.at("id").get<std::string>();sound.name=value.at("name").get<std::string>();
                sound.reference=value.at("reference").get<std::string>();
                sound.filename=std::filesystem::path(sound.reference).filename().string();
                sound.volume=value.value("volume",1.f);sound.loop=value.value("loop",false);
                sound.kind=value.value("kind",std::string("sound"));
                a.sounds.push_back(std::move(sound));
            }
        }
        const auto& p = j.at("player"); auto& out = a.player;
#define READ_FLOAT(field) out.field = p.at(#field).get<float>()
        READ_FLOAT(walkSpeed); READ_FLOAT(turnSpeed); READ_FLOAT(acceleration); READ_FLOAT(deceleration);
        READ_FLOAT(jumpHeight); READ_FLOAT(gravity); READ_FLOAT(maxHealth); READ_FLOAT(health);
        READ_FLOAT(collisionHeight); READ_FLOAT(collisionRadius); READ_FLOAT(minZoom); READ_FLOAT(maxZoom);
        READ_FLOAT(cameraDistance); READ_FLOAT(sensitivity);
        out.cameraFov=p.value("cameraFov",60.f);
#undef READ_FLOAT
        out.autoRotate = p.at("autoRotate").get<bool>();
        out.cameraMode = static_cast<CameraMode>(ReadInt(p.at("cameraMode"), 0, 2));
        out.allowFirstPerson = p.at("allowFirstPerson").get<bool>(); out.cameraLock = p.at("cameraLock").get<bool>();
        out.walkAnimation = p.at("walkAnimation").get<std::string>(); out.idleAnimation = p.at("idleAnimation").get<std::string>();
        out.jumpAnimation=p.value("jumpAnimation",std::string());
        out.fallAnimation=p.value("fallAnimation",std::string());
        out.respawnDelay=p.value("respawnDelay",2.f);out.healthDisplayType=p.contains("healthDisplayType")?ReadInt(p.at("healthDisplayType"),0,2):2;
        out.healthDisplayDistance=p.value("healthDisplayDistance",45.f);out.nameDisplayDistance=p.value("nameDisplayDistance",0.f);
        out.enableShiftLock=p.value("enableShiftLock",false);
        if (!a.validate(error)) return false;
        assets = std::move(a); error.clear(); return true;
    } catch (const std::exception& e) { error = std::string("game assets: ") + e.what(); return false; }
}
} // namespace loblox::game

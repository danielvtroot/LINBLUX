#include "objects/Part.h"

#include <algorithm>
#include <cmath>

namespace loblox {

namespace {

const char* const kShapeNames[] = {"Block", "Sphere"};

const char* const kMaterialNames[] = {"SmoothPlastic", "Plastic", "Wood",
                                      "Metal",         "Concrete", "Neon"};

bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool Finite(Quaternion value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
           std::isfinite(value.w) &&
           (value.x * value.x + value.y * value.y + value.z * value.z +
            value.w * value.w) > 1e-8f;
}

}  // namespace

const char* PartShapeName(PartShape shape) {
    const int index = static_cast<int>(shape);
    if (index < 0 || index >= static_cast<int>(sizeof(kShapeNames) / sizeof(kShapeNames[0]))) {
        return "Block";
    }
    return kShapeNames[index];
}

bool PartShapeFromName(const std::string& name, PartShape& out) {
    if (name == "Block") {
        out = PartShape::Block;
        return true;
    }
    if (name == "Sphere") {
        out = PartShape::Sphere;
        return true;
    }
    return false;
}

const char* PartMaterialName(PartMaterial material) {
    const int index = static_cast<int>(material);
    if (index < 0 ||
        index >= static_cast<int>(sizeof(kMaterialNames) / sizeof(kMaterialNames[0]))) {
        return "SmoothPlastic";
    }
    return kMaterialNames[index];
}

bool PartMaterialFromName(const std::string& name, PartMaterial& out) {
    for (int i = 0; i < PartMaterialCount(); ++i) {
        if (name == kMaterialNames[i]) {
            out = static_cast<PartMaterial>(i);
            return true;
        }
    }
    return false;
}

int PartMaterialCount() {
    return static_cast<int>(sizeof(kMaterialNames) / sizeof(kMaterialNames[0]));
}

PartMaterial PartMaterialFromIndex(int index) {
    if (index < 0 || index >= PartMaterialCount()) {
        return PartMaterial::SmoothPlastic;
    }
    return static_cast<PartMaterial>(index);
}

Part::Part(InstanceId id, std::string name, PartShape shape)
    : Instance(id, std::move(name)), shape_(shape) {}

bool BodyProperties::validate(std::string& error) const {
    const auto in=[](float v,float low,float high){return std::isfinite(v)&&v>=low&&v<=high;};
    if(!in(density,.001f,1000)||!in(friction,0,10)||!in(restitution,0,1)||
       !in(linearDamping,0,1)||!in(angularDamping,0,1)) {
        error="Propriedades físicas fora dos limites: densidade 0,001–1000; atrito 0–10; restituição e amortecimento 0–1.";return false;
    }
    error.clear();return true;
}
bool NpcProperties::validate(std::string& error) const {
    const auto in=[](float v,float low,float high){return std::isfinite(v)&&v>=low&&v<=high;};
    if(!in(speed,0,1000)||!in(maxHealth,1,100000)||!in(respawnDelay,0,3600)||!in(turnSpeed,0,20)) {
        error="Propriedades de NPC fora dos limites: velocidade 0–1000; vida 1–100000; respawn 0–3600 s; giro 0–20.";
        return false;
    }
    if(rig.size()>128 || rig.find_first_of("\r\n")!=std::string::npos) {
        error="Rig de NPC deve ter até 128 bytes, sem quebra de linha.";
        return false;
    }
    error.clear();return true;
}

bool Part::setNpcProperties(const NpcProperties& value,std::string& error) {
    if(!value.validate(error))return false;
    npcProperties_=value;
    // O corpo precisa aplicar/retirar o travamento de rotacao do NPC.
    dirty_.body=true;
    return true;
}

bool Part::setBodyProperties(const BodyProperties& value,std::string& error) {
    if(!value.validate(error))return false;
    if(bodyProperties_.density!=value.density||bodyProperties_.friction!=value.friction||
       bodyProperties_.restitution!=value.restitution||bodyProperties_.linearDamping!=value.linearDamping||
       bodyProperties_.angularDamping!=value.angularDamping){bodyProperties_=value;dirty_.body=true;}
    return true;
}

void Part::setShape(PartShape shape) {
    if (shape_ == shape) {
        return;
    }
    shape_ = shape;
    dirty_.shape = true;
}

void Part::setTransform(const Transform& transform) {
    if (!Finite(transform.position) || !Finite(transform.size) ||
        !Finite(transform.rotation)) {
        return;
    }
    const Transform sanitized{transform.position, CanonicalQuaternion(QuaternionNormalize(transform.rotation)),
                              ClampSize(transform.size)};
    if (NearlyEqual(transform_.position, sanitized.position) &&
        NearlyEqual(transform_.rotation, sanitized.rotation) &&
        NearlyEqual(transform_.size, sanitized.size)) {
        return;
    }
    if (!NearlyEqual(transform_.size, sanitized.size)) {
        dirty_.shape = true;
    }
    transform_ = sanitized;
    dirty_.transform = true;
}

void Part::setPosition(Vector3 position) {
    if (!Finite(position) || NearlyEqual(transform_.position, position)) {
        return;
    }
    transform_.position = position;
    dirty_.transform = true;
}

void Part::setRotation(Quaternion rotation) {
    if (!Finite(rotation)) {
        return;
    }
    const Quaternion normalized = CanonicalQuaternion(QuaternionNormalize(rotation));
    if (NearlyEqual(transform_.rotation, normalized)) {
        return;
    }
    transform_.rotation = normalized;
    dirty_.transform = true;
}

void Part::setRotationEulerDegrees(Vector3 degrees) {
    setRotation(QuatFromEulerDegrees(degrees));
}

void Part::setSize(Vector3 size) {
    if (!Finite(size)) {
        return;
    }
    const Vector3 clamped = ClampSize(size);
    if (NearlyEqual(transform_.size, clamped)) {
        return;
    }
    transform_.size = clamped;
    dirty_.shape = true;
}

float Part::sphereRadius() const {
    return 0.5f * std::min(transform_.size.x, std::min(transform_.size.y, transform_.size.z));
}

void Part::setColor(Color color) {
    color_ = color;
}

void Part::setMaterial(PartMaterial material) {
    material_ = material;
}

void Part::setAnchored(bool anchored) {
    if (anchored_ == anchored) {
        return;
    }
    anchored_ = anchored;
    dirty_.anchored = true;
}

void Part::setCanCollide(bool canCollide) {
    if (canCollide_ == canCollide) {
        return;
    }
    canCollide_ = canCollide;
    dirty_.collide = true;
}

void Part::syncFromPhysics(Vector3 position, Quaternion rotation) {
    if (!Finite(position) || !Finite(rotation)) {
        return;
    }
    transform_.position = position;
    transform_.rotation = CanonicalQuaternion(QuaternionNormalize(rotation));
}

}  // namespace loblox

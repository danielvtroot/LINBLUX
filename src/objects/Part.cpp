#include "objects/Part.h"

#include <algorithm>
#include <cmath>

namespace loblox {

namespace {

const char* const kShapeNames[] = {"Block", "Sphere"};

const char* const kMaterialNames[] = {"SmoothPlastic", "Plastic", "Wood",
                                      "Metal",         "Concrete", "Neon"};

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

void Part::setShape(PartShape shape) {
    if (shape_ == shape) {
        return;
    }
    shape_ = shape;
    dirty_.shape = true;
}

void Part::setTransform(const Transform& transform) {
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
    if (NearlyEqual(transform_.position, position)) {
        return;
    }
    transform_.position = position;
    dirty_.transform = true;
}

void Part::setRotation(Quaternion rotation) {
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
    // A Baseplate e estrutural: nunca pode ser desancorada (cairia da cena).
    if (instanceClass() == InstanceClass::Baseplate && !anchored) {
        return;
    }
    anchored_ = anchored;
    dirty_.anchored = true;
}

void Part::setCanCollide(bool canCollide) {
    if (canCollide_ == canCollide) {
        return;
    }
    // A Baseplate precisa continuar sendo o piso fisico da cena.
    if (instanceClass() == InstanceClass::Baseplate && !canCollide) {
        return;
    }
    canCollide_ = canCollide;
    dirty_.collide = true;
}

void Part::syncFromPhysics(Vector3 position, Quaternion rotation) {
    transform_.position = position;
    transform_.rotation = CanonicalQuaternion(QuaternionNormalize(rotation));
}

}  // namespace loblox

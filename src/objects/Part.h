#pragma once

#include <cstdint>
#include <string>

#include "core/Math.h"
#include "objects/Instance.h"

namespace loblox {

enum class PartShape { Block, Sphere };

enum class PartMaterial {
    SmoothPlastic,
    Plastic,
    Wood,
    Metal,
    Concrete,
    Neon,
};

const char* PartShapeName(PartShape shape);
bool PartShapeFromName(const std::string& name, PartShape& out);

const char* PartMaterialName(PartMaterial material);
bool PartMaterialFromName(const std::string& name, PartMaterial& out);
int PartMaterialCount();
PartMaterial PartMaterialFromIndex(int index);

struct Transform {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 size{4.0f, 4.0f, 4.0f};
};

// Alteracoes pendentes que a camada de fisica deve aplicar no proximo sync.
struct PartDirty {
    bool transform = false;
    bool shape = false;
    bool anchored = false;
    bool collide = false;

    bool any() const { return transform || shape || anchored || collide; }
    void clear() { transform = shape = anchored = collide = false; }
};

class Part : public Instance {
public:
    Part(InstanceId id, std::string name, PartShape shape);

    const char* className() const override { return "Part"; }
    InstanceClass instanceClass() const override { return InstanceClass::Part; }
    bool isPart() const override { return true; }

    PartShape shape() const { return shape_; }
    void setShape(PartShape shape);

    const Transform& transform() const { return transform_; }
    void setTransform(const Transform& transform);

    Vector3 position() const { return transform_.position; }
    Quaternion rotation() const { return transform_.rotation; }
    Vector3 size() const { return transform_.size; }
    Vector3 rotationEulerDegrees() const { return EulerDegreesFromQuat(transform_.rotation); }

    void setPosition(Vector3 position);
    void setRotation(Quaternion rotation);
    void setRotationEulerDegrees(Vector3 degrees);
    void setSize(Vector3 size);

    Vector3 halfExtents() const { return HalfExtents(transform_.size); }
    // A esfera usa o menor eixo como diametro (forma uniforme no Bullet).
    float sphereRadius() const;

    Color color() const { return color_; }
    void setColor(Color color);

    PartMaterial material() const { return material_; }
    void setMaterial(PartMaterial material);

    bool anchored() const { return anchored_; }
    void setAnchored(bool anchored);

    bool canCollide() const { return canCollide_; }
    void setCanCollide(bool canCollide);

    // Escrita vinda do motor fisico: nao marca dirty (evita loop de sync).
    void syncFromPhysics(Vector3 position, Quaternion rotation);

    PartDirty& dirty() { return dirty_; }
    const PartDirty& dirty() const { return dirty_; }
    void clearDirty() { dirty_.clear(); }

private:
    Vector3 ClampSize(Vector3 size) const { return ClampPartSize(size); }

    PartShape shape_;
    Transform transform_;
    Color color_{163, 162, 165, 255};
    PartMaterial material_ = PartMaterial::SmoothPlastic;
    bool anchored_ = false;
    bool canCollide_ = true;
    PartDirty dirty_;
};

}  // namespace loblox

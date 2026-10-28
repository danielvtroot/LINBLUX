#pragma once
#include "objects/Material3D.h"

#include <cstdint>
#include <optional>
#include <string>

#include "core/Math.h"
#include "core/Geometry.h"
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

struct BodyProperties {
    float density = 1.0f;
    float friction = 0.8f;
    float restitution = 0.0f;
    float linearDamping = 0.02f;
    float angularDamping = 0.1f;
    bool validate(std::string& error) const;
};

// Authoring/runtime configuration of an NPC component attached to a regular
// Part. It contains only serializable data; live pathfinding, health and
// references are owned by the runtime NpcSystem and never written to a file.
struct NpcProperties {
    bool enabled = true;
    float speed = 8.0f;
    float maxHealth = 100.0f;
    float respawnDelay = 0.0f; // 0 disables automatic respawn
    float turnSpeed = 8.0f;
    // Rig R6 opcional usado como visual do NPC. Vazio mantém a caixa da Part.
    // Guarda o id do rig (nunca um ponteiro) e aceita "player"/nome na API Lua.
    std::string rig;
    bool validate(std::string& error) const;
};

// Alteracoes pendentes que a camada de fisica deve aplicar no proximo sync.
struct PartDirty {
    bool transform = false;
    bool shape = false;
    bool anchored = false;
    bool collide = false;
    bool body = false;

    bool any() const { return transform || shape || anchored || collide || body; }
    void clear() { transform = shape = anchored = collide = body = false; }
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
    OrientedBox bounds() const { const float r=sphereRadius();return {position(),rotation(),
        shape_==PartShape::Sphere?Vector3{r,r,r}:halfExtents()}; }
    RayCollision intersect(Ray ray) const { return shape_==PartShape::Sphere?
        GetRayCollisionSphere(ray,position(),sphereRadius()):bounds().intersect(ray); }
    // A esfera usa o menor eixo como diametro (forma uniforme no Bullet).
    float sphereRadius() const;

    Color color() const { return color_; }
    void setColor(Color color);

    PartMaterial material() const { return material_; }
    void setMaterial(PartMaterial material);
    const std::optional<Material3D>& material3D() const {return material3D_;}
    bool setMaterial3D(const Material3D& value,std::string& error) {
        if(!value.validate(error))return false;material3D_=value;return true;
    }
    void clearMaterial3D() {material3D_.reset();}

    bool anchored() const { return anchored_; }
    void setAnchored(bool anchored);

    bool canCollide() const { return canCollide_; }
    void setCanCollide(bool canCollide);
    const BodyProperties& bodyProperties() const { return bodyProperties_; }
    bool setBodyProperties(const BodyProperties& properties, std::string& error);

    const std::optional<NpcProperties>& npcProperties() const { return npcProperties_; }
    bool hasNpc() const { return npcProperties_.has_value(); }
    bool setNpcProperties(const NpcProperties& properties, std::string& error);
    // Remover o componente devolve a rotacao livre ao corpo no proximo sync.
    void clearNpcProperties() { if(npcProperties_){npcProperties_.reset();dirty_.body=true;} }

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
    std::optional<Material3D> material3D_;
    bool anchored_ = false;
    bool canCollide_ = true;
    BodyProperties bodyProperties_;
    std::optional<NpcProperties> npcProperties_;
    PartDirty dirty_;
};

}  // namespace loblox

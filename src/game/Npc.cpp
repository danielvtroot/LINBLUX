#include "game/Npc.h"

#include <algorithm>
#include <cmath>

#include "game/Runtime.h"
#include "objects/Part.h"
#include "physics/PhysicsWorld.h"
#include "scene/Scene.h"

namespace loblox::game {
namespace {
float Horizontal(Vector3 value) { return std::sqrt(value.x * value.x + value.z * value.z); }
float HorizontalDistance(Vector3 a, Vector3 b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
float WrapDegrees(float value) {
    while (value > 180.f) value -= 360.f;
    while (value < -180.f) value += 360.f;
    return value;
}
}  // namespace

const char* NpcStateName(NpcState state) {
    switch (state) {
    case NpcState::Idle: return "idle";
    case NpcState::Moving: return "moving";
    case NpcState::Arrived: return "arrived";
    case NpcState::Unreachable: return "unreachable";
    case NpcState::Dead: return "dead";
    }
    return "idle";
}

bool NpcStateFromName(const std::string& name, NpcState& out) {
    if (name == "idle") out = NpcState::Idle;
    else if (name == "moving") out = NpcState::Moving;
    else if (name == "arrived") out = NpcState::Arrived;
    else if (name == "unreachable") out = NpcState::Unreachable;
    else if (name == "dead") out = NpcState::Dead;
    else return false;
    return true;
}

void NpcSystem::reset() {
    controllers_.clear();
    grid_.reset();
    gridDirty_ = true;
    gridParts_ = 0;
}

NpcSystem::Controller* NpcSystem::find(InstanceId part) {
    const auto it = controllers_.find(part);
    return it == controllers_.end() ? nullptr : &it->second;
}

const NpcSystem::Controller* NpcSystem::find(InstanceId part) const {
    const auto it = controllers_.find(part);
    return it == controllers_.end() ? nullptr : &it->second;
}

float NpcSystem::waypointRadius() const {
    // O raio de chegada nunca passa de meia celula: um raio maior poderia
    // "alcancar" um waypoint atraves de uma parede.
    return std::max(.75f, grid_.ready() ? grid_.cellSize() * .35f : .75f);
}

bool NpcSystem::attach(RuntimeSession& runtime, InstanceId part, std::string& error) {
    if (!runtime.scene_) { error = "Sessão sem cena."; return false; }
    Part* object = runtime.scene_->findPart(part);
    if (!object || !object->hasNpc()) {
        error = "A peça não possui propriedades de NPC.";
        return false;
    }
    if (object->anchored()) {
        error = "NPC requer uma peça dinâmica (anchored=false).";
        return false;
    }
    const auto& properties = *object->npcProperties();
    if (!properties.validate(error)) return false;
    runtime.physics_->syncScene(*runtime.scene_);
    if (!runtime.physics_->isDynamic(part)) {
        error = "Corpo dinâmico indisponível para o NPC.";
        return false;
    }
    runtime.physics_->lockAngularMotion(part, true);
    Controller controller;
    controller.part = part;
    controller.health = properties.maxHealth;
    controller.maxHealth = properties.maxHealth;
    controller.speed = properties.speed;
    controller.turnSpeed = properties.turnSpeed;
    controller.respawnDelay = properties.respawnDelay;
    controller.spawn = object->position();
    controller.state = properties.enabled ? NpcState::Idle : NpcState::Idle;
    controllers_[part] = controller;
    error.clear();
    return true;
}

bool NpcSystem::attachAuthored(RuntimeSession& runtime, std::string& error) {
    if (!runtime.scene_) { error = "Sessão sem cena."; return false; }
    std::vector<InstanceId> ids;
    for (const Part* part : runtime.scene_->parts()) {
        if (part && part->hasNpc() && part->npcProperties()->enabled) ids.push_back(part->id());
    }
    std::sort(ids.begin(), ids.end());
    for (const auto id : ids) {
        std::string detail;
        if (!attach(runtime, id, detail)) {
            // Uma Part ancorada ou inválida não impede as demais.
            error = detail;
            runtime.scripts_.hostError("NPC ignorado: " + detail);
        }
    }
    error.clear();
    return true;
}

bool NpcSystem::detach(InstanceId part) {
    return controllers_.erase(part) > 0;
}

bool NpcSystem::attached(InstanceId part) const { return find(part) != nullptr; }

NpcState NpcSystem::state(InstanceId part) const {
    const auto* controller = find(part);
    return controller ? controller->state : NpcState::Idle;
}

std::optional<Vector3> NpcSystem::target(InstanceId part) const {
    const auto* controller = find(part);
    return controller ? controller->target : std::nullopt;
}

std::vector<Vector3> NpcSystem::path(InstanceId part) const {
    const auto* controller = find(part);
    if (!controller) return {};
    std::vector<Vector3> rest;
    for (std::size_t i = controller->waypoint; i < controller->waypoints.size(); ++i) {
        rest.push_back(controller->waypoints[i]);
    }
    return rest;
}

float NpcSystem::speed(InstanceId part) const {
    const auto* controller = find(part);
    return controller ? controller->speed : 0;
}

bool NpcSystem::setSpeed(InstanceId part, float value, std::string& error) {
    auto* controller = find(part);
    if (!controller) { error = "NPC não anexado a esta peça."; return false; }
    NpcProperties properties;
    properties.speed = value;
    if (!properties.validate(error)) return false;
    if (!(value >= 0 && value <= 1000) || !std::isfinite(value)) {
        error = "Velocidade de NPC deve estar entre 0 e 1000.";
        return false;
    }
    controller->speed = value;
    error.clear();
    return true;
}

float NpcSystem::health(InstanceId part) const {
    const auto* controller = find(part);
    return controller ? controller->health : 0;
}

float NpcSystem::maxHealth(InstanceId part) const {
    const auto* controller = find(part);
    return controller ? controller->maxHealth : 0;
}

bool NpcSystem::setHealth(RuntimeSession& runtime, InstanceId part, float value, std::string& error) {
    auto* controller = find(part);
    if (!controller) { error = "NPC não anexado a esta peça."; return false; }
    if (!std::isfinite(value) || std::fabs(value) > 100000) {
        error = "Vida de NPC fora do intervalo suportado.";
        return false;
    }
    controller->health = std::clamp(value, 0.f, controller->maxHealth);
    if (controller->health <= 0) die(runtime, *controller);
    else if (controller->state == NpcState::Dead) {
        controller->state = NpcState::Idle;
        controller->deadTime = 0;
    }
    error.clear();
    return true;
}

bool NpcSystem::damage(RuntimeSession& runtime, InstanceId part, float amount, std::string& error) {
    if (!std::isfinite(amount) || amount < 0 || amount > 100000) {
        error = "Dano deve estar entre 0 e 100000.";
        return false;
    }
    auto* controller = find(part);
    if (!controller) { error = "NPC não anexado a esta peça."; return false; }
    return setHealth(runtime, part, controller->health - amount, error);
}

bool NpcSystem::heal(RuntimeSession& runtime, InstanceId part, float amount, std::string& error) {
    if (!std::isfinite(amount) || amount < 0 || amount > 100000) {
        error = "Cura deve estar entre 0 e 100000.";
        return false;
    }
    auto* controller = find(part);
    if (!controller) { error = "NPC não anexado a esta peça."; return false; }
    return setHealth(runtime, part, controller->health + amount, error);
}

void NpcSystem::halt(RuntimeSession& runtime, Controller& controller) {
    const Vector3 velocity = runtime.partVelocity(controller.part);
    if (Horizontal(velocity) <= .001f) return;
    std::string error;
    runtime.partMotion(controller.part, {0, velocity.y, 0}, PartMotion::Velocity, error);
}

void NpcSystem::die(RuntimeSession& runtime, Controller& controller) {
    if (controller.state == NpcState::Dead) return;
    controller.state = NpcState::Dead;
    controller.deadTime = 0;
    controller.waypoints.clear();
    controller.waypoint = 0;
    controller.target.reset();
    controller.stuckTime = 0;
    controller.replans = 0;
    halt(runtime, controller);
}

void NpcSystem::respawn(RuntimeSession& runtime, Controller& controller) {
    Part* part = runtime.scene_ ? runtime.scene_->findPart(controller.part) : nullptr;
    if (!part) return;
    controller.health = controller.maxHealth;
    controller.state = NpcState::Idle;
    controller.deadTime = 0;
    controller.stuckTime = 0;
    controller.replans = 0;
    controller.waypoints.clear();
    controller.waypoint = 0;
    controller.target.reset();
    // Reposicionamento de respawn: nao e usado como deslocamento normal.
    part->setPosition(controller.spawn);
    runtime.physics_->syncScene(*runtime.scene_);
}

void NpcSystem::ensureNavigation(RuntimeSession& runtime) {
    if (!runtime.scene_ || !runtime.physics_) return;
    const std::size_t parts = runtime.scene_->parts().size();
    if (grid_.ready() && !gridDirty_ && parts == gridParts_) return;
    runtime.physics_->syncScene(*runtime.scene_);
    grid_.build(*runtime.scene_, *runtime.physics_, config_);
    gridParts_ = parts;
    gridDirty_ = false;
}

void NpcSystem::rebuildNavigation(RuntimeSession& runtime) {
    gridDirty_ = true;
    ensureNavigation(runtime);
}

bool NpcSystem::findPath(RuntimeSession& runtime, Vector3 from, Vector3 to, NavPath& path) {
    ensureNavigation(runtime);
    return grid_.findPath(from, to, path);
}

bool NpcSystem::plan(RuntimeSession& runtime, Controller& controller, Vector3 destination) {
    Part* part = runtime.scene_ ? runtime.scene_->findPart(controller.part) : nullptr;
    if (!part) return false;
    controller.target = destination;
    controller.waypoint = 0;
    controller.replans = 0;
    controller.stuckTime = 0;
    ensureNavigation(runtime);
    NavPath path;
    if (!grid_.findPath(part->position(), destination, path) || !path.reachable) {
        controller.waypoints.clear();
        controller.state = NpcState::Unreachable;
        return false;
    }
    controller.waypoints = path.points;
    controller.state = controller.waypoints.empty() ? NpcState::Arrived : NpcState::Moving;
    return true;
}

bool NpcSystem::moveTo(RuntimeSession& runtime, InstanceId part, Vector3 destination, std::string& error) {
    auto* controller = find(part);
    if (!controller) { error = "NPC não anexado a esta peça."; return false; }
    if (!std::isfinite(destination.x) || !std::isfinite(destination.y) || !std::isfinite(destination.z) ||
        std::fabs(destination.x) > 1000000 || std::fabs(destination.y) > 1000000 ||
        std::fabs(destination.z) > 1000000) {
        error = "Destino de NPC fora do intervalo suportado.";
        return false;
    }
    if (controller->state == NpcState::Dead || controller->health <= 0) {
        error = "NPC morto não pode receber destino.";
        return false;
    }
    error.clear();
    // Destino inacessível e um resultado esperado, nao um erro de uso.
    return plan(runtime, *controller, destination);
}

bool NpcSystem::stopMoving(InstanceId part) {
    auto* controller = find(part);
    if (!controller) return false;
    controller->waypoints.clear();
    controller->waypoint = 0;
    controller->target.reset();
    controller->stuckTime = 0;
    controller->replans = 0;
    if (controller->state != NpcState::Dead) controller->state = NpcState::Idle;
    return true;
}

void NpcSystem::tick(RuntimeSession& runtime, float dt) {
    if (!runtime.scene_ || !runtime.physics_ || !std::isfinite(dt) || dt <= 0) return;
    if (runtime.scene_->parts().size() != gridParts_) gridDirty_ = true;
    for (auto it = controllers_.begin(); it != controllers_.end();) {
        Controller& controller = it->second;
        Part* part = runtime.scene_->findPart(controller.part);
        if (!part || !part->hasNpc()) { it = controllers_.erase(it); continue; }
        const NpcProperties& properties = *part->npcProperties();
        controller.speed = properties.speed;
        controller.maxHealth = properties.maxHealth;
        controller.turnSpeed = properties.turnSpeed;
        controller.respawnDelay = properties.respawnDelay;
        if (controller.health > controller.maxHealth) controller.health = controller.maxHealth;

        if (!properties.enabled) {
            halt(runtime, controller);
            controller.waypoints.clear();
            controller.state = NpcState::Idle;
            ++it;
            continue;
        }
        if (controller.health <= 0 || controller.state == NpcState::Dead) {
            controller.deadTime += dt;
            halt(runtime, controller);
            if (controller.state != NpcState::Dead) die(runtime, controller);
            if (controller.respawnDelay > 0 && controller.deadTime >= controller.respawnDelay) {
                respawn(runtime, controller);
            }
            ++it;
            continue;
        }
        if (controller.waypoints.empty()) {
            halt(runtime, controller);
            if (controller.state == NpcState::Moving) controller.state = NpcState::Arrived;
            ++it;
            continue;
        }

        const Vector3 position = part->position();
        const float radius = waypointRadius();
        while (controller.waypoint < controller.waypoints.size() &&
               HorizontalDistance(position, controller.waypoints[controller.waypoint]) <= radius) {
            ++controller.waypoint;
        }
        if (controller.waypoint >= controller.waypoints.size()) {
            controller.waypoints.clear();
            controller.waypoint = 0;
            controller.target.reset();
            controller.state = NpcState::Arrived;
            halt(runtime, controller);
            ++it;
            continue;
        }

        Vector3 destination = controller.waypoints[controller.waypoint];
        destination.y = position.y;
        Vector3 direction{destination.x - position.x, 0, destination.z - position.z};
        const float distance = Vector3Length(direction);
        if (distance < 1e-4f) {
            ++controller.waypoint;
            ++it;
            continue;
        }
        direction = Vector3Scale(direction, 1.f / distance);

        const Vector3 velocity = runtime.partVelocity(controller.part);
        const float moving = Horizontal(velocity);
        Vector3 desired{direction.x * controller.speed, velocity.y, direction.z * controller.speed};
        std::string error;
        if (!runtime.partMotion(controller.part, desired, PartMotion::Velocity, error)) {
            controller.state = NpcState::Unreachable;
            controller.waypoints.clear();
            ++it;
            continue;
        }
        // Giro suave: turnSpeed (rad/s) limita a mudanca de orientacao por passo.
        if (controller.turnSpeed > 0) {
            const float current = EulerDegreesFromQuat(part->rotation()).y;
            const float target = std::atan2(-direction.x, -direction.z) * RAD2DEG;
            const float limit = controller.turnSpeed * RAD2DEG * dt;
            const float delta = std::clamp(WrapDegrees(target - current), -limit, limit);
            runtime.physics_->setBodyYaw(controller.part, current + delta);
        }
        if (controller.state != NpcState::Moving) controller.state = NpcState::Moving;

        if (controller.speed > .01f && moving < controller.speed * .2f) {
            controller.stuckTime += dt;
            if (controller.stuckTime >= 1.f) {
                controller.stuckTime = 0;
                const auto planned = controller.target;
                ++controller.replans;
                if (!planned || controller.replans > 2 || !plan(runtime, controller, *planned)) {
                    controller.waypoints.clear();
                    controller.state = NpcState::Unreachable;
                    halt(runtime, controller);
                }
            }
        } else {
            controller.stuckTime = 0;
        }
        ++it;
    }
}

bool NpcSystem::validate(const RuntimeSession& runtime, std::string& error) const {
    if (!runtime.scene_) {
        if (!controllers_.empty()) { error = "npc: controladores sem cena"; return false; }
        error.clear();
        return true;
    }
    for (const auto& pair : controllers_) {
        const Part* part = runtime.scene_->findPart(pair.first);
        if (!part || !part->hasNpc()) {
            error = "npc: controlador órfão " + std::to_string(pair.first);
            return false;
        }
        if (pair.second.maxHealth <= 0 || pair.second.health < 0 ||
            pair.second.health > pair.second.maxHealth) {
            error = "npc: vida inválida " + std::to_string(pair.first);
            return false;
        }
        for (const auto& waypoint : pair.second.waypoints) {
            if (!std::isfinite(waypoint.x) || !std::isfinite(waypoint.y) || !std::isfinite(waypoint.z)) {
                error = "npc: waypoint não finito";
                return false;
            }
        }
    }
    error.clear();
    return true;
}

}  // namespace loblox::game

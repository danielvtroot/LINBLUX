#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/Math.h"
#include "game/Navigation.h"
#include "objects/Instance.h"
#include "objects/Part.h"

namespace loblox::game {

class RuntimeSession;

enum class NpcState { Idle, Moving, Arrived, Unreachable, Dead };
const char* NpcStateName(NpcState state);
bool NpcStateFromName(const std::string& name, NpcState& out);

// Controlador de NPC anexado a uma Part comum da cena. Nao existe um
// personagem fixo no C++: a entidade e a Part (com NpcProperties serializadas)
// e o comportamento vem de Lua pela API linblux.npc.
//
// O movimento usa o corpo Bullet existente: a velocidade horizontal e reescrita
// a cada passo fixo em direcao ao proximo waypoint, por isso paredes e limites
// de piso continuam valendo. Nao ha teletransporte de deslocamento; apenas o
// reposicionamento de respawn configuravel e a posicao inicial.
class NpcSystem {
public:
    void reset();
    // Le NpcProperties da Part e cria o controlador. A Part precisa ser dinamica.
    bool attach(RuntimeSession& runtime, InstanceId part, std::string& error);
    bool attachAuthored(RuntimeSession& runtime, std::string& error);
    bool detach(InstanceId part);
    bool attached(InstanceId part) const;
    std::size_t count() const { return controllers_.size(); }

    NpcState state(InstanceId part) const;
    // false quando a grade nao alcanca o destino (estado Unreachable).
    bool moveTo(RuntimeSession& runtime, InstanceId part, Vector3 target, std::string& error);
    bool stopMoving(InstanceId part);
    std::optional<Vector3> target(InstanceId part) const;
    std::vector<Vector3> path(InstanceId part) const;

    float speed(InstanceId part) const;
    bool setSpeed(InstanceId part, float value, std::string& error);
    float health(InstanceId part) const;
    float maxHealth(InstanceId part) const;
    bool setHealth(RuntimeSession& runtime, InstanceId part, float value, std::string& error);
    bool damage(RuntimeSession& runtime, InstanceId part, float amount, std::string& error);
    bool heal(RuntimeSession& runtime, InstanceId part, float amount, std::string& error);

    void rebuildNavigation(RuntimeSession& runtime);
    bool findPath(RuntimeSession& runtime, Vector3 from, Vector3 to, NavPath& path);
    const NavGrid& navigation() const { return grid_; }

    // Passo fixo: conduz corpos, avanca waypoints, aplica respawn e descarta
    // controladores de Parts destruidas.
    void tick(RuntimeSession& runtime, float dt);
    bool validate(const RuntimeSession& runtime, std::string& error) const;

private:
    struct Controller {
        InstanceId part = 0;
        float health = 100;
        float maxHealth = 100;
        float speed = 8;
        float turnSpeed = 8;
        float respawnDelay = 0;
        Vector3 spawn{};
        NpcState state = NpcState::Idle;
        std::vector<Vector3> waypoints;
        std::size_t waypoint = 0;
        std::optional<Vector3> target;
        float deadTime = 0;
        float stuckTime = 0;
        unsigned replans = 0;
    };
    Controller* find(InstanceId part);
    const Controller* find(InstanceId part) const;
    void ensureNavigation(RuntimeSession& runtime);
    bool plan(RuntimeSession& runtime, Controller& controller, Vector3 target);
    void halt(RuntimeSession& runtime, Controller& controller);
    void die(RuntimeSession& runtime, Controller& controller);
    void respawn(RuntimeSession& runtime, Controller& controller);
    float waypointRadius() const;

    std::map<InstanceId, Controller> controllers_;
    NavGrid grid_;
    NavGrid::Config config_{};
    bool gridDirty_ = true;
    std::size_t gridParts_ = 0;
};

}  // namespace loblox::game

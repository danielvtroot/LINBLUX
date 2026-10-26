#pragma once
#include <map>
#include <string>
#include <vector>

#include "objects/Instance.h"

namespace loblox::game {

// Estado de runtime de um item equipável qualquer, dirigido por Lua. Nada aqui
// sabe que tipo de item é: o script decide o efeito (raycast, projétil, som,
// luz) e este componente só aplica as regras genéricas de uso — intervalo entre
// ativações, consumo, munição e recarga. Uma lanterna, uma ferramenta ou uma
// arma fictícia usam exatamente a mesma infraestrutura.
struct ItemConfig {
    int magazine = 0;            // 0 = item sem munição (ferramenta/consumível infinito)
    int reserve = 0;
    int consume = 1;             // munição por ativação
    float interval = 0.2f;       // segundos entre ativações
    float reloadSeconds = 2.f;   // 0 = recarga instantânea no próximo passo
    bool valid() const;
};

struct ItemRuntimeState {
    ItemConfig config{};
    int ammo = 0, reserve = 0;
    float cooldown = 0;
    float reloadElapsed = 0;
    bool reloading = false;
    bool active = false;         // ativado e ainda não desativado
    int capacityValue() const { return config.magazine; }
    float reloadProgress() const;
};

// Transição observável. `state` já vem preenchido com os valores depois da
// mudança, para que o listener publique HUD e áudio sem depender de ordem.
struct ItemActionChange {
    InstanceId item = 0;
    std::string action;
    ItemRuntimeState state{};
};

// Vive na sessão de Play. Só guarda IDs; a existência real é verificada pelo
// chamador (RuntimeSession), que conhece a cena e o inventário. Nunca mantém
// ponteiro ou referência C++ para o item.
class ItemActions {
public:
    void clear();
    // Esquece o item e todo o estado. Não emite evento; o dono já removeu.
    void forget(InstanceId item);
    bool known(InstanceId item) const;

    // Configura e reinicia a munição: pente cheio, reserva informada.
    bool configure(InstanceId item, const ItemConfig& config, std::string& error);
    // Concede/define munição. Nunca aceita valores negativos.
    bool setAmmo(InstanceId item, int ammo, int reserve, std::string& error);
    int ammo(InstanceId item) const;
    int reserve(InstanceId item) const;
    const ItemRuntimeState* state(InstanceId item) const;

    // Consome munição quando o estado permite; o efeito é responsabilidade do
    // script, que recebe a ação "activate" depois.
    bool activate(InstanceId item, std::string& error);
    bool deactivate(InstanceId item, std::string& error);
    bool reload(InstanceId item, std::string& error);
    bool cancelReload(InstanceId item, std::string& error);

    // Interrompe estados que não fazem sentido sem o item nas mãos. Chamado
    // pelo RuntimeSession quando o inventário muda (unequip/drop/remove).
    void itemUnavailable(InstanceId item);

    // Avança cooldown e recarga. `equipped` é o item realmente equipado agora.
    void tick(float dt, InstanceId equipped);

    std::vector<ItemActionChange> takeChanges();

private:
    std::map<InstanceId, ItemRuntimeState> states_;
    std::vector<ItemActionChange> changes_;
    void change(InstanceId item, const char* action);
};

} // namespace loblox::game

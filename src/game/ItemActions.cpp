#include "game/ItemActions.h"

#include <algorithm>
#include <cmath>

namespace loblox::game {
namespace {
void Need(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

bool ItemConfig::valid() const {
    return magazine >= 0 && magazine <= 100000 && reserve >= 0 && reserve <= 1000000 &&
           consume >= 1 && consume <= 100000 && std::isfinite(interval) && interval >= 0 &&
           interval <= 60 && std::isfinite(reloadSeconds) && reloadSeconds >= 0 && reloadSeconds <= 60 &&
           (magazine > 0 || (consume == 1 && reserve == 0));
}

float ItemRuntimeState::reloadProgress() const {
    if (!reloading || config.reloadSeconds <= 0) return reloading ? 1.f : 0.f;
    return std::clamp(reloadElapsed / config.reloadSeconds, 0.f, 1.f);
}

void ItemActions::clear() {
    states_.clear();
    changes_.clear();
}

void ItemActions::forget(InstanceId item) {
    if (!item) return;
    states_.erase(item);
    changes_.erase(std::remove_if(changes_.begin(), changes_.end(),
        [item](const ItemActionChange& change) { return change.item == item; }), changes_.end());
}

bool ItemActions::known(InstanceId item) const { return states_.count(item) != 0; }

const ItemRuntimeState* ItemActions::state(InstanceId item) const {
    const auto it = states_.find(item);
    return it == states_.end() ? nullptr : &it->second;
}

int ItemActions::ammo(InstanceId item) const {
    const auto* current = state(item);
    return current ? current->ammo : 0;
}

int ItemActions::reserve(InstanceId item) const {
    const auto* current = state(item);
    return current ? current->reserve : 0;
}

void ItemActions::change(InstanceId item, const char* action) {
    if (!item) return;
    const auto* current = state(item);
    if (!current) return;
    // Uma rajada não pode encher a fila de eventos: a última transição do mesmo
    // item no mesmo quadro substitui a anterior quando a ação se repete.
    if (changes_.size() >= 256) changes_.erase(changes_.begin());
    changes_.push_back({item, action, *current});
}

bool ItemActions::configure(InstanceId item, const ItemConfig& config, std::string& error) {
    if (!item) { error = "Item inválido."; return false; }
    if (states_.size() >= 256 && !states_.count(item)) { error = "Limite de 256 itens com estado atingido."; return false; }
    if (!config.valid()) { error = "Configuração de item inválida: pente 0..100000, reserva 0..1000000, consumo 1..100000, intervalo e recarga 0..60 s."; return false; }
    ItemRuntimeState state;
    state.config = config;
    state.ammo = config.magazine;
    state.reserve = config.reserve;
    states_[item] = state;
    error.clear();
    return true;
}

bool ItemActions::setAmmo(InstanceId item, int ammoValue, int reserveValue, std::string& error) {
    const auto it = states_.find(item);
    if (it == states_.end()) { error = "Item sem estado de combate configurado."; return false; }
    if (ammoValue < 0 || reserveValue < 0 || ammoValue > it->second.config.magazine ||
        reserveValue > 1000000) {
        error = "Munição fora do intervalo do pente ou da reserva.";
        return false;
    }
    it->second.ammo = ammoValue;
    it->second.reserve = reserveValue;
    change(item, "ammo");
    error.clear();
    return true;
}

bool ItemActions::activate(InstanceId item, std::string& error) {
    auto it = states_.find(item);
    if (it == states_.end()) { error = "Item sem estado de combate configurado."; return false; }
    auto& state = it->second;
    if (state.reloading) { error = "Recarregando."; return false; }
    if (state.cooldown > 0) { error = "Intervalo entre ativações ainda ativo."; return false; }
    if (state.config.magazine > 0 && state.ammo < state.config.consume) { error = "Sem munição."; return false; }
    if (state.config.magazine > 0) state.ammo -= state.config.consume;
    state.cooldown = state.config.interval;
    state.active = true;
    change(item, "activate");
    error.clear();
    return true;
}

bool ItemActions::deactivate(InstanceId item, std::string& error) {
    auto it = states_.find(item);
    if (it == states_.end()) { error = "Item sem estado de combate configurado."; return false; }
    if (!it->second.active) { error.clear(); return true; }
    it->second.active = false;
    change(item, "deactivate");
    error.clear();
    return true;
}

bool ItemActions::reload(InstanceId item, std::string& error) {
    auto it = states_.find(item);
    if (it == states_.end()) { error = "Item sem estado de combate configurado."; return false; }
    auto& state = it->second;
    if (state.config.magazine <= 0) { error = "Item não usa recarga."; return false; }
    if (state.reloading) { error = "Recarga já em andamento."; return false; }
    if (state.ammo >= state.config.magazine) { error = "Pente já está cheio."; return false; }
    if (state.reserve <= 0) { error = "Sem reserva para recarregar."; return false; }
    state.reloading = true;
    state.reloadElapsed = 0;
    change(item, "reload_start");
    error.clear();
    return true;
}

bool ItemActions::cancelReload(InstanceId item, std::string& error) {
    auto it = states_.find(item);
    if (it == states_.end()) { error = "Item sem estado de combate configurado."; return false; }
    error.clear();
    if (!it->second.reloading) return true;
    it->second.reloading = false;
    it->second.reloadElapsed = 0;
    change(item, "reload_cancel");
    return true;
}

void ItemActions::itemUnavailable(InstanceId item) {
    auto it = states_.find(item);
    if (it == states_.end()) return;
    if (it->second.reloading) {
        it->second.reloading = false;
        it->second.reloadElapsed = 0;
        change(item, "reload_cancel");
    }
    if (it->second.active) {
        it->second.active = false;
        change(item, "deactivate");
    }
}

void ItemActions::tick(float dt, InstanceId equipped) {
    if (!std::isfinite(dt) || dt < 0) return;
    if (dt > 1.f) dt = 1.f; // uma pausa longa não completa várias recargas de uma vez
    for (auto& entry : states_) {
        auto& state = entry.second;
        if (state.cooldown > 0) state.cooldown = std::max(0.f, state.cooldown - dt);
        if (!state.reloading) continue;
        // Recarregar exige o item em mãos: soltar/destruir cancela pelo dono,
        // mas trocar de item também encerra a recarga pendente.
        if (entry.first != equipped) { itemUnavailable(entry.first); continue; }
        state.reloadElapsed += dt;
        if (state.reloadElapsed + 1e-6f < state.config.reloadSeconds) continue;
        const int needed = state.config.magazine - state.ammo;
        const int moved = std::min(needed, state.reserve);
        state.ammo += moved;
        state.reserve -= moved;
        state.reloading = false;
        state.reloadElapsed = 0;
        change(entry.first, "reload_end");
    }
}

std::vector<ItemActionChange> ItemActions::takeChanges() {
    std::vector<ItemActionChange> result;
    result.swap(changes_);
    return result;
}

} // namespace loblox::game

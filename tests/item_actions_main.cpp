// Generic item actions, headless: interval, ammo, reload, cleanup, attachment
// origin, self-hit guard, script events and the Lua ammo HUD. Uses the same
// RuntimeSession as Studio Play and the Player, never a window.
#include "core/Diagnostics.h"
#include "experiences/ExperienceManager.h"
#include "game/ImageAssets.h"
#include "game/Runtime.h"
#include "objects/Part.h"
#include "scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace loblox;
namespace fs = std::filesystem;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
double attributeNumber(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return 0;
    if (const auto* value = std::get_if<double>(&it->second)) return *value;
    return 0;
}
bool attributeBool(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return false;
    if (const auto* value = std::get_if<bool>(&it->second)) return *value;
    return false;
}
std::string attributeString(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return {};
    if (const auto* value = std::get_if<std::string>(&it->second)) return *value;
    return {};
}
std::string scriptErrors(const game::RuntimeSession& runtime) {
    std::string text;
    for (const auto& message : runtime.scripts().messages())
        if (message.error) text += message.text + "\n";
    return text;
}
const Attachment* attachmentOf(const Scene& scene, InstanceId model) {
    const auto* root = scene.find(model);
    if (!root) return nullptr;
    for (const auto& object : scene.objects()) {
        if (object->instanceClass() != InstanceClass::Attachment) continue;
        if (root->isAncestorOf(object.get())) return static_cast<const Attachment*>(object.get());
    }
    return nullptr;
}
const char* kItemScript = R"lua(
-- Todo item programável usa a mesma API: este script não é de rifle, apenas
-- reage a item_action e desenha munição quando o item está equipado.
local arma = linblux.self
local painel, rotulo

local function criar()
    painel = linblux.ui.create("panel", {anchor="bottom_right", x=-24, y=-24,
        width=200, height=52, background="#101014E6"})
    rotulo = linblux.ui.create("label", {parent=painel, anchor="center", width=180,
        height=26, text="", color="#FFFFFF", font_size=16})
    arma:set_attribute("ui_rotulo", rotulo)
end

local function atualizar()
    local estado = linblux.items.combat(arma)
    if not estado then return end
    if linblux.items.equipped() ~= arma then
        if painel then linblux.ui.destroy(painel); painel = nil; rotulo = nil
            arma:set_attribute("ui_rotulo", 0) end
        return
    end
    if not painel then criar() end
    local texto = estado.reloading and "RECARREGANDO"
        or (tostring(estado.ammo).." / "..tostring(estado.magazine).." + "..tostring(estado.reserve))
    linblux.ui.set(rotulo, "text", texto)
end

linblux.events.on("item_action", function(evento)
    if evento.item and evento.item.id == arma.id then atualizar() end
end)
linblux.events.on("update", function(dt) atualizar() end)
linblux.events.on("destroy", function()
    if painel then linblux.ui.destroy(painel); painel = nil end
end)
)lua";
const char* kDriverScript = R"lua(
local v = linblux.vec3
local raiz = linblux.scene.root
local function pub(chave, valor) raiz:set_attribute(chave, valor) end

-- Alvo real com controlador de NPC: o dano vem da API de vida existente.
local alvo = linblux.npc.create{name="Alvo", position=v(0,5,-26), size=v(8,10,3),
    max_health=100, speed=0}
pub("alvo_id", alvo.id)

local arma = linblux.scene.find("Fuzil")
linblux.items.register("fuzil", arma, {name="Fuzil de prova", grip_position=v(1.1,0.2,-2.6)})
local item = linblux.items.spawn("fuzil", {position=v(7,3,0)})
local clone = linblux.items.spawn("fuzil", {position=v(11,3,0), scale=2})
pub("item_id", item.id)
pub("clone_id", clone.id)
linblux.items.collect(item)
linblux.items.equip(item)
linblux.items.configure(item, {magazine=6, reserve=6, consume=1, interval=0.25, reload_time=0.3})
pub("ammo_inicial", linblux.items.combat(item).ammo)

linblux.events.on("item_action", function(evento)
    pub("ultima_acao", evento.action)
    if evento.action == "activate" then pub("ammo_evento", evento.ammo) end
    if evento.action == "reload_end" then
        pub("ammo_recarregada", evento.ammo)
        pub("reserva_recarregada", evento.reserve)
    end
end)

local primeiro, motivo = linblux.items.activate(item)
pub("tiro_ok", primeiro)
pub("motivo", motivo)
pub("tiro_intervalo", linblux.items.activate(item))
pub("cooldown", linblux.items.combat(item).cooldown)
task.wait(0.3)
pub("tiro_apos_intervalo", linblux.items.activate(item))
pub("ammo_apos_tiros", linblux.items.combat(item).ammo)

pub("reload_ok", linblux.items.reload(item))
local durante, motivo2 = linblux.items.activate(item)
pub("tiro_durante", durante)
pub("motivo_recarga", motivo2)
pub("ammo_durante", linblux.items.combat(item).ammo)
task.wait(0.45)
local estado = linblux.items.combat(item)
pub("reload_terminou", not estado.reloading)
pub("ammo_final", estado.ammo)
pub("reserva_final", estado.reserve)

-- Origem real: o Attachment do item equipado, nunca uma coordenada fixa.
local boca = linblux.scene.find("BocaDoCano")
pub("boca_x", boca.world_position.X)
pub("boca_y", boca.world_position.Y)
pub("boca_z", boca.world_position.Z)
pub("boca_dir_z", boca.direction.Z)
local acerto = linblux.physics.raycast(boca.world_position, boca.direction, 120, item)
pub("acerto", acerto and acerto.object.name or "nenhum")
pub("acerto_distancia", acerto and acerto.distance or -1)
if acerto and acerto.object == alvo then linblux.npc.damage(alvo, 25) end
pub("alvo_vida", linblux.npc.health(alvo))

-- Sem ignorar a subárvore do item, o raio do próprio pivô o atinge.
local sem = linblux.physics.raycast(item.position, v(0,0,1), 12)
local com = linblux.physics.raycast(item.position, v(0,0,1), 12, item)
pub("auto_sem", sem and sem.object.name or "nenhum")
pub("auto_com", com and com.object.name or "nenhum")

-- Recusas: ativar item guardado e recarregar item guardado.
local guardado_erro = pcall(function() linblux.items.unequip() end)
local ativacao_guardada = pcall(function() return linblux.items.activate(item) end)
pub("ativacao_guardada_falhou", not ativacao_guardada)
pub("unequip_ok", guardado_erro)
linblux.items.equip(item)
task.wait(0.05)
pub("fim", true)
)lua";
}

int main() {
    SetTraceLogLevel(LOG_ERROR);
    const auto root = fs::temp_directory_path() / ("linblux-item-actions-" + experiences::newId());
    int failures = 0;
    try {
        fs::create_directories(root / "assets");
        game::SetProjectAssetRoot(root.string());

        Scene scene;
        scene.buildDefaultScene();
        std::string error;
        auto* body = scene.createPart(PartShape::Block, {0, 0, 0}, {1, 1, 4}, "Corpo");
        auto* barrel = scene.createPart(PartShape::Block, {0, 0, -3}, {.4f, .4f, 3}, "Cano");
        auto* model = scene.group({body->id(), barrel->id()}, "Fuzil", error);
        require(model && model->instanceClass() == InstanceClass::Model, "group: " + error);
        auto* muzzle = scene.createAttachment(model, "BocaDoCano");
        ModelFrame frame;
        require(scene.modelFrame(model->id(), frame, error), "model frame: " + error);
        const Vector3 tip{barrel->position().x, barrel->position().y, barrel->position().z - 2.2f};
        require(muzzle->setLocalFrame({Vector3Subtract(tip, frame.position), {0, 0, 0, 1}, 1}),
                "attachment local frame");
        const InstanceId modelId = model->id(), muzzleId = muzzle->id();
        auto* itemScript = scene.createScript(InstanceClass::Script, model, "Item.lua");
        itemScript->source = kItemScript;
        auto* driver = scene.createScript(InstanceClass::Script,
            scene.ensureService(InstanceClass::ServerScriptService), "Combate.lua");
        driver->source = kDriverScript;

        // --- Parte A: contrato da máquina de estados, sem cena -------------
        {
            game::ItemActions actions;
            game::ItemConfig config;
            config.magazine = 5; config.reserve = 4; config.consume = 1;
            config.interval = 0.5f; config.reloadSeconds = 1.f;
            require(actions.configure(7, config, error), "configure: " + error);
            require(actions.ammo(7) == 5 && actions.reserve(7) == 4, "initial magazine");
            require(actions.activate(7, error), "first activation: " + error);
            require(actions.ammo(7) == 4, "ammo was not consumed");
            require(!actions.activate(7, error) && error.find("Intervalo") != std::string::npos,
                    "interval not enforced: " + error);
            actions.tick(0.5f, 7);
            require(actions.activate(7, error), "activation after interval: " + error);
            require(actions.deactivate(7, error), "deactivate: " + error);
            require(actions.reload(7, error), "reload: " + error);
            require(!actions.activate(7, error), "activation during reload was accepted");
            actions.tick(0.4f, 7);
            require(!actions.state(7)->reloading, "reload ended before its time");
            actions.tick(0.7f, 7);
            require(!actions.state(7)->reloading, "reload did not end");
            require(actions.ammo(7) == 5 && actions.reserve(7) == 1, "reload did not move reserve ammo");
            require(!actions.reload(7, error) && error.find("cheio") != std::string::npos,
                    "reloading a full magazine was accepted");
            require(!actions.setAmmo(7, 99, 0, error), "out-of-magazine ammo accepted");
            require(actions.setAmmo(7, 0, 2, error), "setAmmo: " + error);
            require(!actions.activate(7, error) && error.find("Sem muni") != std::string::npos,
                    "empty magazine fired: " + error);
            require(actions.reload(7, error), "reload from empty: " + error);
            actions.itemUnavailable(7);
            require(!actions.state(7)->reloading, "unavailable item kept reloading");
            const auto changes = actions.takeChanges();
            require(!changes.empty(), "state changes were not published");
            bool cancelled = false;
            for (const auto& change : changes) cancelled |= change.action == "reload_cancel";
            require(cancelled, "reload_cancel was not published");
            actions.forget(7);
            require(!actions.known(7) && actions.takeChanges().empty(), "forget kept state");
            game::ItemConfig broken;
            broken.magazine = -1;
            require(!actions.configure(9, broken, error), "negative magazine accepted");
        }

        // --- Parte B: fluxo real no RuntimeSession -------------------------
        game::RuntimeSession runtime;
        require(runtime.start(scene, error), "runtime start: " + error);
        const auto* workspace = runtime.scene().workspace();
        int frames = 0;
        while (frames < 900 && !attributeBool(*workspace, "fim")) {
            runtime.update(1.f / 60, {});
            ++frames;
        }
        require(attributeBool(*workspace, "fim"), "driver script did not finish:\n" + scriptErrors(runtime));
        require(runtime.scripts().errorCount() == 0, "script errors:\n" + scriptErrors(runtime));
        const auto itemId = InstanceId(attributeNumber(*workspace, "item_id"));
        const auto cloneId = InstanceId(attributeNumber(*workspace, "clone_id"));
        require(itemId && cloneId && itemId != cloneId, "items were not spawned");
        require(runtime.inventory().equipped() == itemId, "item is not equipped");

        require(attributeBool(*workspace, "tiro_ok"), "first activation failed: " +
                attributeString(*workspace, "motivo"));
        require(attributeNumber(*workspace, "ammo_evento") == 5, "activate event carried the wrong ammo");
        require(!attributeBool(*workspace, "tiro_intervalo"), "interval was not enforced in Lua");
        require(attributeNumber(*workspace, "cooldown") > 0, "cooldown was not exposed");
        require(attributeBool(*workspace, "tiro_apos_intervalo"), "activation after interval failed");
        require(attributeNumber(*workspace, "ammo_apos_tiros") == 4, "ammo did not reach 4");
        require(attributeString(*workspace, "ultima_acao") == "activate", "item_action was not delivered");
        require(attributeBool(*workspace, "reload_ok"), "reload did not start");
        require(!attributeBool(*workspace, "tiro_durante"), "activation during reload was accepted");
        require(attributeString(*workspace, "motivo_recarga") == "Recarregando.", "wrong reload refusal");
        require(attributeNumber(*workspace, "ammo_durante") == 4, "ammo changed during reload");
        require(attributeBool(*workspace, "reload_terminou"), "reload did not finish");
        require(attributeNumber(*workspace, "ammo_final") == 6, "reload did not fill the magazine");
        require(attributeNumber(*workspace, "reserva_final") == 4, "reload did not consume the reserve");
        require(attributeNumber(*workspace, "ammo_recarregada") == 6, "reload_end event ammo");
        require(attributeNumber(*workspace, "reserva_recarregada") == 4, "reload_end event reserve");

        // Attachment: origem do efeito e direção frontal acompanham o Modelo.
        const auto* muzzleNode = static_cast<const Attachment*>(runtime.scene().find(muzzleId));
        require(muzzleNode, "muzzle attachment vanished");
        ModelFrame muzzleFrame;
        require(runtime.scene().attachmentFrame(muzzleId, muzzleFrame, error), "attachment frame: " + error);
        require(std::fabs(muzzleFrame.position.x - attributeNumber(*workspace, "boca_x")) < .01f &&
                std::fabs(muzzleFrame.position.z - attributeNumber(*workspace, "boca_z")) < .01f,
                "Lua world_position disagrees with the engine frame");
        ModelFrame itemFrame;
        require(runtime.scene().modelFrame(itemId, itemFrame, error), "item frame: " + error);
        const Vector3 muzzleOffset = Vector3Subtract(muzzleFrame.position, itemFrame.position);
        require(Vector3Length(muzzleOffset) > 2.f, "muzzle is not offset from the item pivot");
        const Vector3 forward = Vector3RotateByQuaternion({0, 0, -1}, muzzleFrame.rotation);
        require(Vector3Dot(Vector3Normalize(muzzleOffset), forward) > .99f,
                "muzzle is not along the attachment forward axis");
        require(attributeNumber(*workspace, "boca_dir_z") < -.99, "equipped muzzle does not face -Z");

        // Clonagem: instâncias independentes, mesma direção, posições distintas.
        const auto* cloneMuzzle = attachmentOf(runtime.scene(), cloneId);
        require(cloneMuzzle, "clone lost its attachment");
        ModelFrame cloneFrame;
        require(runtime.scene().attachmentFrame(cloneMuzzle->id(), cloneFrame, error), "clone frame: " + error);
        require(Vector3Distance(cloneFrame.position, muzzleFrame.position) > 3.f,
                "clone shares the prototype attachment transform");
        ModelFrame cloneModel;
        require(runtime.scene().modelFrame(cloneId, cloneModel, error), "clone model frame: " + error);
        require(std::fabs(cloneModel.scale - 2.f) < .01f, "clone scale was not applied");

        // Impacto: o raio parte do Attachment e ignora o próprio item.
        require(attributeString(*workspace, "acerto") == "Alvo",
                "raycast from the muzzle hit '" + attributeString(*workspace, "acerto") + "'");
        require(attributeNumber(*workspace, "alvo_vida") == 75, "damage was not applied by the health API");
        require(attributeString(*workspace, "auto_com") == "nenhum", "ignoring the item still hit itself");
        require(attributeString(*workspace, "auto_sem") == "Corpo" || attributeString(*workspace, "auto_sem") == "Cano",
                "self-hit fixture is not meaningful: '" + attributeString(*workspace, "auto_sem") + "'");
        require(attributeBool(*workspace, "ativacao_guardada_falhou"), "activation of a stored item was accepted");

        // HUD Lua: só existe com o item equipado e acompanha a munição real.
        auto uiLabel = [&](InstanceId item) {
            const auto* modelNode = runtime.scene().find(item);
            return modelNode ? std::uint64_t(attributeNumber(*modelNode, "ui_rotulo")) : 0;
        };
        require(uiLabel(itemId) != 0, "item script did not create its ammo HUD");
        const auto* label = runtime.ui().find(uiLabel(itemId));
        require(label && label->text == "6 / 6 + 4", "HUD text does not match the real state: '" +
                (label ? label->text : std::string("missing")) + "'");

        // Mover o jogador move o Attachment junto (mesma pose do item equipado).
        Vector3 before = muzzleFrame.position;
        require(runtime.teleportPlayer(Vector3Add(runtime.position(), {2.f, 0, 0}), error),
                "teleport: " + error);
        for (int i = 0; i < 4; ++i) runtime.update(1.f / 60, {});
        require(runtime.scene().attachmentFrame(muzzleId, muzzleFrame, error), "attachment frame after teleport");
        require(muzzleFrame.position.x - before.x > 1.5f, "attachment did not follow the equipped item");

        // Desequipar remove o HUD e encerra recarga/ativação pendentes.
        runtime.inventory().unequip();
        for (int i = 0; i < 4 && runtime.ui().count() > 0; ++i) runtime.update(1.f / 60, {});
        require(runtime.ui().count() == 0, "HUD survived unequip");
        require(uiLabel(itemId) == 0, "item script kept a stale HUD id");

        // Girar o Modelo guardado gira o Attachment no mundo real.
        runtime.inventory().equip(itemId);
        for (int i = 0; i < 3; ++i) runtime.update(1.f / 60, {});
        runtime.inventory().unequip();
        require(runtime.scene().modelFrame(itemId, itemFrame, error), "model frame before rotation");
        ModelFrame rotated = itemFrame;
        rotated.rotation = QuatFromEulerDegreesZYX({0, 90, 0});
        require(runtime.scene().transformModel(itemId, rotated, error), "rotate model: " + error);
        require(runtime.scene().attachmentFrame(muzzleId, muzzleFrame, error), "attachment after rotation");
        const Vector3 rotatedOffset = Vector3Subtract(muzzleFrame.position, rotated.position);
        require(std::fabs(rotatedOffset.x) > 2.f && std::fabs(rotatedOffset.z) < 1.f,
                "attachment did not rotate with the model");

        // Soltar, recolher e continuar funcionando com o mesmo estado de munição.
        runtime.inventory().equip(itemId);
        for (int i = 0; i < 3; ++i) runtime.update(1.f / 60, {});
        const int ammoBeforeDrop = runtime.itemActions().ammo(itemId);
        runtime.inventory().drop(itemId);
        for (int i = 0; i < 4; ++i) runtime.update(1.f / 60, {});
        require(runtime.ui().count() == 0, "HUD survived drop");
        require(runtime.itemActions().ammo(itemId) == ammoBeforeDrop, "dropping reset the magazine");
        runtime.inventory().collect(itemId);
        runtime.inventory().equip(itemId);
        for (int i = 0; i < 4; ++i) runtime.update(1.f / 60, {});
        require(runtime.ui().count() > 0, "HUD did not return after recollecting");

        // Destruição: estado, callbacks e HUD não sobrevivem ao item.
        const auto scriptId = itemScript->id();
        runtime.inventory().remove(itemId);
        for (int i = 0; i < 6; ++i) runtime.update(1.f / 60, {});
        require(!runtime.itemActions().known(itemId), "destroyed item kept action state");
        require(runtime.ui().count() == 0, "HUD survived destroy");
        require(runtime.scene().find(scriptId) == nullptr, "item script survived destroy");
        require(runtime.scripts().errorCount() == 0, "errors after destroy:\n" + scriptErrors(runtime));
        require(runtime.inventory().equipped() == 0, "destroyed item is still equipped");

        runtime.stop(false);
        require(!runtime.itemActions().known(itemId), "stop kept item state");
        std::printf("Item actions: activation, ammo, reload, attachment, raycast, damage, HUD and cleanup OK\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Item actions failed: %s\n", e.what());
        ++failures;
    }
    std::error_code ignored;
    fs::remove_all(root, ignored);
    return failures ? 1 : 0;
}

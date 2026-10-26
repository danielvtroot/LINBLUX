// RifleDemo ponta a ponta, sem janela: a experiência versionada é carregada pelo
// mesmo ProjectIO do Player e usada pelas APIs públicas (E, 1, clique, R e
// Delete). Prova coleta, equipamento, tiro com dano real, munição, recarga,
// soltura e recoleta — e que nada disso depende de C++ específico de arma.
#include "core/Diagnostics.h"
#include "game/Audio.h"
#include "game/ImageAssets.h"
#include "game/Runtime.h"
#include "serialization/ProjectIO.h"

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
bool attributeFlag(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return false;
    const auto* value = std::get_if<bool>(&it->second);
    return value && *value;
}
std::string scriptErrors(const game::RuntimeSession& runtime) {
    std::string text;
    for (const auto& message : runtime.scripts().messages())
        if (message.error) text += message.text + "\n";
    return text;
}
std::vector<InstanceId> targets(const Scene& scene) {
    std::vector<InstanceId> ids;
    for (const auto& object : scene.objects())
        if (object->isPart() && attributeFlag(*object, "alvo")) ids.push_back(object->id());
    return ids;
}
std::size_t ammoHudLabels(const game::UiModel& ui) {
    std::size_t count = 0;
    for (const auto* element : ui.drawOrder())
        if (element->kind == game::UiKind::Label && element->text.find(" / ") != std::string::npos) ++count;
    return count;
}
} // namespace

int main() {
    SetTraceLogLevel(LOG_ERROR);
    const auto demo = fs::path(__FILE__).parent_path().parent_path() / "assets/examples/rifledemo";
    int failures = 0;
    try {
        game::SetProjectAssetRoot(demo.string());
        Scene world;
        std::string error;
        require(projectio::load(world, (demo / "world.loblox.json").string(), error), "load: " + error);

        game::RuntimeSession runtime;
        require(runtime.start(world, error), "start: " + error);
        auto step = [&](int frames, game::PlayerInput input) {
            for (int i = 0; i < frames; ++i) runtime.update(1.f / 60, input);
        };
        step(12, {});
        require(runtime.scripts().errorCount() == 0, "startup errors:\n" + scriptErrors(runtime));

        auto alvos = targets(runtime.scene());
        require(alvos.size() == 3, "demo did not create the three targets");
        for (const auto id : alvos) {
            require(runtime.npcs().attached(id), "target has no NPC controller");
            require(std::fabs(runtime.npcs().health(id) - 100) < .01f, "target did not start at 100");
        }

        // 1. Segurar E perto do item no chão coleta pelo sistema de interação.
        game::PlayerInput hold;
        hold.script.keys.set(KEY_E, true);
        step(45, hold);
        const auto bag = runtime.inventory().list();
        require(bag.size() == 1, "holding E did not collect the item");
        const auto itemId = bag.front();
        step(3, {});

        // 2. A tecla 1 equipa pelo inventário e a HUD Lua aparece.
        game::PlayerInput equip;
        equip.script.keys.set(KEY_ONE, true);
        step(2, equip);
        step(4, {});
        require(runtime.inventory().equipped() == itemId, "key 1 did not equip the item");
        require(runtime.itemActions().known(itemId), "item has no action state");
        require(runtime.itemActions().ammo(itemId) == 12 && runtime.itemActions().reserve(itemId) == 24,
                "item ammo was not configured by the script");
        require(ammoHudLabels(runtime.ui()) == 1, "ammo HUD did not appear when equipped");

        // 3. Clique atira: munição real, dano real por raycast no Attachment.
        game::PlayerInput fire;
        fire.script.mouse.set(MOUSE_BUTTON_LEFT, true);
        step(4, fire);
        require(runtime.itemActions().ammo(itemId) == 11, "shot did not consume ammo");
        float bestHealth = 100;
        for (const auto id : alvos) bestHealth = std::min(bestHealth, runtime.npcs().health(id));
        require(std::fabs(bestHealth - 66) < .01f,
                "raycast shot did not damage a target (best health " + std::to_string(bestHealth) + ") Lua: " + scriptErrors(runtime));
        bool tracer = false;
        for (const auto& object : runtime.scene().objects())
            if (object->isPart() && object->name().rfind("Tracante", 0) == 0) tracer = true;
        require(tracer, "the shot did not create its physical tracer part");
        step(3, {});

        // 4. R recarrega: estado pendente visível, depois pente cheio.
        game::PlayerInput reload;
        reload.script.keys.set(KEY_R, true);
        step(2, reload);
        step(1, {});
        require(runtime.itemActions().state(itemId)->reloading, "R did not start the reload");
        step(120, {});
        require(!runtime.itemActions().state(itemId)->reloading, "reload never finished");
        require(runtime.itemActions().ammo(itemId) == 12, "reload did not fill the magazine");
        require(runtime.itemActions().reserve(itemId) == 23, "reload did not consume the reserve");
        require(scriptErrors(runtime).empty(), "script errors during play:\n" + scriptErrors(runtime));

        // 5. Delete solta, a HUD desaparece e o item volta ao mundo.
        game::PlayerInput drop;
        drop.script.keys.set(KEY_DELETE, true);
        step(2, drop);
        step(5, {});
        require(runtime.inventory().list().empty(), "Delete did not drop the item");
        require(runtime.inventory().info(itemId).state == "world", "dropped item is not in the world");
        require(ammoHudLabels(runtime.ui()) == 0, "ammo HUD survived the drop");

        // 6. Recoletar com E e usar de novo mantém a munição e o comportamento.
        step(2, {});
        step(45, hold);
        require(runtime.inventory().list().size() == 1, "the dropped item could not be collected again");
        step(2, equip);
        step(4, {});
        require(runtime.inventory().equipped() == itemId, "recollected item did not equip");
        step(140, fire);
        require(runtime.itemActions().ammo(itemId) < 12, "recollected item could not fire");
        step(3, {});
        require(scriptErrors(runtime).empty(), "script errors after recollecting:\n" + scriptErrors(runtime));

        // 7. Física, NPCs e cenário continuam íntegros; a experiência foi validada.
        require(runtime.validate(error), "runtime invalid: " + error);

        // Os sons importados existem e são decodificáveis (o dispositivo de áudio
        // não é aberto em teste headless; o Player executa a reprodução real).
        std::size_t sounds = 0;
        for (const auto& sound : runtime.scene().gameAssets().sounds) {
            double seconds = 0;
            require(runtime.audio().probe(sound.reference, seconds, error),
                    "sound " + sound.reference + ": " + error);
            require(seconds > 0, "sound " + sound.reference + " has no duration");
            ++sounds;
        }
        require(sounds >= 4, "the demo must publish its activation/reload/impact/equip sounds");

        runtime.stop(false);
        std::printf("RifleDemo: E coleta, 1 equipa, clique atira, R recarrega, Delete solta e E recolhe; "
                    "%zu sons válidos\n", sounds);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "RifleDemo failed: %s\n", e.what());
        ++failures;
    }
    return failures ? 1 : 0;
}

#include "core/Diagnostics.h"
#include "experiences/ExperienceManager.h"
#include "game/Audio.h"
#include "game/ImageAssets.h"
#include "game/Runtime.h"
#include "objects/Part.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace loblox;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
fs::path fixture(const char* name) {
    return fs::path(__FILE__).parent_path() / "fixtures/audio" / name;
}
double attributeNumber(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return 0;
    if (const auto* number = std::get_if<double>(&it->second)) return *number;
    return 0;
}
bool attributeBool(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return false;
    if (const auto* flag = std::get_if<bool>(&it->second)) return *flag;
    return false;
}
}  // namespace

int main() {
    SetTraceLogLevel(LOG_ERROR);
    const auto root = fs::temp_directory_path() / ("linblux-audio-" + experiences::newId());
    try {
        diag::configure((root / "audio.jsonl").string());
        fs::create_directories(root / "assets");
        game::SetProjectAssetRoot(root.string());

        // MP3, WAV e OGG reais: a importação valida o container e a decodificação.
        std::string wav, ogg, mp3, error;
        require(game::ImportMediaTo((root / "assets").string(), fixture("tone.wav").string(), game::MediaKind::Audio, wav, error), error.c_str());
        require(game::ImportMediaTo((root / "assets").string(), fixture("tone.ogg").string(), game::MediaKind::Audio, ogg, error), error.c_str());
        require(game::ImportMediaTo((root / "assets").string(), fixture("tone.mp3").string(), game::MediaKind::Audio, mp3, error), error.c_str());
        const std::string wavRef = "assets/" + fs::path(wav).filename().string();
        const std::string oggRef = "assets/" + fs::path(ogg).filename().string();
        const std::string mp3Ref = "assets/" + fs::path(mp3).filename().string();
        for (const auto& ref : {wavRef, oggRef, mp3Ref}) {
            require(fs::is_regular_file(root / ref), "Áudio importado não foi gravado na experiência");
            double seconds = 0;
            require(game::ProbeAudio((root / ref).string(), seconds, error), error.c_str());
            require(std::fabs(seconds - 0.25) < 0.08, "Duração decodificada não corresponde ao áudio real");
        }
        require(game::ListAudioAssets((root / "assets").string()).size() == 3, "Listagem de áudio incompleta");

        // Arquivo renomeado não vira áudio válido.
        const auto fake = root / "fake.mp3";
        fs::copy_file(fixture("../image-test.jpg"), fake, fs::copy_options::overwrite_existing);
        std::string ignored;
        require(!game::ImportMediaTo((root / "assets").string(), fake.string(), game::MediaKind::Audio, ignored, error), "MP3 falso foi aceito");
        const auto fakeWav = root / "fake.wav";
        { std::ofstream out(fakeWav, std::ios::binary); out << "not a wave file"; }
        require(!game::ImportMediaTo((root / "assets").string(), fakeWav.string(), game::MediaKind::Audio, ignored, error), "WAV inválido foi aceito");

        // Contrato de reprodução sem dispositivo de áudio: mesmas transições.
        game::AudioSystem audio;
        require(!audio.deviceReady(), "Teste headless deveria estar sem dispositivo de áudio");
        const int handle = audio.play(wavRef, .5f, false, error);
        require(handle > 0, error.c_str());
        require(audio.playing(handle) && !audio.isPaused(handle), "Som recém-tocado deveria estar ativo");
        require(std::fabs(audio.volume(handle) - .5f) < 1e-4, "Volume inicial não aplicado");
        require(std::fabs(audio.duration(handle) - 0.25) < 0.08, "Duração do handle incorreta");
        require(audio.play(wavRef, 2.f, false, error) == 0 && !error.empty(), "Volume fora de 0..1 deveria falhar");
        require(audio.play("assets/inexistente.ogg", 1, false, error) == 0, "Áudio ausente deveria falhar");
        require(audio.play("som-invalido.txt", 1, false, error) == 0, "Referência não-áudio deveria falhar");
        audio.pause(handle);
        require(audio.isPaused(handle) && !audio.playing(handle), "Pause não alterou o estado");
        audio.update(1.f);
        require(audio.isPaused(handle), "Pause continuou avançando o tempo");
        audio.resume(handle);
        require(audio.playing(handle), "Resume não retomou");
        require(audio.setVolume(handle, .2f, error), error.c_str());
        audio.setLoop(handle, true);
        audio.update(1.f);
        require(audio.playing(handle), "Loop deveria manter o som ativo");
        audio.setLoop(handle, false);
        audio.update(.3f);
        require(audio.voiceCount() == 0, "Som não-loop deveria terminar pela duração decodificada");
        const int second = audio.play(oggRef, 1, true, error);
        const int third = audio.play(mp3Ref, 1, false, error);
        require(second > 0 && third > 0, "OGG/MP3 não tocaram");
        audio.stopAll();
        require(audio.voiceCount() == 0, "stop_all deixou vozes ativas");

        // Biblioteca persistida com a cena + API Lua real no RuntimeSession.
        experiences::ExperienceManager::writeFactoryResources(root / "factory");
        fs::create_directories(root / "defaults");
        experiences::ExperienceManager library(root / "data/linblux", root / "factory", root / "defaults");
        experiences::CreateOptions options;
        options.name = "Experiência com áudio";
        auto created = library.create(options);
        Scene scene;
        library.open(created, scene);
        const auto storedRef = library.importAsset(fixture("tone.ogg"));
        auto& entry = scene.gameAssets().addSound(storedRef, "Tom de teste");
        entry.volume = .4f;
        entry.loop = true;
        entry.kind = "music";
        const std::string soundId = entry.id;
        auto* script = scene.createScript(InstanceClass::Script, scene.ensureService(InstanceClass::ServerScriptService), "AudioDemo");
        script->source =
            "local h = linblux.audio.play('" + soundId + "')\n"
            "workspace:SetAttribute('audio_handle', h)\n"
            "workspace:SetAttribute('audio_playing', linblux.audio.is_playing(h))\n"
            "workspace:SetAttribute('audio_volume', linblux.audio.volume(h))\n"
            "workspace:SetAttribute('audio_duration', linblux.audio.duration('" + soundId + "'))\n"
            "local count = 0\n"
            "for _, entry in ipairs(linblux.audio.list()) do count = count + 1 end\n"
            "workspace:SetAttribute('audio_count', count)\n"
            "linblux.audio.pause(h)\n"
            "workspace:SetAttribute('audio_paused', linblux.audio.is_paused(h))\n"
            "linblux.audio.resume(h)\n"
            "linblux.audio.set_volume(h, 0.6)\n"
            "workspace:SetAttribute('audio_volume2', linblux.audio.volume(h))\n"
            "linblux.audio.stop(h)\n"
            "workspace:SetAttribute('audio_stopped', not linblux.audio.is_playing(h))\n"
            "local h2 = linblux.audio.play('" + soundId + "', {volume = 0.3, loop = true})\n"
            "workspace:SetAttribute('audio_handle2', h2)\n";
        game::RuntimeSession runtime;
        require(runtime.start(scene, error), error.c_str());
        runtime.update(1.f / 60, {});
        const auto* workspace = runtime.scene().workspace();
        require(attributeNumber(*workspace, "audio_handle") > 0, "Lua não devolveu handle de áudio");
        require(attributeBool(*workspace, "audio_playing"), "Lua não viu o som tocando");
        require(std::fabs(attributeNumber(*workspace, "audio_volume") - .4f) < 1e-3, "Lua ignorou o volume da biblioteca");
        require(attributeNumber(*workspace, "audio_duration") > 0.1, "Lua não obteve a duração real");
        require(attributeNumber(*workspace, "audio_count") == 1, "Lua não listou a biblioteca de sons");
        require(attributeBool(*workspace, "audio_paused"), "Lua não pausou o som");
        require(std::fabs(attributeNumber(*workspace, "audio_volume2") - .6f) < 1e-3, "Lua não ajustou o volume");
        require(attributeBool(*workspace, "audio_stopped"), "Lua não parou o som");
        require(attributeNumber(*workspace, "audio_handle2") > 0, "Segundo play não funcionou");
        require(runtime.audio().voiceCount() == 1, "Runtime deveria manter apenas o segundo som");
        runtime.stop();
        require(runtime.audio().voiceCount() == 0, "Stop deixou áudio tocando");

        // Persistência: salvar, reabrir e herdar em nova experiência do padrão.
        library.save(scene);
        library.close();
        Scene reopened;
        library.open(created, reopened);
        const auto* saved = reopened.gameAssets().findSound(soundId);
        require(saved && saved->reference == storedRef, "Biblioteca de sons não sobreviveu ao salvamento");
        require(std::fabs(saved->volume - .4f) < 1e-4 && saved->loop && saved->kind == "music",
                "Metadados de áudio não persistiram");
        require(fs::is_regular_file(created.root / storedRef), "Arquivo de áudio sumiu ao salvar");
        require(game::SoundInUse(reopened, storedRef), "Uso do áudio não foi detectado na cena");
        bool refused = false;
        try { library.removeAsset(storedRef, game::SoundInUse(reopened, storedRef)); }
        catch (const std::exception&) { refused = true; }
        require(refused && fs::is_regular_file(created.root / storedRef), "Áudio em uso foi apagado");
        library.removeAsset(storedRef, false); // sem uso declarado: remove o arquivo
        require(!fs::exists(created.root / storedRef), "Remoção autorizada não apagou o áudio");

        std::printf("AUDIO: MP3/WAV/OGG reais, contrato de reprodução, API Lua, persistência e remoção segura passaram. Fixture: %s\n",
                    root.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Áudio: %s\n", error.what());
        return 1;
    }
}

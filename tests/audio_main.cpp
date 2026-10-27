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
#include <sys/resource.h>
#include <cstdint>

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
void musicContracts(game::AudioSystem& audio,const std::string& wav,const std::string& ogg) {
    std::string error;game::GameAssets assets;game::MusicPlaylist music;
    require(!music.start(audio,assets,error)&&error.empty()&&audio.voiceCount()==0,"Música vazia criou voz automática");
    auto& a=assets.addSound(wav,"Primeira");a.volume=.5f;a.durationSeconds=.25;const auto first=a.id;
    auto& b=assets.addSound(ogg,"Segunda");b.durationSeconds=.25;const auto second=b.id;
    assets.music.background=first;assets.music.volume=.6f;assets.music.loop=false;
    require(music.start(audio,assets,error),error.c_str());
    require(music.currentId()==first&&audio.voiceCount()==1&&!audio.looping(music.handle()),"Fundo não usou faixa única");
    require(std::fabs(audio.volume(music.handle())-.3f)<.0001f,"Volume de música ignorou volume da biblioteca");
    music.pause(audio);audio.update(.4f);music.update(audio);
    require(music.paused()&&audio.hasVoice(music.handle()),"Pause avançou a faixa");
    music.resume(audio);audio.update(.3f);music.update(audio);require(!music.active()&&audio.voiceCount()==0,"Faixa única não terminou");
    assets.music.playlist={"id-ausente",first,second};assets.music.repeat=false;
    require(music.start(audio,assets,error)&&music.count()==2,error.c_str());
    audio.update(.3f);music.update(audio);require(music.currentId()==second&&audio.voiceCount()==1,"Playlist não avançou");
    audio.update(.3f);music.update(audio);require(!music.active()&&audio.voiceCount()==0,"Playlist sem repetição não terminou");
    assets.music.repeat=true;require(music.start(audio,assets,error),error.c_str());
    audio.update(.3f);music.update(audio);audio.update(.3f);music.update(audio);
    require(music.currentId()==first&&audio.voiceCount()==1,"Playlist não repetiu sem duplicar vozes");
    audio.stopAll();music.update(audio);
    require(!music.active()&&audio.voiceCount()==0,"Playlist reiniciou após stop_all explícito de Lua");
    music.stop(audio);
    Scene scene;scene.gameAssets()=assets;const auto serialized=projectio::toJson(scene);Scene reopened;
    require(projectio::fromJson(reopened,serialized,error),error.c_str());
    require(reopened.gameAssets().music.playlist==assets.music.playlist&&reopened.gameAssets().music.background==first,"Música não persistiu por IDs");
    require(game::DescribeSoundUsage(reopened,wav).size()==2,"Referências de fundo/playlist não foram detectadas");
    auto invalid=json::parse(serialized);invalid["game"]["music"]["volume"]=2;
    require(!projectio::fromJson(reopened,invalid.dump(),error)&&projectio::toJson(reopened)==serialized,"Música inválida corrompeu a cena");
    reopened.gameAssets().music.playlist={first,second};reopened.gameAssets().removeSound(first);
    require(reopened.gameAssets().music.background.empty()&&reopened.gameAssets().music.playlist==std::vector<std::string>{second},"Remoção deixou ID de áudio órfão");
}
}  // namespace

int main() {
    SetTraceLogLevel(LOG_ERROR);
    const auto root = fs::temp_directory_path() / ("linblux-audio-" + experiences::newId());
    try {
        diag::configure((root / "audio.jsonl").string());
        fs::create_directories(root / "assets");
        game::SetProjectAssetRoot(root.string());

        // One hour of PCM in a sparse file: probing must never decode it all.
        const auto longFile=root/"long.wav";constexpr uint32_t bytes=44100u*2u*3600u;
        {std::ofstream file(longFile,std::ios::binary);auto u16=[&](uint16_t n){char b[2]={char(n),char(n>>8)};file.write(b,2);};auto u32=[&](uint32_t n){char b[4]={char(n),char(n>>8),char(n>>16),char(n>>24)};file.write(b,4);};
         file.write("RIFF",4);u32(bytes+36);file.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(44100);u32(88200);u16(2);u16(16);file.write("data",4);u32(bytes);file.seekp(uint64_t(bytes)+43);file.put(0);}
        rusage before{},after{};getrusage(RUSAGE_SELF,&before);double longSeconds=0;std::string longError;
        require(game::ProbeAudio(longFile.string(),longSeconds,longError),longError.c_str());getrusage(RUSAGE_SELF,&after);
        require(std::fabs(longSeconds-3600)<1,"Duração da música longa incorreta");
        require(after.ru_maxrss-before.ru_maxrss<64*1024,"Probe decodificou a música inteira na RAM");
        std::printf("Probe PCM1h: duração%.2fs, aumento de pico RSS%ldKiB\n",longSeconds,after.ru_maxrss-before.ru_maxrss);

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
        musicContracts(audio,wavRef,oggRef);

        Scene ownerScene;auto* owner=ownerScene.createPart(PartShape::Block,{0,5,0});
        const auto ownedVoice=audio.play(wavRef,1,true,error,owner->id());require(ownedVoice>0,error.c_str());
        audio.update(.01f,&ownerScene);require(audio.voiceCount()==1,"Voz associada não foi criada");ownerScene.remove(owner);audio.update(.01f,&ownerScene);require(audio.voiceCount()==0,"Voz sobreviveu ao objeto destruído");

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
        entry.durationSeconds=.25;
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
        refused=false;try{library.removeAsset(storedRef,false);}catch(const std::exception&){refused=true;}
        require(refused,"Flag do chamador burlou referências na cena salva");
        require(reopened.removeById(script->id()),"Não removeu script que usa áudio");
        const auto originalScene=projectio::toJson(reopened);
        require(!game::RemoveSoundAsset(reopened,(created.root/"assets").string(),storedRef,[](std::string& e){e="Falha simulada de salvamento";return false;},error),"Exclusão ignorou falha de salvamento");
        require(fs::exists(created.root/storedRef)&&projectio::toJson(reopened)==originalScene,"Falha de salvamento perdeu arquivo ou metadados");
        require(game::RemoveSoundAsset(reopened,(created.root/"assets").string(),storedRef,[&](std::string& e){try{library.save(reopened);return true;}catch(const std::exception& ex){e=ex.what();return false;}},error),error.c_str());
        require(!fs::exists(created.root / storedRef)&&reopened.gameAssets().sounds.empty(),"Remoção autorizada não apagou o áudio");

        std::printf("AUDIO: MP3/WAV/OGG reais, contrato de reprodução, API Lua, persistência e remoção segura passaram. Fixture: %s\n",
                    root.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Áudio: %s\n", error.what());
        return 1;
    }
}

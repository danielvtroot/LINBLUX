#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"

namespace loblox {
class Scene;
}

namespace loblox::game {

struct GameAssets;

// Áudio de projeto: o arquivo é copiado para `assets/` da experiência e a
// referência canônica é `assets/<arquivo>.{wav,ogg,mp3}`. A biblioteca abaixo
// é apenas metadado serializado com a cena (nome, volume, loop). Nenhum
// ponteiro de runtime é gravado no projeto.
struct SoundAsset {
    std::string id;
    std::string name;
    std::string filename;
    std::string reference;
    double durationSeconds=0;
    float volume = 1.f;
    bool loop = false;
    std::string kind = "sound"; // "sound" ou "music"
};

// Música declarativa da experiência. Os ids são os mesmos IDs estáveis da
// biblioteca de sons; referências ausentes são ignoradas em runtime para que
// um projeto antigo continue abrindo e tocando o restante da playlist.
// `background` é a faixa única (loop controlado por `loop`). Quando há
// playlist, ela tem precedência e cada faixa toca uma vez, em ordem; `repeat`
// reinicia a lista ao terminar.
struct MusicSettings {
    std::string background;
    std::vector<std::string> playlist;
    float volume = 0.8f;
    bool loop = true;
    bool repeat = true;
};

bool IsAudioReference(const std::string& reference);

// true quando a biblioteca de sons, um atributo de som ou o código de um
// Script ainda cita o arquivo. A remoção consulta isto antes de apagar.
struct SoundUse {std::uint64_t object=0;std::string where,property;};
std::vector<SoundUse> DescribeSoundUsage(const Scene& scene,const std::string& reference);
bool SoundInUse(const Scene& scene, const std::string& reference,bool includeLibrary=true);
bool RemoveSoundAsset(Scene& scene,const std::string& assetsDir,const std::string& reference,
    const std::function<bool(std::string&)>& save,std::string& error);

// Lista os áudios existentes em assets/ (usada pela UI de importação).
std::vector<SoundAsset> ListAudioAssets(const std::string& assetsDir);

// Lê metadados com orçamento/timeout, sem decodificar o arquivo inteiro.
// Headless usa FFmpeg; sem ele, a raylib requer dispositivo de áudio aberto.
bool ProbeAudio(const std::string& resolvedPath, double& duration, std::string& error);

// Reprodução 2D global. Cada voice é independente: play devolve um handle
// estável; stop/pause/resume/setVolume/setLoop atuam por handle. Sem dispositivo
// de áudio (testes, CI) a voz é conduzida por tempo usando a duração decodificada,
// então a API se comporta igual e nada finge tocar em um dispositivo inexistente.
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool deviceReady() const;
    // reference precisa ser `assets/<arquivo>` resolvido dentro do projeto.
    int play(const std::string& reference, float volume, bool loop, std::string& error,std::uint64_t owner=0);
    bool stop(int handle);
    bool pause(int handle);
    bool resume(int handle);
    bool setVolume(int handle, float volume, std::string& error);
    bool setLoop(int handle, bool loop);
    float volume(int handle) const;
    bool isPaused(int handle) const;
    bool playing(int handle) const;
    bool looping(int handle) const;
    bool hasVoice(int handle) const;
    std::string reference(int handle) const;
    double duration(int handle) const;
    std::size_t voiceCount() const { return voices_.size(); }
    bool streaming(int handle) const;
    void update(float dt,const Scene* scene=nullptr);
    void stopAll();
    // Recalcula duração a partir do arquivo; usado antes de play e por Lua.
    bool probe(const std::string& reference, double& seconds, std::string& error) const;

private:
    struct Voice {
        std::string path;
        std::string reference;
        float volume = 1.f;
        bool loop = false;
        bool paused = false;
        double duration = 0;
        double played = 0;
        std::uint64_t owner=0;
        Music music{};
        bool stream = false;
    };
    Voice* find(int handle);
    const Voice* find(int handle) const;
    void release(Voice& voice);

    std::map<int, Voice> voices_;
    int nextHandle_ = 1;
    static constexpr std::size_t kMaxVoices = 64;
};

// Conduz a música declarativa com um único handle ativo. Não cria uma segunda
// biblioteca: resolve SoundAsset por ID e usa AudioSystem para tocar. A classe
// é determinística e headless-safe, como AudioSystem.
class MusicPlaylist {
public:
    // Para qualquer voz anterior, resolve as faixas configuradas e inicia a
    // primeira válida. Faixas ausentes/inválidas são puladas. Retorna false
    // apenas quando nada pôde ser iniciado; `error` explica o motivo.
    bool start(AudioSystem& audio,const GameAssets& assets,std::string& error);
    void stop(AudioSystem& audio);
    void pause(AudioSystem& audio);
    void resume(AudioSystem& audio);
    // Avança quando a voz atual terminou. Deve ser chamado depois de
    // AudioSystem::update(), que remove vozes concluídas.
    void update(AudioSystem& audio);
    bool active() const { return active_; }
    bool paused() const { return paused_; }
    bool playlist() const { return playlist_; }
    int handle() const { return handle_; }
    std::size_t count() const { return tracks_.size(); }
    std::size_t index() const { return index_; }
    const std::string& currentId() const { return currentId_; }
    const std::string& lastError() const { return lastError_; }
private:
    struct Track {
        std::string id,reference;
        float volume=1;
    };
    bool playIndex(AudioSystem& audio,std::size_t index,std::string& error);
    std::vector<Track> tracks_;
    std::size_t index_=0;
    int handle_=0;
    bool playlist_=false,repeat_=false,loop_=true,active_=false,paused_=false;
    float volume_=1;
    std::string currentId_,lastError_;
};

}  // namespace loblox::game

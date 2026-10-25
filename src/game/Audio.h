#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"

namespace loblox {
class Scene;
}

namespace loblox::game {

// Áudio de projeto: o arquivo é copiado para `assets/` da experiência e a
// referência canônica é `assets/<arquivo>.{wav,ogg,mp3}`. A biblioteca abaixo
// é apenas metadado serializado com a cena (nome, volume, loop). Nenhum
// ponteiro de runtime é gravado no projeto.
struct SoundAsset {
    std::string id;
    std::string name;
    std::string filename;
    std::string reference;
    float volume = 1.f;
    bool loop = false;
    std::string kind = "sound"; // "sound" ou "music"
};

bool IsAudioReference(const std::string& reference);

// true quando a biblioteca de sons, um atributo de som ou o código de um
// Script ainda cita o arquivo. A remoção consulta isto antes de apagar.
bool SoundInUse(const Scene& scene, const std::string& reference);

// Lista os áudios existentes em assets/ (usada pela UI de importação).
std::vector<SoundAsset> ListAudioAssets(const std::string& assetsDir);

// Decodifica o container para descobrir duração real. Funciona sem dispositivo
// de áudio no host (dr_wav/dr_mp3/stb_vorbis), então serve de teste headless.
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
    int play(const std::string& reference, float volume, bool loop, std::string& error);
    bool stop(int handle);
    bool pause(int handle);
    bool resume(int handle);
    bool setVolume(int handle, float volume, std::string& error);
    bool setLoop(int handle, bool loop);
    float volume(int handle) const;
    bool isPaused(int handle) const;
    bool playing(int handle) const;
    bool looping(int handle) const;
    std::string reference(int handle) const;
    double duration(int handle) const;
    std::size_t voiceCount() const { return voices_.size(); }
    void update(float dt);
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

}  // namespace loblox::game

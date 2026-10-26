#include "game/Audio.h"

#include <algorithm>
#include <chrono>
#ifdef LINBLUX_AUDIO_METADATA_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
}
#endif
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <unordered_map>

#include "game/ImageAssets.h"
#include "scene/Scene.h"

namespace loblox::game {
namespace {
std::string LowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (auto& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return extension;
}

// Decodificar um arquivo é caro; a duração é a mesma para uma referência que já
// existe no disco. O cache é invalidado pelo tamanho/mtime do arquivo.
struct ProbeEntry {
    std::uintmax_t size = 0;
    std::filesystem::file_time_type modified{};
    double duration = 0;
};
std::unordered_map<std::string, ProbeEntry> probeCache;
std::mutex probeMutex;
}  // namespace

bool IsAudioReference(const std::string& reference) {
    const auto extension = LowerExtension(reference);
    return extension == ".wav" || extension == ".ogg" || extension == ".mp3";
}

bool SoundInUse(const Scene& scene, const std::string& reference) {
    if (reference.empty()) return false;
    const auto filename = std::filesystem::path(reference).filename().string();
    if (filename.empty()) return false;
    for (const auto& sound : scene.gameAssets().sounds)
        if (std::filesystem::path(sound.reference).filename().string() == filename) return true;
    for (const auto& owned : scene.objects()) {
        const Instance* object = owned.get();
        if (!object) continue;
        for (const auto& attribute : object->attributes()) {
            const auto* text = std::get_if<std::string>(&attribute.second);
            if (text && text->find(filename) != std::string::npos) return true;
        }
        if (IsScriptClass(object->instanceClass()) &&
            static_cast<const Script*>(object)->source.find(filename) != std::string::npos) return true;
    }
    return false;
}

std::vector<SoundAsset> ListAudioAssets(const std::string& assetsDir) {
    std::vector<SoundAsset> list;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(assetsDir, error)) {
        if (!entry.is_regular_file(error)) continue;
        if (!IsAudioReference(entry.path().filename().string())) continue;
        SoundAsset asset;
        asset.id = entry.path().stem().string();
        asset.filename = entry.path().filename().string();
        asset.reference = "assets/" + asset.filename;
        if (asset.name.empty()) asset.name = asset.id;
        list.push_back(std::move(asset));
    }
    std::sort(list.begin(), list.end(),
              [](const SoundAsset& a, const SoundAsset& b) { return a.reference < b.reference; });
    return list;
}

bool ProbeAudio(const std::string& resolvedPath, double& duration, std::string& error) {
    error.clear();
    duration = 0;
    std::error_code fsError;
    if (resolvedPath.empty() || !std::filesystem::is_regular_file(resolvedPath, fsError)) {
        error = "Arquivo de áudio não encontrado: " + resolvedPath;
        return false;
    }
    const auto size = std::filesystem::file_size(resolvedPath, fsError);
    const auto modified = std::filesystem::last_write_time(resolvedPath, fsError);
    {
        std::lock_guard<std::mutex> lock(probeMutex);
        if (const auto it = probeCache.find(resolvedPath); it != probeCache.end() &&
            it->second.size == size && it->second.modified == modified) {
            duration = it->second.duration;
            return true;
        }
    }
#ifdef LINBLUX_AUDIO_METADATA_FFMPEG
    AVFormatContext* format=avformat_alloc_context();
    if(!format){error="Memória insuficiente para inspecionar áudio.";return false;}
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    format->interrupt_callback.callback=[](void* data){return std::chrono::steady_clock::now()>*static_cast<const std::chrono::steady_clock::time_point*>(data)?1:0;};
    format->interrupt_callback.opaque=const_cast<void*>(static_cast<const void*>(&deadline));
    format->flags|=AVFMT_FLAG_NOBUFFER;
    AVDictionary* options=nullptr;av_dict_set(&options,"protocol_whitelist","file",0);av_dict_set(&options,"format_whitelist","wav,ogg,mp3",0);
    av_dict_set_int(&options,"probesize",262144,0);av_dict_set_int(&options,"analyzeduration",200000,0);
    const int opened=avformat_open_input(&format,resolvedPath.c_str(),nullptr,&options);av_dict_free(&options);
    if(opened<0){if(format)avformat_close_input(&format);error="Container de áudio inválido ou não suportado.";return false;}
    const int found=avformat_find_stream_info(format,nullptr);bool audio=false;
    for(unsigned i=0;i<format->nb_streams;++i)if(format->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
        const auto* stream=format->streams[i];audio=true;
        if(stream->codecpar->codec_id==AV_CODEC_ID_OPUS){audio=false;break;}
        if(stream->duration!=AV_NOPTS_VALUE&&stream->duration>0)duration=std::max(duration,stream->duration*av_q2d(stream->time_base));
    }
    if(format->duration!=AV_NOPTS_VALUE&&format->duration>0)duration=std::max(duration,double(format->duration)/AV_TIME_BASE);
    avformat_close_input(&format);
    if(found<0||!audio||!std::isfinite(duration)||duration<=0){error="Não foi possível obter duração válida de WAV/MP3/OGG Vorbis.";return false;}
#else
    // raylib can inspect a stream when its audio device has already been opened.
    if(!IsAudioDeviceReady()){error="Inspeção de áudio sem dispositivo requer FFmpeg nesta compilação.";return false;}
    Music probe=LoadMusicStream(resolvedPath.c_str());
    if(!probe.stream.buffer){error="Falha ao abrir stream de áudio.";return false;}
    duration=GetMusicTimeLength(probe);UnloadMusicStream(probe);
    if(!std::isfinite(duration)||duration<=0){error="Duração de áudio inválida.";return false;}
#endif
    {
        std::lock_guard<std::mutex> lock(probeMutex);
        if(probeCache.size()>=1024)probeCache.erase(probeCache.begin());
        probeCache[resolvedPath] = {size, modified, duration};
    }
    return true;
}

AudioSystem::AudioSystem() = default;

AudioSystem::~AudioSystem() { stopAll(); }

bool AudioSystem::deviceReady() const { return IsAudioDeviceReady(); }

AudioSystem::Voice* AudioSystem::find(int handle) {
    const auto it = voices_.find(handle);
    return it == voices_.end() ? nullptr : &it->second;
}

const AudioSystem::Voice* AudioSystem::find(int handle) const {
    const auto it = voices_.find(handle);
    return it == voices_.end() ? nullptr : &it->second;
}

void AudioSystem::release(Voice& voice) {
    if (voice.stream) {
        StopMusicStream(voice.music);
        UnloadMusicStream(voice.music);
        voice.music = {};
        voice.stream = false;
    }
}

bool AudioSystem::probe(const std::string& reference, double& seconds, std::string& error) const {
    const auto path = ResolveImagePath(AssetsDir(), reference);
    if (path.empty()) {
        error = "Áudio não encontrado no projeto: " + reference;
        return false;
    }
    return ProbeAudio(path, seconds, error);
}

int AudioSystem::play(const std::string& reference, float volume, bool loop, std::string& error) {
    error.clear();
    if (!std::isfinite(volume) || volume < 0 || volume > 1) {
        error = "Volume deve estar entre 0 e 1.";
        return 0;
    }
    if (!IsAudioReference(reference)) {
        error = "Use uma referência de áudio assets/<arquivo>.wav/.ogg/.mp3.";
        return 0;
    }
    if (voices_.size() >= kMaxVoices) {
        error = "Limite de 64 sons simultâneos atingido.";
        return 0;
    }
    const auto path = ResolveImagePath(AssetsDir(), reference);
    if (path.empty()) {
        error = "Áudio não encontrado no projeto: " + reference;
        return 0;
    }
    double seconds = 0;
    if (!ProbeAudio(path, seconds, error)) return 0;
    Voice voice;
    voice.path = path;
    voice.reference = reference;
    voice.volume = volume;
    voice.loop = loop;
    voice.duration = seconds;
    if (deviceReady()) {
        voice.music = LoadMusicStream(path.c_str());
        if (!voice.music.stream.buffer) {error="O backend não conseguiu abrir o áudio importado.";return 0;}
        if (voice.music.stream.buffer) {
            voice.stream = true;
            voice.music.looping = loop;
            SetMusicVolume(voice.music, volume);
            PlayMusicStream(voice.music);
        }
    }
    const int handle = nextHandle_++;
    voices_[handle] = std::move(voice);
    return handle;
}

bool AudioSystem::stop(int handle) {
    const auto it = voices_.find(handle);
    if (it == voices_.end()) return false;
    release(it->second);
    voices_.erase(it);
    return true;
}

bool AudioSystem::pause(int handle) {
    Voice* voice = find(handle);
    if (!voice || voice->paused) return voice != nullptr;
    voice->paused = true;
    if (voice->stream) PauseMusicStream(voice->music);
    return true;
}

bool AudioSystem::resume(int handle) {
    Voice* voice = find(handle);
    if (!voice || !voice->paused) return voice != nullptr;
    voice->paused = false;
    if (voice->stream) ResumeMusicStream(voice->music);
    return true;
}

bool AudioSystem::setVolume(int handle, float volume, std::string& error) {
    error.clear();
    Voice* voice = find(handle);
    if (!voice) {
        error = "Handle de áudio inválido.";
        return false;
    }
    if (!std::isfinite(volume) || volume < 0 || volume > 1) {
        error = "Volume deve estar entre 0 e 1.";
        return false;
    }
    voice->volume = volume;
    if (voice->stream) SetMusicVolume(voice->music, volume);
    return true;
}

bool AudioSystem::setLoop(int handle, bool loop) {
    Voice* voice = find(handle);
    if (!voice) return false;
    voice->loop = loop;
    if (voice->stream) voice->music.looping = loop;
    return true;
}

float AudioSystem::volume(int handle) const {
    const Voice* voice = find(handle);
    return voice ? voice->volume : 0;
}

bool AudioSystem::isPaused(int handle) const {
    const Voice* voice = find(handle);
    return voice && voice->paused;
}

bool AudioSystem::playing(int handle) const {
    const Voice* voice = find(handle);
    return voice && !voice->paused;
}

bool AudioSystem::looping(int handle) const {
    const Voice* voice = find(handle);
    return voice && voice->loop;
}

std::string AudioSystem::reference(int handle) const {
    const Voice* voice = find(handle);
    return voice ? voice->reference : std::string();
}

double AudioSystem::duration(int handle) const {
    const Voice* voice = find(handle);
    return voice ? voice->duration : 0;
}

void AudioSystem::update(float dt) {
    if (!std::isfinite(dt) || dt < 0) return;
    for (auto it = voices_.begin(); it != voices_.end();) {
        Voice& voice = it->second;
        if (voice.paused) {
            ++it;
            continue;
        }
        if (voice.stream) {
            UpdateMusicStream(voice.music);
            if (voice.music.looping) {
                ++it;
                continue;
            }
            if (!IsMusicStreamPlaying(voice.music)) {
                release(voice);
                it = voices_.erase(it);
                continue;
            }
            ++it;
            continue;
        }
        // Sem dispositivo: o mesmo contrato, conduzido pela duração decodificada.
        voice.played += dt;
        if (!voice.loop && voice.played >= voice.duration) {
            it = voices_.erase(it);
            continue;
        }
        ++it;
    }
}

void AudioSystem::stopAll() {
    for (auto& entry : voices_) release(entry.second);
    voices_.clear();
}

}  // namespace loblox::game

#pragma once

#include <string>

#include "core/Math.h"
#include "objects/Instance.h"

namespace loblox {

// Modo de renderização do céu.
enum class SkyMode {
    Classic,      // Skybox cúbico (legado)
    Atmospheric   // Skydome hemisférico com gradiente atmosférico
};

// Servico de iluminacao/ceu. Guarda apenas configuracao; a renderizacao le
// estes valores a cada frame (nada de recursos graficos aqui).
class Lighting : public Instance {
public:
    explicit Lighting(InstanceId id, std::string name = "Lighting");

    const char* className() const override { return "Lighting"; }
    InstanceClass instanceClass() const override { return InstanceClass::Lighting; }

    Color skyColor() const { return skyColor_; }
    void setSkyColor(Color color) { skyColor_ = color; }

    Color ambientColor() const { return ambientColor_; }
    void setAmbientColor(Color color) { ambientColor_ = color; }

    Vector3 sunDirection() const { return sunDirection_; }
    void setSunDirection(Vector3 direction);

    float sunIntensity() const { return sunIntensity_; }
    void setSunIntensity(float intensity);

    float brightness() const { return brightness_; }
    void setBrightness(float brightness);

    // Céu procedural: ligado por padrão, desligável por projeto.
    bool skyEnabled() const { return skyEnabled_; }
    void setSkyEnabled(bool enabled) { skyEnabled_ = enabled; }

    // Modo de céu: Classic (cubo) ou Atmospheric (skydome hemisférico).
    SkyMode skyMode() const { return skyMode_; }
    void setSkyMode(SkyMode mode) { skyMode_ = mode; }

    // Cores do céu procedural (gradiente vertical: zenith -> horizon -> ground)
    Color skyZenithColor() const { return skyZenithColor_; }
    void setSkyZenithColor(Color color) { skyZenithColor_ = color; }

    Color skyHorizonColor() const { return skyHorizonColor_; }
    void setSkyHorizonColor(Color color) { skyHorizonColor_ = color; }

    Color skyGroundColor() const { return skyGroundColor_; }
    void setSkyGroundColor(Color color) { skyGroundColor_ = color; }

    // Sol
    Color sunColor() const { return sunColor_; }
    void setSunColor(Color color) { sunColor_ = color; }

    float sunSize() const { return sunSize_; }
    void setSunSize(float size) { sunSize_ = std::isfinite(size) ? std::clamp(size, 0.0f, 1.0f) : 0.0f; }

    // Atmosfera / névoa
    float atmosphereStrength() const { return atmosphereStrength_; }
    void setAtmosphereStrength(float strength) {
        atmosphereStrength_ = std::isfinite(strength) ? std::clamp(strength, 0.0f, 1.0f) : 0.0f;
    }

    // Nuvens procedurais
    float cloudDensity() const { return cloudDensity_; }
    void setCloudDensity(float density) { cloudDensity_ = std::isfinite(density) ? std::clamp(density, 0.0f, 1.0f) : 0.0f; }

    float cloudScale() const { return cloudScale_; }
    void setCloudScale(float scale) { cloudScale_ = std::isfinite(scale) ? std::clamp(scale, 0.1f, 10.0f) : 1.0f; }

    float cloudSpeed() const { return cloudSpeed_; }
    void setCloudSpeed(float speed) { cloudSpeed_ = std::isfinite(speed) ? std::clamp(speed, 0.0f, 5.0f) : 0.0f; }

    float cloudSoftness() const { return cloudSoftness_; }
    void setCloudSoftness(float softness) { cloudSoftness_ = std::isfinite(softness) ? std::clamp(softness, 0.0f, 1.0f) : 0.0f; }

    // Rotação do céu em graus ao redor do eixo Y
    float skyRotation() const { return skyRotation_; }
    void setSkyRotation(float degrees) {
        skyRotation_ = std::isfinite(degrees) ? degrees : 0.0f;
    }

    Color fogColor() const { return fogColor_; }
    void setFogColor(Color color) { fogColor_ = color; }

    // 0 = fog desativada; 1 = opaca a ~1 de distância da câmera.
    float fogDensity() const { return fogDensity_; }
    void setFogDensity(float density) {
        fogDensity_ = std::isfinite(density) ? std::clamp(density, 0.0f, 1.0f) : 0.0f;
    }

private:
    Color skyColor_{120, 180, 240, 255};
    Color ambientColor_{150, 150, 160, 255};
    // Light comes from the front-right-above of the default camera so the
    // faces the player sees first receive the warm key light.
    Vector3 sunDirection_{-0.45f, -1.0f, 0.55f};
    float sunIntensity_ = 1.0f;
    float brightness_ = 1.0f;
    Color fogColor_{120, 180, 240, 255};
    float fogDensity_ = 0.0f;
    bool skyEnabled_ = true;
    SkyMode skyMode_ = SkyMode::Atmospheric;
    
    // Céu procedural
    Color skyZenithColor_{80, 140, 220, 255};    // Azul profundo no topo
    Color skyHorizonColor_{180, 200, 230, 255};  // Horizonte mais claro
    Color skyGroundColor_{60, 80, 100, 255};     // Base (abaixo do horizonte)
    Color sunColor_{255, 240, 200, 255};         // Cor do sol
    float sunSize_ = 0.04f;                       // Tamanho angular do sol
    float atmosphereStrength_ = 0.5f;             // Força do espalhamento atmosférico
    float cloudDensity_ = 0.3f;                   // Densidade das nuvens
    float cloudScale_ = 1.0f;                     // Escala das nuvens
    float cloudSpeed_ = 0.1f;                     // Velocidade das nuvens
    float cloudSoftness_ = 0.5f;                  // Suavidade das nuvens
    float skyRotation_ = 0.0f;                    // Rotação do céu (graus)
};

}  // namespace loblox

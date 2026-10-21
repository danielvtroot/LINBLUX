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

    // Skybox das imagens skyline1/: ligada por padrão, desligável por projeto.
    bool skyboxEnabled() const { return skyboxEnabled_; }
    void setSkyboxEnabled(bool enabled) { skyboxEnabled_ = enabled; }

    // Modo de céu: Classic (cubo) ou Atmospheric (skydome hemisférico).
    SkyMode skyMode() const { return skyMode_; }
    void setSkyMode(SkyMode mode) { skyMode_ = mode; }

    // Cor do horizonte (usada no modo Atmospheric para gradiente vertical).
    Color horizonColor() const { return horizonColor_; }
    void setHorizonColor(Color color) { horizonColor_ = color; }

    // Força do efeito atmosférico (0 = desligado, 1 = gradiente forte).
    float atmosphereStrength() const { return atmosphereStrength_; }
    void setAtmosphereStrength(float strength) {
        atmosphereStrength_ = std::isfinite(strength) ? std::clamp(strength, 0.0f, 1.0f) : 0.0f;
    }

    // Rotação do céu em graus ao redor do eixo Y (apenas modo Atmospheric).
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
    Vector3 sunDirection_{-0.45f, -1.0f, -0.35f};
    float sunIntensity_ = 1.0f;
    float brightness_ = 1.0f;
    Color fogColor_{120, 180, 240, 255};
    float fogDensity_ = 0.0f;
    bool skyboxEnabled_ = true;
    SkyMode skyMode_ = SkyMode::Atmospheric;
    Color horizonColor_{180, 200, 230, 255};
    float atmosphereStrength_ = 0.6f;
    float skyRotation_ = 0.0f;
};

}  // namespace loblox

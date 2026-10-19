#pragma once

#include <string>

#include "core/Math.h"
#include "objects/Instance.h"

namespace loblox {

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

private:
    Color skyColor_{120, 180, 240, 255};
    Color ambientColor_{150, 150, 160, 255};
    Vector3 sunDirection_{-0.45f, -1.0f, -0.35f};
    float sunIntensity_ = 1.0f;
    float brightness_ = 1.0f;
};

}  // namespace loblox

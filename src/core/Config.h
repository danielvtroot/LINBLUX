#pragma once

#include <string>

namespace loblox {

// Configuracao conservadora: o alvo e um notebook com 3 GB de RAM e HD mecanico.
struct AppConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    int targetFps = 60;            // limite configuravel de renderizacao
    bool vsync = true;
    bool resizable = true;

    // Fisica independente da taxa de renderizacao.
    float physicsFixedStep = 1.0f / 60.0f;
    int physicsMaxSubSteps = 5;
    // 1 unidade de mundo ~ 10 cm; gravidade realista (-9.81 m/s^2) em unidades.
    float gravity = -98.1f;

    std::string defaultProjectPath = "projects/default.loblox.json";
};

}  // namespace loblox

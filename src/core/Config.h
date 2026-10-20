#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace loblox {

inline std::string DefaultProjectPath() {
    namespace fs = std::filesystem;
    const char* dataHome = std::getenv("XDG_DATA_HOME");
    if (dataHome != nullptr && dataHome[0] == '/') {
        return (fs::path(dataHome) / "loblox/projects/default.loblox.json").string();
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return (fs::path(home) / ".local/share/loblox/projects/default.loblox.json").string();
    }
    std::error_code error;
    const fs::path current = fs::current_path(error);
    return ((error ? fs::path(".") : current) / "projects/default.loblox.json").string();
}

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

    // Caminho padrao de save/abertura. Tambem e o "ultimo projeto" usado
    // automaticamente na inicializacao quando o arquivo existe.
    std::string defaultProjectPath = DefaultProjectPath();
    // Definido apenas por `--project`; vazio significa usar o caminho padrao.
    std::string startupProjectPath;
};

}  // namespace loblox

#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "objects/Instance.h"

namespace loblox::diag {

enum class Level { Debug, Info, Warning, Error };

// Evento estruturado e limitado: nao inclui conteudo de projeto nem segredos.
struct Event {
    Level level = Level::Info;
    std::string subsystem;
    std::string name;
    std::string correlationId;
    InstanceId entityId = kInvalidInstanceId;
    std::string file;
    double durationMs = -1.0;
    std::string result;
    std::string error;
    nlohmann::json context = nlohmann::json::object();
};

// Configura o arquivo JSONL. Vazio usa o caminho padrao do usuario.
// Pode ser chamado nos testes para isolar o log em um diretorio temporario.
void configure(const std::string& path);
void log(const Event& event);
std::string newCorrelationId();
const char* LevelName(Level level);

}  // namespace loblox::diag

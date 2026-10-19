#pragma once

#include <string>

namespace loblox {

class Scene;

namespace projectio {

// Versao atual do formato. Arquivos com versao maior sao rejeitados.
constexpr int kFormatVersion = 1;
constexpr const char* kFormatName = "loblox-project";

// Serializa a cena inteira (servicos, hierarquia, propriedades).
std::string toJson(const Scene& scene);

// Valida e reconstroi a cena. Em caso de erro, `error` descreve o problema.
// A cena e limpa apenas depois de o texto passar na validacao estrutural.
bool fromJson(Scene& scene, const std::string& text, std::string& error);

// Escrita atomica: grava em <path>.tmp, sincroniza e renomeia.
bool save(const Scene& scene, const std::string& path, std::string& error);
bool load(Scene& scene, const std::string& path, std::string& error);

}  // namespace projectio
}  // namespace loblox

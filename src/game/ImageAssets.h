#pragma once
#include <string>
#include <vector>
#include "raylib.h"

namespace loblox::game {

// Importação portátila de imagens: o projeto sempre referencia
// `assets/<id>.<ext>` relativo ao diretorio do executável; o ID deriva do
// conteúdo (FNV-1a 64), então reimportar o mesmo PNG reusar o arquivo.
struct ImageAsset {
    std::string id;
    std::string filename;
};

// assets/ padrão na raiz do executável (caminho relativo, portátil).
std::string AssetsDir();
// Bundled user assets can be found when launching from a different CWD.
std::string BuiltinAssetPath(const std::string& relativePath);
void ClearImageTextures(); // call before closing the graphics context

// Copia o arquivo para assets/ com ID estável. Não altera o original.
bool ImportImageTo(const std::string& assetsDir, const std::string& sourcePath,
                   std::string& outAssetPath, std::string& error);

// Lista .png/.jpg existentes em assets/, ordenada por nome.
std::vector<ImageAsset> ListImages(const std::string& assetsDir);

// Caminho absoluto-lógico do asset dentro de assets/ (ou vazio se ausente).
std::string ResolveImagePath(const std::string& assetsDir, const std::string& assetPath);

// Textura cacheada por caminho relativo; nunca falha forte: .id==0 quando
// o arquivo não existe (o render simplesmente pula o quad).
Texture2D AssetTexture(const std::string& relativePath);

}  // namespace loblox::game

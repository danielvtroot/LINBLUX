#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "raylib.h"

namespace loblox {
class Scene;
namespace experiences { struct Manifest; }
}

namespace loblox::game {

// Importação portátila de imagens: o projeto sempre referencia
// `assets/<id>.<ext>` relativo ao diretorio do executável; o ID deriva do
// conteúdo (FNV-1a 64), então reimportar o mesmo PNG reusar o arquivo.
struct ImageAsset {
    std::string id;        // identificador técnico estável (hash do conteúdo)
    std::string filename;  // arquivo físico em assets/
    std::string name;      // nome amigável persistido em assets/image-names.json
};

// Onde uma imagem está sendo usada na cena. A UI usa isto para avisar antes de
// excluir um arquivo, nunca para apagar referências em silêncio.
struct ImageUsage {
    std::string where;   // descrição legível (Peça/Personagem/Adesivo)
    std::string kind;    // decal, texture, body, face, sky
    std::uint64_t object = 0;  // InstanceId da Part, quando aplicável
    std::string rig, decal, property, reference;
    int body = -1;
};

// assets/ padrão na raiz do executável (caminho relativo, portátil).
std::string AssetsDir();
void SetProjectAssetRoot(const std::string& root);
std::string ProjectAssetRoot();
std::string ImageReference(const std::string& path);
Image LoadProjectImage(const std::string& path);
void SetProjectFaceImage(const std::string& reference);
std::string ProjectFaceImage();
// Bundled user assets can be found when launching from a different CWD.
std::string BuiltinAssetPath(const std::string& relativePath);
void ClearImageTextures(); // call before closing the graphics context

enum class MediaKind { Any, Image, Audio };

// Copia o arquivo para assets/ com ID estável. Não altera o original.
// Imagens exigem decodificação real; áudio é validado pelo container
// (RIFF/WAVE, OggS ou quadro MPEG) para que o arquivo gravado seja o mesmo que
// o runtime abrirá depois.
bool ImportMediaTo(const std::string& assetsDir, const std::string& sourcePath,
                   MediaKind kind, std::string& outAssetPath, std::string& error);
bool ImportImageTo(const std::string& assetsDir, const std::string& sourcePath,
                   std::string& outAssetPath, std::string& error);

// Lista .png/.jpg existentes em assets/, ordenada por nome amigável.
std::vector<ImageAsset> ListImages(const std::string& assetsDir);

// Nome amigável do arquivo/referência. Sem nome registrado, devolve
// "Imagem <4 últimos do ID>" em vez do hash completo; o ID técnico fica só nos
// detalhes. Nunca devolve string vazia para uma referência não vazia.
std::string ImageDisplayName(const std::string& assetsDir, const std::string& reference);

// Renomeia só o metadado de exibição. O arquivo físico e o ID não mudam, então
// nenhuma referência quebra. Nomes repetidos são permitidos (IDs diferentes).
bool RenameImageAsset(const std::string& assetsDir, const std::string& reference,
                      const std::string& name, std::string& error);
void ForgetImageAssetName(const std::string& assetsDir, const std::string& reference);

// Lista todos os usos da imagem na cena (peças, atributos, rigs e adesivos).
std::vector<ImageUsage> DescribeImageUsage(const Scene& scene, const std::string& reference);
std::vector<ImageUsage> DescribeImageUsage(const experiences::Manifest& manifest, const std::string& reference);
bool ImageReferenceMatches(const std::string& a, const std::string& b);
bool IsImageProperty(const std::string& property);
// Altera só o vínculo identificado. Recusa descritores obsoletos; vazio remove a associação.
bool SetImageUsage(Scene& scene,const ImageUsage& usage,const std::string& replacement,std::string& error);

// Troca todas as referências de uma imagem por outra em peças e rigs.
// Devolve quantas referências foram reescritas.
std::size_t ReplaceImageReferences(Scene& scene, const std::string& from, const std::string& to);

// Nome de exibição de membro do R6 (Cabeça, Braço esquerdo...). Só apresentação:
// o nome persistido no rig não é alterado.
std::string BodyDisplayName(const std::string& bodyName);

// Referência canônica gravada no projeto: sempre `assets/<arquivo>`.
std::string ImageAssetReference(const std::string& filename);

// Converte uma referência herdada (nome solto, caminho absoluto dentro do
// projeto) na forma canônica. Referências externas e nomes de recursos
// embutidos são devolvidos como estão para que a importação decida. Esta é a
// única regra de normalização: UI e persistência usam a mesma função.
std::string NormalizeImageReference(const std::string& reference);

// true quando alguma Part, rig, decal ou céu da cena usa a imagem. A remoção
// consulta isto antes de apagar o arquivo, para nunca deixar referência órfã.
bool ImageInUse(const Scene& scene, const std::string& reference);

// Remove um asset importado de assets/. Recusa symlinks, caminhos fora do
// diretório de assets e arquivos ainda referenciados.
bool RemoveAssetFile(const std::string& assetsDir, const std::string& filename,
                     bool inUse, std::string& error);

// Caminho absoluto-lógico do asset dentro de assets/ (ou vazio se ausente).
std::string ResolveImagePath(const std::string& assetsDir, const std::string& assetPath);

// Textura cacheada por caminho relativo; nunca falha forte: .id==0 quando
// o arquivo não existe (o render simplesmente pula o quad).
Texture2D AssetTexture(const std::string& relativePath);

}  // namespace loblox::game

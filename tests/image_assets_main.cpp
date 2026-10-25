#include "core/Diagnostics.h"
#include "experiences/ExperienceManager.h"
#include "game/ImageAssets.h"
#include "game/Rig.h"
#include "objects/Part.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"
#include "nlohmann/json.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace loblox;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
void write(const fs::path& path, const std::string& text) {
    std::string error;
    if (!projectio::writeFileAtomically(path.string(), text, error)) throw std::runtime_error(error);
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string attribute(const Instance& object, const char* name) {
    const auto it = object.attributes().find(name);
    if (it == object.attributes().end()) return {};
    if (const auto* text = std::get_if<std::string>(&it->second)) return *text;
    return {};
}
}  // namespace

int main() {
    SetTraceLogLevel(LOG_ERROR);
    const auto root = fs::temp_directory_path() / ("linblux-images-" + experiences::newId());
    try {
        diag::configure((root / "images.jsonl").string());
        fs::create_directories(root / "defaults");
        experiences::ExperienceManager::writeFactoryResources(root / "factory");
        experiences::ExperienceManager library(root / "data/linblux", root / "factory", root / "defaults");

        experiences::CreateOptions options;
        options.name = "Experiência com imagens";
        auto created = library.create(options);
        Scene scene;
        library.open(created, scene);

        // O personagem de fábrica nasce sem rosto/decal obrigatório: nem o
        // manifesto da nova experiência nem os corpos do rig carregam imagem.
        require(created.manifest.faceImage.empty(), "Nova experiência herdou rosto de fábrica");
        for (const auto& rig : scene.gameAssets().rigs)
            for (const auto& body : rig.bodies) {
                require(body.image.empty(), "Rig de fábrica veio com imagem de membro");
                for (const auto& decal : body.decals)
                    require(decal.surface.image.empty(), "Rig de fábrica veio com decal de rosto");
            }
        const auto factoryCharacterPath = game::BuiltinAssetPath("assets/defaults/factory/character.loblox.json");
        require(!factoryCharacterPath.empty(), "Personagem de fábrica não encontrado");
        const auto factoryCharacter = json::parse(read(factoryCharacterPath));
        for (const auto& rig : factoryCharacter["game"]["rigs"])
            for (const auto& body : rig["bodies"]) {
                require(!body.contains("image"), "Personagem de fábrica traz imagem embutida");
                require(!body.contains("decals"), "Personagem de fábrica traz decal embutido");
            }

        // Uma imagem real (PNG gerado pelo próprio raylib) aplicada como decal
        // numa Part e como imagem de membro do rig do jogador.
        Image image = GenImageColor(64, 64, {40, 120, 200, 255});
        const auto source = root / "decal-source.png";
        require(ExportImage(image, source.string().c_str()), "PNG de teste não foi criado");
        UnloadImage(image);
        const auto reference = library.importImage(source);
        require(reference.rfind("assets/", 0) == 0, "Importação não devolveu referência canônica assets/");
        require(fs::is_regular_file(created.root / reference), "Asset importado não existe na experiência");

        auto* decalPart = scene.createPart(PartShape::Block, {4, 2, 0}, {4, 4, 1}, "DecalImagem");
        decalPart->setAttribute("decal_image", reference);
        auto& rig = scene.gameAssets().createLinbluxCharacter();
        const std::string rigId = rig.id;
        rig.bodies[game::Head].image = reference;
        require(game::ImageInUse(scene, reference), "Uso da imagem não foi detectado na cena");

        library.save(scene);
        library.close();

        Scene reopened;
        library.open(created, reopened);
        const auto* savedPart = reopened.findPart(decalPart->id());
        require(savedPart && attribute(*savedPart, "decal_image") == reference,
                "Decal não sobreviveu ao salvamento/reabertura");
        const auto* savedRig = reopened.gameAssets().findRig(rigId);
        require(savedRig && savedRig->bodies[game::Head].image == reference,
                "Imagem do rig não sobreviveu ao salvamento");
        require(!game::ResolveImagePath(game::AssetsDir(), reference).empty(),
                "Referência salva não resolve para o arquivo da experiência");

        // Nome amigável é só metadado: não altera arquivo, ID nem referências.
        std::string nameError;
        require(game::RenameImageAsset(game::AssetsDir(), reference, "Textura azul", nameError), nameError.c_str());
        require(game::ImageDisplayName(game::AssetsDir(), reference) == "Textura azul",
                "Nome amigável não foi aplicado");
        require(fs::is_regular_file(created.root / reference),
                "Renomear mudou o caminho físico da imagem");
        require(!game::RenameImageAsset(game::AssetsDir(), reference, "   ", nameError),
                "Nome vazio foi aceito");

        // Duas imagens podem compartilhar o mesmo nome sem trocar de ID.
        Image alternate = GenImageColor(48, 48, {200, 80, 40, 255});
        const auto alternateSource = root / "alternate.png";
        require(ExportImage(alternate, alternateSource.string().c_str()), "PNG alternativo não foi criado");
        UnloadImage(alternate);
        const auto alternateReference = library.importImage(alternateSource);
        require(alternateReference != reference, "Imagens diferentes receberam o mesmo identificador");
        require(game::RenameImageAsset(game::AssetsDir(), alternateReference, "Textura azul", nameError), nameError.c_str());
        const auto named = game::ListImages(game::AssetsDir());
        std::size_t sameName = 0;
        for (const auto& asset : named) sameName += asset.name == "Textura azul";
        require(sameName == 2, "Nomes repetidos não coexistem no gerenciador");
        require(attribute(*savedPart, "decal_image") == reference, "Renomear quebrou a referência do decal");
        library.removeAsset(alternateReference, false);
        require(!fs::exists(created.root / alternateReference), "Imagem não utilizada não foi excluída");

        // Dependências são relatadas com objeto e tipo antes de excluir.
        const auto usage = game::DescribeImageUsage(reopened, reference);
        bool partUse = false, rigUse = false;
        for (const auto& entry : usage) {
            partUse |= entry.where.find("DecalImagem") != std::string::npos && entry.kind == "decal";
            rigUse |= entry.kind == "body";
        }
        require(partUse && rigUse, "Uso da imagem em peça e rig não foi relatado");
        require(game::BodyDisplayName("Head") == "Cabeça", "Nome de exibição do membro não é Cabeça");

        // Substituir reescreve peça, membro e adesivo de uma vez.
        Scene swapScene;
        auto* swapPart = swapScene.createPart(PartShape::Block, {0, 2, 0}, {1, 1, 1}, "Troca");
        swapPart->setAttribute("decal_image", reference);
        auto& swapRig = swapScene.gameAssets().createR6();
        swapRig.bodies[game::Torso].image = reference;
        game::RigDecal swapDecal;
        swapDecal.id = "swap";
        swapDecal.name = "Adesivo";
        swapDecal.surface.image = reference;
        swapRig.bodies[game::Head].decals.push_back(swapDecal);
        const std::string substitute = "assets/substituta.png";
        require(game::ReplaceImageReferences(swapScene, reference, substitute) == 3,
                "Substituição não cobriu peça, membro e adesivo");
        require(attribute(*swapPart, "decal_image") == substitute &&
                    swapScene.gameAssets().rigs.front().bodies[game::Torso].image == substitute &&
                    swapScene.gameAssets().rigs.front().bodies[game::Head].decals.front().surface.image == substitute,
                "Substituição deixou referência para a imagem antiga");

        // Legado: a UI antiga gravava o nome solto. Abrir, salvar e reabrir
        // precisa migrar para assets/<arquivo> sem erro de asset ausente.
        const std::string bare = fs::path(reference).filename().string();
        auto current = library.inspect(created.root);
        auto placeText = read(created.root / current.manifest.mainPlace);
        const auto position = placeText.find(reference);
        require(position != std::string::npos, "JSON salvo não contém a referência canônica");
        placeText.replace(position, reference.size(), bare);
        write(created.root / current.manifest.mainPlace, placeText);
        library.close();
        Scene migrated;
        library.open(created, migrated);
        const auto* migratedPart = migrated.findPart(decalPart->id());
        require(migratedPart && attribute(*migratedPart, "decal_image") == reference,
                "Referência herdada não foi normalizada ao abrir");
        library.save(migrated);
        const auto savedText = read(created.root / library.inspect(created.root).manifest.mainPlace);
        require(savedText.find("\"" + reference + "\"") != std::string::npos,
                "Salvar não regravou a referência canônica");
        require(savedText.find("\"" + bare + "\"") == std::string::npos,
                "Salvar manteve referência de nome solto");
        library.close();

        // Salvar como padrão da engine e criar uma nova experiência a partir dele.
        library.open(created, migrated);
        library.saveEngineDefaults(migrated, rigId);
        const auto defaults = json::parse(read(root / "defaults/index.json"));
        require(defaults.contains("world") && defaults.contains("character"),
                "Padrão da engine não publicou mundo e personagem");
        const auto engineCharacter = json::parse(read(root / "defaults" /
            defaults["character"].get<std::string>() / "experience.json"));
        require(engineCharacter.value("faceImage", std::string()).empty(),
                "Personagem padrão da engine foi salvo com rosto obrigatório");
        experiences::CreateOptions fresh;
        fresh.name = "Nova experiência do padrão";
        const auto derived = library.create(fresh);
        Scene derivedScene;
        library.open(derived, derivedScene);
        require(derived.manifest.faceImage.empty(), "Experiência derivada herdou rosto de fábrica");
        auto* derivedPart = derivedScene.findPart(decalPart->id());
        require(derivedPart && attribute(*derivedPart, "decal_image") == reference,
                "Decal não foi herdado pelo padrão da engine");
        require(fs::is_regular_file(derived.root / reference),
                "Imagem herdada não foi copiada para a nova experiência");
        require(!game::ResolveImagePath(game::AssetsDir(), reference).empty(),
                "Imagem herdada não resolve na nova experiência");

        // Remoção segura: em uso na cena ou na apresentação é recusada.
        bool refused = false;
        try { library.removeAsset(reference, true); } catch (const std::exception&) { refused = true; }
        require(refused && fs::is_regular_file(derived.root / reference), "Asset em uso foi apagado");

        derivedPart->removeAttribute("decal_image");
        for (auto& body : derivedScene.gameAssets().rigs.front().bodies) body.image.clear();
        for (auto& body : derivedScene.gameAssets().rigs.front().bodies) body.decals.clear();
        auto manifest = library.active()->manifest;
        manifest.icon = reference;
        library.updateMetadata(manifest);
        refused = false;
        try { library.removeAsset(reference, game::ImageInUse(derivedScene, reference)); }
        catch (const std::exception&) { refused = true; }
        require(refused && fs::is_regular_file(derived.root / reference),
                "Asset usado na apresentação foi apagado");

        manifest = library.active()->manifest;
        manifest.icon.clear();
        library.updateMetadata(manifest);
        library.removeAsset(reference, game::ImageInUse(derivedScene, reference));
        require(!fs::exists(derived.root / reference), "Remoção autorizada não apagou o arquivo");

        // Referência realmente ausente continua sendo erro, nunca aprovação.
        auto* broken = derivedScene.createPart(PartShape::Block, {0, 2, 0}, {1, 1, 1}, "SemImagem");
        broken->setAttribute("decal_image", "assets/0000000000000000.png");
        refused = false;
        try { library.save(derivedScene); } catch (const std::exception& error) {
            refused = std::string(error.what()).find("Missing image asset") != std::string::npos;
        }
        require(refused, "Salvar com imagem ausente deveria falhar com 'Missing image asset'");

        std::printf("IMAGENS: importação, decal, rig, migração herdada, padrão da engine e remoção segura passaram. Fixture: %s\n",
                    root.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Imagens: %s\n", error.what());
        std::error_code ignored;
        return 1;
    }
}

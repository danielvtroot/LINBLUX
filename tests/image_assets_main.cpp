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
        experiences::CreateOptions fresh;
        fresh.name = "Nova experiência do padrão";
        const auto derived = library.create(fresh);
        Scene derivedScene;
        library.open(derived, derivedScene);
        const auto* derivedPart = derivedScene.findPart(decalPart->id());
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

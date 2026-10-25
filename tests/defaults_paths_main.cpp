#include "experiences/ExperienceManager.h"
#include "serialization/ProjectIO.h"
#include "nlohmann/json.hpp"
#include <cstdio>
#include <cstdlib>
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
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void restorePermissions(const fs::path& root) {
    std::error_code ignored;
    fs::permissions(root / "factory", fs::perms::owner_all, fs::perm_options::add, ignored);
    fs::permissions(root / "defaults", fs::perms::owner_all, fs::perm_options::add, ignored);
}
}  // namespace

int main() {
    const auto root = fs::temp_directory_path() / ("linblux-default-paths-" + experiences::newId());
    try {
        // A distributable factory tree: build/install resources are read-only
        // to the Studio.  The rewrite must never need to touch it.
        fs::create_directories(root / "factory");
        experiences::ExperienceManager::writeFactoryResources(root / "factory");
        fs::copy(fs::path(__FILE__).parent_path().parent_path() / "assets/defaults",
                 root / "defaults", fs::copy_options::recursive);
        const auto originalIndex = read(root / "defaults/index.json");
        fs::permissions(root / "factory",
                        fs::perms::owner_read | fs::perms::owner_exec |
                            fs::perms::group_read | fs::perms::group_exec |
                            fs::perms::others_read | fs::perms::others_exec,
                        fs::perm_options::replace);
        fs::permissions(root / "defaults",
                        fs::perms::owner_read | fs::perms::owner_exec |
                            fs::perms::group_read | fs::perms::group_exec |
                            fs::perms::others_read | fs::perms::others_exec,
                        fs::perm_options::replace);

        // A: a writable user-data root must be chosen independently from the
        // read-only factory tree.  GetApplicationDirectory() ends in '/', so
        // this also protects the old parent_path() resolution bug.
        experiences::ExperienceManager manager(root / "user-data", root / "factory");
        require(manager.engineDefaultsRoot() == fs::absolute(root / "user-data/engine-defaults"),
                "O padrão personalizado não foi resolvido para XDG/user-data");
        require(manager.factoryDefaultsRoot() == fs::absolute(root / "defaults"),
                "O padrão de fábrica não foi mantido separado");
        require(manager.engineDefaultsRoot() != manager.factoryDefaultsRoot(),
                "O padrão personalizado ainda aponta para os recursos de fábrica");

        auto draft = manager.editEngineDefault(false);
        Scene scene;
        manager.open(draft, scene);
        const auto rig = scene.gameAssets().defaultRig;
        require(scene.gameAssets().findRig(rig) != nullptr, "Personagem de fábrica ausente");
        auto* marker = scene.createPart(PartShape::Block, {4, 8, 4}, {2, 2, 2}, "Marcador de caminho");
        const auto markerId = marker->id();
        manager.saveEngineDefaults(scene, rig);

        require(fs::is_regular_file(manager.engineDefaultsRoot() / "index.json"),
                "O padrão personalizado não foi publicado na raiz do usuário");
        require(read(root / "defaults/index.json") == originalIndex,
                "Salvar o padrão alterou o índice de fábrica");
        require(!fs::exists(root / "defaults/revisions"),
                "Salvar o padrão criou revisões dentro dos recursos de fábrica");
        const auto userIndex = read(manager.engineDefaultsRoot() / "index.json");
        const auto parsed = json::parse(userIndex);
        require(parsed.contains("world") && parsed.contains("character"),
                "Mundo e personagem não foram publicados juntos");

        // B/C: a new manager must load the user default after a restart.  A
        // new experience receives a copy, never a link to the default.
        experiences::ExperienceManager reopened(root / "user-data", root / "factory");
        experiences::CreateOptions options;
        options.name = "Experiência herdada";
        const auto created = reopened.create(options);
        Scene world;
        reopened.open(created, world);
        require(world.findPart(markerId) != nullptr, "Nova experiência não herdou o mundo padrão");
        require(world.gameAssets().defaultRig == rig, "Nova experiência não herdou o personagem padrão");
        world.findPart(markerId)->setName("Marcador independente");
        reopened.save(world);

        const auto another = reopened.create(options);
        Scene second;
        reopened.open(another, second);
        require(second.findPart(markerId) && second.findPart(markerId)->name() == "Marcador de caminho",
                "Alterar uma experiência modificou o padrão da engine");
        require(read(reopened.engineDefaultsRoot() / "index.json") == userIndex,
                "Alterar uma experiência modificou o índice do padrão");

        // The environment path is resolved from XDG_DATA_HOME, not the build
        // directory and not the current working directory.
        setenv("XDG_DATA_HOME", (root / "xdg").c_str(), 1);
        experiences::ExperienceManager fromEnvironment({}, root / "factory");
        require(fromEnvironment.engineDefaultsRoot() == fs::absolute(root / "xdg/linblux/engine-defaults"),
                "XDG_DATA_HOME não foi usado para o padrão personalizado");

        // The executable's real runtime path: no data/factory arguments, the
        // build/install resources are factory-only and the user root is XDG.
        setenv("XDG_DATA_HOME", (root / "runtime-xdg").c_str(), 1);
        experiences::ExperienceManager runtime;
        require(runtime.engineDefaultsRoot() == fs::absolute(root / "runtime-xdg/linblux/engine-defaults"),
                "O construtor de runtime não usou XDG");
        require(!runtime.factoryDefaultsRoot().empty() &&
                    fs::is_regular_file(runtime.factoryDefaultsRoot() / "factory/world.loblox.json"),
                "O construtor de runtime não encontrou a fábrica distribuída");
        auto runtimeDraft = runtime.editEngineDefault(false);
        Scene runtimeScene;
        runtime.open(runtimeDraft, runtimeScene);
        runtime.saveEngineDefaults(runtimeScene, runtimeScene.gameAssets().defaultRig);
        require(fs::is_regular_file(runtime.engineDefaultsRoot() / "index.json"),
                "O construtor de runtime não publicou no XDG");
        require(runtime.factoryDefaultsRoot() != runtime.engineDefaultsRoot(),
                "O runtime publicou dentro dos recursos de fábrica");

        // Explicit source publication is opt-in and must be absolute.
        setenv("LINBLUX_DEFAULTS_ROOT", (root / "authoring").c_str(), 1);
        experiences::ExperienceManager authoring;
        require(authoring.engineDefaultsRoot() == fs::absolute(root / "authoring"),
                "LINBLUX_DEFAULTS_ROOT explícito não foi respeitado");
        setenv("LINBLUX_DEFAULTS_ROOT", "relative-path", 1);
        bool relativeRejected = false;
        try {
            experiences::ExperienceManager invalidAuthoring;
        } catch (const std::exception& error) {
            relativeRejected = std::string(error.what()).find("absoluto") != std::string::npos;
        }
        require(relativeRejected, "Uma raiz de autoria relativa foi aceita");
        unsetenv("LINBLUX_DEFAULTS_ROOT");

        // E: a blocked destination must fail with a readable message before
        // changing any default resource.
        std::ofstream(root / "blocked") << "not a directory";
        experiences::ExperienceManager blocked(root / "blocked-data", root / "factory", root / "blocked");
        auto blockedDraft = blocked.editEngineDefault(false);
        Scene blockedScene;
        blocked.open(blockedDraft, blockedScene);
        bool refused = false;
        std::string message;
        try {
            blocked.saveEngineDefaults(blockedScene, blockedScene.gameAssets().defaultRig);
        } catch (const std::exception& error) {
            refused = true;
            message = error.what();
        }
        require(refused, "Destino bloqueado aceitou a publicação");
        require(message.find("filesystem error") == std::string::npos,
                "O erro de escrita ainda expõe std::filesystem cru");
        require(message.find("diretório") != std::string::npos ||
                    message.find("permissão") != std::string::npos,
                "O erro de escrita não é compreensível em português");
        require(read(root / "defaults/index.json") == originalIndex,
                "Falha de escrita alterou os recursos de fábrica");

        restorePermissions(root);
        fs::remove_all(root);
        std::puts("Caminhos: dados do usuário, fábrica somente leitura, XDG, herança/independência e falha clara passaram.");
        return 0;
    } catch (const std::exception& error) {
        restorePermissions(root);
        std::fprintf(stderr, "%s; fixture: %s\n", error.what(), root.c_str());
        return 1;
    }
}

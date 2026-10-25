#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "scene/Scene.h"

namespace loblox::experiences {

struct Manifest {
    int version = 1;
    std::string id, installationId, name, description, created, modified, lastOpened;
    std::string sourceTemplate, mainPlace, icon, banner, loadingBackground;
    std::string faceImage; // preserves the original built-in face when sharing
    bool archived = false, trashed = false;
    std::string serialize() const;
    static Manifest parse(const std::string& text);
    void validate() const;
};
struct Experience {
    std::filesystem::path root;
    Manifest manifest;
};
struct CreateOptions {
    std::string name, description, templateId = "engine-default";
    std::string icon, banner, loadingBackground;
    bool useDefaultCharacter = true;
};
struct Template {
    std::string id, name, description;
    std::filesystem::path place;
};
struct MissingExperience {std::string installationId,name;std::filesystem::path previousRoot;};

// Owns project directories and metadata. ProjectIO remains the only world
// serializer. Errors throw with a user-readable message; callers keep their
// previous scene/active experience until a transaction has succeeded.
class ExperienceManager {
public:
    explicit ExperienceManager(std::filesystem::path dataRoot = {}, std::filesystem::path factoryRoot = {}, std::filesystem::path engineDefaultsRoot = {});
    static std::filesystem::path defaultDataRoot();
    const std::filesystem::path& libraryRoot() const { return library_; }
    void setLibraryRoot(const std::filesystem::path& path);
    std::vector<Experience> list(bool archived = false) const;
    std::vector<MissingExperience> missing() const;
    Experience relocate(const std::string& installationId,const std::filesystem::path& directory);
    void forgetMissing(const std::string& installationId);
    std::vector<Template> templates() const;
    Experience create(const CreateOptions& options);
    Experience importLegacy(const std::filesystem::path& source, const std::string& name);
    Experience duplicate(const Experience& source, const std::string& name);
    Experience inspect(const std::filesystem::path& root) const;
    void open(const Experience& experience, Scene& destination);
    void loadReadOnly(const Experience& experience,Scene& destination) const;
    void save(const Scene& scene);
    void updateMetadata(Manifest manifest);
    void updateMetadata(const Experience& experience, Manifest manifest);
    void setArchived(const Experience& experience, bool archived, bool trash = false);
    void deletePermanently(const Experience& experience);
    void exportPackage(const Experience& experience, const std::filesystem::path& destination) const;
    Experience importPackage(const std::filesystem::path& package);
    void setDefaultTemplate(const Scene& scene);
    void setDefaultCharacter(const Scene& scene, const std::string& rigId);
    void restoreFactoryDefaults();
    void saveEngineWorld(const Scene& scene);
    void saveEngineCharacter(const Scene& scene,const std::string& rigId);
    void saveEngineDefaults(const Scene& scene,const std::string& rigId);
    std::string importCharacter(const std::filesystem::path& source,Scene& destination);
    Experience editEngineDefault(bool character);
    bool isEngineDefault(const Experience& experience) const;
    const std::filesystem::path& lastDefaultsBackup() const {return lastDefaultsBackup_;}
    const std::filesystem::path& engineDefaultsRoot() const {return defaults_;}
    const std::filesystem::path& factoryDefaultsRoot() const {return factoryDefaults_;}

    std::string importImage(const std::filesystem::path& file);
    std::string importImage(const Experience& experience,const std::filesystem::path& file);
    std::filesystem::path thumbnail(const Experience& experience) const;
    const std::optional<Experience>& active() const { return active_; }
    void close();
    std::filesystem::path activePlace() const;
    const std::vector<std::string>& warnings() const { return warnings_; }
    // Versioned factory resources are generated once for distribution/tests,
    // never into an installed user's defaults or personal project.
    static void writeFactoryResources(const std::filesystem::path& destination);
private:
    std::filesystem::path data_, factory_, library_, defaults_, factoryDefaults_, lastDefaultsBackup_;
    std::optional<Experience> active_;
    std::vector<std::string> warnings_;
    std::filesystem::path loadEngineResource(const std::string& key,Scene& scene) const;
    void ensureDefaultsWritable();
    void saveEngineResource(const std::string& key,const Scene& scene);
    void backupDefaultsIndex(const std::string& index);
    void registerRoot(const std::filesystem::path& root);
    Experience snapshot(const Scene& scene, const std::filesystem::path& sourceRoot,
                        const std::filesystem::path& parent, Manifest manifest, bool registerExperience,
                        const CreateOptions* creation = nullptr);
};

bool safeRelativePath(const std::string& path);
std::string newId();
std::filesystem::path containedFile(const std::filesystem::path& root, const std::string& path);

} // namespace loblox::experiences

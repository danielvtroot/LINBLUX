#include "experiences/ExperienceManager.h"

#include "core/Config.h"
#include "core/Diagnostics.h"
#include "core/ImageInfo.h"
#include "game/ImageAssets.h"
#include "serialization/ProjectIO.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <zlib.h>

namespace loblox::experiences {
namespace fs=std::filesystem;
using json=nlohmann::json;
namespace {
constexpr uint64_t MiB=1024*1024, maxTotal=256*MiB, maxFile=64*MiB;
constexpr unsigned maxEntries=2048;
constexpr char magic[8]={'L','O','E','X','P',1,'\r','\n'};
fs::path normalizedDirectory(const fs::path& value) {
    if(value.empty())return {};
    auto result=fs::absolute(value);
    // GetApplicationDirectory() and user-provided paths may end in '/'.  In
    // std::filesystem an empty trailing filename makes parent_path() point to
    // the same directory, not to its parent.
    while(result.filename().empty()&&result.has_parent_path())result=result.parent_path();
    return result;
}
fs::path applicationDirectory() {
    return normalizedDirectory(GetApplicationDirectory());
}
void createDirectory(const fs::path& path,const char* description) {
    std::error_code error;
    fs::create_directories(path,error);
    if(error)throw std::runtime_error(std::string("Não foi possível criar ")+description+" em '"+path.string()+"': "+error.message());
}
void requireWritableDirectory(const fs::path& path,const char* description) {
    if(path.empty())throw std::runtime_error(std::string("Diretório inválido para ")+description);
    std::error_code error;
    const auto status=fs::symlink_status(path,error);
    if(error&&error!=std::errc::no_such_file_or_directory)
        throw std::runtime_error(std::string("Não foi possível verificar ")+description+" em '"+path.string()+"': "+error.message());
    if(!error) {
        if(fs::is_symlink(status))throw std::runtime_error(std::string(description)+" não pode ser um link simbólico: "+path.string());
        if(!fs::is_directory(status))throw std::runtime_error(std::string(description)+" não é um diretório: "+path.string());
    } else {
        createDirectory(path,description);
    }
    // Check real write permission instead of relying on access(2): effective
    // users, read-only mounts and ACLs do not agree with the permission bits.
    std::string pattern=(path/".linblux-write-check-XXXXXX").string();
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back('\0');
    const int descriptor=::mkstemp(name.data());
    if(descriptor<0)throw std::runtime_error(std::string("Sem permissão de escrita em ")+description+" '"+path.string()+"': "+std::strerror(errno));
    const int closeResult=::close(descriptor);
    const int closeError=errno;
    ::unlink(name.data());
    if(closeResult!=0)throw std::runtime_error(std::string("Falha ao fechar a verificação de escrita em ")+description+" '"+path.string()+"': "+std::strerror(closeError));
}
json boundedJson(const std::string& text) {
    return json::parse(text,[](int depth,json::parse_event_t,json&){if(depth>64)throw std::runtime_error("JSON nesting exceeds 64 levels");return true;});
}
std::string read(const fs::path& path,uint64_t limit=maxFile) {
    if(!fs::is_regular_file(path)||fs::is_symlink(fs::symlink_status(path)))throw std::runtime_error("Missing or unsafe file: "+path.string());
    if(fs::file_size(path)>limit)throw std::runtime_error("File exceeds memory budget: "+path.string());
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read "+path.string());
    return {std::istreambuf_iterator<char>(in),{}};
}
void write(const fs::path& path,const std::string& content) {
    std::string error;if(!projectio::writeFileAtomically(path.string(),content,error))throw std::runtime_error(error);
}
std::string now() {
    const auto clock=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};gmtime_r(&clock,&utc);char text[32]{};std::strftime(text,sizeof(text),"%Y-%m-%dT%H:%M:%SZ",&utc);return text;
}
json settings(const fs::path& data) {
    const auto path=data/"library.json";return fs::exists(path)?json::parse(read(path,MiB)):json::object();
}
void saveSettings(const fs::path& data,const json& value){write(data/"library.json",value.dump(2));}
void event(const char* name,const fs::path& root) {
    diag::Event e;e.subsystem="experience";e.name=name;e.file=root.string();e.result="ok";diag::log(e);
}
bool inside(const fs::path& path,const fs::path& directory) {
    const auto root=fs::weakly_canonical(directory);
    auto current=fs::weakly_canonical(path);
    while(!current.empty()) {if(current==root)return true;const auto parent=current.parent_path();if(parent==current)break;current=parent;}
    return false;
}
bool identifier(const std::string& text) {
    return text.size()==32&&std::all_of(text.begin(),text.end(),[](char c){return (c>='a'&&c<='f')||(c>='0'&&c<='9');});
}
void mediaPath(const std::string& path) {
    if(!path.empty()&&(!safeRelativePath(path)||path.rfind("assets/",0)!=0))throw std::runtime_error("Invalid relative image reference: "+path);
}
struct Stage {
    fs::path path;bool committed=false;
    explicit Stage(const fs::path& parent) {
        createDirectory(parent,"o diretório de destino");
        path=parent/(".staging-"+newId());
        std::error_code error;
        if(!fs::create_directory(path,error))throw std::runtime_error("Não foi possível reservar o diretório temporário em '"+parent.string()+"': "+error.message());
        fs::permissions(path,fs::perms::owner_all,error);
        if(error)throw std::runtime_error("Não foi possível proteger o diretório temporário '"+path.string()+"': "+error.message());
    }
    ~Stage(){if(!committed){std::error_code error;fs::remove_all(path,error);}}
    fs::path publish(const std::string& id){const auto target=path.parent_path()/id;if(fs::exists(target))throw std::runtime_error("Installation ID already exists");fs::rename(path,target);committed=true;return target;}
};
std::string imageImport(const fs::path& root,const fs::path& source) {
    std::string path,error;
    if(!game::ImportImageTo((root/"assets").string(),source.string(),path,error))throw std::runtime_error(error);
    return "assets/"+fs::path(path).filename().string();
}
bool isImageKey(const std::string& key) {
    return key=="image"||key=="decal_image"||key=="texture_image"||key=="FaceTexture"||
        key=="skyFront"||key=="skyBack"||key=="skyLeft"||key=="skyRight"||key=="skyUp"||key=="skyDown";
}
// Only known image fields are relocated. Script source and arbitrary string
// attributes are never rewritten by a substring replacement.
void walkImages(json& node,const std::function<std::string(const std::string&)>& visit) {
    if(node.is_object())for(auto it=node.begin();it!=node.end();++it) {
        if(isImageKey(it.key())&&it.value().is_string()&&!it.value().get<std::string>().empty())it.value()=visit(it.value().get<std::string>());
        else walkImages(it.value(),visit);
    } else if(node.is_array())for(auto& item:node)walkImages(item,visit);
}
std::string relocateWorld(const Scene& scene,const fs::path& from,const fs::path& target) {
    auto j=json::parse(projectio::toJson(scene));
    std::map<std::string,std::string> copied;
    walkImages(j,[&](const std::string& ref){
        if(const auto it=copied.find(ref);it!=copied.end())return it->second;
        fs::path source;
        if(safeRelativePath(ref)&&fs::is_regular_file(target/ref)) {
            ImageInfo info;if(!ReadSafeImageInfo(containedFile(target,ref).string(),info))throw std::runtime_error("Invalid project image: "+ref);
            copied[ref]=ref;return ref;
        }
        else if(fs::path(ref).is_absolute())source=ref;
        else if(!from.empty()&&safeRelativePath(ref)&&fs::is_regular_file(from/ref))source=containedFile(from,ref);
        else if(fs::is_regular_file(ref))source=ref;
        else source=game::BuiltinAssetPath(ref);
        if(source.empty())throw std::runtime_error("Missing image asset: "+ref+". Original project was not changed.");
        const auto result=imageImport(target,source);copied[ref]=result;return result;
    });
    return j.dump(2);
}
Scene& loadWorld(Scene& scene,const Experience& e) {
    const auto world=read(containedFile(e.root,e.manifest.mainPlace));
    auto j=boundedJson(world);
    walkImages(j,[&](const std::string& ref){mediaPath(ref);const auto file=containedFile(e.root,ref);ImageInfo info;
        if(!ReadSafeImageInfo(file.string(),info))throw std::runtime_error("Invalid/missing image asset: "+ref);
        return ref;});
    for(const auto& ref:{e.manifest.icon,e.manifest.banner,e.manifest.loadingBackground,e.manifest.faceImage})if(!ref.empty()) {
        ImageInfo info;if(!ReadSafeImageInfo(containedFile(e.root,ref).string(),info))throw std::runtime_error("Invalid presentation image: "+ref);
    }
    std::string error;if(!projectio::fromJson(scene,world,error))throw std::runtime_error("Invalid place: "+error);return scene;
}
void readBytes(std::istream& in,char* destination,size_t size) {
    if(!in.read(destination,std::streamsize(size)))throw std::runtime_error("Truncated .lo package");
}
uint64_t readNumber(std::istream& in,int bytes) {
    uint64_t value=0;for(int i=0;i<bytes;++i){const auto c=in.get();if(c<0)throw std::runtime_error("Truncated .lo header");value|=uint64_t(uint8_t(c))<<(i*8);}return value;
}
void writeNumber(FILE* out,uint64_t value,int bytes) {
    for(int i=0;i<bytes;++i)if(std::fputc(int((value>>(i*8))&255),out)==EOF)throw std::runtime_error("Cannot write .lo package");
}
bool packagePath(const std::string& path) {
    return safeRelativePath(path)&&(path=="experience.json"||path.rfind("places/",0)==0||path.rfind("assets/",0)==0);
}
} // namespace

std::string newId() {
    std::random_device random;std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(int i=0;i<4;++i)out<<std::setw(8)<<uint32_t(random());
    return out.str();
}
bool safeRelativePath(const std::string& text) {
    if(text.empty()||text.size()>512||text.find('\\')!=std::string::npos||text.find(':')!=std::string::npos||std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32||c==127;}))return false;
    const fs::path path(text);if(path.is_absolute()||path.generic_string()!=text)return false;
    for(const auto& item:path)if(item==".."||item=="."||item.empty())return false;
    return true;
}
fs::path containedFile(const fs::path& root,const std::string& name) {
    if(!safeRelativePath(name))throw std::runtime_error("Unsafe project path: "+name);
    auto current=root;
    if(fs::is_symlink(fs::symlink_status(root)))throw std::runtime_error("Symlink project root is not supported");
    for(const auto& item:fs::path(name)){current/=item;if(fs::is_symlink(fs::symlink_status(current)))throw std::runtime_error("Symlink inside experience: "+name);}
    return current;
}
void Manifest::validate() const {
    if(version!=1||!identifier(id)||!identifier(installationId))throw std::runtime_error("Unsupported experience schema or invalid ID");
    if(name.empty()||name.size()>128||name.find_first_of("\r\n")!=std::string::npos||description.size()>4096)throw std::runtime_error("Name (1..128) or description (0..4096) is invalid");
    if(!safeRelativePath(mainPlace)||mainPlace.rfind("places/",0)!=0)throw std::runtime_error("Invalid main place path");
    mediaPath(icon);mediaPath(banner);mediaPath(loadingBackground);mediaPath(faceImage);
    for(const auto& date:{created,modified})if(date.size()!=20||date.back()!='Z')throw std::runtime_error("Invalid experience timestamp");
}
std::string Manifest::serialize() const {
    validate();return json{{"format","loblox-experience"},{"version",version},{"id",id},{"installationId",installationId},
        {"name",name},{"description",description},{"created",created},{"modified",modified},{"lastOpened",lastOpened},
        {"sourceTemplate",sourceTemplate},{"mainPlace",mainPlace},{"icon",icon},{"banner",banner},{"loadingBackground",loadingBackground},
        {"faceImage",faceImage},{"archived",archived},{"trashed",trashed}}.dump(2);
}
Manifest Manifest::parse(const std::string& text) {
    const auto j=boundedJson(text);if(j.at("format")!="loblox-experience")throw std::runtime_error("Não é uma experiência LINBLUX compatível");
    Manifest m;m.version=j.at("version").get<int>();m.id=j.at("id");m.installationId=j.at("installationId");m.name=j.at("name");
    m.description=j.value("description","");m.created=j.at("created");m.modified=j.at("modified");m.lastOpened=j.value("lastOpened","");
    m.sourceTemplate=j.value("sourceTemplate","");m.mainPlace=j.at("mainPlace");m.icon=j.value("icon","");m.banner=j.value("banner","");
    m.loadingBackground=j.value("loadingBackground","");m.faceImage=j.value("faceImage","");m.archived=j.value("archived",false);m.trashed=j.value("trashed",false);m.validate();return m;
}
fs::path ExperienceManager::defaultDataRoot() {
    if(const char* x=std::getenv("XDG_DATA_HOME");x&&x[0]=='/')return (fs::exists(fs::path(x)/"linblux")||!fs::exists(fs::path(x)/"loblox")?fs::path(x)/"linblux":fs::path(x)/"loblox");
    if(const char* h=std::getenv("HOME");h&&h[0]=='/')return (fs::exists(fs::path(h)/".local/share/linblux")||!fs::exists(fs::path(h)/".local/share/loblox")?fs::path(h)/".local/share/linblux":fs::path(h)/".local/share/loblox");
    throw std::runtime_error("Set HOME or XDG_DATA_HOME to an absolute directory");
}
ExperienceManager::ExperienceManager(fs::path dataRoot,fs::path factoryRoot,fs::path engineDefaultsRoot)
    :data_(normalizedDirectory(dataRoot.empty()?defaultDataRoot():dataRoot)),factory_(std::move(factoryRoot)) {
    createDirectory(data_,"o diretório de dados do LINBLUX");
    auto config=settings(data_);
    library_=config.value("library",(data_/"experiences").string());
    createDirectory(library_,"a biblioteca de experiências");
    const auto executable=applicationDirectory();
    const bool discoverFactory=factory_.empty();
    if(discoverFactory) {
        std::vector<fs::path> candidates;
        if(!executable.empty()) {
            candidates.push_back(executable/"share/linblux/templates");
            candidates.push_back(executable.parent_path()/"share/linblux/templates");
        }
        candidates.push_back(fs::path(LINBLUX_FACTORY_DIR));
        for(const auto& candidate:candidates)
            if(fs::is_regular_file(candidate/"baseplate.loblox.json")){factory_=candidate;break;}
    }
    if(!factory_.empty()) {
        factory_=normalizedDirectory(factory_);
        const auto sibling=factory_.parent_path()/"defaults";
        if(fs::is_directory(sibling))factoryDefaults_=sibling;
    }
    if(discoverFactory&&factoryDefaults_.empty()) {
        std::vector<fs::path> candidates;
        if(!executable.empty()) {
            candidates.push_back(executable/"share/linblux/defaults");
            candidates.push_back(executable.parent_path()/"share/linblux/defaults");
            candidates.push_back(executable.parent_path()/"assets/defaults");
        }
        candidates.push_back(fs::path(LINBLUX_FACTORY_DIR).parent_path()/"defaults");
        for(const auto& candidate:candidates)
            if(fs::is_directory(candidate)){factoryDefaults_=normalizedDirectory(candidate);break;}
    }
    // Runtime edits always belong to the user's data directory.  Build and
    // install trees contain the read-only factory resources; writing there was
    // the source of the Permission denied failure when the Studio ran from a
    // root-owned build/share directory.
    if(!engineDefaultsRoot.empty()) {
        defaults_=normalizedDirectory(engineDefaultsRoot);
    } else if(const char* path=std::getenv("LINBLUX_DEFAULTS_ROOT");path&&*path) {
        const fs::path authoring(path);
        if(!authoring.is_absolute())throw std::runtime_error("LINBLUX_DEFAULTS_ROOT deve ser um caminho absoluto");
        defaults_=normalizedDirectory(authoring);
    } else {
        defaults_=data_/"engine-defaults";
    }
}
void ExperienceManager::ensureDefaultsWritable() {
    requireWritableDirectory(defaults_,"o diretório de padrões personalizados");
}
void ExperienceManager::setLibraryRoot(const fs::path& path) {
    if(!path.is_absolute())throw std::runtime_error("Choose an absolute library directory");
    fs::create_directories(path);auto config=settings(data_);config["library"]=fs::canonical(path).string();saveSettings(data_,config);library_=fs::canonical(path);
}
void ExperienceManager::registerRoot(const fs::path& root) {
    if(isEngineDefault(inspect(root)))return;
    auto config=settings(data_);auto& entries=config["roots"];if(!entries.is_array())entries=json::array();
    const auto value=fs::absolute(root).string();if(std::find(entries.begin(),entries.end(),json(value))==entries.end())entries.push_back(value);
    const auto e=inspect(root);config["catalog"][e.manifest.installationId]={{"path",value},{"name",e.manifest.name},{"id",e.manifest.id}};
    saveSettings(data_,config);
}
std::vector<MissingExperience> ExperienceManager::missing() const {
    const auto config=settings(data_);std::vector<MissingExperience> result;
    const auto catalog=config.value("catalog",json::object());
    for(const auto& item:catalog.items()) {
        const fs::path path=item.value().at("path").get<std::string>();
        try{const auto e=inspect(path);if(e.manifest.installationId==item.key())continue;}catch(const std::exception&){}
        result.push_back({item.key(),item.value().value("name","Unavailable experience"),path});
    }
    return result;
}
Experience ExperienceManager::relocate(const std::string& id,const fs::path& directory) {
    auto config=settings(data_);if(!config.contains("catalog")||!config["catalog"].contains(id))throw std::runtime_error("Recent experience not found");
    const auto e=inspect(directory);const auto old=config["catalog"][id];
    if(e.manifest.installationId!=id||e.manifest.id!=old.at("id").get<std::string>())throw std::runtime_error("This directory belongs to a different experience");
    auto& roots=config["roots"];roots.erase(std::remove(roots.begin(),roots.end(),old.at("path")),roots.end());roots.push_back(e.root.string());
    config["catalog"][id]["path"]=e.root.string();saveSettings(data_,config);event("relocate",e.root);return e;
}
void ExperienceManager::forgetMissing(const std::string& id) {
    const auto lost=missing();const auto item=std::find_if(lost.begin(),lost.end(),[&](const auto& e){return e.installationId==id;});
    if(item==lost.end())throw std::runtime_error("Only missing recent entries can be forgotten");
    auto config=settings(data_);auto& roots=config["roots"];roots.erase(std::remove(roots.begin(),roots.end(),json(item->previousRoot.string())),roots.end());
    config["catalog"].erase(id);saveSettings(data_,config);
}
std::vector<Experience> ExperienceManager::list(bool archived) const {
    std::set<fs::path> paths;const auto config=settings(data_);
    for(const auto& path:config.value("roots",json::array()))paths.insert(path.get<std::string>());
    for(const auto& entry:fs::directory_iterator(library_))if(entry.is_directory()&&entry.path().filename().string().front()!='.')paths.insert(entry.path());
    std::vector<Experience> out;
    for(const auto& path:paths)try {auto e=inspect(path);if(e.manifest.sourceTemplate=="engine-default-edit")continue;if((e.manifest.archived||e.manifest.trashed)==archived)out.push_back(std::move(e));}catch(const std::exception&) {/* missing entries stay in registry, never recreated */}
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return std::max(a.manifest.modified,a.manifest.lastOpened)>std::max(b.manifest.modified,b.manifest.lastOpened);});return out;
}
Experience ExperienceManager::inspect(const fs::path& root) const {
    auto absolute=fs::absolute(root);if(fs::is_symlink(fs::symlink_status(absolute)))throw std::runtime_error("Cannot open a symlink experience root");
    return {absolute,Manifest::parse(read(absolute/"experience.json",MiB))};
}
std::vector<Template> ExperienceManager::templates() const {
    std::vector<Template> result;
    const auto config=settings(data_);if(config.contains("defaultTemplate")) {
        const auto root=fs::path(config.at("defaultTemplate").get<std::string>());
        if(fs::exists(root/"experience.json"))result.push_back({"my-default","My Default","Your saved world and character",root});
    }
    bool engineWorld=!factoryDefaults_.empty()&&fs::exists(factoryDefaults_/"factory/world.loblox.json");
    engineWorld=engineWorld||fs::exists(defaults_/"factory/world.loblox.json");
    for(const auto& root:{defaults_,factoryDefaults_}) {
        if(root.empty()||!fs::exists(root/"index.json"))continue;
        const auto index=boundedJson(read(root/"index.json",MiB));
        if(index.value("format","")!="linblux-engine-defaults"||index.value("version",0)!=1)throw std::runtime_error("Versão dos padrões da engine não suportada");
        engineWorld=engineWorld||index.contains("world");
    }
    if(engineWorld)result.push_back({"engine-default","Mundo padrão LINBLUX","Cópia independente do mundo e personagem atuais",defaults_});
    const char* ids[]={"empty","baseplate","classic","obby"};
    const char* names[]={"Empty","Empty Baseplate","Classic Baseplate","Obby Starter"};
    const char* descriptions[]={"An empty world; no invisible floor","Baseplate and SpawnLocation","Baseplate, spawn and an editable R6","Five platforms and a finish platform"};
    for(int i=0;i<4;++i)if(fs::exists(factory_/(std::string(ids[i])+".loblox.json")))result.push_back({ids[i],names[i],descriptions[i],factory_/(std::string(ids[i])+".loblox.json")});
    return result;
}
Experience ExperienceManager::snapshot(const Scene& scene,const fs::path& sourceRoot,const fs::path& parent,Manifest manifest,bool registerExperience,const CreateOptions* creation) {
    Stage stage(parent);createDirectory(stage.path/"places","o diretório de cenas da experiência");
    manifest.mainPlace="places/"+newId()+".loblox.json";manifest.modified=now();
    if(manifest.faceImage.empty()&&fs::is_regular_file(sourceRoot/"experience.json"))manifest.faceImage=inspect(sourceRoot).manifest.faceImage;
    if(!manifest.faceImage.empty())manifest.faceImage=imageImport(stage.path,containedFile(sourceRoot,manifest.faceImage));
    else if(const auto face=game::BuiltinAssetPath("rosto1.png");!face.empty())manifest.faceImage=imageImport(stage.path,face);
    if(fs::is_regular_file(sourceRoot/"experience.json")&&fs::is_directory(sourceRoot/"assets"))
        for(const auto& file:fs::directory_iterator(containedFile(sourceRoot,"assets"))) {
            if(file.is_symlink())throw std::runtime_error("Symlink asset in snapshot");
            if(file.is_regular_file())imageImport(stage.path,file.path());
        }
    for(auto* reference:{&manifest.icon,&manifest.banner,&manifest.loadingBackground})if(!reference->empty())*reference=imageImport(stage.path,containedFile(sourceRoot,*reference));
    Scene merged;
    const Scene* world=&scene;
    if(creation) {
        auto config=settings(data_);
        Scene rigScene;fs::path characterRoot;std::string characterFace;
        if(creation->useDefaultCharacter) {
            if(config.contains("defaultCharacter")) {
                auto character=inspect(config.at("defaultCharacter").get<std::string>());
                loadWorld(rigScene,character);characterRoot=character.root;characterFace=character.manifest.faceImage;
            } else characterRoot=loadEngineResource("character",rigScene);
        }
        if(!characterRoot.empty()) {
            Scene relocated;std::string error;
            if(!projectio::fromJson(relocated,relocateWorld(rigScene,characterRoot,stage.path),error)||!projectio::fromJson(merged,projectio::toJson(scene),error))throw std::runtime_error(error);
            auto& a=merged.gameAssets();const auto& b=relocated.gameAssets();
            if(a.defaultRig!=b.defaultRig&&!a.defaultRig.empty())a.removeRig(a.defaultRig);
            for(const auto& rig:b.rigs){auto* old=a.findRig(rig.id);if(old)*old=rig;else a.rigs.push_back(rig);}
            for(const auto& clip:b.animations){auto* old=a.findAnimation(clip.id);if(old)*old=clip;else a.animations.push_back(clip);}
            a.defaultRig=b.defaultRig;a.player=b.player;world=&merged;
            if(!characterFace.empty())manifest.faceImage=imageImport(stage.path,containedFile(characterRoot,characterFace));
        }
        if(!creation->icon.empty())manifest.icon=imageImport(stage.path,creation->icon);
        if(!creation->banner.empty())manifest.banner=imageImport(stage.path,creation->banner);
        if(!creation->loadingBackground.empty())manifest.loadingBackground=imageImport(stage.path,creation->loadingBackground);
    }
    write(stage.path/manifest.mainPlace,relocateWorld(*world,sourceRoot,stage.path));
    write(stage.path/"experience.json",manifest.serialize());Scene probe;loadWorld(probe,{stage.path,manifest});
    const auto root=stage.publish(manifest.installationId);
    try{if(registerExperience)registerRoot(root);}catch(...){std::error_code ec;fs::remove_all(root,ec);throw;}
    event("create",root);return {root,manifest};
}
Experience ExperienceManager::create(const CreateOptions& options) {
    Scene scene;fs::path source;
    if(options.templateId=="engine-default") {
        source=loadEngineResource("world",scene);
        if(source.empty()) {std::string error;if(!projectio::load(scene,(factory_/"baseplate.loblox.json").string(),error))throw std::runtime_error(error);source=factory_;}
    } else {
        const auto choices=templates();const auto selected=std::find_if(choices.begin(),choices.end(),[&](const Template& t){return t.id==options.templateId;});
        if(selected==choices.end())throw std::runtime_error("Modelo inicial indisponível");
        if(selected->id=="my-default") {auto e=inspect(selected->place);loadWorld(scene,e);source=e.root;}
        else {std::string error;if(!projectio::load(scene,selected->place.string(),error))throw std::runtime_error(error);source=selected->place.parent_path();}
    }
    Manifest m;m.id=newId();m.installationId=newId();m.name=options.name;m.description=options.description;m.created=m.modified=now();m.sourceTemplate=options.templateId;
    return snapshot(scene,source,library_,m,true,&options);
}
Experience ExperienceManager::importLegacy(const fs::path& file,const std::string& name) {
    Scene scene;std::string error;if(!projectio::load(scene,file.string(),error))throw std::runtime_error(error);
    Manifest m;m.id=newId();m.installationId=newId();m.name=name;m.created=m.modified=now();m.sourceTemplate="legacy-import";
    return snapshot(scene,fs::absolute(file).parent_path(),library_,m,true);
}
Experience ExperienceManager::duplicate(const Experience& source,const std::string& name) {
    const auto current=inspect(source.root);Scene scene;loadWorld(scene,current);auto manifest=current.manifest;
    manifest.id=newId();manifest.installationId=newId();manifest.name=name;manifest.created=now();manifest.lastOpened.clear();manifest.archived=manifest.trashed=false;
    return snapshot(scene,current.root,library_,manifest,true);
}
void ExperienceManager::loadReadOnly(const Experience& experience,Scene& destination) const {
    const auto current=inspect(experience.root);loadWorld(destination,current);
    game::SetProjectAssetRoot(current.root.string());game::SetProjectFaceImage(current.manifest.faceImage);
}
void ExperienceManager::open(const Experience& experience,Scene& destination) {
    auto e=inspect(experience.root);Scene probe;loadWorld(probe,e);
    std::string error;const auto text=projectio::toJson(probe);
    e.manifest.lastOpened=now();write(e.root/"experience.json",e.manifest.serialize());registerRoot(e.root);
    if(!projectio::fromJson(destination,text,error))throw std::runtime_error(error);
    active_=e;game::SetProjectAssetRoot(e.root.string());game::SetProjectFaceImage(e.manifest.faceImage);event("open",e.root);
}
fs::path ExperienceManager::activePlace() const {return active_?active_->root/active_->manifest.mainPlace:fs::path{};}
void ExperienceManager::save(const Scene& scene) {
    if(!active_)throw std::runtime_error("No active experience");
    const auto current=inspect(active_->root);
    if(current.manifest.mainPlace!=active_->manifest.mainPlace||current.manifest.installationId!=active_->manifest.installationId)throw std::runtime_error("Experience changed outside LINBLUX; reopen before saving");
    auto next=current.manifest;next.mainPlace="places/"+newId()+".loblox.json";next.modified=now();
    const auto text=relocateWorld(scene,active_->root,active_->root);
    Scene validated;std::string error;if(!projectio::fromJson(validated,text,error))throw std::runtime_error("Cannot save invalid place: "+error);
    // An immutable revision is durable before the atomic manifest pointer swap.
    // A failure leaves the previously committed revision available to readers.
    write(containedFile(active_->root,next.mainPlace),text);write(containedFile(active_->root,"experience.json"),next.serialize());active_->manifest=next;
    // Keep the active revision and two previous revisions; never prune assets.
    try {
        std::vector<fs::directory_entry> revisions;
        for(const auto& file:fs::directory_iterator(active_->root/"places"))if(file.is_regular_file()&&!file.is_symlink())revisions.push_back(file);
        std::sort(revisions.begin(),revisions.end(),[](const auto& a,const auto& b){return a.last_write_time()>b.last_write_time();});
        int retained=0;for(const auto& file:revisions)if(file.path()!=activePlace()&&++retained>2){std::error_code ec;fs::remove(file.path(),ec);}
    } catch(const std::exception& e){warnings_.push_back(std::string("Saved successfully; old revision cleanup failed: ")+e.what());}
    try{registerRoot(active_->root);}catch(const std::exception& e){warnings_.push_back(std::string("Saved successfully; recent index update failed: ")+e.what());}
    event("save",active_->root);
}
void ExperienceManager::updateMetadata(Manifest manifest) {
    if(!active_)throw std::runtime_error("Open an experience first");
    updateMetadata(*active_,std::move(manifest));
}
void ExperienceManager::updateMetadata(const Experience& experience,Manifest manifest) {
    const auto current=inspect(experience.root);
    if(manifest.id!=current.manifest.id||manifest.installationId!=current.manifest.installationId||manifest.mainPlace!=current.manifest.mainPlace)
        throw std::runtime_error("Experience identity/place cannot be edited as metadata");
    manifest.modified=now();manifest.validate();
    for(const auto& ref:{manifest.icon,manifest.banner,manifest.loadingBackground,manifest.faceImage})if(!ref.empty()) {
        ImageInfo info;if(!ReadSafeImageInfo(containedFile(current.root,ref).string(),info))throw std::runtime_error("Invalid/missing presentation image: "+ref);
    }
    write(containedFile(current.root,"experience.json"),manifest.serialize());
    if(active_&&active_->root==current.root)active_->manifest=manifest;
    try{registerRoot(current.root);}catch(const std::exception& e){warnings_.push_back(std::string("Metadata saved; recent index update failed: ")+e.what());}
}
void ExperienceManager::setArchived(const Experience& experience,bool archived,bool trash) {
    auto e=inspect(experience.root);
    if(isEngineDefault(e)||inside(e.root,defaults_))throw std::runtime_error("O Mundo Padrão da Engine não pode ser arquivado");
    const auto entries=list(false),archivedEntries=list(true);
    const auto matches=[&](const Experience& item){return item.root==e.root&&item.manifest.installationId==e.manifest.installationId;};
    if(std::none_of(entries.begin(),entries.end(),matches)&&std::none_of(archivedEntries.begin(),archivedEntries.end(),matches))throw std::runtime_error("Only library experiences can be archived");
    e.manifest.archived=archived;e.manifest.trashed=trash;e.manifest.modified=now();write(e.root/"experience.json",e.manifest.serialize());
    if(active_&&active_->root==e.root)active_->manifest=e.manifest;
    event(trash?"trash":archived?"archive":"restore",e.root);
}
namespace {
struct DirectoryHandle {
    int fd;
    explicit DirectoryHandle(int value):fd(value) {if(fd<0)throw std::runtime_error("Não foi possível abrir o diretório com segurança");}
    ~DirectoryHandle(){::close(fd);}
};
void removeContents(int fd) {
    DIR* raw=fdopendir(dup(fd));if(!raw)throw std::runtime_error("Falha ao listar arquivos para exclusão");
    struct CloseDirectory {void operator()(DIR* value) const {closedir(value);}};
    std::unique_ptr<DIR,CloseDirectory> directory(raw);
    errno=0;
    while(auto* item=readdir(directory.get())) {
        const std::string name=item->d_name;if(name=="."||name=="..")continue;
        struct stat st{};
        if(fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)!=0)throw std::runtime_error("Falha ao verificar arquivo para exclusão");
        if(S_ISDIR(st.st_mode)) {
            DirectoryHandle child(openat(fd,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
            removeContents(child.fd);
            if(unlinkat(fd,name.c_str(),AT_REMOVEDIR)!=0)throw std::runtime_error("Falha ao excluir subdiretório");
        } else if(unlinkat(fd,name.c_str(),0)!=0)throw std::runtime_error("Falha ao excluir arquivo");
        errno=0;
    }
    if(errno)throw std::runtime_error("Falha durante a leitura do diretório");
}
}
void ExperienceManager::deletePermanently(const Experience& experience) {
    if(isEngineDefault(experience)||inside(experience.root,defaults_))throw std::runtime_error("O Mundo Padrão da Engine é protegido contra exclusão");
    const auto library=fs::canonical(library_);
    const auto root=fs::absolute(experience.root).lexically_normal();
    const auto id=experience.manifest.installationId;
    if(!identifier(id)||root!=library/id||root==library||fs::is_symlink(fs::symlink_status(root)))
        throw std::runtime_error("Exclusão recusada: a experiência deve ser um diretório próprio dentro da biblioteca");
    DirectoryHandle parent(::open(library.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    DirectoryHandle target(openat(parent.fd,id.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    DirectoryHandle manifestFile(openat(target.fd,"experience.json",O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    struct stat fileStat{},original{};
    if(fstat(manifestFile.fd,&fileStat)!=0||!S_ISREG(fileStat.st_mode)||fileStat.st_size<=0||fileStat.st_size>int64_t(MiB)||fstat(target.fd,&original)!=0)
        throw std::runtime_error("Manifesto inválido para exclusão");
    std::string text(size_t(fileStat.st_size),'\0');size_t offset=0;
    while(offset<text.size()) {const auto count=::read(manifestFile.fd,text.data()+offset,text.size()-offset);if(count<=0)throw std::runtime_error("Falha ao ler manifesto");offset+=size_t(count);}
    const auto current=Manifest::parse(text);
    if(current.id!=experience.manifest.id||current.installationId!=id)
        throw std::runtime_error("A identidade da experiência mudou; atualize a biblioteca");
    const auto quarantine=".deleting-"+id+"-"+newId();
    if(renameat(parent.fd,id.c_str(),parent.fd,quarantine.c_str())!=0)throw std::runtime_error("Não foi possível reservar a experiência para exclusão");
    struct stat moved{};
    if(fstatat(parent.fd,quarantine.c_str(),&moved,AT_SYMLINK_NOFOLLOW)!=0||moved.st_dev!=original.st_dev||moved.st_ino!=original.st_ino) {
        renameat(parent.fd,quarantine.c_str(),parent.fd,id.c_str());
        throw std::runtime_error("O diretório mudou durante a exclusão; operação interrompida");
    }
    try {removeContents(target.fd);if(unlinkat(parent.fd,quarantine.c_str(),AT_REMOVEDIR)!=0)throw std::runtime_error("Falha ao excluir diretório da experiência");}
    catch(const std::exception& e) {throw std::runtime_error(std::string("Exclusão incompleta; arquivos restantes em ")+quarantine+": "+e.what());}
    if(active_&&active_->root==root)close();
    try {
        auto config=settings(data_);auto& roots=config["roots"];
        if(roots.is_array())roots.erase(std::remove(roots.begin(),roots.end(),json(root.string())),roots.end());
        if(config.contains("catalog"))config["catalog"].erase(id);
        saveSettings(data_,config);
    } catch(const std::exception& e) {throw std::runtime_error(std::string("Arquivos excluídos, mas falhou a atualização da biblioteca: ")+e.what());}
    event("delete_permanently",root);
}
void ExperienceManager::close(){active_.reset();game::SetProjectAssetRoot({});}
std::string ExperienceManager::importImage(const fs::path& file) {if(!active_)throw std::runtime_error("Open an experience first");return imageImport(active_->root,file);}
std::string ExperienceManager::importImage(const Experience& e,const fs::path& file) {return imageImport(inspect(e.root).root,file);}
void ExperienceManager::setDefaultTemplate(const Scene& scene) {
    Manifest m;m.id=newId();m.installationId=newId();m.name="My Default";m.created=m.modified=now();m.sourceTemplate="user-snapshot";
    auto e=snapshot(scene,active_?active_->root:fs::path{},data_/"templates",m,false);
    auto config=settings(data_);config["defaultTemplate"]=e.root.string();saveSettings(data_,config);event("default_template",e.root);
}
void ExperienceManager::setDefaultCharacter(const Scene& source,const std::string& rigId) {
    const auto* rig=source.gameAssets().findRig(rigId);if(!rig)throw std::runtime_error("Select an authored R6 first");
    Scene scene;scene.gameAssets()=source.gameAssets();scene.gameAssets().rigs={*rig};scene.gameAssets().defaultRig=rigId;
    std::string error;if(!scene.gameAssets().rigs.front().recenterAuthoringRoot(error))throw std::runtime_error(error);
    Manifest m;m.id=newId();m.installationId=newId();m.name="My Default Character";m.created=m.modified=now();m.sourceTemplate="user-r6";
    const auto e=snapshot(scene,active_?active_->root:fs::path{},data_/"templates",m,false);
    auto config=settings(data_);config["defaultCharacter"]=e.root.string();saveSettings(data_,config);event("default_character",e.root);
}
fs::path ExperienceManager::loadEngineResource(const std::string& key,Scene& scene) const {
    fs::path loaded;
    const auto loadIndex=[&](const fs::path& root,const char* label)->bool {
        const auto indexFile=root/"index.json";
        if(root.empty()||!fs::exists(indexFile))return false;
        const auto index=boundedJson(read(indexFile,MiB));
        if(index.value("format","")!="linblux-engine-defaults"||index.value("version",0)!=1)
            throw std::runtime_error("Versão dos padrões da engine não suportada");
        if(!index.contains(key))return false;
        if(!index.at(key).is_string())throw std::runtime_error(std::string(label)+" inválido: referência de "+key+" não é texto");
        try {
            const auto rootFile=containedFile(root,index.at(key).get<std::string>());
            const auto experience=inspect(rootFile);
            loadWorld(scene,experience);
            loaded=rootFile;
            return true;
        } catch(const std::exception& error) {
            throw std::runtime_error(std::string(label)+" inválido em '"+indexFile.string()+"': "+error.what()+" O arquivo anterior foi preservado.");
        }
    };
    if(loadIndex(defaults_,"Padrão personalizado"))return loaded;
    if(factoryDefaults_!=defaults_&&loadIndex(factoryDefaults_,"Padrão de fábrica"))return loaded;
    for(const auto& root:{defaults_,factoryDefaults_}) {
        if(root.empty())continue;
        const auto file=root/"factory"/(key+".loblox.json");
        if(!fs::is_regular_file(file))continue;
        std::string error;
        if(!projectio::load(scene,file.string(),error))throw std::runtime_error("Padrão de fábrica inválido: "+error);
        return file.parent_path();
    }
    return {};
}
void ExperienceManager::saveEngineResource(const std::string& key,const Scene& scene) {
    ensureDefaultsWritable();
    auto index=json{{"format","linblux-engine-defaults"},{"version",1}};
    if(fs::exists(defaults_/"index.json")) {
        index=boundedJson(read(defaults_/"index.json",MiB));
        if(index.value("format","")!="linblux-engine-defaults"||index.value("version",0)!=1)throw std::runtime_error("Versão dos padrões não suportada; arquivo preservado");
    }
    Manifest m;m.id=newId();m.installationId=newId();m.name=key=="world"?"Mundo padrão LINBLUX":"Personagem padrão LINBLUX";m.created=m.modified=now();m.sourceTemplate="linblux-engine-default";
    const auto e=snapshot(scene,active_?active_->root:fs::path{},defaults_/"revisions",m,false);
    index[key]=e.root.lexically_relative(defaults_).generic_string();
    write(defaults_/"index.json",index.dump(2));
    auto config=settings(data_);config.erase(key=="world"?"defaultTemplate":"defaultCharacter");saveSettings(data_,config);
    event("engine_default_saved",defaults_);
}
void ExperienceManager::backupDefaultsIndex(const std::string& index) {
    const auto path=defaults_/"backups"/(newId()+"-index.json");
    write(path,index);lastDefaultsBackup_=path;
}
void ExperienceManager::saveEngineDefaults(const Scene& source,const std::string& rigId) {
    if(!source.gameAssets().findRig(rigId))throw std::runtime_error("Defina um personagem de jogador antes de salvar o padrão");
    ensureDefaultsWritable();
    auto index=json{{"format","linblux-engine-defaults"},{"version",1}};
    if(fs::exists(defaults_/"index.json"))index=boundedJson(read(defaults_/"index.json",MiB));
    if(index.value("format","")!="linblux-engine-defaults"||index.value("version",0)!=1)throw std::runtime_error("Versão dos padrões não suportada; recurso preservado");
    Scene world,character;std::string error;
    if(!projectio::fromJson(world,projectio::toJson(source),error))throw std::runtime_error(error);
    world.gameAssets().defaultRig=rigId;
    character.gameAssets()=world.gameAssets();character.gameAssets().rigs={*world.gameAssets().findRig(rigId)};
    const auto make=[&](const Scene& scene,const char* name) {
        Manifest m;m.id=newId();m.installationId=newId();m.name=name;m.created=m.modified=now();m.sourceTemplate="linblux-engine-default";
        return snapshot(scene,active_?active_->root:fs::path{},defaults_/"revisions",m,false);
    };
    const auto worldSnapshot=make(world,"Mundo Padrão da Engine");
    const auto characterSnapshot=make(character,"Personagem Padrão da Engine");
    backupDefaultsIndex(index.dump(2));
    index["world"]=worldSnapshot.root.lexically_relative(defaults_).generic_string();
    index["character"]=characterSnapshot.root.lexically_relative(defaults_).generic_string();
    // Publish both resources together. A failed snapshot never publishes half a pair.
    write(defaults_/"index.json",index.dump(2));
    auto config=settings(data_);config.erase("defaultTemplate");config.erase("defaultCharacter");saveSettings(data_,config);
    event("engine_defaults_saved",defaults_);
}
void ExperienceManager::saveEngineWorld(const Scene& scene) {saveEngineResource("world",scene);}
std::string ExperienceManager::importCharacter(const fs::path& source,Scene& destination) {
    if(!active_)throw std::runtime_error("Abra o Mundo Padrão ou uma experiência antes de importar o personagem");
    Scene imported;fs::path sourceRoot;std::string face;
    if(source.extension()==".lo") {
        ExperienceManager temporary(data_/"character-imports",factory_,defaults_);
        const auto package=temporary.importPackage(source);loadWorld(imported,package);sourceRoot=package.root;face=package.manifest.faceImage;
    } else if(fs::is_directory(source)||source.filename()=="experience.json") {
        const auto package=inspect(source.filename()=="experience.json"?source.parent_path():source);
        loadWorld(imported,package);sourceRoot=package.root;face=package.manifest.faceImage;
    } else {
        std::string error;if(!projectio::load(imported,source.string(),error))throw std::runtime_error(error);
        sourceRoot=fs::absolute(source).parent_path();
    }
    const auto sourceId=imported.gameAssets().defaultRig;
    if(!imported.gameAssets().findRig(sourceId))throw std::runtime_error("O arquivo não contém um personagem configurado");
    Scene relocated;std::string error;
    if(!projectio::fromJson(relocated,relocateWorld(imported,sourceRoot,active_->root),error))throw std::runtime_error(error);
    auto next=destination.gameAssets();auto rig=*relocated.gameAssets().findRig(sourceId);
    rig.id="linblux-import-"+newId();next.rigs.push_back(rig);next.defaultRig=rig.id;
    std::map<std::string,std::string> animationIds;
    for(auto clip:relocated.gameAssets().animations) {
        const auto previous=clip.id;clip.id="linblux-import-"+newId();animationIds[previous]=clip.id;next.animations.push_back(std::move(clip));
    }
    next.player=relocated.gameAssets().player;
    for(auto* reference:{&next.player.walkAnimation,&next.player.idleAnimation,&next.player.jumpAnimation,&next.player.fallAnimation}) {
        const auto key=reference->rfind("anim://",0)==0?reference->substr(7):*reference;
        if(auto it=animationIds.find(key);it!=animationIds.end())*reference=it->second;
    }
    if(!next.validate(error))throw std::runtime_error(error);
    if(!face.empty()) {
        auto manifest=active_->manifest;manifest.faceImage=imageImport(active_->root,containedFile(sourceRoot,face));updateMetadata(manifest);
        game::SetProjectFaceImage(manifest.faceImage);
    }
    destination.gameAssets()=std::move(next);
    game::SetProjectAssetRoot(active_->root.string());event("character_import",active_->root);return rig.id;
}
void ExperienceManager::saveEngineCharacter(const Scene& source,const std::string& rigId) {
    const auto* rig=source.gameAssets().findRig(rigId);if(!rig)throw std::runtime_error("Selecione um personagem no Explorador");
    Scene scene;scene.gameAssets()=source.gameAssets();scene.gameAssets().rigs={*rig};scene.gameAssets().defaultRig=rigId;
    std::string error;if(!scene.gameAssets().rigs.front().recenterAuthoringRoot(error))throw std::runtime_error(error);
    saveEngineResource("character",scene);
}
Experience ExperienceManager::editEngineDefault(bool character) {
    Scene scene;const auto root=loadEngineResource("world",scene);
    if(root.empty())throw std::runtime_error("Recurso padrão da engine não encontrado");
    Manifest m;m.id=newId();m.installationId=newId();m.name=character?"Editar personagem padrão":"Editar mundo padrão";m.created=m.modified=now();m.sourceTemplate="engine-default-edit";
    CreateOptions options;options.useDefaultCharacter=true;
    return snapshot(scene,root,data_/"engine-workspaces",m,false,&options);
}
bool ExperienceManager::isEngineDefault(const Experience& experience) const {
    return experience.manifest.sourceTemplate=="engine-default-edit"&&
        fs::absolute(experience.root).lexically_normal().parent_path()==fs::absolute(data_/"engine-workspaces").lexically_normal();
}
void ExperienceManager::restoreFactoryDefaults() {
    ensureDefaultsWritable();
    auto index=json{{"format","linblux-engine-defaults"},{"version",1}};
    if(fs::exists(defaults_/"index.json"))index=boundedJson(read(defaults_/"index.json",MiB));
    if(index.value("format","")!="linblux-engine-defaults"||index.value("version",0)!=1)throw std::runtime_error("Versão dos padrões não suportada; restauração cancelada");
    backupDefaultsIndex(index.dump(2));
    index.erase("world");index.erase("character");write(defaults_/"index.json",index.dump(2));
    auto config=settings(data_);config.erase("defaultTemplate");config.erase("defaultCharacter");saveSettings(data_,config);
}

fs::path ExperienceManager::thumbnail(const Experience& experience) const {
    const auto& ref=experience.manifest.banner.empty()?experience.manifest.icon:experience.manifest.banner;if(ref.empty())return {};
    const auto source=containedFile(experience.root,ref);ImageInfo info;if(!ReadSafeImageInfo(source.string(),info))return {};
    const auto cache=containedFile(experience.root,".cache");fs::create_directories(cache);
    const auto output=cache/(source.stem().string()+"-cover-v1-thumb.png");
    if(fs::is_symlink(fs::symlink_status(output)))throw std::runtime_error("Symlink thumbnail is not supported");
    if(fs::is_regular_file(output))return output;
    Image image=game::LoadProjectImage(source.string());if(!image.data)return {};
    // Crop before resize, so a 1x16000 source never becomes a huge temporary
    // image just to cover a landscape card. The original asset stays intact.
    int cropWidth=image.width,cropHeight=image.height;
    if(double(cropWidth)/cropHeight>16.0/9.0)cropWidth=std::max(1,int(cropHeight*16.0/9.0));
    else cropHeight=std::max(1,int(cropWidth*9.0/16.0));
    ImageCrop(&image,{float((image.width-cropWidth)/2),float((image.height-cropHeight)/2),float(cropWidth),float(cropHeight)});
    ImageResize(&image,384,216);
    const bool ok=ExportImage(image,output.c_str());UnloadImage(image);return ok?output:fs::path{};
}

void ExperienceManager::exportPackage(const Experience& experience,const fs::path& destination) const {
    const auto e=inspect(experience.root);Scene probe;loadWorld(probe,e);
    if(destination.extension()!=".lo")throw std::runtime_error("Choose a .lo filename");
    std::vector<std::string> paths={"experience.json",e.manifest.mainPlace};
    if(fs::is_directory(e.root/"assets"))for(const auto& entry:fs::recursive_directory_iterator(containedFile(e.root,"assets"))) {
        if(entry.is_symlink())throw std::runtime_error("Cannot export symlink assets");
        if(entry.is_regular_file())paths.push_back(fs::relative(entry.path(),e.root).generic_string());
    }
    if(paths.size()>maxEntries)throw std::runtime_error("Too many files in experience");
    uint64_t total=0,decodedTotal=0;for(const auto& path:paths){const auto file=containedFile(e.root,path);const auto size=fs::file_size(file);if(size>maxFile||total+size>maxTotal)throw std::runtime_error("Package exceeds 256 MiB budget");total+=size;
        if(path.rfind("assets/",0)==0){ImageInfo info;if(!ReadSafeImageInfo(file.string(),info))throw std::runtime_error("Invalid asset in export: "+path);decodedTotal+=uint64_t(info.width)*info.height*4;if(decodedTotal>maxTotal)throw std::runtime_error("Image decoded budget exceeds 256 MiB");}
    }
    const auto target=fs::absolute(destination);fs::create_directories(target.parent_path());
    const auto temporary=target.parent_path()/(".lo-export-"+newId());
    const int fd=::open(temporary.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);if(fd<0)throw std::runtime_error("Cannot create export file");
    FILE* out=fdopen(fd,"wb");if(!out){::close(fd);fs::remove(temporary);throw std::runtime_error("Cannot open export stream");}
    try {
        if(std::fwrite(magic,1,8,out)!=8)throw std::runtime_error("Cannot write package header");
        writeNumber(out,paths.size(),4);
        std::array<char,65536> buffer{};
        for(const auto& path:paths) {
            const auto source=containedFile(e.root,path);const auto size=fs::file_size(source);uLong crc=crc32(0,nullptr,0);
            std::ifstream input(source,std::ios::binary);uint64_t consumed=0;
            while(input){input.read(buffer.data(),buffer.size());const auto n=input.gcount();crc=crc32(crc,reinterpret_cast<const Bytef*>(buffer.data()),unsigned(n));consumed+=n;}
            if(consumed!=size)throw std::runtime_error("Asset changed while exporting");
            writeNumber(out,path.size(),2);writeNumber(out,size,8);writeNumber(out,crc,4);
            if(std::fwrite(path.data(),1,path.size(),out)!=path.size())throw std::runtime_error("Export write failed");
            input.clear();input.seekg(0);consumed=0;uLong copiedCrc=crc32(0,nullptr,0);
            while(input){input.read(buffer.data(),buffer.size());const auto n=input.gcount();if(n>0&&std::fwrite(buffer.data(),1,size_t(n),out)!=size_t(n))throw std::runtime_error("Export write failed");consumed+=n;copiedCrc=crc32(copiedCrc,reinterpret_cast<const Bytef*>(buffer.data()),unsigned(n));}
            if(consumed!=size||copiedCrc!=crc)throw std::runtime_error("Asset changed while exporting");
        }
        if(std::fflush(out)!=0||::fsync(fileno(out))!=0)throw std::runtime_error("Cannot flush package");
        if(std::fclose(out)!=0){out=nullptr;throw std::runtime_error("Cannot close package");}out=nullptr;
        fs::rename(temporary,target);event("export",target);
    }catch(...){if(out)std::fclose(out);std::error_code error;fs::remove(temporary,error);throw;}
}
Experience ExperienceManager::importPackage(const fs::path& package) {
    if(fs::file_size(package)>maxTotal+2*MiB)throw std::runtime_error("Package exceeds 256 MiB budget");
    std::ifstream in(package,std::ios::binary);char signature[8]{};readBytes(in,signature,8);
    if(!std::equal(std::begin(magic),std::end(magic),signature))throw std::runtime_error("Unsupported .lo format/version (ZIP is not LOEXP v1)");
    const auto count=readNumber(in,4);if(count==0||count>maxEntries)throw std::runtime_error("Invalid package file count");
    Stage stage(library_);std::set<std::string> names;uint64_t total=0;std::array<char,65536> buffer{};
    for(uint64_t i=0;i<count;++i) {
        const auto nameLength=readNumber(in,2),size=readNumber(in,8),expectedCrc=readNumber(in,4);
        if(nameLength==0||nameLength>512||size>maxFile||total+size>maxTotal)throw std::runtime_error("Package allocation budget exceeded");
        total+=size;
        std::string name(size_t(nameLength),'\0');readBytes(in,name.data(),name.size());
        if(!packagePath(name)||!names.insert(name).second)throw std::runtime_error("Unsafe or duplicate package path: "+name);
        const auto target=containedFile(stage.path,name);fs::create_directories(target.parent_path());
        std::ofstream output(target,std::ios::binary);if(!output)throw std::runtime_error("Cannot extract package file");
        uint64_t remaining=size;uLong crc=crc32(0,nullptr,0);
        while(remaining){const auto n=size_t(std::min<uint64_t>(remaining,buffer.size()));readBytes(in,buffer.data(),n);output.write(buffer.data(),std::streamsize(n));if(!output)throw std::runtime_error("Cannot write extracted file");crc=crc32(crc,reinterpret_cast<const Bytef*>(buffer.data()),unsigned(n));remaining-=n;}
        if(crc!=expectedCrc)throw std::runtime_error("Package checksum mismatch: "+name);
    }
    if(in.peek()!=EOF)throw std::runtime_error("Trailing data in .lo package");
    auto e=inspect(stage.path);Scene probe;loadWorld(probe,e);
    uint64_t decodedBudget=0;
    for(const auto& name:names)if(name.rfind("assets/",0)==0) {
        ImageInfo info;const auto file=stage.path/name;
        if(!ReadSafeImageInfo(file.string(),info))throw std::runtime_error("Invalid packaged image: "+name);
        decodedBudget+=uint64_t(info.width)*info.height*4;
        if(decodedBudget>maxTotal)throw std::runtime_error("Packaged images exceed decoded memory budget (256 MiB)");
        Image decoded=game::LoadProjectImage(file.string());const bool valid=decoded.data;
        if(decoded.data)UnloadImage(decoded);
        if(!valid)throw std::runtime_error("Image decoding failed: "+name);
    }
    e.manifest.installationId=newId();e.manifest.archived=e.manifest.trashed=false;e.manifest.lastOpened.clear();
    write(stage.path/"experience.json",e.manifest.serialize());e.root=stage.publish(e.manifest.installationId);
    try{registerRoot(e.root);}catch(...){std::error_code ec;fs::remove_all(e.root,ec);throw;}
    event("import",e.root);return e;
}
void ExperienceManager::writeFactoryResources(const fs::path& directory) {
    fs::create_directories(directory);
    for(const std::string type:{"empty","baseplate","classic","obby"}) {
        Scene scene;if(type!="empty")scene.buildDefaultScene();scene.installDefaultScripts();
        if(type!="classic"){scene.gameAssets().rigs.clear();scene.gameAssets().defaultRig.clear();}
        if(type=="obby") {
            auto* base=scene.findPart(3);if(base&&base->instanceClass()==InstanceClass::Baseplate)scene.remove(base);
            for(int i=0;i<6;++i){auto* p=scene.createPart(PartShape::Block,{float(i*9),float(i)*1.1f,0},{6,1,6},i==5?"Finish":"Platform"+std::to_string(i+1));p->setAnchored(true);p->setColor(i==5?Color{80,210,135,255}:Color{90,155,220,255});}
        }
        write(directory/(type+".loblox.json"),projectio::toJson(scene));
        fs::permissions(directory/(type+".loblox.json"),fs::perms::owner_read|fs::perms::owner_write|fs::perms::group_read|fs::perms::others_read);
    }
}
} // namespace loblox::experiences

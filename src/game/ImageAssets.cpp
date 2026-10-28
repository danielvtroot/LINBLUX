#include "game/ImageAssets.h"
#include "core/ImageInfo.h"
#include "game/Rig.h"
#include "objects/Part.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"
#include "nlohmann/json.hpp"
#include "experiences/ExperienceManager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <unistd.h>

// The anisotropic filter probe needs the GL entry points. The header is
// optional: without it the surfaces simply stay trilinear.
#if defined(__has_include)
#  if __has_include(<GL/gl.h>)
#    include <GL/gl.h>
#    define LINBLUX_GL_HEADER 1
#  endif
#endif

namespace loblox::game {
namespace {
std::unordered_map<std::string, Texture2D> textureCache;
std::filesystem::path projectRoot;
std::string projectFace;
std::string Fnv1aHex(const std::string& data) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char byte : data) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    char buffer[17];
    std::snprintf(buffer, sizeof(buffer), "%016llx",
                  static_cast<unsigned long long>(hash));
    return std::string(buffer);
}

std::string LowerExt(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    return ext;
}

// GL_EXT_texture_filter_anisotropic is optional. Asking for
// GL_MAX_ANISOTROPY_EXT on a driver without it is a GL error, so the
// extension is probed once before any texture asks for anisotropic filtering.
std::filesystem::path NamesPath(const std::string& assetsDir) {
    return std::filesystem::path(assetsDir) / "image-names.json";
}

std::map<std::string, std::string> ReadImageNames(const std::string& assetsDir) {
    std::map<std::string, std::string> names;
    std::error_code ec;const auto file=NamesPath(assetsDir);
    if(std::filesystem::is_symlink(std::filesystem::symlink_status(file,ec))||!std::filesystem::is_regular_file(file,ec)||std::filesystem::file_size(file,ec)>512*1024||ec)return names;
    std::ifstream input(file, std::ios::binary);
    if (!input) return names;
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    if (text.empty() || text.size() > 512 * 1024) return names;
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object() || json.value("version", 0) != 1) return names;
        const auto it = json.find("names");
        if (it == json.end() || !it->is_object()) return names;
        for (auto entry = it->begin(); entry != it->end(); ++entry) {
            if (!entry.value().is_string()) continue;
            const auto name = entry.value().get<std::string>();
            if (!name.empty() && name.size() <= 80) names[entry.key()] = name;
        }
    } catch (const std::exception&) {
        // Arquivo de nomes corrompido não impede abrir o projeto; ele é
        // simplesmente regenerado no próximo rename.
    }
    return names;
}

bool WriteImageNames(const std::string& assetsDir, const std::map<std::string, std::string>& names,
                     std::string& error) {
    nlohmann::json json;
    json["version"] = 1;
    json["names"] = names;
    std::error_code fsError;
    std::filesystem::create_directories(assetsDir, fsError);
    if (fsError) { error = "Não foi possível criar " + assetsDir + ": " + fsError.message(); return false; }
    return projectio::writeFileAtomically(NamesPath(assetsDir).string(), json.dump(2), error);
}

std::string FileKey(const std::string& reference) {
    const auto path=std::filesystem::path(NormalizeImageReference(reference)).lexically_normal();
    if(path.is_absolute()||(!path.parent_path().empty()&&path.parent_path()!="assets")||path.filename()==".."||path.filename()==".")return {};
    return path.filename().string();
}

std::string ShortId(const std::string& filename) {
    const auto stem = std::filesystem::path(filename).stem().string();
    return stem.size() <= 4 ? stem : stem.substr(stem.size() - 4);
}

std::string TrimName(const std::string& value) {
    std::size_t begin = 0, end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

bool AnisotropicFilteringSupported() {
#if defined(LINBLUX_GL_HEADER)
    static const bool supported = []() {
        const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        return extensions && std::strstr(extensions, "GL_EXT_texture_filter_anisotropic") != nullptr;
    }();
    return supported;
#else
    return false;
#endif
}
}  // namespace

std::string AssetsDir() { return projectRoot.empty()?"assets":(projectRoot/"assets").string(); }
void SetProjectAssetRoot(const std::string& root) {
    const auto next=root.empty()?std::filesystem::path{}:std::filesystem::absolute(root).lexically_normal();
    if(next==projectRoot)return;
    ClearImageTextures();projectRoot=next;projectFace.clear();
}
std::string ProjectAssetRoot(){return projectRoot.string();}
void SetProjectFaceImage(const std::string& ref){projectFace=ref;}
std::string ProjectFaceImage(){return projectFace;}
std::string ImageReference(const std::string& path) {
    if(projectRoot.empty())return path;
    const auto relative=std::filesystem::path(path).lexically_relative(projectRoot);
    if(relative.empty()||relative.is_absolute())return path;
    for(const auto& piece:relative)if(piece=="..")return path;
    return relative.generic_string();
}

std::string ImageAssetReference(const std::string& filename) {
    const std::string name=std::filesystem::path(filename).filename().string();
    if(name.empty())return {};
    return "assets/"+name;
}

std::string NormalizeImageReference(const std::string& reference) {
    if(reference.empty())return {};
    const std::filesystem::path original(reference);
    if(original.is_absolute()) {
        const auto relative=ImageReference(reference);
        if(relative!=reference)return NormalizeImageReference(relative);
        return reference; // externo: a importação copia no próximo salvamento
    }
    auto text=original.lexically_normal().generic_string();
    while(text.rfind("./",0)==0)text.erase(0,2);
    if(text.empty())return {};
    if(text.rfind("assets/",0)==0)return text;
    if(text.find('/')!=std::string::npos)return text; // subdiretório do projeto
    std::error_code error;
    const std::filesystem::path inside=std::filesystem::path(AssetsDir())/text;
    if(std::filesystem::is_regular_file(inside,error)&&!std::filesystem::is_symlink(std::filesystem::symlink_status(inside,error)))
        return "assets/"+text;
    return text;
}

bool ImageInUse(const Scene& scene,const std::string& reference) {
    return !DescribeImageUsage(scene,reference).empty();
}

bool RemoveAssetFile(const std::string& assetsDir, const std::string& filename,
                     bool inUse, std::string& error) {
    error.clear();
    if(inUse){error="O asset está em uso na cena; remova as referências antes de apagá-lo.";return false;}
    const std::string name=std::filesystem::path(filename).filename().string();
    if(name!=filename||name.empty()){error="Use apenas o nome do arquivo em assets/.";return false;}
    const std::filesystem::path root=std::filesystem::path(assetsDir).lexically_normal();
    const std::filesystem::path target=root/name;
    if(std::filesystem::is_symlink(std::filesystem::symlink_status(root))){error="Diretório de assets não pode ser link simbólico.";return false;}
    std::error_code fsError;
    if(!std::filesystem::is_regular_file(target,fsError)){error="Asset não encontrado: "+name;return false;}
    if(std::filesystem::is_symlink(std::filesystem::symlink_status(target,fsError))){error="Symlink não pode ser removido como asset.";return false;}
    if(!std::filesystem::remove(target,fsError)||fsError){error="Falha ao remover "+name+": "+fsError.message();return false;}
    ForgetImageAssetName(assetsDir,name);
    ClearImageTextures();
    return true;
}

std::string BuiltinAssetPath(const std::string& relativePath) {
    if(relativePath=="loblox.png")return BuiltinAssetPath("logo.png"); // legacy resource alias
    if(relativePath=="rosto1.png")return BuiltinAssetPath("character-face.png"); // original LINBLUX replacement
    if(relativePath=="logo.png") {auto p=BuiltinAssetPath("midia/logo.png");if(!p.empty())return p;}
    if(relativePath=="character-face.png") {auto p=BuiltinAssetPath("assets/builtin/character-face.png");if(!p.empty())return p;}
    std::vector<std::filesystem::path> roots{std::filesystem::current_path()};
    char buffer[4096]{};const auto count=readlink("/proc/self/exe",buffer,sizeof(buffer)-1);
    if(count>0) { buffer[count]=0;const auto executable=std::filesystem::path(buffer).parent_path();
        for(const auto& base:{executable/"share/linblux/builtin",executable.parent_path()/"share/linblux/builtin",executable/"share/linblux",executable.parent_path()/"share/linblux"}) {
            const auto installed=base/relativePath;
            if(std::filesystem::is_regular_file(installed))return installed.string();
        }
        roots.push_back(executable);
    }
    for(auto root:roots) for(int i=0;i<4 && !root.empty();++i,root=root.parent_path()) {
        std::error_code ec;const auto candidate=root/relativePath;
        if(std::filesystem::is_regular_file(candidate,ec)) return candidate.string();
    }
    return {};
}

void ClearImageTextures() {
    for(const auto& entry:textureCache) if(entry.second.id) UnloadTexture(entry.second);
    textureCache.clear();
}

namespace {
bool IsAudioExtension(const std::string& ext) {
    return ext==".mp3"||ext==".wav"||ext==".ogg";
}
// O container e verificado antes de gravar. Sem isso, um arquivo renomeado
// seria persistido e so falharia ao tocar, longe da importacao.
bool ValidAudioContainer(const std::string& head,const std::string& ext,std::string& error) {
    const auto at=[&](std::size_t index) {return static_cast<unsigned char>(index<head.size()?head[index]:0);};
    if(ext==".wav") {
        if(head.size()<12||head.compare(0,4,"RIFF")!=0||head.compare(8,4,"WAVE")!=0) {
            error="Arquivo WAV inválido (cabeçalho RIFF/WAVE ausente)";return false;
        }
        return true;
    }
    if(ext==".ogg") {
        if(head.size()<4||head.compare(0,4,"OggS")!=0){error="Arquivo OGG inválido (OggS ausente)";return false;}
        return true;
    }
    // MP3: tag ID3 ou sincronismo de quadro MPEG (11 bits em 1).
    if(head.size()>=3&&head.compare(0,3,"ID3")==0)return true;
    if(head.size()>=2&&at(0)==0xFF&&(at(1)&0xE0)==0xE0)return true;
    error="Arquivo MP3 inválido (tag ID3 ou quadro MPEG ausente)";return false;
}
}  // namespace

bool ImportMediaTo(const std::string& assetsDir, const std::string& sourcePath,
                   MediaKind kind, std::string& outAssetPath, std::string& error) {
    std::error_code fsError;
    const std::filesystem::path source(sourcePath);
    if (!std::filesystem::exists(source, fsError) || !std::filesystem::is_regular_file(source, fsError)) {
        error = std::string(kind==MediaKind::Audio?"arquivo de áudio não encontrado: ":"arquivo de imagem não encontrado: ") + sourcePath;
        return false;
    }
    const std::string ext = LowerExt(source);
    const bool audioExt = IsAudioExtension(ext);
    const bool imageExt = ext == ".png" || ext == ".jpg" || ext == ".jpeg";
    if (kind==MediaKind::Audio) {
        if(!audioExt){error="formato de áudio não suportado (use MP3, WAV ou OGG): "+ext;return false;}
    } else if (kind==MediaKind::Image && !imageExt) {
        error = "formato não suportado (use PNG ou JPEG): " + ext;
        return false;
    } else if (kind==MediaKind::Any && !imageExt && !audioExt) {
        error = "formato de asset não suportado: " + ext;
        return false;
    }
    if(!audioExt) {
        ImageInfo info;
        if(!ReadSafeImageInfo(sourcePath,info)){error="Invalid PNG/JPEG or image exceeds the 32 MiB file / 64 MiB decoded budget";return false;}
        Image decoded=LoadProjectImage(sourcePath);
        const bool valid=decoded.data&&decoded.width==int(info.width)&&decoded.height==int(info.height);
        if(decoded.data)UnloadImage(decoded);
        if(!valid){error="Image decoding failed: "+sourcePath;return false;}
    }
    std::ifstream in(source, std::ios::binary);
    if (!in) {
        error = "não foi possível abrir: " + sourcePath;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    if (content.empty()) {
        error = "arquivo vazio: " + sourcePath;
        return false;
    }
    if(IsAudioExtension(ext)) {
        if(content.size()>256ull*1024*1024){error="Áudio excede o limite de 256 MiB";return false;}
        if(!ValidAudioContainer(content.substr(0,std::min<std::size_t>(content.size(),4096)),ext,error))return false;
    }
    // Sangue do ID: conteúdo + tamanho (evita colisão entre arquivos vazios).
    const std::string id = Fnv1aHex(content + std::to_string(content.size()));
    std::filesystem::create_directories(assetsDir, fsError);
    if (fsError) {
        error = "não foi possível criar " + assetsDir + ": " + fsError.message();
        return false;
    }
    const std::filesystem::path target =
        std::filesystem::path(assetsDir) / (id + ext);
    const auto rememberName=[&] {
        if(IsAudioExtension(ext)||source.stem()==target.stem())return;
        const auto names=ReadImageNames(assetsDir);if(names.count(target.filename().string()))return;
        std::string ignored;RenameImageAsset(assetsDir,target.filename().string(),source.stem().string(),ignored);
    };
    // Mesmo arquivo => mesmo destino; reuso sem reescrever.
    if (std::filesystem::exists(target, fsError)) {
        if(std::filesystem::is_symlink(std::filesystem::symlink_status(target))){error="Asset destination is a symlink";return false;}
        std::ifstream existing(target,std::ios::binary);
        const std::string saved((std::istreambuf_iterator<char>(existing)),{});
        if(saved!=content){error="Image asset ID collision; existing image was preserved";return false;}
        outAssetPath = target.string();rememberName();
        return true;
    }
    if(!projectio::writeFileAtomically(target.string(),content,error))return false;
    outAssetPath = target.string();rememberName();
    return true;
}

bool ImportImageTo(const std::string& assetsDir, const std::string& sourcePath,
                   std::string& outAssetPath, std::string& error) {
    return ImportMediaTo(assetsDir,sourcePath,MediaKind::Image,outAssetPath,error);
}

std::vector<ImageAsset> ListImages(const std::string& assetsDir) {
    std::vector<ImageAsset> list;
    const auto names = ReadImageNames(assetsDir);
    std::error_code fsError;
    for (const auto& entry : std::filesystem::directory_iterator(assetsDir, fsError)) {
        if (entry.is_symlink(fsError)||!entry.is_regular_file(fsError)) continue;
        const std::string ext = LowerExt(entry.path());
        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") continue;
        ImageAsset asset;
        asset.id = entry.path().stem().string();
        asset.filename = entry.path().filename().string();
        const auto named = names.find(asset.filename);
        asset.name = named != names.end() ? named->second : "Imagem " + ShortId(asset.filename);
        list.push_back(std::move(asset));
    }
    std::sort(list.begin(), list.end(), [](const ImageAsset& a, const ImageAsset& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.filename < b.filename;
    });
    return list;
}

std::string ImageDisplayName(const std::string& assetsDir, const std::string& reference) {
    const auto filename = FileKey(reference);
    if (filename.empty()) return {};
    const auto names = ReadImageNames(assetsDir);
    if (const auto it = names.find(filename); it != names.end()) return it->second;
    return "Imagem " + ShortId(filename);
}

bool RenameImageAsset(const std::string& assetsDir, const std::string& reference,
                      const std::string& name, std::string& error) {
    error.clear();
    const auto filename = FileKey(reference);
    if (filename.empty()) { error = "Selecione uma imagem da experiência para renomear."; return false; }
    std::error_code fsError;
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(std::filesystem::path(assetsDir)/filename,fsError))||!std::filesystem::is_regular_file(std::filesystem::path(assetsDir) / filename, fsError)) {
        error = "Imagem não encontrada em assets/: " + filename;
        return false;
    }
    const auto trimmed = TrimName(name);
    if (trimmed.empty() || trimmed.size() > 80) {
        error = "O nome da imagem deve ter entre 1 e 80 caracteres.";
        return false;
    }
    for (unsigned char c : trimmed)
        if (c < 32 || c == 127) { error = "O nome da imagem não pode ter caracteres de controle."; return false; }
    auto names = ReadImageNames(assetsDir);
    names[filename] = trimmed;
    return WriteImageNames(assetsDir, names, error);
}

void ForgetImageAssetName(const std::string& assetsDir, const std::string& reference) {
    const auto filename = FileKey(reference);
    if (filename.empty()) return;
    auto names = ReadImageNames(assetsDir);
    if (names.erase(filename) == 0) return;
    std::string error;
    WriteImageNames(assetsDir, names, error);
}

bool ImageReferenceMatches(const std::string& a,const std::string& b) {
    const auto key=[](const std::string& raw){
        if(raw.empty())return std::string();
        auto value=NormalizeImageReference(raw);
        if(value.find('/')==std::string::npos&&value!="."&&value!="..")value="assets/"+value;
        return std::filesystem::path(value).lexically_normal().generic_string();
    };
    return !a.empty()&&!b.empty()&&key(a)==key(b);
}
bool IsImageProperty(const std::string& key) {
    return key=="baseColorTexture"||key=="normalTexture"||key=="metallicRoughnessTexture"||key=="occlusionTexture"||
        key=="item_icon"||key=="image"||key=="decal_image"||key=="texture_image"||key=="FaceTexture"||
        key=="skyFront"||key=="skyBack"||key=="skyLeft"||key=="skyRight"||key=="skyUp"||key=="skyDown";
}
std::vector<ImageUsage> DescribeImageUsage(const Scene& scene,const std::string& reference) {
    std::vector<ImageUsage> uses;
    const auto add=[&](ImageUsage use){if(ImageReferenceMatches(use.reference,reference))uses.push_back(std::move(use));};
    for(const auto& object:scene.objects()) {
        if(object->isPart()) {
            const auto& part=static_cast<const Part&>(*object);
            if(part.material3D()) {
                const auto& m=*part.material3D();
                for(const auto& pair:{std::pair<const char*,const std::string*>{"baseColorTexture",&m.baseColorTexture},
                    {"normalTexture",&m.normalTexture},{"metallicRoughnessTexture",&m.metallicRoughnessTexture},{"occlusionTexture",&m.occlusionTexture}}) {
                    ImageUsage use;use.object=part.id();use.property=std::string("material3D.")+pair.first;
                    use.reference=*pair.second;use.kind="material";use.where=part.name()+" · "+pair.first;add(std::move(use));
                }
            }
        }
        for(const auto& [key,value]:object->attributes()) {
            const auto* image=std::get_if<std::string>(&value);if(!image||!IsImageProperty(key))continue;
            ImageUsage use;use.object=object->id();use.property=key;use.reference=*image;
            use.kind=key.rfind("texture",0)==0?"texture":"decal";
            use.where=object->name()+" · "+key;add(std::move(use));
        }
        if(IsSurfaceClass(object->instanceClass())) {
            const auto& surface=static_cast<const Surface&>(*object);
            ImageUsage use;use.object=surface.id();use.property="surface.image";use.reference=surface.properties().image;
            use.kind=surface.instanceClass()==InstanceClass::Texture?"texture":"decal";
            use.where=(surface.parent()?surface.parent()->name()+" / ":"")+surface.name()+" · imagem";add(std::move(use));
        }
    }
    for(const auto& rig:scene.gameAssets().rigs)for(int i=0;i<kBodyCount;++i) {
        const auto& body=rig.bodies[i];ImageUsage use;use.rig=rig.id;use.body=i;
        use.where="Personagem \""+rig.name+"\" / "+BodyDisplayName(body.name);
        use.kind="body";use.property="image";use.reference=body.image;add(use);
        for(const auto& decal:body.decals){auto child=use;child.kind="decal";child.decal=decal.id;
            child.property="surface.image";child.reference=decal.surface.image;child.where+=" / "+decal.name;add(std::move(child));}
    }
    return uses;
}
std::vector<ImageUsage> DescribeImageUsage(const experiences::Manifest& manifest,const std::string& reference) {
    std::vector<ImageUsage> uses;
    const std::string* values[]={&manifest.icon,&manifest.banner,&manifest.loadingBackground,&manifest.faceImage};
    const char* fields[]={"icon","banner","loadingBackground","faceImage"};
    const char* names[]={"Ícone da experiência","Banner da experiência","Fundo de carregamento","Rosto legado da experiência"};
    for(int i=0;i<4;++i)if(ImageReferenceMatches(*values[i],reference)){
        ImageUsage use;use.kind="presentation";use.property=fields[i];use.reference=*values[i];use.where=names[i];uses.push_back(std::move(use));}
    return uses;
}
bool SetImageUsage(Scene& scene,const ImageUsage& use,const std::string& replacement,std::string& error) {
    error="A referência foi alterada; consulte os usos da imagem novamente.";
    const auto target=NormalizeImageReference(replacement);
    if(target.size()>512||target.find('\0')!=std::string::npos){error="Referência de imagem inválida.";return false;}
    if(!use.rig.empty()) {
        auto* rig=scene.gameAssets().findRig(use.rig);if(!rig||use.body<0||use.body>=kBodyCount)return false;
        auto& body=rig->bodies[use.body];
        if(use.decal.empty()){if(use.property!="image"||body.image!=use.reference)return false;body.image=target;}
        else {auto it=std::find_if(body.decals.begin(),body.decals.end(),[&](const RigDecal& d){return d.id==use.decal;});
            if(it==body.decals.end()||it->surface.image!=use.reference)return false;it->surface.image=target;}
    }else {
        auto* object=scene.find(use.object);if(!object)return false;
        if(object->isPart()&&use.property.rfind("material3D.",0)==0) {
            auto& part=static_cast<Part&>(*object);if(!part.material3D())return false;auto m=*part.material3D();
            std::string* field=use.property=="material3D.baseColorTexture"?&m.baseColorTexture:
                use.property=="material3D.normalTexture"?&m.normalTexture:
                use.property=="material3D.metallicRoughnessTexture"?&m.metallicRoughnessTexture:
                use.property=="material3D.occlusionTexture"?&m.occlusionTexture:nullptr;
            if(!field||*field!=use.reference)return false;*field=target;
            if(!part.setMaterial3D(m,error))return false;
        }else if(IsSurfaceClass(object->instanceClass())&&use.property=="surface.image") {
            auto& surface=static_cast<Surface&>(*object);auto value=surface.properties();if(value.image!=use.reference)return false;
            value.image=target;if(!surface.setProperties(value))return false;
        }else {
            if(!IsImageProperty(use.property))return false;
            const auto it=object->attributes().find(use.property);if(it==object->attributes().end())return false;
            const auto* value=std::get_if<std::string>(&it->second);if(!value||*value!=use.reference)return false;
            if(target.empty())object->removeAttribute(use.property);
            else if(!object->setAttribute(use.property,target)){error="Limite de atributos atingido.";return false;}
        }
    }
    error.clear();return true;
}
std::size_t ReplaceImageReferences(Scene& scene,const std::string& from,const std::string& to) {
    const auto target=NormalizeImageReference(to);if(target.empty()||target.size()>512)return 0;
    const auto uses=DescribeImageUsage(scene,from);std::size_t count=0;
    for(const auto& use:uses){std::string error;if(!SetImageUsage(scene,use,target,error))throw std::runtime_error(error);++count;}
    return count;
}

std::string BodyDisplayName(const std::string& bodyName) {
    if (bodyName == "Head") return "Cabeça";
    if (bodyName == "Torso") return "Torso";
    if (bodyName == "Left Arm") return "Braço esquerdo";
    if (bodyName == "Right Arm") return "Braço direito";
    if (bodyName == "Left Leg") return "Perna esquerda";
    if (bodyName == "Right Leg") return "Perna direita";
    return bodyName;
}

std::string ResolveImagePath(const std::string& assetsDir, const std::string& assetPath) {
    if (assetPath.empty()) return {};
    const auto normalized=NormalizeImageReference(assetPath);
    const std::filesystem::path direct(normalized.empty()?assetPath:normalized);
    if(!projectRoot.empty()) {
        const auto relative=direct.is_absolute()?direct.lexically_relative(projectRoot):direct;
        for(const auto& part:relative)if(part=="..") {
            return (direct.string()==BuiltinAssetPath("rosto1.png")||direct.string()==BuiltinAssetPath("logo.png"))?direct.string():std::string{};
        }
        if(relative.empty()||relative.is_absolute())return {};
        auto component=projectRoot;
        for(const auto& part:relative){component/=part;if(std::filesystem::is_symlink(std::filesystem::symlink_status(component)))return {};}
        const auto candidate=projectRoot/relative;
        if(std::filesystem::is_regular_file(candidate))return candidate.string();
        const auto bare=projectRoot/"assets"/relative;
        return std::filesystem::is_regular_file(bare)?bare.string():std::string{};
    }
    if (std::filesystem::exists(direct)) return direct.string();
    const std::filesystem::path inside = std::filesystem::path(assetsDir) / direct;
    if (std::filesystem::exists(inside)) return inside.string();
    const std::string wanted=direct.filename().string();
    std::error_code fsError;
    for (const auto& entry : std::filesystem::directory_iterator(assetsDir, fsError)) {
        if (!entry.is_regular_file(fsError)) continue;
        if (entry.path().filename().string() == wanted) return entry.path().string();
    }
    return {};
}

Texture2D AssetTexture(const std::string& relativePath) {
    if(relativePath.empty())return {};
    const std::filesystem::path input(relativePath);
    const auto key=(input.is_absolute()?input:projectRoot.empty()?std::filesystem::absolute(input):projectRoot/input).lexically_normal().string();
    if(const auto cached=textureCache.find(key);cached!=textureCache.end())return cached->second;
    const auto resolved=ResolveImagePath(AssetsDir(),relativePath);
    if(resolved.empty())return {};
    const auto it = textureCache.find(resolved);
    if (it != textureCache.end()) return it->second;
    Texture2D tex{};
    ImageInfo safe;
    if (ReadSafeImageInfo(resolved,safe)) {
        Image image=LoadProjectImage(resolved);
        if(image.data) {
            ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            if(image.format==PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
                // The quads that sample these textures blend with premultiplied
                // alpha, so the stored colour has to be premultiplied as well.
                // Otherwise the mip chain averages the arbitrary colour that
                // source PNGs keep under alpha = 0 (the LINBLUX logo stores
                // magenta there) into the visible artwork.
                ImageAlphaPremultiply(&image);
#if defined(LINBLUX_GL_HEADER)
                GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
                if(maximum>0&&(image.width>maximum||image.height>maximum)) {
                    const float scale=std::min(float(maximum)/image.width,float(maximum)/image.height);
                    ImageResize(&image,std::max(1,int(image.width*scale)),std::max(1,int(image.height*scale)));
                }
#endif
                tex=LoadTextureFromImage(image);
                UnloadImage(image);
                if(tex.id) {
                    // raylib uploads with a plain bilinear filter and no mip
                    // chain, so a 1600x1600 image minified onto a wall turned
                    // into aliased black and white noise. Mipmaps plus trilinear
                    // minification keep the surface readable at any distance.
                    GenTextureMipmaps(&tex);
                    SetTextureFilter(tex,TEXTURE_FILTER_TRILINEAR);
                    if(AnisotropicFilteringSupported())SetTextureFilter(tex,TEXTURE_FILTER_ANISOTROPIC_4X);
                }
            } else UnloadImage(image);
        }
    }
    textureCache.emplace(key, tex);
    return tex;
}

}  // namespace loblox::game

#include "game/ImageAssets.h"
#include "core/ImageInfo.h"
#include "game/Rig.h"
#include "objects/Part.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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

bool ImageInUse(const Scene& scene, const std::string& reference) {
    if(reference.empty())return false;
    const auto wanted=std::filesystem::path(NormalizeImageReference(reference)).filename().string();
    if(wanted.empty())return false;
    const auto matches=[&](const std::string& value) {
        return !value.empty()&&std::filesystem::path(value).filename().string()==wanted;
    };
    for(const Part* part:scene.parts()) {
        if(!part)continue;
        for(const auto& attribute:part->attributes()) {
            if(attribute.first.find("image")==std::string::npos&&attribute.first!="FaceTexture")continue;
            if(const auto* text=std::get_if<std::string>(&attribute.second))if(matches(*text))return true;
        }
    }
    for(const auto& rig:scene.gameAssets().rigs) {
        for(const auto& body:rig.bodies)if(matches(body.image))return true;
        for(const auto& body:rig.bodies)for(const auto& decal:body.decals)if(matches(decal.surface.image))return true;
    }
    return false;
}

bool RemoveAssetFile(const std::string& assetsDir, const std::string& filename,
                     bool inUse, std::string& error) {
    error.clear();
    if(inUse){error="O asset está em uso na cena; remova as referências antes de apagá-lo.";return false;}
    const std::string name=std::filesystem::path(filename).filename().string();
    if(name!=filename||name.empty()){error="Use apenas o nome do arquivo em assets/.";return false;}
    const std::filesystem::path root=std::filesystem::path(assetsDir).lexically_normal();
    const std::filesystem::path target=root/name;
    std::error_code fsError;
    if(!std::filesystem::is_regular_file(target,fsError)){error="Asset não encontrado: "+name;return false;}
    if(std::filesystem::is_symlink(std::filesystem::symlink_status(target,fsError))){error="Symlink não pode ser removido como asset.";return false;}
    if(!std::filesystem::remove(target,fsError)||fsError){error="Falha ao remover "+name+": "+fsError.message();return false;}
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
    if(kind!=MediaKind::Audio) {
        ImageInfo info;
        if(!ReadSafeImageInfo(sourcePath,info)){error="Invalid PNG/JPEG or image exceeds the 32 MiB file / 64 MiB decoded budget";return false;}
        Image decoded=LoadProjectImage(sourcePath);
        const bool valid=decoded.data&&decoded.width==int(info.width)&&decoded.height==int(info.height);
        if(decoded.data)UnloadImage(decoded);
        if(!valid){error="Image decoding failed: "+sourcePath;return false;}
    }
    if (!std::filesystem::exists(source, fsError) || !std::filesystem::is_regular_file(source, fsError)) {
        error = std::string(kind==MediaKind::Audio?"arquivo de áudio não encontrado: ":"arquivo de imagem não encontrado: ") + sourcePath;
        return false;
    }
    const std::string ext = LowerExt(source);
    if (kind==MediaKind::Audio) {
        if(!IsAudioExtension(ext)){error="formato de áudio não suportado (use MP3, WAV ou OGG): "+ext;return false;}
    } else if (kind==MediaKind::Image && ext != ".png" && ext != ".jpg" && ext != ".jpeg") {
        error = "formato não suportado (use PNG ou JPEG): " + ext;
        return false;
    } else if (kind==MediaKind::Any && ext != ".png" && ext != ".jpg" && ext != ".jpeg" && !IsAudioExtension(ext)) {
        error = "formato de asset não suportado: " + ext;
        return false;
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
    // Mesmo arquivo => mesmo destino; reuso sem reescrever.
    if (std::filesystem::exists(target, fsError)) {
        if(std::filesystem::is_symlink(std::filesystem::symlink_status(target))){error="Asset destination is a symlink";return false;}
        std::ifstream existing(target,std::ios::binary);
        const std::string saved((std::istreambuf_iterator<char>(existing)),{});
        if(saved!=content){error="Image asset ID collision; existing image was preserved";return false;}
        outAssetPath = target.string();
        return true;
    }
    if(!projectio::writeFileAtomically(target.string(),content,error))return false;
    outAssetPath = target.string();
    return true;
}

bool ImportImageTo(const std::string& assetsDir, const std::string& sourcePath,
                   std::string& outAssetPath, std::string& error) {
    return ImportMediaTo(assetsDir,sourcePath,MediaKind::Image,outAssetPath,error);
}

std::vector<ImageAsset> ListImages(const std::string& assetsDir) {
    std::vector<ImageAsset> list;
    std::error_code fsError;
    for (const auto& entry : std::filesystem::directory_iterator(assetsDir, fsError)) {
        if (!entry.is_regular_file(fsError)) continue;
        const std::string ext = LowerExt(entry.path());
        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") continue;
        list.push_back({entry.path().stem().string(), entry.path().filename().string()});
    }
    std::sort(list.begin(), list.end(),
              [](const ImageAsset& a, const ImageAsset& b) { return a.filename < b.filename; });
    return list;
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

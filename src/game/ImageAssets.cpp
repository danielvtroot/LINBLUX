#include "game/ImageAssets.h"
#include "core/ImageInfo.h"
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
#    define LOBLOX_GL_HEADER 1
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
#if defined(LOBLOX_GL_HEADER)
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

std::string BuiltinAssetPath(const std::string& relativePath) {
    std::vector<std::filesystem::path> roots{std::filesystem::current_path()};
    char buffer[4096]{};const auto count=readlink("/proc/self/exe",buffer,sizeof(buffer)-1);
    if(count>0) { buffer[count]=0;const auto executable=std::filesystem::path(buffer).parent_path();
        for(const auto& base:{executable/"share/loblox/builtin",executable.parent_path()/"share/loblox/builtin",executable/"share/loblox",executable.parent_path()/"share/loblox"}) {
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

bool ImportImageTo(const std::string& assetsDir, const std::string& sourcePath,
                   std::string& outAssetPath, std::string& error) {
    std::error_code fsError;
    const std::filesystem::path source(sourcePath);
    ImageInfo info;
    if(!ReadSafeImageInfo(sourcePath,info)){error="Invalid PNG/JPEG or image exceeds the 32 MiB file / 64 MiB decoded budget";return false;}
    Image decoded=LoadProjectImage(sourcePath);
    const bool valid=decoded.data&&decoded.width==int(info.width)&&decoded.height==int(info.height);
    if(decoded.data)UnloadImage(decoded);
    if(!valid){error="Image decoding failed: "+sourcePath;return false;}
    if (!std::filesystem::exists(source, fsError) || !std::filesystem::is_regular_file(source, fsError)) {
        error = "arquivo de imagem não encontrado: " + sourcePath;
        return false;
    }
    const std::string ext = LowerExt(source);
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") {
        error = "formato não suportado (use PNG ou JPEG): " + ext;
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
    const std::filesystem::path direct(assetPath);
    if(!projectRoot.empty()) {
        const auto relative=direct.is_absolute()?direct.lexically_relative(projectRoot):direct;
        for(const auto& part:relative)if(part=="..") {
            return direct.string()==BuiltinAssetPath("rosto1.png")?direct.string():std::string{};
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
    const std::filesystem::path inside =
        std::filesystem::path(assetsDir) / assetPath;
    if (std::filesystem::exists(inside)) return inside.string();
    std::error_code fsError;
    for (const auto& entry : std::filesystem::directory_iterator(assetsDir, fsError)) {
        if (!entry.is_regular_file(fsError)) continue;
        if (entry.path().stem().string() == assetPath) return entry.path().string();
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
                // source PNGs keep under alpha = 0 (the Loblox logo stores
                // magenta there) into the visible artwork.
                ImageAlphaPremultiply(&image);
#if defined(LOBLOX_GL_HEADER)
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

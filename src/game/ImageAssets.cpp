#include "game/ImageAssets.h"

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

std::string AssetsDir() { return "assets"; }

std::string BuiltinAssetPath(const std::string& relativePath) {
    std::vector<std::filesystem::path> roots{std::filesystem::current_path()};
    char buffer[4096]{};const auto count=readlink("/proc/self/exe",buffer,sizeof(buffer)-1);
    if(count>0) { buffer[count]=0;roots.push_back(std::filesystem::path(buffer).parent_path()); }
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
        outAssetPath = target.string();
        return true;
    }
    std::ofstream out(target, std::ios::binary);
    if (out) out.write(content.data(), (std::streamsize)content.size());
    if (!out || fsError) {
        error = "não foi possível gravar: " + target.string();
        return false;
    }
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
    const auto it = textureCache.find(relativePath);
    if (it != textureCache.end()) return it->second;
    Texture2D tex{};
    if (std::filesystem::exists(relativePath)) {
        Image image=LoadImage(relativePath.c_str());
        if(image.data) {
            ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            if(image.format==PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
                // The quads that sample these textures blend with premultiplied
                // alpha, so the stored colour has to be premultiplied as well.
                // Otherwise the mip chain averages the arbitrary colour that
                // source PNGs keep under alpha = 0 (the Loblox logo stores
                // magenta there) into the visible artwork.
                ImageAlphaPremultiply(&image);
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
    textureCache.emplace(relativePath, tex);
    return tex;
}

}  // namespace loblox::game

#pragma once
#include <string>
#include <nlohmann/json_fwd.hpp>
#include "raylib.h"

namespace loblox {
enum class PartMaterial;
// Optional authored material. Legacy Part color/material remain untouched.
struct Material3D {
    Color baseColor{163,162,165,255};
    float metallic=0,roughness=.45f,normalScale=1,emissiveStrength=0;
    Color emissive{0,0,0,255};
    Vector2 studsPerTile{4,4},offset{};
    float rotation=0;
    int finish=0; // 0 smooth, 1 concrete, 2 wood
    // Construction surface. 0 keeps the material's own finish; 1 draws the
    // measured grid; 2 explicitly keeps the surface smooth (opts out of the
    // implicit grid used by untouched default Parts/Baseplates).
    int surface=0;
    float gridScale=1.0f;     // world units per small square
    float gridIntensity=1.0f; // 0..1 line strength
    std::string baseColorTexture,normalTexture,metallicRoughnessTexture,occlusionTexture;
    bool validate(std::string& error) const;
};
Material3D MaterialPreset3D(int index,Color color);
Material3D LegacyMaterial3D(PartMaterial material,Color color);
const char* MaterialPresetName(int index);
nlohmann::json EncodeMaterial3D(const Material3D& material);
Material3D DecodeMaterial3D(const nlohmann::json& value);
}

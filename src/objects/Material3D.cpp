#include "objects/Part.h"
#include "game/ImageAssets.h"
#include <cmath>
#include <stdexcept>
#include <nlohmann/json.hpp>
namespace loblox {
bool Material3D::validate(std::string& error) const {
    const auto valid=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    if(!valid(metallic,0,1)||!valid(roughness,0,1)||!valid(normalScale,0,4)||
       !valid(emissiveStrength,0,20)||!valid(studsPerTile.x,.01f,1e6f)||!valid(studsPerTile.y,.01f,1e6f)||
       !valid(offset.x,-1e6f,1e6f)||!valid(offset.y,-1e6f,1e6f)||!valid(rotation,-36000,36000)||finish<0||finish>2) {
        error="Material 3D: valores fora dos limites";return false;
    }
    for(const auto* path:{&baseColorTexture,&normalTexture,&metallicRoughnessTexture,&occlusionTexture})
        if(path->size()>512||path->find('\0')!=std::string::npos){error="Referência de imagem inválida";return false;}
    error.clear();return true;
}
const char* MaterialPresetName(int i) {
    static const char* names[]={"Plástico","Concreto","Madeira","Metal","Vidro simples","Emissivo"};
    return i>=0&&i<6?names[i]:"Plástico";
}
Material3D MaterialPreset3D(int i,Color color) {
    Material3D m;m.baseColor=color;
    switch(i) {
        case 1:m.roughness=.95f;m.finish=1;break;
        case 2:m.roughness=.65f;m.finish=2;break;
        case 3:m.metallic=1;m.roughness=.22f;break;
        case 4:m.baseColor.a=85;m.roughness=.08f;break;
        case 5:m.emissive=color;m.emissiveStrength=2;m.roughness=.35f;break;
    }
    return m;
}
Material3D LegacyMaterial3D(PartMaterial kind,Color color) {
    int preset=kind==PartMaterial::Concrete?1:kind==PartMaterial::Wood?2:kind==PartMaterial::Metal?3:kind==PartMaterial::Neon?5:0;
    auto m=MaterialPreset3D(preset,color);if(kind==PartMaterial::Plastic)m.roughness=.3f;return m;
}
nlohmann::json EncodeMaterial3D(const Material3D& m) {
    return {{"baseColor",{m.baseColor.r,m.baseColor.g,m.baseColor.b,m.baseColor.a}},
        {"metallic",m.metallic},{"roughness",m.roughness},{"normalScale",m.normalScale},
        {"emissive",{m.emissive.r,m.emissive.g,m.emissive.b,m.emissive.a}},{"emissiveStrength",m.emissiveStrength},
        {"studsPerTile",{m.studsPerTile.x,m.studsPerTile.y}},{"offset",{m.offset.x,m.offset.y}},
        {"rotation",m.rotation},{"finish",m.finish},
        {"baseColorTexture",game::NormalizeImageReference(m.baseColorTexture)},
        {"normalTexture",game::NormalizeImageReference(m.normalTexture)},
        {"metallicRoughnessTexture",game::NormalizeImageReference(m.metallicRoughnessTexture)},
        {"occlusionTexture",game::NormalizeImageReference(m.occlusionTexture)}};
}
Material3D DecodeMaterial3D(const nlohmann::json& j) {
    if(!j.is_object())throw std::runtime_error("material3D deve ser objeto");
    Material3D m;
    const auto color=[&](const char* key,Color fallback){if(!j.contains(key))return fallback;
        const auto& a=j.at(key);if(!a.is_array()||a.size()!=4)throw std::runtime_error("Cor requer RGBA");
        Color c;for(int i=0;i<4;++i){if(!a[i].is_number_integer()||a[i].get<double>()<0||a[i].get<double>()>255)throw std::runtime_error("RGBA fora de 0..255");(&c.r)[i]=a[i].get<int>();}return c;};
    m.baseColor=color("baseColor",m.baseColor);m.emissive=color("emissive",m.emissive);
    m.metallic=j.value("metallic",m.metallic);m.roughness=j.value("roughness",m.roughness);
    m.normalScale=j.value("normalScale",m.normalScale);m.emissiveStrength=j.value("emissiveStrength",m.emissiveStrength);
    m.finish=j.value("finish",0);m.rotation=j.value("rotation",0.f);
    for(const auto& field:{std::pair<const char*,Vector2*>{"studsPerTile",&m.studsPerTile},{"offset",&m.offset}})
        if(j.contains(field.first)){const auto& v=j.at(field.first);if(!v.is_array()||v.size()!=2)throw std::runtime_error("UV requer dois números");*field.second={v[0].get<float>(),v[1].get<float>()};}
    m.baseColorTexture=j.value("baseColorTexture",std::string());m.normalTexture=j.value("normalTexture",std::string());
    m.metallicRoughnessTexture=j.value("metallicRoughnessTexture",std::string());m.occlusionTexture=j.value("occlusionTexture",std::string());
    std::string error;if(!m.validate(error))throw std::runtime_error(error);return m;
}
}

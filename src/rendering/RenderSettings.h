#pragma once
namespace loblox {
enum class RenderQuality { Legacy, Low, Medium, High };
inline const char* RenderQualityName(RenderQuality q) {
    switch(q){case RenderQuality::Legacy:return "Compatível";case RenderQuality::Low:return "Baixo";
        case RenderQuality::High:return "Alto (PBR leve)";default:return "Médio";}
}
}

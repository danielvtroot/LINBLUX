#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace loblox::numfmt {

// Tolerancia de exibicao: valores menores que isto aparecem como 0. O valor
// real do objeto nunca muda; apenas o texto e arredondado.
constexpr float kDisplayZero = 1e-4f;

// 0 para residuos de ponto flutuante (-9.7e-12 vira 0), o proprio valor caso
// contrario. Nao arredonda valores significativos.
inline float DisplayValue(float value) {
    if (!std::isfinite(value)) return 0.0f;
    return std::fabs(value) < kDisplayZero ? 0.0f : value;
}

// Texto sem notacao cientifica e sem zeros a direita.
inline void Format(char* out, std::size_t size, float value, int decimals = 4) {
    if (out == nullptr || size == 0) return;
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, DisplayValue(value));
    char* end = buffer;
    while (*end != '\0') ++end;
    if (std::strchr(buffer, '.') != nullptr) {
        while (end > buffer && end[-1] == '0') --end;
        if (end > buffer && end[-1] == '.') --end;
    }
    if (end == buffer) {
        // "-0" tambem e ruido de arredondamento.
        std::snprintf(out, size, "0");
        return;
    }
    const std::size_t length = static_cast<std::size_t>(end - buffer);
    const std::size_t copy = length + 1 < size ? length : size - 1;
    for (std::size_t i = 0; i < copy; ++i) out[i] = buffer[i];
    out[copy] = '\0';
}

inline std::string Text(float value, int decimals = 4) {
    char buffer[64];
    Format(buffer, sizeof(buffer), value, decimals);
    return std::string(buffer);
}

// Aceita apenas o numero inteiro do texto (sem lixo, sem notacao incompleta).
inline bool Parse(const char* text, float& value) {
    if (text == nullptr) return false;
    while (*text == ' ' || *text == '\t') ++text;
    if (*text == '\0') return false;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != '\0') return false;
    if (!std::isfinite(parsed)) return false;
    if (std::fabs(parsed) > 1.0e6) return false;
    value = static_cast<float>(parsed);
    return true;
}

}  // namespace loblox::numfmt

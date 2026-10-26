#include "rendering/LuaUiRenderer.h"

#include "game/ImageAssets.h"
#include "game/Runtime.h"
#include "rendering/ExperienceOverlay.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace loblox {
namespace {
Font UiFont() { return ExperienceOverlayFont(); }

Color Fade(Color color, float opacity) {
    color.a = static_cast<unsigned char>(std::clamp(color.a * opacity, 0.f, 255.f));
    return color;
}

// Never let a long label overflow its own box: truncate on a UTF-8 boundary.
std::string Fit(Font font, const std::string& value, float available, float size, float spacing) {
    if (value.empty() || available <= 0) return {};
    if (MeasureTextEx(font, value.c_str(), size, spacing).x <= available) return value;
    std::string text = value;
    while (!text.empty()) {
        std::size_t index = text.size() - 1;
        while (index && (static_cast<unsigned char>(text[index]) & 0xc0) == 0x80) --index;
        text.resize(index);
        if (MeasureTextEx(font, (text + "...").c_str(), size, spacing).x <= available) return text + "...";
    }
    return {};
}
} // namespace

void DrawLuaUi(const game::RuntimeSession& runtime, int width, int height, Font font) {
    const auto& ui = runtime.ui();
    if (runtime.state() == game::RuntimeState::Editing || runtime.state() == game::RuntimeState::StoppingPlay) return;
    if (!ui.count() || width <= 0 || height <= 0) return;
    if (font.texture.id == GetFontDefault().texture.id) font = UiFont();
    const float scale = game::UiModel::scaleFor(float(width), float(height));
    for (const auto* element : ui.drawOrder()) {
        if (element->opacity <= 0.004f) continue;
        const Rectangle rect = ui.resolve(*element, float(width), float(height));
        if (rect.width <= 0 || rect.height <= 0) continue;
        const Color color = Fade(element->color, element->opacity);
        const Color background = Fade(element->background, element->opacity);
        const float font_size = element->fontSize * scale;
        const float spacing = std::max(1.f, scale);
        switch (element->kind) {
            case game::UiKind::Panel:
                DrawRectangleRounded(rect, .08f, 6, background);
                break;
            case game::UiKind::Label: {
                const auto text = Fit(font, element->text, rect.width, font_size, spacing);
                DrawTextEx(font, text.c_str(), {rect.x, rect.y + (rect.height - font_size) * .5f},
                           font_size, spacing, color);
                break;
            }
            case game::UiKind::Button: {
                DrawRectangleRounded(rect, .22f, 6, background);
                const auto text = Fit(font, element->text, rect.width - 12 * scale, font_size, spacing);
                const Vector2 size = MeasureTextEx(font, text.c_str(), font_size, spacing);
                DrawTextEx(font, text.c_str(),
                           {rect.x + (rect.width - size.x) * .5f, rect.y + (rect.height - size.y) * .5f},
                           font_size, spacing, color);
                break;
            }
            case game::UiKind::Image: {
                if (element->image.empty()) break;
                const auto path = game::ResolveImagePath(game::AssetsDir(), element->image);
                if (path.empty()) break;
                const auto texture = game::AssetTexture(path);
                if (!texture.id) break;
                DrawTexturePro(texture, {0, 0, float(texture.width), float(texture.height)}, rect,
                               {0, 0}, 0, Fade(WHITE, element->opacity));
                break;
            }
            case game::UiKind::Bar: {
                DrawRectangleRounded(rect, .5f, 8, background);
                const float maximum = element->maxValue > 0 ? element->maxValue : 1;
                const float ratio = std::clamp(element->value / maximum, 0.f, 1.f);
                if (ratio > 0) {
                    Rectangle fill = rect;
                    fill.width = std::max(2.f, rect.width * ratio);
                    DrawRectangleRounded(fill, .5f, 8, color);
                }
                if (!element->text.empty()) {
                    const auto text = Fit(font, element->text, rect.width - 8 * scale, font_size, spacing);
                    const Vector2 size = MeasureTextEx(font, text.c_str(), font_size, spacing);
                    DrawTextEx(font, text.c_str(),
                               {rect.x + (rect.width - size.x) * .5f, rect.y + (rect.height - size.y) * .5f},
                               font_size, spacing, Fade(WHITE, element->opacity));
                }
                break;
            }
        }
    }
}
} // namespace loblox

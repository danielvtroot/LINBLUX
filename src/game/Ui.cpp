#include "game/Ui.h"

#include <algorithm>
#include <cmath>

namespace loblox::game {
namespace {
const char* const kAnchors[] = {"top-left", "top", "top-right", "left", "center",
                                "right", "bottom-left", "bottom", "bottom-right"};

bool Finite(double value) { return std::isfinite(value); }

void DefaultSize(UiKind kind, float& width, float& height) {
    if (width > 0 && height > 0) return;
    switch (kind) {
        case UiKind::Panel: if (width <= 0) width = 240; if (height <= 0) height = 140; break;
        case UiKind::Label: if (width <= 0) width = 220; if (height <= 0) height = 30; break;
        case UiKind::Button: if (width <= 0) width = 200; if (height <= 0) height = 44; break;
        case UiKind::Image: if (width <= 0) width = 96; if (height <= 0) height = 96; break;
        case UiKind::Bar: if (width <= 0) width = 220; if (height <= 0) height = 28; break;
    }
}

bool KnownAnchor(const std::string& anchor) {
    return std::any_of(std::begin(kAnchors), std::end(kAnchors),
                       [&](const char* value) { return anchor == value; });
}

float Scaled(float value, float scale) { return value * scale; }
} // namespace

const char* UiKindName(UiKind kind) {
    switch (kind) {
        case UiKind::Panel: return "panel";
        case UiKind::Label: return "label";
        case UiKind::Button: return "button";
        case UiKind::Image: return "image";
        case UiKind::Bar: return "bar";
    }
    return "panel";
}

bool UiKindFromName(const std::string& name, UiKind& kind) {
    if (name == "panel" || name == "frame") { kind = UiKind::Panel; return true; }
    if (name == "label" || name == "text") { kind = UiKind::Label; return true; }
    if (name == "button") { kind = UiKind::Button; return true; }
    if (name == "image" || name == "icon") { kind = UiKind::Image; return true; }
    if (name == "bar" || name == "indicator" || name == "progress") { kind = UiKind::Bar; return true; }
    return false;
}

void UiModel::clear() {
    elements_.clear();
    clicks_.clear();
    hovers_.clear();
    hovered_ = 0;
    mutations_ = 0;
}

bool UiModel::clearOwner(std::uint64_t owner) {
    if (!owner) return false;
    // Children of another owner can only exist under the same owner, so a
    // single pass is enough; erase from the leaves to keep parents valid.
    for (auto it = elements_.begin(); it != elements_.end();) {
        if (it->second.owner == owner) it = elements_.erase(it);
        else ++it;
    }
    if (hovered_ && !elements_.count(hovered_)) hovered_ = 0;
    return true;
}

std::size_t UiModel::ownedBy(std::uint64_t owner) const {
    return static_cast<std::size_t>(std::count_if(elements_.begin(), elements_.end(),
        [&](const auto& entry) { return entry.second.owner == owner; }));
}

bool UiModel::mutate() {
    if (mutations_ >= kMaxMutationsPerFrame) return false;
    ++mutations_;
    return true;
}

bool UiModel::create(UiKind kind, std::uint64_t owner, std::uint64_t parent,
                     std::uint64_t& id, std::string& error) {
    error.clear();
    if (elements_.size() >= kMaxElements) { error = "Limite de 128 elementos de UI atingido."; return false; }
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    UiElement element;
    element.id = next_++;
    element.owner = owner;
    element.kind = kind;
    element.z = int(elements_.size());
    if (parent) {
        const auto* ownerElement = find(parent);
        if (!ownerElement) { error = "Elemento pai não existe."; return false; }
        if (ownerElement->owner != owner) { error = "Elemento pai pertence a outro script."; return false; }
        if (ownerElement->depth + 1 >= kMaxDepth) { error = "Profundidade máxima de UI (4) atingida."; return false; }
        element.parent = parent;
        element.depth = ownerElement->depth + 1;
    }
    DefaultSize(kind, element.width, element.height);
    elements_.emplace(element.id, std::move(element));
    id = next_ - 1;
    return true;
}

const UiElement* UiModel::find(std::uint64_t id) const {
    const auto it = elements_.find(id);
    return it == elements_.end() ? nullptr : &it->second;
}

UiElement* UiModel::mutableFind(std::uint64_t id, std::uint64_t owner, std::string& error) {
    const auto it = elements_.find(id);
    if (it == elements_.end()) { error = "Elemento de UI não existe (ou já foi destruído)."; return nullptr; }
    if (owner && it->second.owner != owner) { error = "Elemento de UI pertence a outro script."; return nullptr; }
    return &it->second;
}

bool UiModel::destroy(std::uint64_t id, std::uint64_t owner, std::string& error) {
    error.clear();
    if (!mutableFind(id, owner, error)) return false;
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    for (auto it = elements_.begin(); it != elements_.end();) {
        if (it->first == id || it->second.parent == id) it = elements_.erase(it);
        else ++it;
    }
    if (hovered_ && !elements_.count(hovered_)) hovered_ = 0;
    return true;
}

bool UiModel::setText(std::uint64_t id, std::uint64_t owner, const std::string& value, std::string& error) {
    error.clear();
    auto* element = mutableFind(id, owner, error);
    if (!element) return false;
    if (value.size() > kMaxText) { error = "Texto de UI acima de 512 bytes."; return false; }
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    element->text = value;
    return true;
}

bool UiModel::setNumber(std::uint64_t id, std::uint64_t owner, const std::string& key, double value,
                        std::string& error) {
    error.clear();
    auto* element = mutableFind(id, owner, error);
    if (!element) return false;
    if (!Finite(value)) { error = "Valor de UI não é finito."; return false; }
    if (key == "x" || key == "y") {
        if (std::fabs(value) > 10000) { error = "Posição de UI fora do intervalo."; return false; }
        (key == "x" ? element->x : element->y) = float(value);
    } else if (key == "width" || key == "height") {
        if (value < 0 || value > 4000) { error = "Tamanho de UI deve ficar entre 0 e 4000."; return false; }
        (key == "width" ? element->width : element->height) = float(value);
    } else if (key == "opacity" || key == "transparency") {
        if (value < 0 || value > 1) { error = "Transparência de UI deve ficar entre 0 e 1."; return false; }
        element->opacity = float(key == "transparency" ? 1.0 - value : value);
    } else if (key == "font_size") {
        if (value < 8 || value > 96) { error = "Tamanho de fonte de UI deve ficar entre 8 e 96."; return false; }
        element->fontSize = float(value);
    } else if (key == "value") {
        element->value = float(value);
    } else if (key == "max") {
        if (value <= 0 || value > 1000000) { error = "Máximo de indicador deve ser positivo."; return false; }
        element->maxValue = float(value);
    } else if (key == "z") {
        if (value < -1000 || value > 1000) { error = "Ordem visual deve ficar entre -1000 e 1000."; return false; }
        element->z = int(value);
    } else {
        error = "Propriedade numérica de UI desconhecida: " + key;
        return false;
    }
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    return true;
}

bool UiModel::setBool(std::uint64_t id, std::uint64_t owner, const std::string& key, bool value,
                      std::string& error) {
    error.clear();
    auto* element = mutableFind(id, owner, error);
    if (!element) return false;
    if (key != "visible") { error = "Propriedade booleana de UI desconhecida: " + key; return false; }
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    element->visible = value;
    return true;
}

bool UiModel::setColor(std::uint64_t id, std::uint64_t owner, const std::string& key, Color value,
                       std::string& error) {
    error.clear();
    auto* element = mutableFind(id, owner, error);
    if (!element) return false;
    if (key == "color") element->color = value;
    else if (key == "background") element->background = value;
    else { error = "Propriedade de cor de UI desconhecida: " + key; return false; }
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    return true;
}

bool UiModel::setAnchor(std::uint64_t id, std::uint64_t owner, const std::string& value, std::string& error) {
    error.clear();
    auto* element = mutableFind(id, owner, error);
    if (!element) return false;
    if (!KnownAnchor(value)) { error = "Âncora inválida; use top-left, center, bottom-right, etc."; return false; }
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    element->anchor = value;
    return true;
}

bool UiModel::setImage(std::uint64_t id, std::uint64_t owner, const std::string& value, std::string& error) {
    error.clear();
    auto* element = mutableFind(id, owner, error);
    if (!element) return false;
    if (value.size() > 512) { error = "Caminho de imagem de UI acima de 512 bytes."; return false; }
    if (!mutate()) { error = "Limite de alterações de UI por quadro atingido."; return false; }
    element->image = value;
    return true;
}

std::vector<const UiElement*> UiModel::drawOrder() const {
    std::vector<const UiElement*> ordered;
    ordered.reserve(elements_.size());
    for (const auto& entry : elements_) if (entry.second.visible) ordered.push_back(&entry.second);
    std::stable_sort(ordered.begin(), ordered.end(), [](const UiElement* a, const UiElement* b) {
        if (a->z != b->z) return a->z < b->z;
        return a->id < b->id; // parents are always created before children
    });
    return ordered;
}

Rectangle UiModel::anchored(const UiElement& element, Rectangle parent) {
    const float width = element.width, height = element.height;
    float x = 0, y = 0;
    const std::string& anchor = element.anchor;
    if (anchor == "top") x = (parent.width - width) * .5f;
    else if (anchor == "top-right") x = parent.width - width;
    else if (anchor == "left") y = (parent.height - height) * .5f;
    else if (anchor == "center") { x = (parent.width - width) * .5f; y = (parent.height - height) * .5f; }
    else if (anchor == "right") { x = parent.width - width; y = (parent.height - height) * .5f; }
    else if (anchor == "bottom-left") y = parent.height - height;
    else if (anchor == "bottom") { x = (parent.width - width) * .5f; y = parent.height - height; }
    else if (anchor == "bottom-right") { x = parent.width - width; y = parent.height - height; }
    return {parent.x + x + element.x, parent.y + y + element.y, width, height};
}

Rectangle UiModel::resolve(const UiElement& element, float viewportWidth, float viewportHeight) const {
    // Compose anchors from the root down so a child is placed inside the
    // resolved rectangle of its parent, once each, with a single scale.
    std::vector<const UiElement*> chain;
    const UiElement* current = &element;
    while (current && chain.size() < 8) {
        chain.push_back(current);
        current = current->parent ? find(current->parent) : nullptr;
    }
    std::reverse(chain.begin(), chain.end());
    const float scale = scaleFor(viewportWidth, viewportHeight);
    Rectangle parent{0, 0, viewportWidth, viewportHeight};
    for (const auto* node : chain) {
        UiElement scaled = *node;
        scaled.x = Scaled(node->x, scale);
        scaled.y = Scaled(node->y, scale);
        scaled.width = Scaled(node->width, scale);
        scaled.height = Scaled(node->height, scale);
        parent = anchored(scaled, parent);
    }
    return parent;
}

std::uint64_t UiModel::hitTest(Vector2 point, float viewportWidth, float viewportHeight) const {
    const auto ordered = drawOrder();
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
        const auto& element = **it;
        if (element.kind != UiKind::Button || element.opacity <= 0.01f) continue;
        if (CheckCollisionPointRec(point, resolve(element, viewportWidth, viewportHeight))) return element.id;
    }
    return 0;
}

void UiModel::pointer(Vector2 point, float viewportWidth, float viewportHeight) {
    const auto hit = hitTest(point, viewportWidth, viewportHeight);
    if (hit == hovered_) return;
    if (hovered_ && hovers_.size() < 32) hovers_.push_back({hovered_, false});
    if (hit && hovers_.size() < 32) hovers_.push_back({hit, true});
    hovered_ = hit;
}

void UiModel::click(Vector2 point, float viewportWidth, float viewportHeight) {
    const auto hit = hitTest(point, viewportWidth, viewportHeight);
    if (!hit) return;
    if (clicks_.size() >= 8) clicks_.erase(clicks_.begin());
    clicks_.push_back(hit);
}

std::uint64_t UiModel::takeClick() {
    if (clicks_.empty()) return 0;
    const auto id = clicks_.front();
    clicks_.erase(clicks_.begin());
    return id;
}

bool UiModel::takeHover(std::uint64_t& id, bool& entering) {
    if (hovers_.empty()) return false;
    id = hovers_.front().first;
    entering = hovers_.front().second;
    hovers_.erase(hovers_.begin());
    return true;
}

} // namespace loblox::game

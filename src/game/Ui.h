#pragma once

#include "raylib.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace loblox::game {

// Declarative, script-owned UI. The model never holds Lua or authoring
// pointers: it is a bounded tree of visual nodes that the runtime validates,
// the hosts render with raylib, and the Lua layer mutates through checked
// handles. Coordinates are design units on a 1280x720 canvas and are scaled
// by the shorter viewport axis, so anchors keep the layout at any resolution.
enum class UiKind { Panel, Label, Button, Image, Bar };

const char* UiKindName(UiKind kind);
bool UiKindFromName(const std::string& name, UiKind& kind);

struct UiElement {
    std::uint64_t id = 0;
    std::uint64_t parent = 0;
    std::uint64_t owner = 0; // owning Script instance; 0 = host
    UiKind kind = UiKind::Panel;
    int depth = 0;
    std::string text;
    std::string image;
    std::string anchor = "top-left";
    float x = 0, y = 0, width = 0, height = 0;
    float opacity = 1, value = 0, maxValue = 100, fontSize = 18;
    Color color{242, 242, 245, 255};
    Color background{15, 15, 19, 225};
    bool visible = true;
    int z = 0;
};

class UiModel {
public:
    static constexpr std::size_t kMaxElements = 128;
    static constexpr int kMaxDepth = 4;
    static constexpr std::size_t kMaxText = 512;
    static constexpr unsigned kMaxMutationsPerFrame = 64;
    static constexpr float kDesignWidth = 1280.f, kDesignHeight = 720.f;

    void clear();
    bool clearOwner(std::uint64_t owner);
    bool create(UiKind kind, std::uint64_t owner, std::uint64_t parent,
                std::uint64_t& id, std::string& error);
    bool destroy(std::uint64_t id, std::uint64_t owner, std::string& error);
    const UiElement* find(std::uint64_t id) const;
    std::size_t count() const { return elements_.size(); }
    std::size_t ownedBy(std::uint64_t owner) const;

    // Property writes share one path so Lua and native callers validate alike.
    bool setText(std::uint64_t id, std::uint64_t owner, const std::string& value, std::string& error);
    bool setNumber(std::uint64_t id, std::uint64_t owner, const std::string& key, double value, std::string& error);
    bool setBool(std::uint64_t id, std::uint64_t owner, const std::string& key, bool value, std::string& error);
    bool setColor(std::uint64_t id, std::uint64_t owner, const std::string& key, Color value, std::string& error);
    bool setAnchor(std::uint64_t id, std::uint64_t owner, const std::string& value, std::string& error);
    bool setImage(std::uint64_t id, std::uint64_t owner, const std::string& value, std::string& error);

    std::vector<const UiElement*> drawOrder() const; // visible, z then id
    Rectangle resolve(const UiElement& element, float viewportWidth, float viewportHeight) const;
    static float scaleFor(float viewportWidth, float viewportHeight) {
        const float width = viewportWidth > 0 ? viewportWidth / kDesignWidth : 1;
        const float height = viewportHeight > 0 ? viewportHeight / kDesignHeight : 1;
        return width < height ? width : height;
    }
    std::uint64_t hitTest(Vector2 point, float viewportWidth, float viewportHeight) const;

    // Pointer state is produced by the host renderer, which alone knows the
    // viewport rectangle. Clicks and hover changes are drained by the runtime
    // on the next update, never from inside the render pass.
    void pointer(Vector2 point, float viewportWidth, float viewportHeight);
    void click(Vector2 point, float viewportWidth, float viewportHeight);
    std::uint64_t takeClick();
    bool takeHover(std::uint64_t& id, bool& entering);
    std::uint64_t hovered() const { return hovered_; }

    void beginFrame() { mutations_ = 0; }
    unsigned mutations() const { return mutations_; }

private:
    UiElement* mutableFind(std::uint64_t id, std::uint64_t owner, std::string& error);
    bool mutate();
    static Rectangle anchored(const UiElement& element, Rectangle parent);
    std::map<std::uint64_t, UiElement> elements_;
    std::uint64_t next_ = 1;
    unsigned mutations_ = 0;
    std::uint64_t hovered_ = 0;
    std::vector<std::uint64_t> clicks_;
    std::vector<std::pair<std::uint64_t, bool>> hovers_;
};

} // namespace loblox::game

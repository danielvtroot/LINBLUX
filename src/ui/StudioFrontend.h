#pragma once
#include <memory>
#include <string>
#include "raylib.h"
#include "editor/InputRouting.h"
namespace loblox {
class EditorUI;
class StudioFrontend {
public:
    explicit StudioFrontend(EditorUI& editor);
    ~StudioFrontend();
    bool initialize(std::string& error);
    void draw();
    void render(); // after rlImGuiEnd, before EndDrawing
    void filter(EditorInput& input,InputContext& context);
    Rectangle viewport() const;
    void prepareAdvanced();
    // Diagnostic geometry/state, also used by actual GUI integration tests.
    Rectangle elementBounds(const std::string& id) const;
    std::string elementText(const std::string& id) const;
    bool textFocused() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}

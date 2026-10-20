#pragma once

#include "raylib.h"

namespace loblox {

// One normalized snapshot per UI frame. Controllers never poll global input.
struct EditorInput {
    Vector2 mouse{}, delta{};
    Vector3 movement{};
    float wheel = 0;
    bool leftPressed = false, leftDown = false;
    bool rightPressed = false, rightDown = false;
    bool middlePressed = false, middleDown = false;
    bool escape = false, shift = false, modified = false;
    bool save = false, open = false, remove = false, focus = false;
    int tool = -1;
};

struct InputContext {
    bool windowFocused = false;
    bool viewportVisible = false, hovered = false, focused = false;
    bool popup = false, text = false, widget = false;
    bool keyboardCaptured = false;
    bool transforming = false, navigating = false;
};

enum class InputOwner { UI, Cancel, Transform, Camera, Viewport };
struct InputRoute {
    InputOwner owner = InputOwner::UI;
    const char* reason = "outside_viewport";
    bool editorKeys = false, globalKeys = false;
};

inline const char* InputOwnerName(InputOwner owner) {
    switch (owner) {
        case InputOwner::UI: return "ui";
        case InputOwner::Cancel: return "cancel";
        case InputOwner::Transform: return "transform";
        case InputOwner::Camera: return "camera";
        case InputOwner::Viewport: return "viewport";
    }
    return "ui";
}

inline InputRoute RouteInput(const EditorInput& input, const InputContext& ctx) {
    if (!ctx.windowFocused || !ctx.viewportVisible || ctx.popup || ctx.text || ctx.widget) {
        const char* reason = !ctx.windowFocused ? "window_focus_lost" :
            !ctx.viewportVisible ? "viewport_hidden" : ctx.popup ? "popup" :
            ctx.text ? "text_input" : "active_widget";
        return {ctx.transforming || ctx.navigating ? InputOwner::Cancel : InputOwner::UI, reason};
    }
    if (input.escape) return {InputOwner::Cancel, "escape"};
    // Existing sessions own motion and release, even outside the image.
    if (ctx.transforming) return {InputOwner::Transform, "active_drag", true, true};
    if (ctx.navigating) return {InputOwner::Camera, "active_navigation"};
    // Navigation presses beat a passive gizmo hover. A handle owns only LMB.
    if (ctx.hovered && (input.rightPressed || input.middlePressed))
        return {InputOwner::Camera, "navigation_begin"};
    const bool global = !ctx.keyboardCaptured;
    if (!ctx.hovered) return {InputOwner::UI, "outside_viewport", ctx.focused, global};
    // A held navigation button originating outside cannot start a viewport action.
    if (input.rightDown || input.middleDown) return {InputOwner::UI, "foreign_navigation"};
    return {InputOwner::Viewport, "viewport", ctx.focused, global || ctx.focused};
}

} // namespace loblox

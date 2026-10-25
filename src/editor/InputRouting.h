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
    bool escape = false, shift = false, alt = false, modified = false;
    bool save = false, open = false, remove = false, focus = false, duplicate = false;
    bool play = false, stop = false, jump = false;
    bool maximizeGame = false, fullscreen = false;
    bool rename = false, copy = false, paste = false;
    bool undo = false, redo = false;
    // Ctrl+L alterna World/Local, como no Studio; nao conflita com os atalhos
    // existentes (1-4, F, Ctrl+S/O/D/C/V, Delete, F2, F5, F10/F11).
    bool toggleLocalSpace = false;
    int tool = -1;
};

enum class InputMode { Editing, Playing, Paused };
struct InputContext {
    bool windowFocused = false;
    bool viewportVisible = false, hovered = false, focused = false;
    bool popup = false, text = false, widget = false;
    bool keyboardCaptured = false;
    bool transforming = false, navigating = false;
    InputMode mode = InputMode::Editing;
    int menuDepth = 0;
};

enum class InputOwner { UI, Cancel, Transform, Camera, Viewport, Gameplay, Pause, Resume, Back, Stop };
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
        case InputOwner::Gameplay: return "gameplay";
        case InputOwner::Pause: return "pause";
        case InputOwner::Resume: return "resume";
        case InputOwner::Back: return "menu_back";
        case InputOwner::Stop: return "stop";
    }
    return "ui";
}

inline InputRoute RouteInput(const EditorInput& input, const InputContext& ctx) {
    if (ctx.mode != InputMode::Editing) {
        if (input.stop && ctx.windowFocused) return {InputOwner::Stop, "stop_play"};
        const bool paused = ctx.mode == InputMode::Paused;
        if (!ctx.windowFocused || !ctx.viewportVisible)
            return {paused ? InputOwner::UI : InputOwner::Pause, "game_focus_lost"};
        if (paused) {
            if (input.escape && !ctx.text && !ctx.widget)
                return {InputOwner::Resume, "escape"}; // ESC fecha o menu em qualquer aba
            return {InputOwner::UI, "pause_menu"};
        }
        if (ctx.popup || ctx.text || ctx.widget) return {InputOwner::UI, "game_ui"};
        if (input.escape) return {InputOwner::Pause, "escape"};
        return {ctx.focused ? InputOwner::Gameplay : InputOwner::UI,
                ctx.focused ? "gameplay" : "game_viewport_unfocused"};
    }
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

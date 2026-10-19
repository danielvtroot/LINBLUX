#include "app/Application.h"

#include <cstdio>

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

#include "editor/EditorUI.h"
#include "rendering/SceneRenderer.h"

namespace loblox {

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    unsigned int flags = FLAG_WINDOW_RESIZABLE;
    if (config_.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    SetConfigFlags(flags);
    InitWindow(config_.windowWidth, config_.windowHeight, "Loblox Open-Source");
    windowReady_ = true;
    SetWindowMinSize(900, 560);
    SetExitKey(KEY_NULL);
    SetTargetFPS(config_.targetFps);

    rlImGuiSetup(true);
    imguiReady_ = true;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;  // layout deterministico; nada de arquivos extras
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.115f, 0.115f, 0.125f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.36f, 0.55f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.44f, 0.68f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.34f, 0.42f, 1.0f);

    scene_.buildDefaultScene();
    physics_.attach(scene_);
    physics_.setGravity(Vector3{0.0f, config_.gravity, 0.0f});
    physics_.setFixedStep(config_.physicsFixedStep, config_.physicsMaxSubSteps);

    renderer_ = std::make_unique<SceneRenderer>();
    ui_ = std::make_unique<EditorUI>(scene_, physics_, camera_, *renderer_, config_);
    ui_->setOnSceneLoaded([this]() { physics_.rebuildAll(scene_); });
    ui_->setStatus("Loblox Open-Source pronto. Toolbox a esquerda para construir.");
    return true;
}

void Application::shutdown() {
    // Recursos graficos precisam ser liberados antes de fechar a janela.
    ui_.reset();
    renderer_.reset();
    if (imguiReady_) {
        rlImGuiShutdown();
        imguiReady_ = false;
    }
    if (windowReady_) {
        CloseWindow();
        windowReady_ = false;
    }
}

int Application::run() {
    if (!initialize()) {
        std::fprintf(stderr, "Falha ao inicializar a janela.\n");
        return 1;
    }

    while (!WindowShouldClose()) {
        const float deltaSeconds = GetFrameTime();

        // A fisica roda continuamente (sem Play/Stop), com passo fixo interno.
        physics_.simulate(scene_, deltaSeconds);

        BeginDrawing();
        ClearBackground(Color{18, 18, 20, 255});
        rlImGuiBegin();
        if (ui_) {
            ui_->draw();
        }
        rlImGuiEnd();
        EndDrawing();
    }

    shutdown();
    return 0;
}

}  // namespace loblox

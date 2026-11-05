#include "app/Application.h"
#include "editor/StudioTheme.h"
#include "game/ImageAssets.h"
#include "experiences/ExperienceManager.h"
#include "ui/StudioFrontend.h"

#include <cstdio>
#include <filesystem>
#include <utility>

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

#include "core/Diagnostics.h"
#include "editor/EditorUI.h"
#include "rendering/SceneRenderer.h"
#include "serialization/ProjectIO.h"

namespace loblox {

Application::Application(AppConfig config) : config_(std::move(config)) {}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    unsigned int flags = FLAG_WINDOW_RESIZABLE;
    if (config_.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    SetConfigFlags(flags);
    InitWindow(config_.windowWidth, config_.windowHeight, "LINBLUX Studio");
    // Áudio 2D do Play. Sem dispositivo (host sem servidor de som) o runtime
    // continua funcionando: AudioSystem só abre streams quando o host responde.
    if (IsWindowReady()) InitAudioDevice();

    if (!IsWindowReady()) {
        diag::Event event;
        event.level = diag::Level::Error;
        event.subsystem = "app";
        event.name = "startup_window_failed";
        event.result = "error";
        event.error = "janela grafica indisponivel";
        diag::log(event);
        std::fprintf(stderr,
                     "Falha ao criar a janela grafica. Verifique DISPLAY/WAYLAND_DISPLAY "
                     "e o driver OpenGL.\n");
        return false;
    }
    windowReady_ = true;
    const auto iconPath=game::BuiltinAssetPath("logo.png");
    if(!iconPath.empty()) {
        Image icon=LoadImage(iconPath.c_str());
        if(icon.data){SetWindowIcon(icon);UnloadImage(icon);}
    }
    SetWindowMinSize(900, 560);
    SetExitKey(KEY_NULL);
    SetTargetFPS(config_.targetFps);

    rlImGuiSetup(true);
    imguiReady_ = true;

    ImGuiIO& io = ImGui::GetIO();
    const char* sansFonts[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    };
    for (const char* fontPath : sansFonts) {
        if (!std::filesystem::exists(fontPath)) continue;
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 14.0f)) {
            io.FontDefault = font;
            break;
        }
    }
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "linblux_imgui.ini";
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    studio::Apply();

    scene_.buildDefaultScene();
    const bool home=config_.modernUI&&config_.startupProjectPath.empty();
    if(config_.modernUI) {
        try{experiences_=std::make_unique<experiences::ExperienceManager>();}
        catch(const std::exception& error){std::fprintf(stderr,"Biblioteca de experiências indisponível: %s\n",error.what());}
    }

    // Politica de inicializacao:
    //   1. `--project PATH` e sempre tentado e nunca e sobrescrito;
    //   2. sem flag, RmlUi abre Home; o modo de diagnostico legado ainda
    //      pode carregar o caminho padrao se existir;
    //   3. qualquer falha mantem a cena padrao intacta e aparece na UI/stderr.
    std::string startupPath = config_.startupProjectPath.empty()
                                        ? config_.defaultProjectPath
                                        : config_.startupProjectPath;
    std::error_code startupPathError;
    const bool defaultExists = std::filesystem::is_regular_file(startupPath, startupPathError);
    const bool inaccessible = startupPathError &&
        startupPathError != std::errc::no_such_file_or_directory &&
        startupPathError != std::errc::not_a_directory;
    const bool shouldLoad = !home&&(!config_.startupProjectPath.empty() || defaultExists || inaccessible);
    bool projectLoaded = false;
    std::string projectLoadError;
    if (shouldLoad) {
        try {
            const std::filesystem::path path(startupPath);
            if(experiences_&&(path.extension()==".lo"||path.filename()=="experience.json"||std::filesystem::is_directory(path))) {
                const auto experience=path.extension()==".lo"?experiences_->importPackage(path):experiences_->inspect(path.filename()=="experience.json"?path.parent_path():path);
                experiences_->open(experience,scene_);startupPath=experiences_->activePlace().string();projectLoaded=true;
            } else projectLoaded = projectio::load(scene_, startupPath, projectLoadError);
        }catch(const std::exception& error){projectLoaded=false;projectLoadError=error.what();}
        diag::Event event;
        event.level = projectLoaded ? diag::Level::Info : diag::Level::Error;
        event.subsystem = "app";
        event.name = projectLoaded ? "startup_project_loaded" : "startup_project_failed";
        event.file = startupPath;
        event.result = projectLoaded ? "ok" : "error";
        event.error = projectLoaded ? std::string() : projectLoadError;
        diag::log(event);
        if (!projectLoaded) {
            std::fprintf(stderr, "Falha ao carregar '%s': %s. Cena padrao preservada.\n",
                         startupPath.c_str(), projectLoadError.c_str());
        }
    }

    if(home&&experiences_)scene_.clearObjects();
    else if (!projectLoaded) scene_.installDefaultScripts();
    physics_.attach(scene_);
    physics_.setGravity(Vector3{0.0f, config_.gravity, 0.0f});
    physics_.setFixedStep(config_.physicsFixedStep, config_.physicsMaxSubSteps);

    renderer_ = std::make_unique<SceneRenderer>();
    // Keep allocation and constructor built against the same frozen UI header.
    ui_ = std::make_unique<EditorUI>(scene_, physics_, camera_, *renderer_, config_);
    ui_->setOnSceneLoaded([this]() { physics_.rebuildAll(scene_); });
    if(config_.modernUI) {
        std::string error;
        if(!ui_->enableModernFrontend(error)) {
            std::fprintf(stderr,"RmlUi indisponível; usando interface de diagnóstico: %s\n",error.c_str());
            ui_->setStatus("Falha ao iniciar RmlUi: "+error,true);
        }
        if(ui_->frontend()&&experiences_)ui_->frontend()->enableExperiences(*experiences_,home);
    }
    if (projectLoaded) {
        std::error_code pathError;
        const std::filesystem::path absolute =
            std::filesystem::absolute(startupPath, pathError);
        const std::string resolved = pathError ? startupPath : absolute.string();
        config_.defaultProjectPath = resolved;
        ui_->setProjectPath(resolved);
        ui_->setStatus("Projeto carregado: " + resolved);
    } else if (shouldLoad) {
        ui_->setStatus("Falha ao carregar '" + startupPath +
                           "': " + projectLoadError + ". Cena padrao preservada.",
                       true);
    } else {
        ui_->setStatus("LINBLUX Studio pronto. Use as ferramentas para construir.");
    }
    diag::Event ready;
    ready.subsystem = "app";
    ready.name = "startup_ready";
    ready.result = "ok";
    ready.context = {{"project_loaded", projectLoaded}, {"parts", scene_.parts().size()}};
    diag::log(ready);
    return true;
}

void Application::shutdown() {
    // Recursos graficos precisam ser liberados antes de fechar a janela.
    ui_.reset();
    renderer_.reset();
    if(experiences_)experiences_->close();
    if (imguiReady_) {
        rlImGuiShutdown();
        imguiReady_ = false;
    }
    if (IsAudioDeviceReady()) CloseAudioDevice();
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

    const bool reportTiming=std::getenv("LOBLOX_UI_TIMINGS")!=nullptr;
    double frameTotal=0;int frameSamples=0;
    while (!WindowShouldClose()) {
        const float deltaSeconds = GetFrameTime();

        // Play owns a different Scene and Bullet world. Authoring freezes exactly.
        if (!ui_ || (!ui_->playing()&&!ui_->experienceOverlay())) physics_.simulate(scene_, deltaSeconds);

        BeginDrawing();
        ClearBackground(Color{18, 18, 20, 255});
        rlImGuiBegin();
        if (ui_) {
            ui_->draw();
        }
        rlImGuiEnd();
        if(ui_)ui_->renderModernFrontend();
        EndDrawing();
        if(reportTiming) {frameTotal+=GetFrameTime();if(++frameSamples==60){std::fprintf(stderr,"Application frame ms (%s): %.2f\n",config_.modernUI?"rml":"legacy",frameTotal*1000/frameSamples);frameSamples=0;frameTotal=0;}}
    }

    shutdown();
    return 0;
}

}  // namespace loblox

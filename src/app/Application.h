#pragma once

#include <memory>

#include "core/Config.h"
#include "physics/PhysicsWorld.h"
#include "rendering/EditorCamera.h"
#include "scene/Scene.h"

namespace loblox {

class SceneRenderer;
class EditorUI;
namespace experiences {class ExperienceManager;}

class Application {
public:
    explicit Application(AppConfig config = AppConfig{});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    bool initialize();
    void shutdown();

    AppConfig config_;
    Scene scene_;
    PhysicsWorld physics_;
    EditorCamera camera_;
    std::unique_ptr<SceneRenderer> renderer_;
    std::unique_ptr<EditorUI> ui_;
    std::unique_ptr<experiences::ExperienceManager> experiences_;
    bool windowReady_ = false;
    bool imguiReady_ = false;
};

}  // namespace loblox

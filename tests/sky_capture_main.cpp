// Comprehensive sky capture test
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <filesystem>
#include "raylib.h"
#include "rendering/SceneRenderer.h"
#include "rendering/EditorCamera.h"
#include "core/Diagnostics.h"
#include "serialization/ProjectIO.h"
#include "objects/Lighting.h"
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"

using namespace loblox;

int main() {
    char temp[] = "/tmp/loblox-sky-capture-XXXXXX";
    const char* directory = mkdtemp(temp);
    if (!directory) return 1;

    diag::configure(std::string(directory) + "/events.jsonl");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1920, 1080, "Loblox Sky Capture");
    SetWindowPosition(0, 0);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    std::printf("Window created\n");

    // Load the default project
    Scene scene;
    std::string error;
    const std::string projectPath = "/home/labubu/projeto/loblox/projects/default.loblox.json";
    bool loaded = projectio::load(scene, projectPath, error);
    if (!loaded) {
        std::fprintf(stderr, "Failed to load project: %s\n", error.c_str());
        scene.buildDefaultScene();
    }
    scene.installDefaultScripts();

    std::printf("Scene built\n");

    // Force Atmospheric mode and configure sky
    Lighting* lighting = scene.lighting();
    lighting->setSkyMode(SkyMode::Atmospheric);
    lighting->setSkyColor(Color{120, 180, 240, 255});
    lighting->setHorizonColor(Color{180, 200, 230, 255});
    lighting->setAtmosphereStrength(0.6f);
    lighting->setSkyRotation(0.0f);
    lighting->setSkyboxEnabled(true);

    std::printf("Lighting configured (Atmospheric)\n");

    PhysicsWorld physics;
    physics.attach(scene);
    std::printf("Physics attached\n");

    EditorCamera camera;
    std::printf("Camera created\n");

    SceneRenderer* renderer = new SceneRenderer();
    std::printf("Renderer created\n");

    auto frames = [&](int count = 3) {
        for (int i = 0; i < count; ++i) {
            BeginDrawing();
            ClearBackground(BLACK);
            renderer->drawScene(scene, camera.camera(), 1920, 1080, kInvalidInstanceId, nullptr, true);
            EndDrawing();
        }
    };

    auto capture = [&](const std::string& name) {
        frames(2);
        TakeScreenshot((std::string(directory) + "/" + name + ".png").c_str());
        std::printf("Captured: %s/%s.png\n", directory, name.c_str());
    };

    std::printf("Starting frames...\n");
    frames(10);
    std::printf("Initial frames done\n");

    // Atmospheric mode captures
    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, -0.3f);
    capture("sky_atmospheric_top_down");

    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, 0.0f);
    capture("sky_atmospheric_horizon");

    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, -1.3f);
    capture("sky_atmospheric_zenith");

    for (int i = 0; i < 4; ++i) {
        float yaw = i * PI / 2.0f;
        camera.setOrbit(Vector3{0, 3, 0}, 50.0f, yaw, 0.0f);
        capture("sky_atmospheric_yaw_" + std::to_string(i * 90));
    }

    // Classic mode for comparison
    lighting->setSkyMode(SkyMode::Classic);
    frames(5);
    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, 0.0f);
    capture("sky_classic_horizon");
    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, -1.3f);
    capture("sky_classic_zenith");

    // Restore Atmospheric
    lighting->setSkyMode(SkyMode::Atmospheric);
    frames(5);

    // Test with sky rotation
    lighting->setSkyRotation(45.0f);
    frames(5);
    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, 0.0f);
    capture("sky_atmospheric_rotated_45");

    // Test with atmosphere strength variations
    lighting->setSkyRotation(0.0f);
    lighting->setAtmosphereStrength(0.0f);
    frames(5);
    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, 0.0f);
    capture("sky_atmospheric_strength_0");

    lighting->setAtmosphereStrength(1.0f);
    frames(5);
    camera.setOrbit(Vector3{0, 3, 0}, 50.0f, 0.0f, 0.0f);
    capture("sky_atmospheric_strength_1");

    std::printf("\nAll screenshots saved to: %s\n", directory);
    std::printf("Sky Mode final: %d\n", static_cast<int>(lighting->skyMode()));

    CloseWindow();
    return 0;
}
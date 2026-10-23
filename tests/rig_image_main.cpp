#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <string>

#include "core/Diagnostics.h"
#include "game/GameAssetsIO.h"
#include "game/ImageAssets.h"
#include "game/Rig.h"
#include "objects/Lighting.h"
#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"

namespace {
int checks = 0;
int failures = 0;

void Check(bool value, const char* expression, int line) {
    ++checks;
    if (!value) {
        ++failures;
        std::fprintf(stderr, "FAIL %d: %s\n", line, expression);
    }
}
#define CHECK(x) Check((x), #x, __LINE__)

bool Magenta(Color c) {
    return c.r > 120 && c.b > 95 && c.g < 115 && c.r > c.g + 45 && c.b > c.g + 35;
}

Color Sample(const Image& image, Vector2 screen) {
    const int x = std::clamp(static_cast<int>(std::lround(screen.x)), 0, image.width - 1);
    const int y = std::clamp(static_cast<int>(std::lround(screen.y)), 0, image.height - 1);
    return GetImageColor(image, x, y);
}

int CountFaceInk(const Image& image, Vector2 center) {
    int count = 0;
    for (int y = static_cast<int>(center.y) - 50; y <= static_cast<int>(center.y) + 50; ++y) {
        for (int x = static_cast<int>(center.x) - 70; x <= static_cast<int>(center.x) + 70; ++x) {
            if (x < 0 || y < 0 || x >= image.width || y >= image.height) continue;
            const Color c = GetImageColor(image, x, y);
            if (c.r < 100 && c.g < 100 && c.b < 100) ++count;
        }
    }
    return count;
}
}  // namespace

int main() {
    char temp[] = "/tmp/loblox-rig-image-XXXXXX";
    const char* directory = mkdtemp(temp);
    if (!directory) return 1;
    std::filesystem::current_path(directory);
    std::filesystem::create_directories("assets");
    loblox::diag::configure(std::string(directory) + "/events.jsonl");

    // The image is deliberately magenta so it cannot be confused with the
    // blue torso, yellow head, green legs, or the default face texture.
    const std::string imagePath = "assets/body.png";
    Image source = GenImageColor(64, 64, {232, 38, 176, 255});
    CHECK(ExportImage(source, imagePath.c_str()));
    UnloadImage(source);
    CHECK(std::filesystem::is_regular_file(imagePath));

    using namespace loblox;
    using namespace loblox::game;
    std::string error;

    // ---- Persistence and model validation ---------------------------------
    GameAssets assets;
    auto& rig = assets.createR6();
    rig.root.position = {0, 6, 0};
    for (auto& body : rig.bodies) body.image = "body.png";
    rig.bodies[Torso].image = "body.png";
    CHECK(assets.validate(error));
    CHECK(assets.rigs[0].bodies[Head].image == "body.png");
    rig.resizeBody(RightArm, {1.3f, 2.2f, 1.1f});
    CHECK(rig.bodies[RightArm].image == "body.png");

    auto encoded = EncodeAssets(assets);
    CHECK(encoded["rigs"][0]["bodies"][Torso]["image"] == "body.png");
    GameAssets decoded;
    CHECK(DecodeAssets(encoded, decoded, error));
    CHECK(decoded.rigs.size() == 1);
    CHECK(decoded.rigs[0].bodies[Head].image == "body.png");
    CHECK(decoded.rigs[0].bodies[RightLeg].image == "body.png");

    auto cleared = decoded;
    cleared.rigs[0].bodies[Torso].image.clear();
    const auto clearedJson = EncodeAssets(cleared);
    CHECK(!clearedJson["rigs"][0]["bodies"][Torso].contains("image"));

    auto invalid = assets;
    invalid.rigs[0].bodies[Torso].image = std::string(1025, 'x');
    CHECK(!invalid.validate(error));
    CHECK(MakeR6().bodies[Torso].image.empty());

    Scene project;
    project.buildDefaultScene();
    auto& projectRig = project.gameAssets().createR6();
    projectRig.root.position = {0, 6, 0};
    projectRig.bodies[Torso].image = "body.png";
    projectRig.bodies[Head].image = "body.png";
    const auto projectJson = projectio::toJson(project);
    Scene loaded;
    CHECK(projectio::fromJson(loaded, projectJson, error));
    CHECK(loaded.gameAssets().rigs[0].bodies[Torso].image == "body.png");
    CHECK(loaded.gameAssets().rigs[0].bodies[Head].image == "body.png");

    // ---- Rendered R6 skin ------------------------------------------------
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(512, 512, "Loblox R6 image regression");
    SetTargetFPS(60);
    CHECK(!BuiltinAssetPath("rosto1.png").empty());

    {
        Scene scene;
        scene.lighting()->setSkyEnabled(false);
        auto& renderRig = scene.gameAssets().createR6();
        renderRig.root.position = {0, 6, 0};
        for (auto& body : renderRig.bodies) body.image = "body.png";
        Camera3D camera{{0, 6, -12}, {0, 6, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
        SceneRenderer renderer;
        RenderTexture2D target = LoadRenderTexture(512, 512);
        CHECK(IsRenderTextureValid(target));

        auto render = [&](const Pose& pose) {
            RigRenderState state{&renderRig, pose, renderRig.root};
            BeginDrawing();
            BeginTextureMode(target);
            renderer.drawScene(scene, camera, 512, 512, kInvalidInstanceId, &state, false);
            EndTextureMode();
            EndDrawing();
            Image result = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&result);
            return result;
        };

        Pose pose{};
        auto front = render(pose);
        const auto neutral = renderRig.evaluate(pose, renderRig.root);
        const Vector2 torsoAt = GetWorldToScreen(neutral[Torso].position, camera);
        const Vector2 armAt = GetWorldToScreen(neutral[RightArm].position, camera);
        const Vector2 headAt = GetWorldToScreen(neutral[Head].position, camera);
        const Color torsoPixel = Sample(front, torsoAt);
        const Color armPixel = Sample(front, armAt);
        CHECK(Magenta(torsoPixel));
        CHECK(Magenta(armPixel));

        // The default face art must stay in front of the head image. The
        // centre of rosto1.png is transparent, so count the black line art
        // instead of expecting an opaque centre pixel. Repeating the render
        // catches depth-fighting flicker between image and face.
        const int faceInk = CountFaceInk(front, headAt);
        CHECK(faceInk > 50);
        for (int pass = 0; pass < 2; ++pass) {
            auto repeat = render(pose);
            CHECK(std::abs(CountFaceInk(repeat, headAt) - faceInk) <= 8);
            UnloadImage(repeat);
        }
        // Just outside the face decal, the head image is still visible.
        const Vector3 headEdge = Vector3Add(neutral[Head].position,
                                            Vector3{0.95f, 0.0f, -0.52f});
        const Vector2 headEdgeAt = GetWorldToScreen(headEdge, camera);
        CHECK(Magenta(Sample(front, headEdgeAt)));
        ExportImage(front, (std::string(directory) + "/rig-image-front.png").c_str());
        UnloadImage(front);

        // Rotating the shoulder moves the limb and its image together. The
        // new centre is outside the old arm silhouette, so it is a useful
        // before/after probe for image animation following.
        Pose raised{};
        raised[RightShoulder].rotation = QuaternionFromEuler(0, 0, 1.25f);
        const auto raisedWorld = renderRig.evaluate(raised, renderRig.root);
        const Vector2 raisedAt = GetWorldToScreen(raisedWorld[RightArm].position, camera);
        auto beforeRaiseAtNewCentre = render(pose);
        const Color beforeColor = Sample(beforeRaiseAtNewCentre, raisedAt);
        UnloadImage(beforeRaiseAtNewCentre);
        auto afterRaise = render(raised);
        const Color afterColor = Sample(afterRaise, raisedAt);
        CHECK(!Magenta(beforeColor));
        CHECK(Magenta(afterColor));
        CHECK(Vector3Distance(raisedWorld[RightArm].position, neutral[RightArm].position) > .5f);
        ExportImage(afterRaise, (std::string(directory) + "/rig-image-animated.png").c_str());
        UnloadImage(afterRaise);

        // Removing a body image reveals the underlying body color again.
        renderRig.bodies[Torso].image.clear();
        renderRig.bodies[Head].image.clear();
        auto removed = render(pose);
        CHECK(!Magenta(Sample(removed, torsoAt)));
        CHECK(!Magenta(Sample(removed, headAt)));
        ExportImage(removed, (std::string(directory) + "/rig-image-removed.png").c_str());
        UnloadImage(removed);

        // A missing asset is non-fatal and falls back to the plain body.
        renderRig.bodies[Torso].image = "missing-body-image.png";
        auto missing = render(pose);
        CHECK(!Magenta(Sample(missing, torsoAt)));
        UnloadImage(missing);
        renderRig.bodies[Torso].image = "body.png";

        UnloadRenderTexture(target);
    }

    ClearImageTextures();
    CloseWindow();
    std::printf("R6 image: %d checks, %d failures; captures: %s\n",
                checks, failures, directory);
    return failures ? 1 : 0;
}

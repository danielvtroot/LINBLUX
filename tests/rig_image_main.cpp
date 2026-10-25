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
    Image backSource=GenImageColor(64,64,{25,230,210,255});
    CHECK(ExportImage(backSource,"assets/back.png"));UnloadImage(backSource);

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

    // Independent child identity, face and image survive the actual project
    // format. Erasing one child must not erase its sibling or reuse its ID.
    auto& frontDecal=projectRig.addDecal(Torso);
    frontDecal.name="DecalFront";frontDecal.surface.image="body.png";
    const auto frontId=frontDecal.id;
    auto& backDecal=projectRig.addDecal(Torso);
    backDecal.name="DecalBack";backDecal.surface.face=1;backDecal.surface.image="back.png";backDecal.surface.transparency=.25f;
    const auto backId=backDecal.id;
    auto& armDecal=projectRig.addDecal(LeftArm);
    armDecal.surface.face=2;armDecal.surface.image="back.png";
    CHECK(projectio::fromJson(loaded,projectio::toJson(project),error));
    auto& savedRig=loaded.gameAssets().rigs.front();
    CHECK(savedRig.bodies[Torso].decals.size()==2);
    CHECK(savedRig.findDecal(frontId)->name=="DecalFront");
    CHECK(savedRig.findDecal(backId)->surface.face==1);
    CHECK(savedRig.findDecal(backId)->surface.image=="back.png");
    CHECK(savedRig.findDecal(backId)->surface.transparency==.25f);
    CHECK(savedRig.bodies[LeftArm].decals.front().surface.face==2);
    savedRig.resizeBody(Torso,{2.5f,2.2f,1.2f});
    CHECK(savedRig.bodies[Torso].decals.size()==2);
    CHECK(savedRig.eraseDecal(frontId));CHECK(!savedRig.eraseDecal(frontId));
    CHECK(savedRig.findDecal(backId)!=nullptr);
    const auto replacement=savedRig.addDecal(Torso).id;CHECK(replacement!=frontId&&replacement!=backId);
    Scene reloaded;CHECK(projectio::fromJson(reloaded,projectio::toJson(loaded),error));
    CHECK(reloaded.gameAssets().rigs[0].findDecal(frontId)==nullptr);
    CHECK(reloaded.gameAssets().rigs[0].findDecal(backId)!=nullptr);
    auto badFace=EncodeAssets(loaded.gameAssets());badFace["rigs"][0]["bodies"][Torso]["decals"][0]["face"]=6;
    CHECK(!DecodeAssets(badFace,decoded,error));
    auto duplicate=loaded.gameAssets();duplicate.rigs[0].bodies[Head].decals.push_back(duplicate.rigs[0].bodies[Torso].decals.front());
    CHECK(!duplicate.validate(error));

    // ---- Rendered R6 skin ------------------------------------------------
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(512, 512, "LINBLUX R6 image regression");
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

        // Real renderer, two different images on opposing LOCAL faces. A
        // six-face paint loop would fail these front/back/side pixel probes.
        for(auto& body:renderRig.bodies)body.image.clear();
        auto& a=renderRig.addDecal(Torso);a.name="Front";a.surface.image="body.png";
        const auto renderedFrontId=a.id;
        auto& b=renderRig.addDecal(Torso);b.name="Back";b.surface.face=1;b.surface.image="back.png";
        auto cyan=[](Color c){return c.g>100&&c.b>95&&c.r<90;};
        auto frontOnly=render(pose);CHECK(Magenta(Sample(frontOnly,torsoAt)));
        ExportImage(frontOnly,(std::string(directory)+"/decals-front.png").c_str());UnloadImage(frontOnly);
        camera.position={0,6,12};
        const auto backAt=GetWorldToScreen(neutral[Torso].position,camera);
        auto backOnly=render(pose);CHECK(cyan(Sample(backOnly,backAt)));CHECK(!Magenta(Sample(backOnly,backAt)));
        ExportImage(backOnly,(std::string(directory)+"/decals-back.png").c_str());UnloadImage(backOnly);
        camera.position={-12,6,0};
        const auto sideAt=GetWorldToScreen(neutral[Torso].position,camera);
        auto side=render(pose);CHECK(!Magenta(Sample(side,sideAt)));CHECK(!cyan(Sample(side,sideAt)));UnloadImage(side);
        // A limb decal follows the evaluated shoulder, including rotation.
        camera.position={0,6,-12};
        auto& arm=renderRig.addDecal(RightArm);arm.surface.image="body.png";
        auto animated=render(raised);CHECK(Magenta(Sample(animated,raisedAt)));
        ExportImage(animated,(std::string(directory)+"/decals-animated.png").c_str());UnloadImage(animated);
        // Same-face layers compose in persistent child order with no depth
        // fighting. Last opaque child wins on repeated frames.
        auto& layer=renderRig.addDecal(Torso);layer.surface.image="back.png";
        const auto layerId=layer.id;
        for(int pass=0;pass<3;++pass){auto image=render(pose);CHECK(cyan(Sample(image,torsoAt)));UnloadImage(image);}
        renderRig.findDecal(layerId)->surface.transparency=1;
        auto transparent=render(pose);CHECK(Magenta(Sample(transparent,torsoAt)));UnloadImage(transparent);
        CHECK(renderRig.eraseDecal(layerId));CHECK(renderRig.eraseDecal(renderedFrontId));
        auto deleted=render(pose);CHECK(!Magenta(Sample(deleted,torsoAt)));UnloadImage(deleted);
        camera.position={0,6,12};auto kept=render(pose);CHECK(cyan(Sample(kept,backAt)));UnloadImage(kept);
        const Vector3 normals[]={{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
        for(int face=2;face<6;++face) {
            const int body=face==2?LeftArm:RightArm;
            auto& surface=renderRig.addDecal(body);surface.surface.face=face;surface.surface.image="back.png";
            const auto surfaceId=surface.id;
            camera.target=neutral[body].position;camera.up=face>=4?Vector3{0,0,1}:Vector3{0,1,0};
            camera.position=Vector3Add(camera.target,Vector3Scale(normals[face],12));
            auto selectedFace=render(pose);CHECK(cyan(Sample(selectedFace,{256,256})));UnloadImage(selectedFace);
            camera.position=Vector3Subtract(camera.target,Vector3Scale(normals[face],12));
            auto oppositeFace=render(pose);CHECK(!cyan(Sample(oppositeFace,{256,256})));UnloadImage(oppositeFace);
            CHECK(renderRig.eraseDecal(surfaceId));
        }

        UnloadRenderTexture(target);
    }

    ClearImageTextures();
    CloseWindow();
    std::printf("R6 image: %d checks, %d failures; captures: %s\n",
                checks, failures, directory);
    return failures ? 1 : 0;
}

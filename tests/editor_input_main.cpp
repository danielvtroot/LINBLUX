#include "ui/Localization.h"
// Real X11 events -> GLFW/raylib -> rlImGui -> EditorUI -> camera/gizmo/Bullet.
// Run in a private Xvfb display, never against the user's desktop.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <chrono>
#include <limits>
#include <vector>
#include <unistd.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "editor/EditorUI.h"
#include "editor/GizmoLayout.h"
#include "physics/PhysicsWorld.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"
#include "core/Diagnostics.h"
#include "serialization/ProjectIO.h"

namespace loblox {
struct EditorUITestAccess {
    static bool dragging(EditorUI& ui) { return ui.transform_.active(); }
    static void tool(EditorUI& ui, ToolMode mode) { ui.setTool(mode); }
    static void modal(EditorUI& ui) { ui.requestOpenPopup_ = true; }
    static std::size_t undoCount(EditorUI& ui) { return ui.history_.undoCount(); }
    static bool readoutVisible(EditorUI& ui) { return ui.readout_.visible; }
    static bool readoutEditing(EditorUI& ui) { return ui.readout_.editing; }
    static float readoutValue(EditorUI& ui) { return ui.readout_.measurement.value; }
    static Rectangle readoutRect(EditorUI& ui) { return ui.readoutRect_; }
    static bool localSpace(EditorUI& ui) { return ui.gizmoLocalSpace_; }
};
}
using namespace loblox;
unsigned long InputTestRootWindow();
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); } } while (0)

bool Equivalent(const nlohmann::json& a, const nlohmann::json& b) {
    if (a.is_number() && b.is_number()) return std::fabs(a.get<double>() - b.get<double>()) < 0.0001;
    if (a.type() != b.type() || a.size() != b.size()) return false;
    if (a.is_object()) {
        for (auto it = a.begin(); it != a.end(); ++it)
            if (!b.contains(it.key()) || !Equivalent(it.value(), b[it.key()])) return false;
        return true;
    }
    if (a.is_array()) {
        for (std::size_t i = 0; i < a.size(); ++i) if (!Equivalent(a[i], b[i])) return false;
        return true;
    }
    return a == b;
}

int main() {
    char temporary[] = "/tmp/loblox-input-XXXXXX";
    const char* directory = mkdtemp(temporary);
    if (!directory) return 1;
    diag::configure(std::string(directory) + "/input.jsonl");
    setenv("LOBLOX_TRACE_INPUT", "1", 1);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 800, "LINBLUX input regression");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::GetStyle().WindowPadding = ImVec2(8, 7);
    {
        Scene scene;
        scene.buildDefaultScene();
        Part* part = scene.createPart(PartShape::Block, V3(0, 8, 0), V3(8, 2, 4), "InputFixture");
        part->setAnchored(false);
        scene.select(part);
        PhysicsWorld physics;
        physics.attach(scene);
        physics.setGravity(V3(0, 0, 0));
        EditorCamera camera;
        SceneRenderer renderer;
        AppConfig config;
        config.defaultProjectPath = std::string(directory) + "/scene.json";
        EditorUI ui(scene, physics, camera, renderer, config);
        const auto frames = [&](int count = 3) {
            for (int i = 0; i < count; ++i) {
                physics.simulate(scene, 1.0f / 60);
                BeginDrawing();
                ClearBackground(RAYWHITE);
                rlImGuiBegin();
                ui.draw();
                rlImGuiEnd();
                EndDrawing();
            }
        };
        const auto command = [&](const std::string& args) {
            CHECK(std::system(("xdotool " + args).c_str()) == 0);
            frames();
        };
        frames(5);
        std::vector<double> frameTimes;
        for (int i = 0; i < 60; ++i) {
            const auto started = std::chrono::steady_clock::now();
            frames(1);
            frameTimes.push_back(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count());
        }
        std::sort(frameTimes.begin(), frameTimes.end());
        std::printf("Idle fixture (1280x800, llvmpipe, cap 60): median %.2fms; p95 %.2fms\n",
                    frameTimes[30], frameTimes[57]);
        command("search --name '^LINBLUX input regression$' windowfocus --sync");
        camera.setOrbit(part->position(), 22, 0.75f, 0.4f);
        frames();
        const auto viewport = [&]() {
            ImGuiWindow* window = ImGui::FindWindowByName(loblox::i18n::Label("Viewport"));
            return Rectangle{window->InnerRect.Min.x, window->InnerRect.Min.y,
                window->InnerRect.GetWidth(), window->InnerRect.GetHeight()};
        };
        const auto mouse = [&](Vector2 point) {
            command("mousemove " + std::to_string(static_cast<int>(point.x)) + " " +
                    std::to_string(static_cast<int>(point.y)));
        };
        const auto handle = [&]() {
            const Rectangle rect = viewport();
            const auto layout = gizmolayout::Build(*part, ui.tool(), camera, rect, false);
            Vector2 point = layout.axisButton[0];
            return Vector2{rect.x + point.x, rect.y + point.y};
        };
        EditorUITestAccess::tool(ui, ToolMode::Move);
        frames();
        mouse(handle());
        const Vector3 beforeCapture = camera.position();
        const Vector3 beforeLookTarget = camera.target();
        command("mousedown 3");
        CHECK(camera.cursorCaptured()); // Hover must not deny legitimate RMB.
        CHECK(NearlyEqual(camera.position(), beforeCapture));
        command("mousemove_relative -- 12 7");
        CHECK(NearlyEqual(camera.position(), beforeCapture,.0001f));
        CHECK(Vector3Distance(camera.target(),beforeLookTarget)>.01f);
        command("mouseup 3");
        const Vector3 afterOrbit = camera.position();
        const Vector3 afterOrbitTarget = camera.target();
        frames(6);
        CHECK(NearlyEqual(camera.position(), afterOrbit));
        CHECK(NearlyEqual(camera.target(), afterOrbitTarget));

        scene.select(nullptr);
        mouse(Vector2{viewport().x+40,viewport().y+90});
        const auto emptyEye=camera.position();
        command("mousedown 3");command("mousemove_relative -- -19 14");command("mouseup 3");
        CHECK(NearlyEqual(camera.position(),emptyEye,.0001f));
        scene.select(part);
        camera.setNavigationMode(EditorCamera::NavigationMode::Focus);
        const auto focusEye=camera.position(),focusPivot=camera.target();
        command("mousedown 3");command("mousemove_relative -- 21 -11");command("mouseup 3");
        CHECK(Vector3Distance(camera.position(),focusEye)>.01f);
        CHECK(NearlyEqual(camera.target(),focusPivot));
        const auto orbitEye=camera.position();
        camera.setNavigationMode(EditorCamera::NavigationMode::FPS);
        CHECK(NearlyEqual(camera.position(),orbitEye,.0001f));

        // Losing focus while orbiting releases the captured cursor and must not
        // replay the warp delta into the camera when focus returns.
        const Rectangle focusRect = viewport();
        mouse(Vector2{focusRect.x + 35, focusRect.y + 90});
        command("mousedown 3");
        CHECK(camera.cursorCaptured());
        const Vector3 beforeCameraFocusLoss = camera.position();
        const unsigned long cameraRootWindow = InputTestRootWindow();
        CHECK(cameraRootWindow != 0);
        command("windowfocus " + std::to_string(cameraRootWindow));
        frames(4);
        CHECK(!camera.cursorCaptured());
        command("mouseup 3");
        command("search --name '^LINBLUX input regression$' windowfocus --sync");
        frames(4);
        CHECK(NearlyEqual(camera.position(), beforeCameraFocusLoss));

        const Vector3 beforePan = camera.target();
        const Vector3 beforePanOffset = Vector3Subtract(camera.position(), camera.target());
        command("mousedown 2");
        CHECK(camera.cursorCaptured());
        command("mousemove_relative -- 12 7");
        CHECK(Vector3Distance(camera.target(), beforePan) > 0.01f);
        CHECK(NearlyEqual(Vector3Subtract(camera.position(), camera.target()), beforePanOffset));
        command("mouseup 2");
        CHECK(!camera.cursorCaptured());
        const Vector3 afterPan = camera.position();
        const Vector3 afterPanTarget = camera.target();
        frames(6);
        CHECK(NearlyEqual(camera.position(), afterPan));
        CHECK(NearlyEqual(camera.target(), afterPanTarget));
        const Vector3 beforeFreeMove = camera.target();
        command("keydown w");
        command("keyup w");
        CHECK(Vector3Distance(camera.target(), beforeFreeMove) > 0.01f);
        CHECK(ui.tool() == ToolMode::Move);
        const Vector3 beforeUp = camera.target();
        command("keydown e");
        frames(4);
        command("keyup e");
        CHECK(camera.target().y > beforeUp.y + 0.01f);
        CHECK(std::fabs(camera.target().x - beforeUp.x) < 0.01f);
        CHECK(std::fabs(camera.target().z - beforeUp.z) < 0.01f);
        const Vector3 beforeDown = camera.target();
        command("keydown q");
        frames(4);
        command("keyup q");
        CHECK(camera.target().y < beforeDown.y - 0.01f);
        const float beforeZoom = camera.distance();
        command("click 4");
        CHECK(camera.distance() < beforeZoom);
        const Vector3 finitePosition = camera.position();
        const Vector3 finiteTarget = camera.target();
        const float finiteDistance = camera.distance();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        camera.rotateBy({nan, nan});
        camera.zoomBy(nan);
        camera.moveFree({nan, nan, nan}, 1.0f / 60.0f, false);
        camera.setOrbit({nan, nan, nan}, nan, nan, nan);
        CHECK(NearlyEqual(camera.position(), finitePosition));
        CHECK(NearlyEqual(camera.target(), finiteTarget));
        CHECK(NearlyEqual(camera.distance(), finiteDistance));
        camera.setOrbit(part->position(), 22, 0.75f, 0.4f);
        frames();

        Rectangle rect = viewport();
        mouse(Vector2{rect.x + 35, rect.y + 90});
        command("mousedown 3");
        CHECK(camera.cursorCaptured());
        command("mousedown 1");
        CHECK(scene.selection() == part);
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(camera.cursorCaptured());
        command("mouseup 1");
        command("keydown 4");
        command("keyup 4");
        CHECK(ui.tool() == ToolMode::Move); // Navigation owns keys until release.
        command("mouseup 3");

        // Ctrl+S is not a backwards camera movement.
        mouse(Vector2{rect.x + 35, rect.y + 90});
        command("click 3");
        const Vector3 target = camera.target();
        command("keydown ctrl keydown s");
        frames(5);
        command("keyup s keyup ctrl");
        CHECK(Vector3Distance(target, camera.target()) < 0.001f);

        // A docked Properties window is not the focused viewport.
        ImGui::FocusWindow(ImGui::FindWindowByName(loblox::i18n::Label("Properties")));
        frames();
        command("keydown 3");
        command("keyup 3");
        CHECK(ui.tool() == ToolMode::Move);
        const Vector3 blockedTarget = camera.target();
        command("keydown w");
        frames(5);
        command("keyup w");
        CHECK(NearlyEqual(camera.target(), blockedTarget));
        const float blockedDistance = camera.distance();
        command("keydown f");
        command("keyup f");
        frames(3);
        CHECK(NearlyEqual(camera.distance(), blockedDistance));
        CHECK(NearlyEqual(camera.target(), blockedTarget));

        // The same keys work only after the viewport itself owns focus.
        ImGui::FocusWindow(ImGui::FindWindowByName(loblox::i18n::Label("Viewport")));
        frames();
        const Vector3 beforeViewportW = camera.target();
        command("keydown w");
        frames(5);
        command("keyup w");
        CHECK(Vector3Distance(camera.target(), beforeViewportW) > 0.01f);
        camera.setOrbit(part->position(), 80, 0.75f, 0.4f);
        frames();
        const float beforeFocusFit = camera.distance();
        command("keydown f");
        command("keyup f");
        frames(3);
        CHECK(NearlyEqual(camera.target(), part->position()));
        CHECK(camera.distance() < beforeFocusFit);

        const auto key = [&](const char* name) {
            command(std::string("keydown ") + name);
            command(std::string("keyup ") + name);
        };
        const auto toolbar = [&](ToolMode mode, int index) {
            ImGuiWindow* window = ImGui::FindWindowByName(loblox::i18n::Label("##Toolbar"));
            // Use the translated captions and current font to locate the actual buttons.
            const char* captions[]={"Select","Move","Scale","Rotate"};float x=window->DC.CursorStartPos.x;
            for(int i=0;i<index;++i)x+=std::max(ImGui::CalcTextSize(loblox::i18n::Label(captions[i])).x+16.f,54.f)+ImGui::GetStyle().ItemSpacing.x;
            const float width=std::max(ImGui::CalcTextSize(loblox::i18n::Label(captions[index])).x+16.f,54.f);
            mouse({x+width*.5f,window->Pos.y+66});
            command("mousedown 1");
            command("mouseup 1");
            CHECK(ui.tool() == mode);
            CHECK(scene.selection() == part);
            TakeScreenshot((std::string(directory) + "/" + ToolModeName(mode) + ".png").c_str());
        };
        const auto layout = [&]() {
            return gizmolayout::Build(*part, ui.tool(), camera, viewport(), false);
        };
        const auto screen = [&](Vector2 point) {
            const Rectangle v = viewport();
            return Vector2{v.x + point.x, v.y + point.y};
        };
        const auto beginMove = [&]() {
            mouse(screen(layout().axisButton[0]));
            command("mousedown 1");
            CHECK(EditorUITestAccess::dragging(ui));
            CHECK(physics.isPartEditing(part->id()));
        };
        const loblox::Transform start = part->transform();
        toolbar(ToolMode::Move, 1);
        const Vector3 eye = camera.position();
        const Vector2 moveBegin = layout().axisButton[0];
        const Vector2 direction = layout().axisDirection[0];
        beginMove();
        mouse(screen(Vector2Add(moveBegin, Vector2Scale(direction, 45))));
        CHECK(Vector3Distance(part->position(), start.position) > 0.1f);
        CHECK(NearlyEqual(part->size(), start.size));
        CHECK(NearlyEqual(camera.position(), eye));
        command("mouseup 1");
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!physics.isPartEditing(part->id()));
        CHECK(NearlyEqual(physics.bodyPosition(part->id()), part->position()));
        const loblox::Transform moved = part->transform();

        toolbar(ToolMode::Scale, 2);
        const auto scaleLayout = layout();
        mouse(screen(scaleLayout.scaleHandle[1]));
        command("mousedown 1");
        CHECK(EditorUITestAccess::dragging(ui));
        mouse(screen(Vector2Add(scaleLayout.scaleHandle[1],
                                 Vector2Scale(scaleLayout.axisDirection[0], 40))));
        CHECK(std::fabs(part->size().x - moved.size.x) > 0.1f);
        CHECK(std::fabs(part->size().y - moved.size.y) < 0.001f);
        CHECK(std::fabs((part->position().x - part->size().x / 2) -
                         (moved.position.x - moved.size.x / 2)) < 0.001f);
        command("mouseup 1");
        const loblox::Transform scaled = part->transform();

        toolbar(ToolMode::Rotate, 3);
        const auto rotateLayout = layout();
        mouse(screen(rotateLayout.rotateMarker[1]));
        command("mousedown 1");
        CHECK(EditorUITestAccess::dragging(ui));
        mouse(screen(Vector2Add(rotateLayout.rotateMarker[1], Vector2{32, 28})));
        command("mouseup 1");
        CHECK(std::fabs(Vector4DotProduct(part->rotation(), scaled.rotation)) < 0.9999f);
        CHECK(NearlyEqual(part->position(), scaled.position));
        CHECK(NearlyEqual(part->size(), scaled.size));
        CHECK(NearlyEqual(camera.position(), eye));

        // Pose conhecida para medir studs/graus sem herdar o arraste anterior.
        part->setTransform(loblox::Transform{{0.0f, 6.0f, 0.0f}, {0, 0, 0, 1}, {4, 4, 4}});
        physics.applyChanges(part);
        camera.setOrbit(part->position(), 22, 0.75f, 0.4f);
        frames(2);

        // --- Readout numerico, valor exato e Undo/Redo de uma operacao ----
        const std::size_t undoBeforeMove = EditorUITestAccess::undoCount(ui);
        toolbar(ToolMode::Move, 1);
        const loblox::Transform readoutStart = part->transform();
        beginMove();
        mouse(screen(Vector2Add(layout().axisButton[0], Vector2{36, 12})));
        CHECK(EditorUITestAccess::readoutVisible(ui));
        CHECK(NearlyEqual(EditorUITestAccess::readoutValue(ui),
                          part->position().x - readoutStart.position.x, 0.02f));
        command("mouseup 1");
        frames(2);
        CHECK(EditorUITestAccess::readoutVisible(ui));
        CHECK(EditorUITestAccess::undoCount(ui) == undoBeforeMove + 1);
        const loblox::Transform dragged = part->transform();

        // Clicar no chip e digitar 4 studs aplica exatamente 4 a partir da
        // pose em que o arraste comecou (nao do frame anterior).
        Rectangle chip = EditorUITestAccess::readoutRect(ui);
        mouse({chip.x + chip.width * 0.5f, chip.y + chip.height * 0.5f});
        command("mousedown 1"); command("mouseup 1"); frames(2);
        CHECK(EditorUITestAccess::readoutEditing(ui));
        command("type 4"); frames(2);
        command("keydown Return"); command("keyup Return"); frames(2);
        CHECK(!EditorUITestAccess::readoutEditing(ui));
        CHECK(NearlyEqual(part->position().x, readoutStart.position.x + 4.0f, 0.002f));
        CHECK(NearlyEqual(part->position().y, readoutStart.position.y, 0.002f));
        CHECK(NearlyEqual(part->position().z, readoutStart.position.z, 0.002f));
        // Uma edicao numerica tambem e UMA operacao de Undo.
        command("keydown ctrl keydown z"); frames(2); command("keyup z keyup ctrl"); frames(2);
        CHECK(NearlyEqual(part->position(), dragged.position));
        command("keydown ctrl keydown y"); frames(2); command("keyup y keyup ctrl"); frames(2);
        CHECK(NearlyEqual(part->position().x, readoutStart.position.x + 4.0f, 0.002f));
        // Dois Ctrl+Z desfazem a edicao e o arraste inteiro, em UMA operacao
        // cada (nunca uma por frame).
        command("keydown ctrl keydown z"); frames(2); command("keyup z keyup ctrl"); frames(2);
        CHECK(NearlyEqual(part->position(), dragged.position));
        command("keydown ctrl keydown z"); frames(2); command("keyup z keyup ctrl"); frames(2);
        CHECK(NearlyEqual(part->position(), readoutStart.position));
        CHECK(NearlyEqual(physics.bodyPosition(part->id()), readoutStart.position));
        CHECK(EditorUITestAccess::undoCount(ui) == undoBeforeMove);
        command("keydown ctrl keydown y"); frames(2); command("keyup y keyup ctrl"); frames(2);
        command("keydown ctrl keydown y"); frames(2); command("keyup y keyup ctrl"); frames(2);
        CHECK(NearlyEqual(part->position().x, readoutStart.position.x + 4.0f, 0.002f));
        // Tira o mouse do chip: atalhos e cliques na viewport voltam a valer.
        mouse(screen(Vector2{40.0f, 40.0f}));
        frames(1);

        // Scale com tamanho final exato e face oposta estavel.
        toolbar(ToolMode::Scale, 2);
        const loblox::Transform scaleStart = part->transform();
        const auto exactScaleLayout = layout();
        mouse(screen(exactScaleLayout.scaleHandle[1]));
        command("mousedown 1");
        mouse(screen(Vector2Add(exactScaleLayout.scaleHandle[1],
                                 Vector2Scale(exactScaleLayout.axisDirection[0], 22))));
        command("mouseup 1"); frames(2);
        chip = EditorUITestAccess::readoutRect(ui);
        mouse({chip.x + chip.width * 0.5f, chip.y + chip.height * 0.5f});
        command("mousedown 1"); command("mouseup 1"); frames(2);
        CHECK(EditorUITestAccess::readoutEditing(ui));
        command("type 8.5"); frames(2);
        command("keydown Return"); command("keyup Return"); frames(2);
        CHECK(NearlyEqual(part->size().x, 8.5f, 0.002f));
        CHECK(NearlyEqual(part->position().x - part->size().x * 0.5f,
                          scaleStart.position.x - scaleStart.size.x * 0.5f, 0.002f));
        CHECK(NearlyEqual(part->size().y, scaleStart.size.y));

        // Alt: escala centrada — centro parado, as duas faces se afastam.
        const loblox::Transform centeredStart = part->transform();
        const auto centeredLayout = layout();
        command("keydown alt");
        mouse(screen(centeredLayout.scaleHandle[1]));
        command("mousedown 1");
        mouse(screen(Vector2Add(centeredLayout.scaleHandle[1],
                                 Vector2Scale(centeredLayout.axisDirection[0], 18))));
        command("mouseup 1"); frames(2);
        command("keyup alt"); frames(1);
        CHECK(NearlyEqual(part->position(), centeredStart.position, 0.001f));
        CHECK(part->size().x > centeredStart.size.x + 0.1f);
        CHECK(NearlyEqual(part->size().y, centeredStart.size.y));

        // Rotate com angulo exato em graus.
        toolbar(ToolMode::Rotate, 3);
        const loblox::Transform rotateStart = part->transform();
        const auto exactRotateLayout = layout();
        mouse(screen(exactRotateLayout.rotateMarker[1]));
        command("mousedown 1");
        mouse(screen(Vector2Add(exactRotateLayout.rotateMarker[1], Vector2{26, 20})));
        command("mouseup 1"); frames(2);
        chip = EditorUITestAccess::readoutRect(ui);
        mouse({chip.x + chip.width * 0.5f, chip.y + chip.height * 0.5f});
        command("mousedown 1"); command("mouseup 1"); frames(2);
        CHECK(EditorUITestAccess::readoutEditing(ui));
        command("type 45"); frames(2);
        command("keydown Return"); command("keyup Return"); frames(2);
        CHECK(NearlyEqual(EditorUITestAccess::readoutValue(ui), 45.0f, 0.02f));
        CHECK(NearlyEqual(part->position(), rotateStart.position));
        CHECK(NearlyEqual(part->size(), rotateStart.size));

        // World/Local alterna a matematica de verdade (Ctrl+L).
        const bool wasLocal = EditorUITestAccess::localSpace(ui);
        command("keydown ctrl keydown l"); frames(2); command("keyup l keyup ctrl"); frames(2);
        CHECK(EditorUITestAccess::localSpace(ui) == !wasLocal);
        CHECK(!EditorUITestAccess::readoutVisible(ui));
        command("keydown ctrl keydown l"); frames(2); command("keyup l keyup ctrl"); frames(2);
        CHECK(EditorUITestAccess::localSpace(ui) == wasLocal);

        // Local space move segue o eixo da Part (girada 90 graus em Y), com
        // Snap de 1 stud ativo: o eixo local X aponta para -Z do mundo.
        command("keydown ctrl keydown l"); frames(2); command("keyup l keyup ctrl"); frames(2);
        CHECK(EditorUITestAccess::localSpace(ui));
        part->setRotation(QuaternionFromAxisAngle({0, 1, 0}, 90.0f * DEG2RAD));
        physics.applyChanges(part);
        frames(2);
        toolbar(ToolMode::Move, 1);
        const loblox::Transform localStart = part->transform();
        const auto localLayout = gizmolayout::Build(*part, ui.tool(), camera, viewport(), true);
        mouse(screen(localLayout.axisButton[0]));
        command("mousedown 1");
        mouse(screen(Vector2Add(localLayout.axisButton[0],
                                 Vector2Scale(localLayout.axisDirection[0], 40))));
        command("mouseup 1"); frames(2);
        const Vector3 localDelta = Vector3Subtract(part->position(), localStart.position);
        CHECK(std::fabs(localDelta.z) > 0.5f);
        CHECK(std::fabs(localDelta.x) < 0.01f);
        CHECK(std::fabs(localDelta.y) < 0.01f);
        CHECK(std::fabs(localDelta.z / 1.0f - std::round(localDelta.z / 1.0f)) < 0.01f);
        command("keydown ctrl keydown l"); frames(2); command("keyup l keyup ctrl"); frames(2);
        CHECK(!EditorUITestAccess::localSpace(ui));

        toolbar(ToolMode::Select, 0);
        toolbar(ToolMode::Move, 1);
        const loblox::Transform beforeCancel = part->transform();
        beginMove();
        mouse(screen(Vector2Add(layout().axisButton[0], Vector2{40, 15})));
        key("Escape");
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(NearlyEqual(part->position(), beforeCancel.position));
        command("mouseup 1");
        beginMove();
        command("mouseup 1");
        CHECK(!EditorUITestAccess::dragging(ui));

        // Holding the current tool key past ImGui's repeat delay cannot cancel.
        command("keydown 2");
        beginMove();
        frames(35);
        CHECK(EditorUITestAccess::dragging(ui));
        command("mouseup 1");
        command("keyup 2");

        // A different tool cancels the existing session and restores its transform.
        beginMove();
        key("4");
        CHECK(ui.tool() == ToolMode::Rotate);
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!physics.isPartEditing(part->id()));
        command("mouseup 1");
        key("2");

        // Focus loss must cancel even if release happens on another window.
        beginMove();
        const unsigned long rootWindow = InputTestRootWindow();
        CHECK(rootWindow != 0);
        command("windowfocus " + std::to_string(rootWindow));
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!physics.isPartEditing(part->id()));
        command("mouseup 1");
        command("search --name '^LINBLUX input regression$' windowfocus --sync");
        beginMove();
        command("mouseup 1");

        // An actual shortcut must work after a viewport click (no stale focus).
        key("3");
        CHECK(ui.tool() == ToolMode::Scale);
        key("2");
        CHECK(ui.tool() == ToolMode::Move);

        // Opening a modal during drag cancels, and its keys cannot change tools.
        beginMove();
        command("keydown ctrl keydown o");
        command("keyup o keyup ctrl");
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(!camera.cursorCaptured());
        CHECK(ImGui::IsPopupOpen("Abrir projeto", ImGuiPopupFlags_AnyPopupId));
        key("4");
        CHECK(ui.tool() == ToolMode::Move);
        const auto modalPartCount = scene.parts().size();
        command("keydown ctrl keydown d");
        command("keyup d keyup ctrl");
        CHECK(scene.parts().size() == modalPartCount);
        command("mouseup 1");
        // Close through the real Cancel button in the modal.
        ImGuiWindow* modal = ImGui::FindWindowByName(loblox::i18n::Label("Abrir projeto"));
        CHECK(modal != nullptr);
        if (modal) {
            mouse({modal->Pos.x + 195, modal->Pos.y + modal->Size.y - 18});
            command("mousedown 1");
            command("mouseup 1");
        }
        CHECK(!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup));

        // Properties Name really receives typing; editor shortcuts stay blocked.
        ImGuiWindow* properties = ImGui::FindWindowByName(loblox::i18n::Label("Properties"));
        // Two-column Properties: Data/Name value, after title/search/category.
        mouse({properties->InnerRect.Min.x + properties->InnerRect.GetWidth()*.7f,
               properties->InnerRect.Min.y + 76});
        command("mousedown 1");
        command("mouseup 1");
        CHECK(ImGui::GetIO().WantTextInput);
        key("2");
        key("4");
        CHECK(ui.tool() == ToolMode::Move);
        const auto typingPartCount = scene.parts().size();
        command("keydown ctrl keydown d");
        command("keyup d keyup ctrl");
        CHECK(scene.parts().size() == typingPartCount);
        key("Return");
        CHECK(part->name().find("24") != std::string::npos);

        // Save using the editor, then load into a fresh scene with fresh Bullet.
        mouse({rect.x + 35, rect.y + 90});
        command("mousedown 3");
        command("mouseup 3");
        // Ctrl+D creates exactly one selected copy without moving the camera.
        // Newly inserted editor Parts are anchored. Use that normal workflow
        // here so Bullet does not separate overlapping dynamic bodies while
        // we hold the shortcut for several frames; dynamic flags are tested headlessly.
        part->setAnchored(true);
        physics.syncScene(scene);
        const auto beforeDuplicateCount = scene.parts().size();
        const Vector3 beforeDuplicateCamera = camera.target();
        command("keydown ctrl keydown d");
        frames(35); // Holding past the repeat delay must not make more copies.
        command("keyup d keyup ctrl");
        CHECK(scene.parts().size() == beforeDuplicateCount + 1);
        Instance* duplicate = scene.selection();
        CHECK(duplicate && duplicate->isPart() && duplicate != part);
        if (duplicate && duplicate->isPart() && duplicate != part) {
            Part* copy = static_cast<Part*>(duplicate);
            CHECK(copy->id() != part->id());
            CHECK(copy->name() != part->name());
            CHECK(NearlyEqual(copy->position(), part->position()));
            CHECK(NearlyEqual(copy->size(), part->size()));
            CHECK(NearlyEqual(copy->rotation(), part->rotation()));
            CHECK(copy->anchored() == part->anchored());
            CHECK(physics.hasBody(copy->id()));
            scene.remove(copy);
        }
        CHECK(NearlyEqual(camera.target(), beforeDuplicateCamera));
        CHECK(ui.tool() == ToolMode::Move);
        scene.select(part);
        part->setAnchored(false);
        physics.syncScene(scene);
        const std::string expected = projectio::toJson(scene);
        command("keydown ctrl keydown s");
        command("keyup s keyup ctrl");
        Scene reopened;
        std::string loadError;
        CHECK(projectio::load(reopened, config.defaultProjectPath, loadError));
        CHECK(Equivalent(nlohmann::json::parse(expected),
                         nlohmann::json::parse(projectio::toJson(reopened))));
        PhysicsWorld reopenedPhysics;
        reopenedPhysics.attach(reopened);
        CHECK(reopenedPhysics.validate(reopened, loadError));

        // Delete from a live drag must remove both Part and collider safely.
        const InstanceId removedId = part->id();
        beginMove();
        key("Delete");
        command("mouseup 1");
        CHECK(scene.findPart(removedId) == nullptr);
        CHECK(!physics.hasBody(removedId));
        CHECK(!EditorUITestAccess::dragging(ui));
        CHECK(scene.selection() == nullptr);
        CHECK(physics.validate(scene, loadError));
        TakeScreenshot((std::string(directory) + "/editor.png").c_str());

        std::string error;
        CHECK(scene.validate(error));
        CHECK(physics.validate(scene, error));
    }
    rlImGuiShutdown();
    CloseWindow();
    std::printf("Editor input: %d checks, %d failures; artifacts: %s\n", checks, failures, directory);
    std::fflush(stdout); // Keep functional results visible if LSan reports driver leaks at exit.
    return failures ? 1 : 0;
}

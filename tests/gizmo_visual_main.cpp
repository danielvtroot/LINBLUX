// Teste grafico de regressao dos gizmos (requer janela OpenGL).
//
// Renderiza a cena real + o passe de gizmo em uma RenderTexture2D e confere,
// pixel a pixel, que cada manipulador aparece exatamente onde o hit test o
// encontra. Cobre distancias, angulos, tamanhos e rotacao da Part, alem do
// near plane. Executado pelo CTest via xvfb-run quando disponivel; retorna 77
// quando nao ha contexto grafico.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "editor/Gizmo.h"
#include "editor/GizmoLayout.h"
#include "editor/GizmoMath.h"
#include "rendering/EditorCamera.h"
#include "rendering/SceneRenderer.h"
#include "scene/Scene.h"

using namespace loblox;

namespace {

int gChecks = 0;
int gFailures = 0;
int gCaseFailures = 0;
std::string gCase;

const Color kAxis[3] = {{235, 72, 66, 255}, {62, 174, 92, 255}, {55, 125, 225, 255}};
const char kAxisName[3] = {'X', 'Y', 'Z'};

void Check(bool condition, const std::string& what, int line) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        ++gCaseFailures;
        if (gCaseFailures <= 8) {
            std::printf("  FAIL [%s] linha %d: %s\n", gCase.c_str(), line, what.c_str());
        }
    }
}
#define CHECK(cond) Check((cond), #cond, __LINE__)

bool Nearly(Color a, Color b, int tolerance = 18) {
    return std::abs(a.r - b.r) <= tolerance && std::abs(a.g - b.g) <= tolerance &&
           std::abs(a.b - b.b) <= tolerance && a.a > 200;
}

Color PixelAt(const Image& image, Vector2 point) {
    const int x = static_cast<int>(std::lround(point.x));
    const int y = static_cast<int>(std::lround(point.y));
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return Color{0, 0, 0, 0};
    }
    return GetImageColor(image, x, y);
}

bool Inside(const Image& image, Vector2 point) {
    return point.x >= 0.0f && point.y >= 0.0f && point.x < static_cast<float>(image.width) &&
           point.y < static_cast<float>(image.height);
}

float DistanceToSegment(Vector2 p, Vector2 a, Vector2 b) {
    const Vector2 ab{b.x - a.x, b.y - a.y};
    const Vector2 ap{p.x - a.x, p.y - a.y};
    const float lengthSqr = ab.x * ab.x + ab.y * ab.y;
    const float t = lengthSqr > 1e-6f
                        ? std::clamp((ap.x * ab.x + ap.y * ab.y) / lengthSqr, 0.0f, 1.0f)
                        : 0.0f;
    const Vector2 closest{a.x + ab.x * t, a.y + ab.y * t};
    const float dx = p.x - closest.x;
    const float dy = p.y - closest.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool PointInRect(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.y >= r.y && p.x <= r.x + r.width && p.y <= r.y + r.height;
}

// Distancia livre entre um ponto do anel e os outros aneis/marcadores. Usada
// para escolher um ponto em que a cor do eixo nao seja coberta por outro
// controle desenhado depois (dois aneis sempre se cruzam em dois pontos).
float RingClearance(const gizmolayout::Layout& layout, int axis, Vector2 point) {
    float clearance = 1e9f;
    for (int other = 0; other < 3; ++other) {
        if (other == axis) continue;
        for (const gizmolayout::Segment& segment :
             layout.ring[static_cast<std::size_t>(other)]) {
            clearance = std::min(clearance, DistanceToSegment(point, segment.a, segment.b));
        }
        const Rectangle marker = gizmolayout::RotateMarkerRect(layout, other);
        if (PointInRect(point, marker)) return 0.0f;
        const Vector2 center{marker.x + marker.width * 0.5f,
                             marker.y + marker.height * 0.5f};
        const float dx = point.x - center.x;
        const float dy = point.y - center.y;
        clearance = std::min(clearance, std::sqrt(dx * dx + dy * dy) - 14.0f);
    }
    return clearance;
}

struct PartConfig {
    const char* label;
    Vector3 position;
    Vector3 size;
    Vector3 rotationDegrees;
};

struct ViewConfig {
    const char* label;
    float yaw;
    float pitch;
};

struct CaseResult {
    int checked = 0;
    int failed = 0;
};

// Verifica, para um layout: pixel do handle tem a cor do eixo e o picking no
// mesmo ponto devolve o mesmo HandleId que o desenho representa.
void VerifyMove(const gizmolayout::Layout& layout, const Image& image) {
    for (int axis = 0; axis < 3; ++axis) {
        const Rectangle rect = gizmolayout::AxisButtonRect(layout, axis);
        const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
        // Longe do rotulo e da borda arredondada (o circulo tem raio de 10 px).
        const Vector2 sample{center.x + 6.0f, center.y - 3.0f};
        ++gChecks;
        if (!Inside(image, sample) || !Nearly(PixelAt(image, sample), kAxis[axis])) {
            ++gFailures; ++gCaseFailures;
            const Color got = PixelAt(image, sample);
            std::printf("  FAIL [%s] botao %c em (%.0f,%.0f) cor=(%d,%d,%d)\n",
                        gCase.c_str(), kAxisName[axis], center.x, center.y, got.r, got.g,
                        got.b);
        }
        ++gChecks;
        if (gizmolayout::PickAxisButton(layout, center) != axis) {
            ++gFailures; ++gCaseFailures;
            std::printf("  FAIL [%s] botao %c nao e clicavel no centro desenhado\n",
                        gCase.c_str(), kAxisName[axis]);
        }
        // Ponto no meio da linha do eixo: desenho e hit test coincidem.
        const Vector2 middle{layout.center.x + (center.x - layout.center.x) * 0.55f,
                             layout.center.y + (center.y - layout.center.y) * 0.55f};
        if (gizmolayout::PickPlane(layout, middle) < 0) {
            ++gChecks;
            if (gizmolayout::PickAxisLine(layout, middle) != axis) {
                ++gFailures; ++gCaseFailures;
                std::printf("  FAIL [%s] linha %c nao e clicavel no meio\n", gCase.c_str(),
                            kAxisName[axis]);
            }
        }
    }
    for (int plane = 0; plane < 3; ++plane) {
        const Rectangle rect = gizmolayout::PlaneButtonRect(layout, plane);
        const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
        ++gChecks;
        if (gizmolayout::PickPlane(layout, center) != plane) {
            ++gFailures; ++gCaseFailures;
            std::printf("  FAIL [%s] plano %d nao e clicavel no centro\n", gCase.c_str(), plane);
        }
    }
}

void VerifyScale(const gizmolayout::Layout& layout, const Image& image) {
    for (int axis = 0; axis < 3; ++axis) {
        for (int side : {-1, 1}) {
            const int index = axis * 2 + (side > 0 ? 1 : 0);
            const Rectangle rect = gizmolayout::ScaleHandleRect(layout, index);
            const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            const Vector2 sample{center.x, center.y - 7.0f};
            ++gChecks;
            if (!Inside(image, sample) || !Nearly(PixelAt(image, sample), kAxis[axis])) {
                ++gFailures; ++gCaseFailures;
                const Color got = PixelAt(image, sample);
                std::printf("  FAIL [%s] alca %c%c em (%.0f,%.0f) cor=(%d,%d,%d)\n",
                            gCase.c_str(), kAxisName[axis], side > 0 ? '+' : '-', center.x,
                            center.y, got.r, got.g, got.b);
            }
            const gizmomath::HandleId expected =
                gizmomath::ScaleFaceHandleId(axis, side);
            ++gChecks;
            const gizmomath::HandleId picked = gizmolayout::PickScaleHandle(layout, center);
            if (picked != expected) {
                ++gFailures; ++gCaseFailures;
                std::printf("  FAIL [%s] alca %c%c nao e clicavel no centro desenhado\n",
                            gCase.c_str(), kAxisName[axis], side > 0 ? '+' : '-');
            }
        }
    }
}

void VerifyRotate(const gizmolayout::Layout& layout, const Image& image) {
    for (int axis = 0; axis < 3; ++axis) {
        const Rectangle rect = gizmolayout::RotateMarkerRect(layout, axis);
        const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
        const Vector2 sample{center.x + 8.0f, center.y - 3.0f};
        ++gChecks;
        if (!Inside(image, sample) || !Nearly(PixelAt(image, sample), kAxis[axis])) {
            ++gFailures; ++gCaseFailures;
            const Color got = PixelAt(image, sample);
            std::printf("  FAIL [%s] marcador %c em (%.0f,%.0f) cor=(%d,%d,%d)\n",
                        gCase.c_str(), kAxisName[axis], center.x, center.y, got.r, got.g,
                        got.b);
        }
        ++gChecks;
        if (gizmolayout::PickRotateMarker(layout, center) != axis) {
            ++gFailures; ++gCaseFailures;
            std::printf("  FAIL [%s] marcador %c nao e clicavel no centro desenhado\n",
                        gCase.c_str(), kAxisName[axis]);
        }
        // Um segmento do anel com folga dos outros aneis e marcadores: o
        // ponto escolhido e o mesmo para desenho e hit test.
        const auto& segments = layout.ring[static_cast<std::size_t>(axis)];
        ++gChecks;
        if (segments.empty()) {
            ++gFailures; ++gCaseFailures;
            std::printf("  FAIL [%s] anel %c sem segmentos\n", gCase.c_str(),
                        kAxisName[axis]);
            continue;
        }
        Vector2 middle{};
        float bestClearance = -1.0f;
        for (const gizmolayout::Segment& segment : segments) {
            const Vector2 candidate{(segment.a.x + segment.b.x) * 0.5f,
                                    (segment.a.y + segment.b.y) * 0.5f};
            if (!Inside(image, candidate)) continue;
            const float clearance = RingClearance(layout, axis, candidate);
            if (clearance > bestClearance) {
                bestClearance = clearance;
                middle = candidate;
            }
        }
        if (bestClearance >= 6.0f) {
            ++gChecks;
            if (!Nearly(PixelAt(image, middle), kAxis[axis], 24)) {
                ++gFailures; ++gCaseFailures;
                const Color got = PixelAt(image, middle);
                std::printf("  FAIL [%s] anel %c meio=(%.0f,%.0f) cor=(%d,%d,%d)\n",
                            gCase.c_str(), kAxisName[axis], middle.x, middle.y, got.r, got.g,
                            got.b);
            }
            ++gChecks;
            if (gizmolayout::PickRotateRing(layout, middle) != axis) {
                ++gFailures; ++gCaseFailures;
                std::printf("  FAIL [%s] anel %c nao e clicavel no mesmo ponto\n",
                            gCase.c_str(), kAxisName[axis]);
            }
        }
    }
}

gizmomath::HandleId PickLikeEditor(const gizmolayout::Layout& layout, ToolMode mode,
                                   Vector2 mouse) {
    if (mode == ToolMode::Move) {
        const int plane = gizmolayout::PickPlane(layout, mouse);
        if (plane >= 0) {
            return plane == 0 ? gizmomath::HandleId::XYPlane
                 : plane == 1 ? gizmomath::HandleId::XZPlane : gizmomath::HandleId::YZPlane;
        }
        const int axis = gizmolayout::PickAxisButton(layout, mouse);
        if (axis >= 0) {
            return axis == 0 ? gizmomath::HandleId::XAxis
                 : axis == 1 ? gizmomath::HandleId::YAxis : gizmomath::HandleId::ZAxis;
        }
        const int lineAxis = gizmolayout::PickAxisLine(layout, mouse);
        if (lineAxis >= 0) {
            return lineAxis == 0 ? gizmomath::HandleId::XAxis
                 : lineAxis == 1 ? gizmomath::HandleId::YAxis : gizmomath::HandleId::ZAxis;
        }
        return gizmomath::HandleId::None;
    }
    if (mode == ToolMode::Rotate) {
        const int marker = gizmolayout::PickRotateMarker(layout, mouse);
        const int axis = marker >= 0 ? marker : gizmolayout::PickRotateRing(layout, mouse);
        if (axis < 0) return gizmomath::HandleId::None;
        return axis == 0 ? gizmomath::HandleId::RotateX
             : axis == 1 ? gizmomath::HandleId::RotateY : gizmomath::HandleId::RotateZ;
    }
    return gizmolayout::PickScaleHandle(layout, mouse);
}

void VerifyAgainstSceneObjects(const gizmolayout::Layout& layout, const Image& sceneImage,
                               ToolMode mode) {
    // O layout e usado tanto pelo desenho quanto pelo picking. Aqui conferimos
    // que o pixel onde a alca e desenhada nao e apenas a geometria da Part:
    // a alca precisa sobreviver ao depth buffer.
    const auto checkPoint = [&](Vector2 point, gizmomath::HandleId expected) {
        ++gChecks;
        if (!Inside(sceneImage, point)) {
            ++gFailures; ++gCaseFailures;
            std::printf("  FAIL [%s] handle fora da viewport\n", gCase.c_str());
        }
        ++gChecks;
        if (PickLikeEditor(layout, mode, point) != expected) {
            ++gFailures; ++gCaseFailures;
            std::printf("  FAIL [%s] picking divergente do desenho\n", gCase.c_str());
        }
    };
    if (mode == ToolMode::Scale) {
        for (int axis = 0; axis < 3; ++axis) {
            for (int side : {-1, 1}) {
                const int index = axis * 2 + (side > 0 ? 1 : 0);
                const Rectangle rect = gizmolayout::ScaleHandleRect(layout, index);
                checkPoint(Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f},
                           gizmomath::ScaleFaceHandleId(axis, side));
            }
        }
    } else if (mode == ToolMode::Move) {
        for (int axis = 0; axis < 3; ++axis) {
            const Rectangle rect = gizmolayout::AxisButtonRect(layout, axis);
            checkPoint(Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f},
                       axis == 0 ? gizmomath::HandleId::XAxis
                       : axis == 1 ? gizmomath::HandleId::YAxis
                                   : gizmomath::HandleId::ZAxis);
        }
    } else {
        for (int axis = 0; axis < 3; ++axis) {
            const Rectangle rect = gizmolayout::RotateMarkerRect(layout, axis);
            checkPoint(Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f},
                       axis == 0 ? gizmomath::HandleId::RotateX
                       : axis == 1 ? gizmomath::HandleId::RotateY
                                   : gizmomath::HandleId::RotateZ);
        }
    }
}

int CountAxisPixels(const Image& image, int axis) {
    int count = 0;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            if (Nearly(GetImageColor(image, x, y), kAxis[axis])) ++count;
        }
    }
    return count;
}

Image RenderCase(RenderTexture2D target, SceneRenderer& renderer, Scene& scene, Part& part,
                 Gizmo& gizmo, ToolMode mode, EditorCamera& camera, Vector3 targetPoint,
                 float distance, float yaw, float pitch) {
    camera.setOrbit(targetPoint, distance, yaw, pitch);
    BeginTextureMode(target);
    renderer.drawScene(scene, camera, target.texture.width, target.texture.height, part.id());
    gizmo.drawOverlay(part, mode, camera,
                      Rectangle{0.0f, 0.0f, static_cast<float>(target.texture.width),
                                static_cast<float>(target.texture.height)});
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    return image;
}

}  // namespace

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 620, "loblox gizmo visual tests");
    if (!IsWindowReady()) {
        std::printf("SKIP: sem contexto grafico (use xvfb-run)\n");
        return 77;
    }

    const int kWidth = 640;
    const int kHeight = 400;
    RenderTexture2D target = LoadRenderTexture(kWidth, kHeight);
    if (target.id == 0) {
        std::printf("SKIP: render texture indisponivel\n");
        CloseWindow();
        return 77;
    }

    Scene scene;
    Part* nearPart = scene.createPart(PartShape::Block, Vector3{0.0f, 4.0f, 0.0f},
                                      Vector3{4.0f, 4.0f, 4.0f}, "GizmoTest");
    Part* farPart = scene.createPart(PartShape::Block, Vector3{0.0f, 4.0f, -30.0f},
                                     Vector3{20.0f, 20.0f, 4.0f}, "Background");
    nearPart->setAnchored(true);
    nearPart->setColor(Color{200, 200, 200, 255});
    farPart->setAnchored(true);
    farPart->setColor(Color{30, 90, 30, 255});
    SceneRenderer renderer;
    Gizmo gizmo;
    EditorCamera camera;

    const ToolMode modes[3] = {ToolMode::Move, ToolMode::Scale, ToolMode::Rotate};
    const char* modeNames[3] = {"Move", "Scale", "Rotate"};
    const ViewConfig views[5] = {
        {"front", 0.0f, 0.0f},
        {"top", 0.6f, 1.2f},
        {"bottom", 0.6f, -1.2f},
        {"oblique", 0.75f, 0.48f},
        {"side", 1.57f, 0.05f},
    };
    const float distances[5] = {2.0f, 4.0f, 12.0f, 42.0f, 200.0f};
    const PartConfig parts[4] = {
        {"small", {0.0f, 3.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f}},
        {"medium", {0.0f, 4.0f, 0.0f}, {4.0f, 4.0f, 4.0f}, {0.0f, 0.0f, 0.0f}},
        {"large", {0.0f, 25.0f, 0.0f}, {40.0f, 40.0f, 40.0f}, {0.0f, 0.0f, 0.0f}},
        {"rotated", {0.0f, 4.0f, 0.0f}, {4.0f, 6.0f, 8.0f}, {23.0f, 37.0f, -16.0f}},
    };

    int casesRun = 0;
    for (int partIndex = 0; partIndex < 4; ++partIndex) {
        const PartConfig& config = parts[partIndex];
        nearPart->setTransform(loblox::Transform{config.position,
                                         QuatFromEulerDegrees(config.rotationDegrees),
                                         config.size});
        for (int modeIndex = 0; modeIndex < 3; ++modeIndex) {
            for (const ViewConfig& view : views) {
                for (float distance : distances) {
                    gCase = std::string(modeNames[modeIndex]) + "/" + config.label + "/" +
                            view.label + "/d" + std::to_string(static_cast<int>(distance));
                    gCaseFailures = 0;
                    const Image image = RenderCase(target, renderer, scene, *nearPart, gizmo,
                                                   modes[modeIndex], camera, config.position,
                                                   distance, view.yaw, view.pitch);
                    const gizmolayout::Layout layout = gizmolayout::Build(
                        *nearPart, modes[modeIndex], camera,
                        Rectangle{0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)},
                        false);
                    ++casesRun;
                    CHECK(layout.valid);
                    if (layout.valid) {
                        VerifyAgainstSceneObjects(layout, image, modes[modeIndex]);
                        if (modes[modeIndex] == ToolMode::Move) {
                            VerifyMove(layout, image);
                        } else if (modes[modeIndex] == ToolMode::Scale) {
                            VerifyScale(layout, image);
                        } else {
                            VerifyRotate(layout, image);
                        }
                        // Nenhum controle pode ser desenhado fora da viewport.
                        for (int i = 0; i < 6; ++i) {
                            if (modes[modeIndex] == ToolMode::Scale) {
                                const Rectangle rect = gizmolayout::ScaleHandleRect(layout, i);
                                ++gChecks;
                                if (rect.x < 0.0f || rect.y < 0.0f ||
                                    rect.x + rect.width > kWidth ||
                                    rect.y + rect.height > kHeight) {
                                    ++gFailures; ++gCaseFailures;
                                    std::printf("  FAIL [%s] alca %d fora da viewport\n",
                                                gCase.c_str(), i);
                                }
                            }
                        }
                        int visibleAxis = 0;
                        for (int axis = 0; axis < 3; ++axis) {
                            if (CountAxisPixels(image, axis) > 0) ++visibleAxis;
                        }
                        ++gChecks;
                        if (visibleAxis < 3) {
                            ++gFailures; ++gCaseFailures;
                            std::printf("  FAIL [%s] eixos sem pixels visiveis (%d/3)\n",
                                        gCase.c_str(), visibleAxis);
                        }
                    }
                    UnloadImage(image);
                }
            }
        }
    }

    // Cena normal continua com depth test: a Part da frente cobre a de tras.
    {
        gCase = "depth/cena";
        gCaseFailures = 0;
        camera.setOrbit(Vector3{0.0f, 4.0f, 0.0f}, 20.0f, 0.0f, 0.0f);
        BeginTextureMode(target);
        renderer.drawScene(scene, camera, kWidth, kHeight, kInvalidInstanceId);
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        const Color center = PixelAt(image, Vector2{kWidth * 0.5f, kHeight * 0.5f});
        ++gChecks;
        if (!Nearly(center, Color{200, 200, 200, 255}, 90)) {
            ++gFailures;
            std::printf("  FAIL [cena] cubo da frente nao ocluiu o de tras: (%d,%d,%d)\n",
                        center.r, center.g, center.b);
        }
        UnloadImage(image);
    }

    // Near plane: pivo a 0.02 e atras da camera nao podem gerar geometria.
    {
        const char* labels[2] = {"near-plane", "behind-camera"};
        for (int variant = 0; variant < 2; ++variant) {
            gCase = labels[variant];
            gCaseFailures = 0;
            nearPart->setTransform(loblox::Transform{Vector3{0.0f, 4.0f, 0.0f},
                                             Quaternion{0.0f, 0.0f, 0.0f, 1.0f},
                                             Vector3{4.0f, 4.0f, 4.0f}});
            const float yaw = 0.4f;
            const float pitch = 0.3f;
            const Vector3 direction{std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                                    std::cos(pitch) * std::cos(yaw)};
            const float offset = variant == 0 ? 2.02f : 2.5f;
            const Vector3 targetPoint =
                Vector3Subtract(nearPart->position(), Vector3Scale(direction, offset));
            camera.setOrbit(targetPoint, variant == 0 ? 2.02f : 2.0f, yaw, pitch);
            BeginTextureMode(target);
            renderer.drawScene(scene, camera, kWidth, kHeight, nearPart->id());
            gizmo.drawOverlay(*nearPart, ToolMode::Move, camera,
                              Rectangle{0, 0, (float)kWidth, (float)kHeight});
            gizmo.drawOverlay(*nearPart, ToolMode::Scale, camera,
                              Rectangle{0, 0, (float)kWidth, (float)kHeight});
            gizmo.drawOverlay(*nearPart, ToolMode::Rotate, camera,
                              Rectangle{0, 0, (float)kWidth, (float)kHeight});
            EndTextureMode();
            Image image = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&image);
            const gizmolayout::Layout layout = gizmolayout::Build(
                *nearPart, ToolMode::Move, camera,
                Rectangle{0, 0, (float)kWidth, (float)kHeight}, false);
            ++gChecks;
            if (layout.valid) {
                ++gFailures;
                std::printf("  FAIL [%s] layout deveria ser invalido (profundidade %.4f)\n",
                            labels[variant], camera.depthOf(nearPart->position()));
            }
            int colored = 0;
            for (int axis = 0; axis < 3; ++axis) colored += CountAxisPixels(image, axis);
            ++gChecks;
            if (colored != 0) {
                ++gFailures;
                std::printf("  FAIL [%s] %d pixels de gizmo desenhados\n", labels[variant],
                            colored);
            }
            UnloadImage(image);
        }
    }

    // Resolucao: o tamanho do gizmo acompanha a viewport e continua visivel.
    {
        gCase = "resolucao";
        gCaseFailures = 0;
        RenderTexture2D big = LoadRenderTexture(1280, 720);
        const float lengths[3] = {gizmolayout::PixelLength(Vector2{480.0f, 300.0f}),
                                  gizmolayout::PixelLength(Vector2{900.0f, 600.0f}),
                                  gizmolayout::PixelLength(Vector2{1280.0f, 720.0f})};
        ++gChecks;
        if (!(lengths[0] < lengths[1] && lengths[1] <= lengths[2])) {
            ++gFailures;
            std::printf("  FAIL [%s] tamanho nao acompanha a resolucao: %.1f %.1f %.1f\n",
                        gCase.c_str(), lengths[0], lengths[1], lengths[2]);
        }
        ++gChecks;
        if (lengths[0] < 60.0f - 0.01f || lengths[2] > 132.0f + 0.01f) {
            ++gFailures;
            std::printf("  FAIL [%s] tamanho fora dos limites: %.1f %.1f\n", gCase.c_str(),
                        lengths[0], lengths[2]);
        }
        nearPart->setTransform(loblox::Transform{Vector3{0.0f, 4.0f, 0.0f},
                                         Quaternion{0.0f, 0.0f, 0.0f, 1.0f},
                                         Vector3{4.0f, 4.0f, 4.0f}});
        camera.setOrbit(Vector3{0.0f, 4.0f, 0.0f}, 6.0f, 0.75f, 0.48f);
        BeginTextureMode(big);
        renderer.drawScene(scene, camera, 1280, 720, nearPart->id());
        gizmo.drawOverlay(*nearPart, ToolMode::Scale, camera,
                          Rectangle{0, 0, 1280, 720});
        EndTextureMode();
        Image image = LoadImageFromTexture(big.texture);
        ImageFlipVertical(&image);
        const gizmolayout::Layout layout = gizmolayout::Build(
            *nearPart, ToolMode::Scale, camera, Rectangle{0, 0, 1280, 720}, false);
        for (int axis = 0; axis < 3; ++axis) {
            for (int side : {-1, 1}) {
                const int index = axis * 2 + (side > 0 ? 1 : 0);
                const Rectangle rect = gizmolayout::ScaleHandleRect(layout, index);
                const Vector2 sample{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f + 7.0f};
                ++gChecks;
                if (!Nearly(PixelAt(image, sample), kAxis[axis])) {
                    ++gFailures;
                    std::printf("  FAIL [%s] alca %d x %c nao visivel\n", gCase.c_str(), index,
                                kAxisName[axis]);
                }
            }
        }
        UnloadImage(image);
        UnloadRenderTexture(big);
    }

    UnloadRenderTexture(target);
    CloseWindow();
    std::printf("\n%d casos renderizados, %d checagens, %d falhas\n", casesRun, gChecks,
                gFailures);
    if (gFailures != 0) {
        std::printf("RESULTADO: FALHOU\n");
        return 1;
    }
    std::printf("RESULTADO: OK\n");
    return 0;
}

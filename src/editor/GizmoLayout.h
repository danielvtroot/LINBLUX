#pragma once

#include <array>
#include <vector>

#include "core/Math.h"
#include "editor/GizmoMath.h"
#include "editor/Tool.h"
#include "objects/Part.h"

namespace loblox { class EditorCamera; }

// Geometria de tela do gizmo. Tudo o que aparece e clicavel vem daqui: o
// desenho (Gizmo::drawOverlay) e o hit test (Gizmo::update) consomem o MESMO
// Layout, entao nao existe icone desenhado em uma posicao e clicavel em outra.
//
// Precision Tools: compact geometry around the projected pivot. Face size
// never controls handle distance; no leaders run to offscreen world faces.
namespace loblox::gizmolayout {

struct Settings {
    bool showPlanes=false;
    // Tamanho visual: 70 px na referência, limitado a 64–80 px antes da escala
    // para continuar utilizavel em resolucoes pequenas ou grandes.
    float basePixelLength = 108.0f;
    float minPixelLength = 94.0f;
    float maxPixelLength = 118.0f;
    float referenceViewportMin = 600.0f;

    float axisButtonHalf = 10.0f;
    float planeButtonHalf = 9.0f;
    float ringRadiusFactor = 0.96f;
    float planeOffsetFactor = 0.32f;

    float rotateMarkerHalfWidth = 7.0f;
    float rotateMarkerHalfHeight = 7.0f;

    float scaleHandleHalfWidth = 6.0f;
    float scaleHandleHalfHeight = 6.0f;
    float scaleHandleSeparation = 24.0f;

    float viewportMargin = 8.0f;

    float axisLineTolerance = 5.0f;
    float ringTolerance = 5.0f;
    int ringSegments = 96;
};

struct Triangle {std::array<Vector3,3> world{};std::array<Vector2,3> screen{};float depth=0;};
struct Mesh {std::vector<Triangle> triangles;};
struct Face {
    bool visible=false;Vector3 center{},normal{},anchor{};Vector2 screen{};std::vector<Vector3> polygon;
    std::vector<Vector2> projected;
};
struct FacePick {int index=-1;Vector3 point{};float distance=INFINITY;};

struct Segment {
    Vector2 a{};
    Vector2 b{};
};

struct Layout {
    bool valid = false;  // pivo projetado a frente do near plane
    Vector2 center{};
    bool centerInViewport = false;
    float pixelLength = 0.0f;
    float ringRadiusPx = 0.0f;

    Camera3D camera{};Vector2 viewport{};Vector3 pivot{};bool sphere=false;float sphereRadius=0;
    std::array<Mesh,3> arrow{},shaft{},rotation{};
    std::array<Face,6> faces{};
    // Move
    std::array<Vector2, 3> axisDirection{};
    std::array<bool, 3> axisDirectionFallback{};
    std::array<Vector2, 3> axisButton{};
    // Axis * 2 + side retained for handle IDs; only positive arrows are visible.
    std::array<Vector2, 6> moveHandle{};
    std::array<bool, 6> moveVisible{};
    std::array<Vector2, 3> planeCenter{};
    std::array<std::array<Vector2,4>,3> planeCorners{};
    std::array<bool,3> planeVisible{};

    // Face anchors, not six pivot controls. Only the hovered/active face is drawn.
    std::array<Vector2, 6> scaleHandle{};

    // Rotate: anel amostrado em tela (trocado em runs quando um ponto cai
    // atras da camera) e os tres marcadores com id por eixo.
    std::array<std::vector<Segment>, 3> ring{};
    std::array<Vector2, 3> rotateMarker{};
};

// Tamanho do gizmo em pixels para a viewport atual.
float PixelLength(Vector2 viewportSize, const Settings& settings = {});
// Snapshot usado pela matematica de escala (Sphere usa diametro uniforme).
gizmomath::TransformSnapshot MakeSnapshot(const Part& part);
// Layout completo da ferramenta. Funcao pura: mesmos argumentos, mesmo layout.
Layout Build(const Part& part, ToolMode mode, const EditorCamera& camera, Rectangle viewport,
             bool localSpace, const Settings& settings = {});

// Retangulos exatos usados no desenho e no picking.
Rectangle AxisButtonRect(const Layout& layout, int axis, const Settings& settings = {});
Rectangle PlaneButtonRect(const Layout& layout, int plane, const Settings& settings = {});
Rectangle RotateMarkerRect(const Layout& layout, int axis, const Settings& settings = {});
Rectangle ScaleHandleRect(const Layout& layout, int index, const Settings& settings = {});

FacePick FaceUnderCursor(const Layout& layout,Vector2 mouse);
Mesh FaceGrip(const Part& part,const EditorCamera& camera,Rectangle viewport,Vector3 point,Vector3 normal);
int PickMesh(const std::array<Mesh,3>& meshes,Vector2 mouse);

std::array<Vector2,3> MoveArrowCorners(const Layout& layout,int axis,const Settings& settings = {});

// Hit test. Prioridade e ordem ficam em Gizmo::update; aqui so geometria.
int PickPlane(const Layout& layout, Vector2 mouse, const Settings& settings = {});
int PickAxisButton(const Layout& layout, Vector2 mouse, const Settings& settings = {});
int PickAxisLine(const Layout& layout, Vector2 mouse, const Settings& settings = {});
gizmomath::HandleId PickMoveArrow(const Layout& layout, Vector2 mouse,
                                const Settings& settings = {});
gizmomath::HandleId PickMoveShaft(const Layout& layout, Vector2 mouse,
                                const Settings& settings = {});
int PickRotateMarker(const Layout& layout, Vector2 mouse, const Settings& settings = {});
int PickRotateRing(const Layout& layout, Vector2 mouse, const Settings& settings = {});
gizmomath::HandleId PickScaleHandle(const Layout& layout, Vector2 mouse,
                                   const Settings& settings = {});

}  // namespace loblox::gizmolayout

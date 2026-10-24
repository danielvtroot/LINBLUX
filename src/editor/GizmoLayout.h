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
// O overlay e screen-space por decisao de projeto:
//   - o tamanho em pixels nao depende da distancia da camera (com limites);
//   - pontos atras do near plane sao rejeitados (projectChecked), nunca
//     projetados como coordenadas espelhadas;
//   - alças que sairiam da viewport sao trazidas para dentro com linhas-guia,
//     de modo que Parts grandes ou cameras muito proximas nao as escondem.
namespace loblox::gizmolayout {

struct Settings {
    // Tamanho visual: 90 px em uma viewport de referencia de 600 px, limitado
    // para continuar utilizavel em resolucoes pequenas ou grandes.
    float basePixelLength = 90.0f;
    float minPixelLength = 60.0f;
    float maxPixelLength = 132.0f;
    float referenceViewportMin = 600.0f;

    float axisButtonHalf = 16.0f;
    float planeButtonHalf = 12.0f;
    float ringRadiusFactor = 0.78f;
    float planeOffsetFactor = 0.23f;

    float rotateMarkerHalfWidth = 13.0f;
    float rotateMarkerHalfHeight = 11.0f;
    // > diagonal do retangulo (26x22) para que dois marcadores nunca se cubram.
    float rotateMarkerSeparation = 36.0f;

    float scaleHandleHalfWidth = 17.0f;
    float scaleHandleHalfHeight = 17.0f;
    // Distancia do icone ate a face, medida em pixels.
    float scaleHandleOffsetPx = 15.0f;
    float scaleHandleSeparation = 35.0f;

    float viewportMargin = 8.0f;
    // Raio maximo (fracao do menor lado da viewport) para icones que
    // pertenceriam a uma face fora da tela.
    float maxHandleRadiusFactor = 0.46f;

    float axisLineTolerance = 8.0f;
    float ringTolerance = 8.0f;
    int ringSegments = 96;
};

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

    // Move
    std::array<Vector2, 3> axisDirection{};
    std::array<bool, 3> axisDirectionFallback{};
    std::array<Vector2, 3> axisButton{};
    // axis * 2 + (side > 0). Six arrows share one layout for draw/picking.
    std::array<Vector2, 6> moveHandle{};
    std::array<Vector2, 3> planeCenter{};
    std::array<std::array<Vector2,4>,3> planeCorners{};
    std::array<bool,3> planeVisible{};

    // Scale: seis alças (axis * 2 + lado), nunca uma repetindo a outra.
    std::array<Vector2, 6> scaleHandle{};
    std::array<Vector2, 6> scaleAnchor{};
    std::array<bool, 6> scaleAnchorValid{};

    // Rotate: anel amostrado em tela (trocado em runs quando um ponto cai
    // atras da camera) e os tres marcadores com id por eixo.
    std::array<std::vector<Segment>, 3> ring{};
    std::array<Vector2, 3> rotateMarker{};
    std::array<Vector2, 3> rotateAnchor{};
    std::array<bool, 3> rotateAnchorValid{};
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

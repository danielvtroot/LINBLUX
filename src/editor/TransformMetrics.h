#pragma once

#include <cstddef>

#include "core/Math.h"
#include "editor/GizmoMath.h"

namespace loblox::transformmetrics {

// O que o arraste esta medindo. O tipo e fixado no inicio do arraste e nao
// muda no meio dele, para que o texto, o snapping e a edicao numerica usem
// sempre a mesma grandeza.
enum class Kind { None, Move, MovePlane, Size, Angle };

// Espaco dos eixos: World usa as direcoes globais, Local usa as direcoes da
// propria Part. E o mesmo espaco que o Gizmo usa no picking.
enum class Space { World, Local };

// Contexto imutavel de um arraste: transformacao inicial e grandeza medida.
// Toda medida/edicao numerica e calculada a partir daqui, nunca acumulando
// sobre o frame anterior.
struct Context {
    Kind kind = Kind::None;
    int axis = -1;
    int side = 1;
    Space space = Space::World;
    // Escala centrada (Alt): o centro fica parado e as duas faces se afastam.
    bool centered = false;
    gizmomath::TransformSnapshot start{};
    bool valid() const { return kind != Kind::None && axis >= 0 && axis <= 2; }
};

struct Measurement {
    Kind kind = Kind::None;
    int axis = -1;
    int axisB = -1;      // segundo eixo de MovePlane
    float value = 0.0f;  // studs (Move/Size) ou graus (Angle)
    float valueB = 0.0f;
    bool valid = false;
    // Somente um eixo por vez pode ser digitado com precisao.
    bool editable() const { return valid && kind != Kind::MovePlane; }
};

// Direcao mundial do eixo medido, derivada da rotacao inicial.
Vector3 AxisVector(const Context& context);

// Componente local de tamanho afetada por uma escala. Em World e o eixo local
// mais alinhado com o eixo mundial escolhido, a mesma regra de GizmoMath.
int SizeComponent(const Context& context);

// Lado local efetivamente movido na escala: em World o eixo positivo pode
// apontar para a face local negativa.
int SizeLocalSide(const Context& context);

Measurement Measure(const Context& context, const gizmomath::TransformSnapshot& current);

// Aplica um valor exato (studs ou graus) sobre a transformacao inicial.
gizmomath::TransformSnapshot Apply(const Context& context, float value,
                                   float minimumSize = kMinPartSize);

// Formata a medida exatamente como o viewport mostra:
//   Move      -> "X  +3.50 studs"
//   MovePlane -> "X +2.00 . Z -1.00 studs"
//   Size      -> "Size X: 8.50 studs"
//   Angle     -> "Y  +45 deg"
// Graus usam "deg" (ASCII) para nao depender da fonte do overlay.
bool Format(const Measurement& measurement, char* out, std::size_t size);

}  // namespace loblox::transformmetrics

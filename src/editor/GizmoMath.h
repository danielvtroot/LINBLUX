#pragma once

#include <array>

#include "core/Math.h"

namespace loblox::gizmomath {

// Stable identity shared by rendering, hit testing and drag math. In
// particular, every unilateral scale face has its own id; no axis fallback
// is allowed when a face could not be picked.
enum class HandleId {
    None,
    XAxis, YAxis, ZAxis,
    XYPlane, XZPlane, YZPlane,
    XPositive, XNegative, YPositive, YNegative, ZPositive, ZNegative,
    RotateX, RotateY, RotateZ,
    XNegativeAxis, YNegativeAxis, ZNegativeAxis
};

int HandleAxis(HandleId id);
int HandleSide(HandleId id);
bool IsScaleFace(HandleId id);
HandleId MoveHandleId(int axisIndex, int side);
HandleId ScaleFaceHandleId(int axisIndex, int side);
HandleId PickScaleFaceHandle(const std::array<Vector2, 6>& centers, Vector2 mouse,
                             float halfWidth = 12.0f, float halfHeight = 10.0f);

struct TransformSnapshot {
    Vector3 position;
    Quaternion rotation;
    Vector3 size;
};

Vector3 Axis(int index, Quaternion rotation, bool local);
float SnapDelta(float delta, float increment);
Vector3 MoveAxis(const TransformSnapshot& start, Vector3 axis, float delta,
                 float snapIncrement);
Vector3 MovePlane(const TransformSnapshot& start, Vector3 axisA, Vector3 axisB,
                  Vector3 worldDelta, float snapIncrement);
TransformSnapshot ScaleFace(const TransformSnapshot& start, Vector3 axis, int side,
                            float delta, float snapIncrement,
                            float minimumSize = kMinPartSize);
TransformSnapshot ScaleFaceAlongAxis(const TransformSnapshot& start, int axisIndex, int side,
                                     bool localSpace, float delta, float snapIncrement,
                                     float minimumSize = kMinPartSize);
TransformSnapshot ScaleFaceForHandle(const TransformSnapshot& start, HandleId handle,
                                     bool localSpace, float delta, float snapIncrement,
                                     float minimumSize = kMinPartSize);
TransformSnapshot ScaleUniformFace(const TransformSnapshot& start, Vector3 axis, int side,
                                   float delta, float snapIncrement,
                                   float minimumSize = kMinPartSize);
// Escala centrada (Alt): as duas faces se afastam igualmente e o centro fica
// parado, em vez de manter a face oposta.
TransformSnapshot ScaleCenteredFace(const TransformSnapshot& start, int axisIndex, int side,
                                    bool localSpace, float delta, float snapIncrement,
                                    float minimumSize = kMinPartSize);
// Local/world position for one scale handle, including its outward pixel-sized offset.
Vector3 ScaleHandleCenter(const TransformSnapshot& start, int axisIndex, int side,
                          bool localSpace, float handleOffset);
float WrappedAngleDelta(float startAngle, float currentAngle);
// Afasta pontos projetados que coincidem na tela. Deterministico: desenho e
// hit test obtem exatamente as mesmas posicoes ao chamar com os mesmos dados.
void SeparateScreenPoints(Vector2* points, int count, float minimumDistance);
std::array<Vector2, 6> SeparateScreenHandles(const std::array<Vector2, 6>& input,
                                             float minimumDistance);
Quaternion Rotate(const TransformSnapshot& start, Vector3 axis, float radians,
                  float snapDegrees);

}  // namespace loblox::gizmomath

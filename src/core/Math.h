#pragma once

#include <algorithm>
#include <cmath>

#include "raylib.h"
#include "raymath.h"

namespace loblox {

// Menor dimensao aceita para uma peca (unidades de mundo).
constexpr float kMinPartSize = 0.2f;

inline bool NearlyEqual(float a, float b, float epsilon = 1e-4f) {
    return std::fabs(a - b) <= epsilon;
}

inline bool NearlyEqual(Vector3 a, Vector3 b, float epsilon = 1e-4f) {
    return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon) &&
           NearlyEqual(a.z, b.z, epsilon);
}

inline bool NearlyEqual(Quaternion a, Quaternion b, float epsilon = 1e-4f) {
    return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon) &&
           NearlyEqual(a.z, b.z, epsilon) && NearlyEqual(a.w, b.w, epsilon);
}

inline Vector3 V3(float x, float y, float z) {
    return Vector3{x, y, z};
}

inline Vector3 ClampPartSize(Vector3 size) {
    return Vector3{std::max(kMinPartSize, std::fabs(size.x)),
                   std::max(kMinPartSize, std::fabs(size.y)),
                   std::max(kMinPartSize, std::fabs(size.z))};
}

inline Vector3 HalfExtents(Vector3 size) {
    return Vector3Scale(size, 0.5f);
}

inline Quaternion QuatFromEulerDegrees(Vector3 degrees) {
    return QuaternionFromEuler(degrees.x * DEG2RAD, degrees.y * DEG2RAD,
                               degrees.z * DEG2RAD);
}

inline Vector3 EulerDegreesFromQuat(Quaternion rotation) {
    const Vector3 radians = QuaternionToEuler(rotation);
    return Vector3{radians.x * RAD2DEG, radians.y * RAD2DEG, radians.z * RAD2DEG};
}

// Ordem canonica para comparar/serializar rotacoes (q e -q sao iguais).
inline Quaternion CanonicalQuaternion(Quaternion q) {
    if (q.w < 0.0f) {
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
        q.w = -q.w;
    }
    return q;
}

inline Quaternion NegateQuaternion(Quaternion q) {
    return Quaternion{-q.x, -q.y, -q.z, -q.w};
}

inline float QuaternionDotProduct(Quaternion a, Quaternion b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline Quaternion UnitQuaternion(Quaternion q) {
    const float lengthSqr = QuaternionDotProduct(q, q);
    if (!std::isfinite(lengthSqr) || lengthSqr < 1e-12f) return Quaternion{0, 0, 0, 1};
    const float inverse = 1.0f / std::sqrt(lengthSqr);
    return Quaternion{q.x * inverse, q.y * inverse, q.z * inverse, q.w * inverse};
}

// SLERP de arco curto, deterministico: q e -q sao a mesma orientacao, entao o
// destino e alinhado ao hemisferio da origem antes da interpolacao. Sem esse
// alinhamento o sinal escolhido pelo armazenamento (gizmo, Euler, save) muda o
// caminho e a mesma animacao gira em sentidos diferentes.
inline Quaternion QuaternionSlerpShortest(Quaternion from, Quaternion to, float alpha) {
    Quaternion a = UnitQuaternion(from);
    Quaternion b = UnitQuaternion(to);
    if (QuaternionDotProduct(a, b) < 0.0f) b = NegateQuaternion(b);
    alpha = std::isfinite(alpha) ? std::clamp(alpha, 0.0f, 1.0f) : 0.0f;
    if (alpha <= 0.0f) return a;
    if (alpha >= 1.0f) return b;
    const float cosine = std::clamp(QuaternionDotProduct(a, b), -1.0f, 1.0f);
    if (cosine > 0.9995f) {
        return UnitQuaternion(Quaternion{a.x + (b.x - a.x) * alpha, a.y + (b.y - a.y) * alpha,
                                          a.z + (b.z - a.z) * alpha, a.w + (b.w - a.w) * alpha});
    }
    const float halfTheta = std::acos(cosine);
    const float sineHalfTheta = std::sqrt(std::max(0.0f, 1.0f - cosine * cosine));
    if (sineHalfTheta < 1e-6f) return a;
    const float ratioA = std::sin((1.0f - alpha) * halfTheta) / sineHalfTheta;
    const float ratioB = std::sin(alpha * halfTheta) / sineHalfTheta;
    return UnitQuaternion(Quaternion{a.x * ratioA + b.x * ratioB, a.y * ratioA + b.y * ratioB,
                                      a.z * ratioA + b.z * ratioB, a.w * ratioA + b.w * ratioB});
}

// Par Euler consistente para o editor de animacao: roll(X) * pitch(Y) * yaw(Z),
// ou seja R = Rz * Ry * Rx. As duas funcoes sao inversas uma da outra, ao
// contrario do par QuaternionFromEuler/QuaternionToEuler da raylib, que usa
// ordens diferentes e altera a orientacao quando um valor e redigitado.
inline Quaternion QuatFromEulerDegreesZYX(Vector3 degrees) {
    const Quaternion roll = QuaternionFromAxisAngle({1, 0, 0}, degrees.x * DEG2RAD);
    const Quaternion pitch = QuaternionFromAxisAngle({0, 1, 0}, degrees.y * DEG2RAD);
    const Quaternion yaw = QuaternionFromAxisAngle({0, 0, 1}, degrees.z * DEG2RAD);
    return UnitQuaternion(QuaternionMultiply(QuaternionMultiply(yaw, pitch), roll));
}

inline Vector3 EulerDegreesZYXFromQuat(Quaternion rotation) {
    const Quaternion q = UnitQuaternion(rotation);
    const float r00 = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    const float r01 = 2.0f * (q.x * q.y - q.w * q.z);
    const float r10 = 2.0f * (q.x * q.y + q.w * q.z);
    const float r11 = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    const float r20 = 2.0f * (q.x * q.z - q.w * q.y);
    const float r21 = 2.0f * (q.y * q.z + q.w * q.x);
    const float r22 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    const float pitch = std::asin(std::clamp(-r20, -1.0f, 1.0f));
    // Gimbal lock: roll and yaw are degenerate, so keep roll at zero and fold
    // the remaining rotation into yaw. The orientation is still preserved.
    const float roll = std::fabs(r20) < 0.99999f ? std::atan2(r21, r22) : 0.0f;
    const float yaw = std::fabs(r20) < 0.99999f ? std::atan2(r10, r00) : std::atan2(-r01, r11);
    return Vector3{roll * RAD2DEG, pitch * RAD2DEG, yaw * RAD2DEG};
}

}  // namespace loblox

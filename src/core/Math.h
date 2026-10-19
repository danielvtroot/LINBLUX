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

}  // namespace loblox

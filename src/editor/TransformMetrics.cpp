#include "editor/TransformMetrics.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/NumberFormat.h"
#include "raylib.h"
#include "raymath.h"

namespace loblox::transformmetrics {
namespace {

constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kDegToRad = 0.017453292519943295f;

const char* AxisLabel(int axis) {
    return axis == 0 ? "X" : axis == 1 ? "Y" : axis == 2 ? "Z" : "?";
}

float Component(const Vector3& value, int axis) {
    return axis == 0 ? value.x : axis == 1 ? value.y : value.z;
}

// Assinatura de exibicao: o sinal e explicito em Move/Angle (o usuario precisa
// saber a direcao), mas zero nunca aparece como "+0".
// Medidas usam sempre duas casas ("+3.50") para que o numero nao "pule" de
// largura durante o arraste. Zero nao recebe sinal.
void AppendSigned(char* out, std::size_t size, float value) {
    const bool nonzero = numfmt::DisplayValue(value) != 0.0f;
    std::snprintf(out, size, "%s%.2f", nonzero ? (value < 0.0f ? "-" : "+") : "",
                  static_cast<double>(std::fabs(value)));
}

}  // namespace

Vector3 AxisVector(const Context& context) {
    return gizmomath::Axis(context.axis, context.start.rotation,
                           context.space == Space::Local);
}

int SizeComponent(const Context& context) {
    if (context.axis < 0 || context.axis > 2) return 0;
    if (context.space == Space::Local) return context.axis;
    const Vector3 worldAxis = AxisVector(context);
    int component = 0;
    float alignment = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float candidate = Vector3DotProduct(
            worldAxis, gizmomath::Axis(i, context.start.rotation, true));
        if (std::fabs(candidate) > std::fabs(alignment)) {
            alignment = candidate;
            component = i;
        }
    }
    return component;
}

int SizeLocalSide(const Context& context) {
    const int side = context.side < 0 ? -1 : 1;
    if (context.space == Space::Local) return side;
    const int component = SizeComponent(context);
    const float alignment = Vector3DotProduct(
        AxisVector(context), gizmomath::Axis(component, context.start.rotation, true));
    return alignment < 0.0f ? -side : side;
}

Measurement Measure(const Context& context, const gizmomath::TransformSnapshot& current) {
    Measurement result;
    if (!context.valid()) return result;
    result.kind = context.kind;
    result.axis = context.axis;
    result.valid = true;
    switch (context.kind) {
        case Kind::Move: {
            const Vector3 delta = Vector3Subtract(current.position, context.start.position);
            result.value = Vector3DotProduct(delta, AxisVector(context));
            break;
        }
        case Kind::MovePlane: {
            // Planos: XY = 0, XZ = 1, YZ = 2 (mesma ordem de GizmoLayout).
            const int axes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
            const int plane = context.axis < 0 || context.axis > 2 ? 0 : context.axis;
            const Vector3 delta = Vector3Subtract(current.position, context.start.position);
            const Vector3 a = gizmomath::Axis(axes[plane][0], context.start.rotation,
                                             context.space == Space::Local);
            const Vector3 b = gizmomath::Axis(axes[plane][1], context.start.rotation,
                                             context.space == Space::Local);
            result.axis = axes[plane][0];
            result.axisB = axes[plane][1];
            result.value = Vector3DotProduct(delta, a);
            result.valueB = Vector3DotProduct(delta, b);
            break;
        }
        case Kind::Size:
            result.value = Component(current.size, SizeComponent(context));
            break;
        case Kind::Angle: {
            const Quaternion relative = QuaternionNormalize(QuaternionMultiply(
                current.rotation, QuaternionInvert(context.start.rotation)));
            const Quaternion canonical = CanonicalQuaternion(relative);
            const Vector3 axis = AxisVector(context);
            const float sine = Vector3DotProduct(
                Vector3{canonical.x, canonical.y, canonical.z}, axis);
            result.value = 2.0f * std::atan2(sine, canonical.w) * kRadToDeg;
            break;
        }
        case Kind::None:
            result.valid = false;
            break;
    }
    if (context.kind == Kind::MovePlane && !std::isfinite(result.valueB)) {
        result.valueB = 0.0f;
        result.valid = false;
    }
    if (!std::isfinite(result.value)) {
        result.value = 0.0f;
        result.valid = false;
    }
    return result;
}

gizmomath::TransformSnapshot Apply(const Context& context, float value,
                                   float minimumSize) {
    gizmomath::TransformSnapshot result = context.start;
    if (!context.valid() || !std::isfinite(value)) return result;
    switch (context.kind) {
        case Kind::Move:
            result.position = Vector3Add(context.start.position,
                                         Vector3Scale(AxisVector(context), value));
            break;
        case Kind::MovePlane:
            // Movimento em plano tem dois graus de liberdade; a edicao
            // numerica e unilateral e nao deve inventar o segundo.
            break;
        case Kind::Size: {
            const int component = SizeComponent(context);
            float& size = component == 0 ? result.size.x
                         : component == 1 ? result.size.y : result.size.z;
            const float previous = Component(context.start.size, component);
            size = std::max(std::max(minimumSize, kMinPartSize), value);
            const float delta = size - previous;
            if (!context.centered) {
                // Face oposta estavel: o centro anda metade do crescimento.
                result.position = Vector3Add(
                    context.start.position,
                    Vector3Scale(gizmomath::Axis(component, context.start.rotation, true),
                                 static_cast<float>(SizeLocalSide(context)) * delta * 0.5f));
            }
            // Centrada: posicao permanece a inicial e as duas faces se movem.
            break;
        }
        case Kind::Angle: {
            const Quaternion delta = QuaternionFromAxisAngle(
                Vector3Normalize(AxisVector(context)), value * kDegToRad);
            result.rotation = CanonicalQuaternion(QuaternionNormalize(
                QuaternionMultiply(delta, context.start.rotation)));
            break;
        }
        case Kind::None:
            break;
    }
    return result;
}

bool Format(const Measurement& measurement, char* out, std::size_t size) {
    if (out == nullptr || size == 0 || !measurement.valid) return false;
    char signedValue[40];
    switch (measurement.kind) {
        case Kind::Move:
            AppendSigned(signedValue, sizeof(signedValue), measurement.value);
            std::snprintf(out, size, "%s  %s studs", AxisLabel(measurement.axis), signedValue);
            return true;
        case Kind::MovePlane: {
            char second[40];
            AppendSigned(signedValue, sizeof(signedValue), measurement.value);
            AppendSigned(second, sizeof(second), measurement.valueB);
            std::snprintf(out, size, "%s %s | %s %s studs", AxisLabel(measurement.axis),
                          signedValue, AxisLabel(measurement.axisB), second);
            return true;
        }
        case Kind::Size: {
            std::snprintf(out, size, "Size %s: %.2f studs", AxisLabel(measurement.axis),
                          static_cast<double>(numfmt::DisplayValue(measurement.value)));
            return true;
        }
        case Kind::Angle:
            AppendSigned(signedValue, sizeof(signedValue), measurement.value);
            std::snprintf(out, size, "%s  %s deg", AxisLabel(measurement.axis), signedValue);
            return true;
        case Kind::None:
            break;
    }
    return false;
}

}  // namespace loblox::transformmetrics

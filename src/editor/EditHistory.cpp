#include "editor/EditHistory.h"

#include <cmath>

#include "raylib.h"
#include "raymath.h"

namespace loblox {

bool SameTransform(const Transform& a, const Transform& b) {
    if (!NearlyEqual(a.position, b.position, 1e-5f)) return false;
    if (!NearlyEqual(a.size, b.size, 1e-5f)) return false;
    // q e -q representam a mesma orientacao; comparar o produto escalar
    // evita registrar uma edicao inexistente so porque o sinal mudou.
    const float dot = a.rotation.x * b.rotation.x + a.rotation.y * b.rotation.y +
                      a.rotation.z * b.rotation.z + a.rotation.w * b.rotation.w;
    return std::fabs(dot) > 1.0f - 1e-5f;
}

bool EditHistory::record(InstanceId id, const Transform& before, const Transform& after) {
    if (id == kInvalidInstanceId || SameTransform(before, after)) return false;
    // Qualquer edicao nova invalida o ramo refeito.
    redo_.clear();
    undo_.push_back(TransformEdit{id, before, after});
    while (undo_.size() > kLimit) undo_.pop_front();
    return true;
}

bool EditHistory::undo(TransformEdit& out) {
    if (undo_.empty()) return false;
    out = undo_.back();
    undo_.pop_back();
    redo_.push_back(out);
    return true;
}

bool EditHistory::redo(TransformEdit& out) {
    if (redo_.empty()) return false;
    out = redo_.back();
    redo_.pop_back();
    undo_.push_back(out);
    return true;
}

void EditHistory::clear() {
    undo_.clear();
    redo_.clear();
}

}  // namespace loblox

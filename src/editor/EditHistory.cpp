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
    return record(TransformEdit{id,before,after});
}
bool EditHistory::record(TransformEdit edit) {
    if(!edit.id)return false;
    bool changed=edit.parts.empty()&&edit.pivots.empty()&&!SameTransform(edit.before,edit.after);
    for(const auto& part:edit.parts)changed|=!SameTransform(part.before,part.after);
    for(const auto& pivot:edit.pivots)changed|=pivot.beforeExplicit!=pivot.afterExplicit||
        !SameTransform({pivot.before.position,pivot.before.rotation,{pivot.before.scale,pivot.before.scale,pivot.before.scale}},
                       {pivot.after.position,pivot.after.rotation,{pivot.after.scale,pivot.after.scale,pivot.after.scale}});
    if(!changed)return false;
    // Qualquer edicao nova invalida o ramo refeito.
    redo_.clear();
    undo_.push_back(std::move(edit));
    while (undo_.size() > kLimit) undo_.pop_front();
    std::size_t transforms=0;for(const auto& entry:undo_)transforms+=std::max<std::size_t>(1,entry.parts.size()+entry.pivots.size());
    while(transforms>32768&&!undo_.empty()){transforms-=std::max<std::size_t>(1,undo_.front().parts.size()+undo_.front().pivots.size());undo_.pop_front();}
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

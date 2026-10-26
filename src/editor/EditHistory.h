#pragma once

#include <cstddef>
#include <deque>
#include <vector>

#include "core/Math.h"
#include "objects/Part.h"

namespace loblox {

// Uma entrada = um arraste completo (ou uma edicao numerica). O historico
// guarda o par antes/depois, nunca um passo por frame.
struct PartTransformEdit {
    InstanceId id = kInvalidInstanceId;
    Transform before{};
    Transform after{};
};
struct ModelPivotEdit {
    InstanceId id=0;
    ModelFrame before{},after{};
    bool beforeExplicit=false,afterExplicit=false;
};
struct TransformEdit : PartTransformEdit {
    TransformEdit()=default;
    TransformEdit(InstanceId value,const Transform& a,const Transform& b):PartTransformEdit{value,a,b}{}
    std::vector<PartTransformEdit> parts;
    std::vector<ModelPivotEdit> pivots;
};

class EditHistory {
public:
    static constexpr std::size_t kLimit = 128;

    // Ignora transformacoes iguais: clicar num handle sem arrastar nao cria
    // uma operacao fantasma. Retorna false quando nada foi registrado.
    bool record(InstanceId id, const Transform& before, const Transform& after);
    bool record(TransformEdit edit);
    bool undo(TransformEdit& out);
    bool redo(TransformEdit& out);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    std::size_t undoCount() const { return undo_.size(); }
    std::size_t redoCount() const { return redo_.size(); }
    void clear();

private:
    std::deque<TransformEdit> undo_;
    std::deque<TransformEdit> redo_;
};

bool SameTransform(const Transform& a, const Transform& b);

}  // namespace loblox

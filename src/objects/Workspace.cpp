#include "objects/Workspace.h"

#include <utility>

namespace loblox {

Workspace::Workspace(InstanceId id, std::string name)
    : Instance(id, std::move(name)) {
    // Servico raiz: pode ser renomeado, mas nao apagado.
    setDeletable(false);
}

}  // namespace loblox

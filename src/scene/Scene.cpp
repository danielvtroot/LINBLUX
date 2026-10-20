#include "scene/Scene.h"
#include "core/Diagnostics.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace loblox {

namespace {
constexpr InstanceId kFirstId = 1;

bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool Finite(Quaternion value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
           std::isfinite(value.w);
}
}  // namespace

Scene::Scene() {
    auto workspace = std::make_unique<Workspace>(allocateId());
    Workspace* workspacePointer = workspace.get();
    adopt(std::move(workspace), nullptr);
    workspace_ = workspacePointer;

    auto lighting = std::make_unique<Lighting>(allocateId(), "Lighting");
    Lighting* lightingPointer = lighting.get();
    adopt(std::move(lighting), nullptr);
    lighting_ = lightingPointer;
}

void Scene::buildDefaultScene() {
    if (workspace_ == nullptr || !workspace_->children().empty()) {
        return;
    }
    createBaseplate();
    createSpawnLocation();
}

bool Scene::isService(const Instance* object) const {
    return object == workspace_ || object == lighting_;
}

InstanceId Scene::allocateId() {
    while (byId_.find(nextId_) != byId_.end()) {
        ++nextId_;
    }
    return nextId_++;
}

void Scene::setNextId(InstanceId id) {
    nextId_ = std::max(nextId_, std::max(kFirstId, id));
}

bool Scene::validate(std::string& error) const {
    error.clear();
    if (workspace_ == nullptr || lighting_ == nullptr) {
        error = "cena sem Workspace ou Lighting";
        return false;
    }
    if (workspace_->instanceClass() != InstanceClass::Workspace ||
        lighting_->instanceClass() != InstanceClass::Lighting) {
        error = "servicos com classes invalidas";
        return false;
    }
    if (workspace_->parent() != nullptr || lighting_->parent() != nullptr) {
        error = "servicos devem estar na raiz";
        return false;
    }
    if (byId_.size() != objects_.size()) {
        error = "indice de ids inconsistente com os objetos";
        return false;
    }
    if (nextId_ == kInvalidInstanceId) {
        error = "nextId invalido";
        return false;
    }

    std::set<InstanceId> objectIds;
    for (const std::unique_ptr<Instance>& object : objects_) {
        if (!object) {
            error = "objeto nulo na cena";
            return false;
        }
        if (object->id() == kInvalidInstanceId || !objectIds.insert(object->id()).second) {
            error = "id invalido ou duplicado: " + std::to_string(object->id());
            return false;
        }
        const auto indexed = byId_.find(object->id());
        if (indexed == byId_.end() || indexed->second != object.get()) {
            error = "indice de id nao aponta para o objeto " + std::to_string(object->id());
            return false;
        }
        if (object->parent() != nullptr && object->parent() != workspace_ &&
            object->parent() != lighting_) {
            error = "objeto com pai invalido: " + std::to_string(object->id());
            return false;
        }
        if (object->isPart() && object->parent() != workspace_) {
            error = "Part fora do Workspace: " + std::to_string(object->id());
            return false;
        }
    }
    if (byId_.find(workspace_->id()) == byId_.end() ||
        byId_.find(lighting_->id()) == byId_.end()) {
        error = "servicos ausentes do indice de ids";
        return false;
    }
    if (workspace_->children().size() != parts_.size()) {
        error = "Workspace e lista de Parts divergem";
        return false;
    }
    if (!lighting_->children().empty()) {
        error = "Lighting nao pode conter Parts";
        return false;
    }

    std::set<InstanceId> partIds;
    for (const Part* part : parts_) {
        if (part == nullptr) {
            error = "Part nula na lista de Parts";
            return false;
        }
        if (!partIds.insert(part->id()).second) {
            error = "Part duplicada: " + std::to_string(part->id());
            return false;
        }
        const auto indexed = byId_.find(part->id());
        if (indexed == byId_.end() || indexed->second != part ||
            part->parent() != workspace_) {
            error = "Part orfa ou com pai invalido: " + std::to_string(part->id());
            return false;
        }
        const Transform& transform = part->transform();
        if (!Finite(transform.position) || !Finite(transform.size) ||
            !Finite(transform.rotation)) {
            error = "transform nao finito na Part " + std::to_string(part->id());
            return false;
        }
        if (transform.size.x < kMinPartSize || transform.size.y < kMinPartSize ||
            transform.size.z < kMinPartSize) {
            error = "tamanho abaixo do minimo na Part " + std::to_string(part->id());
            return false;
        }
        const float lengthSqr =
            transform.rotation.x * transform.rotation.x +
            transform.rotation.y * transform.rotation.y +
            transform.rotation.z * transform.rotation.z +
            transform.rotation.w * transform.rotation.w;
        if (std::fabs(lengthSqr - 1.0f) > 0.002f) {
            error = "quaternion nao normalizado na Part " + std::to_string(part->id());
            return false;
        }
    }
    if (selection_ != nullptr) {
        const auto selected = byId_.find(selection_->id());
        if (selected == byId_.end() || selected->second != selection_) {
            error = "selecao aponta para objeto inexistente";
            return false;
        }
    }
    return true;
}

bool Scene::adopt(std::unique_ptr<Instance> object, Instance* parent) {
    if (!object) {
        return false;
    }
    if (byId_.find(object->id()) != byId_.end()) {
        return false;
    }
    if (parent == nullptr) {
        if (object->instanceClass() != InstanceClass::Workspace &&
            object->instanceClass() != InstanceClass::Lighting) {
            return false;
        }
        if ((workspace_ != nullptr && object->instanceClass() == InstanceClass::Workspace) ||
            (lighting_ != nullptr && object->instanceClass() == InstanceClass::Lighting)) {
            return false;
        }
    } else if (parent != workspace_ && parent != lighting_) {
        return false;
    } else if (parent == lighting_ && object->isPart()) {
        return false;
    } else if (parent == workspace_ && !object->isPart()) {
        return false;
    }

    Instance* raw = object.get();
    if (parent != nullptr) {
        parent->attachChild(raw);
    }
    byId_[raw->id()] = raw;
    if (raw->isPart()) {
        parts_.push_back(static_cast<Part*>(raw));
    }
    objects_.push_back(std::move(object));
    setNextId(raw->id() + 1);
    return true;
}

Part* Scene::createPart(PartShape shape, Vector3 position, Vector3 size,
                        const std::string& name, InstanceId forcedId) {
    if (forcedId != kInvalidInstanceId && byId_.find(forcedId) != byId_.end()) {
        return nullptr;
    }
    const std::string defaultName = shape == PartShape::Block ? "Part" : "Sphere";
    const std::string requestedName = name.empty() ? defaultName : name;
    const std::string finalName =
        forcedId != kInvalidInstanceId ? requestedName : makeUniqueName(requestedName);
    auto part = std::make_unique<Part>(forcedId != kInvalidInstanceId ? forcedId : allocateId(),
                                       finalName, shape);
    part->setSize(size);
    part->setPosition(position);
    Part* raw = part.get();
    if (!adopt(std::move(part), workspace_)) {
        return nullptr;
    }
    raw->clearDirty();
    return raw;
}

Baseplate* Scene::createBaseplate(Vector3 position, Vector3 size, const std::string& name,
                                  InstanceId forcedId) {
    if (forcedId != kInvalidInstanceId && byId_.find(forcedId) != byId_.end()) {
        return nullptr;
    }
    const std::string requestedName = name.empty() ? "Baseplate" : name;
    const std::string finalName =
        forcedId != kInvalidInstanceId ? requestedName : makeUniqueName(requestedName);
    auto baseplate = std::make_unique<Baseplate>(
        forcedId != kInvalidInstanceId ? forcedId : allocateId(), finalName);
    baseplate->setSize(size);
    baseplate->setPosition(position);
    Baseplate* raw = baseplate.get();
    if (!adopt(std::move(baseplate), workspace_)) {
        return nullptr;
    }
    raw->clearDirty();
    return raw;
}

SpawnLocation* Scene::createSpawnLocation(Vector3 position, Vector3 size,
                                          const std::string& name, InstanceId forcedId) {
    if (forcedId != kInvalidInstanceId && byId_.find(forcedId) != byId_.end()) {
        return nullptr;
    }
    const std::string requestedName = name.empty() ? "SpawnLocation" : name;
    const std::string finalName =
        forcedId != kInvalidInstanceId ? requestedName : makeUniqueName(requestedName);
    auto spawn = std::make_unique<SpawnLocation>(
        forcedId != kInvalidInstanceId ? forcedId : allocateId(), finalName);
    spawn->setSize(size);
    spawn->setPosition(position);
    SpawnLocation* raw = spawn.get();
    if (!adopt(std::move(spawn), workspace_)) {
        return nullptr;
    }
    raw->clearDirty();
    return raw;
}

Part* Scene::duplicatePart(const Part& source) {
    if (findPart(source.id()) != &source) return nullptr;
    Part* copy = nullptr;
    switch (source.instanceClass()) {
        case InstanceClass::Part:
            copy = createPart(source.shape(), source.position(), source.size(), source.name());
            break;
        case InstanceClass::SpawnLocation:
            copy = createSpawnLocation(source.position(), source.size(), source.name());
            break;
        case InstanceClass::Baseplate:
            copy = createBaseplate(source.position(), source.size(), source.name());
            break;
        default: return nullptr;
    }
    if (copy) {
        copy->setShape(source.shape());
        copy->setTransform(source.transform());
        copy->setColor(source.color());
        copy->setMaterial(source.material());
        copy->setAnchored(source.anchored());
        copy->setCanCollide(source.canCollide());
        copy->setDeletable(source.deletable());
    }
    return copy;
}

bool Scene::canDeleteSubtree(const Instance* object) const {
    if (object == nullptr || isService(object) || !object->deletable()) {
        return false;
    }
    for (const Instance* child : object->children()) {
        if (!canDeleteSubtree(child)) {
            return false;
        }
    }
    return true;
}

void Scene::removeSubtree(Instance* object) {
    // Copia os filhos antes de qualquer destruicao.
    const std::vector<Instance*> children = object->children();
    for (Instance* child : children) {
        removeSubtree(child);
    }

    notifyRemoval(object);
    if (selection_ == object) {
        select(nullptr);
    }
    if (object->parent() != nullptr) {
        object->parent()->detachChild(object);
    }
    if (object->isPart()) {
        const auto part = static_cast<Part*>(object);
        parts_.erase(std::remove(parts_.begin(), parts_.end(), part), parts_.end());
    }
    byId_.erase(object->id());

    const auto it = std::find_if(objects_.begin(), objects_.end(),
                                 [object](const std::unique_ptr<Instance>& candidate) {
                                     return candidate.get() == object;
                                 });
    if (it != objects_.end()) {
        objects_.erase(it);
    }
}

bool Scene::remove(Instance* object) {
    if (!canDeleteSubtree(object) || byId_.find(object->id()) == byId_.end()) {
        return false;
    }
    removeSubtree(object);
    return true;
}

bool Scene::removeById(InstanceId id) {
    return remove(find(id));
}

void Scene::clearObjects() {
    if (workspace_ != nullptr) {
        const std::vector<Instance*> children = workspace_->children();
        for (Instance* child : children) {
            removeSubtree(child);
        }
    }
    if (lighting_ != nullptr) {
        const std::vector<Instance*> children = lighting_->children();
        for (Instance* child : children) {
            removeSubtree(child);
        }
    }
    selection_ = nullptr;
    rebuildParts();
}

void Scene::replaceContentsFrom(Scene& source) {
    if (&source == this) {
        return;
    }
    // A cena antiga ainda esta intacta e a nova ja foi validada. Avisar a
    // fisica antes da troca evita que ela mantenha corpos apontando para
    // objetos que serao destruidos junto com `source`.
    for (Part* part : parts_) {
        notifyRemoval(part);
    }
    selection_ = nullptr;
    source.selection_ = nullptr;
    objects_.swap(source.objects_);
    parts_.swap(source.parts_);
    byId_.swap(source.byId_);
    std::swap(workspace_, source.workspace_);
    std::swap(lighting_, source.lighting_);
    std::swap(nextId_, source.nextId_);
}

Instance* Scene::find(InstanceId id) const {
    const auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

Part* Scene::findPart(InstanceId id) const {
    Instance* object = find(id);
    return object != nullptr && object->isPart() ? static_cast<Part*>(object) : nullptr;
}

void Scene::select(Instance* object) {
    if (object != nullptr && (byId_.find(object->id()) == byId_.end() ||
                             byId_.at(object->id()) != object)) object = nullptr;
    if (object == selection_) return;
    diag::Event event;
    event.subsystem = "scene";
    event.name = "selection_change";
    event.entityId = object ? object->id() : kInvalidInstanceId;
    event.context = {{"previous", selection_ ? selection_->id() : kInvalidInstanceId},
                     {"selected", object ? object->id() : kInvalidInstanceId}};
    diag::log(event);
    selection_ = object;
}

std::string Scene::makeUniqueName(const std::string& base) const {
    const auto used = [this](const std::string& candidate) {
        return std::any_of(objects_.begin(), objects_.end(),
                           [&candidate](const std::unique_ptr<Instance>& object) {
                               return object->name() == candidate;
                           });
    };

    if (!used(base)) {
        return base;
    }
    for (int index = 2; index < 100000; ++index) {
        const std::string candidate = base + std::to_string(index);
        if (!used(candidate)) {
            return candidate;
        }
    }
    return base;
}

void Scene::addRemovalListener(RemovalListener listener) {
    if (listener) {
        removalListeners_.push_back(std::move(listener));
    }
}

void Scene::notifyRemoval(Instance* object) {
    for (const RemovalListener& listener : removalListeners_) {
        listener(object);
    }
}

void Scene::rebuildParts() {
    parts_.clear();
    for (const std::unique_ptr<Instance>& object : objects_) {
        if (object->isPart()) {
            parts_.push_back(static_cast<Part*>(object.get()));
        }
    }
}

}  // namespace loblox

#include "game/Navigation.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

#include "objects/Part.h"
#include "physics/PhysicsWorld.h"
#include "scene/Scene.h"

namespace loblox {
namespace {
constexpr float kInfinity = std::numeric_limits<float>::max();
}

void NavGrid::reset() {
    ready_ = false;
    columns_ = rows_ = 0;
    originX_ = originZ_ = 0;
    cells_.clear();
}

bool NavGrid::cellWalkable(int column, int row) const {
    if (column < 0 || row < 0 || column >= int(columns_) || row >= int(rows_)) return false;
    return cells_[std::size_t(row) * columns_ + std::size_t(column)].walkable != 0;
}

bool NavGrid::stepAllowed(int a, int b, bool diagonal) const {
    if (a < 0 || b < 0 || a >= int(cells_.size()) || b >= int(cells_.size())) return false;
    if (!cellWalkable(columnOf(a), rowOf(a)) || !cellWalkable(columnOf(b), rowOf(b))) return false;
    if (std::fabs(cellHeight(a) - cellHeight(b)) > config_.maxStep) return false;
    if (!diagonal) return true;
    // Nao cortar cantos: as duas celulas ortogonais precisam ser alcancaveis.
    const int ca = columnOf(a), ra = rowOf(a), cb = columnOf(b), rb = rowOf(b);
    const int sides[2] = {ra * int(columns_) + cb, rb * int(columns_) + ca};
    for (const int side : sides) {
        if (!cellWalkable(columnOf(side), rowOf(side))) return false;
        if (std::fabs(cellHeight(a) - cellHeight(side)) > config_.maxStep) return false;
    }
    return true;
}

Vector3 NavGrid::cellCenter(int index) const {
    return {originX_ + (float(columnOf(index)) + .5f) * config_.cellSize, cellHeight(index),
            originZ_ + (float(rowOf(index)) + .5f) * config_.cellSize};
}

float NavGrid::cellHeight(int index) const {
    if (index < 0 || index >= int(cells_.size())) return 0;
    return cells_[std::size_t(index)].height;
}

std::size_t NavGrid::walkableCells() const {
    std::size_t count = 0;
    for (const auto& cell : cells_) count += cell.walkable ? 1 : 0;
    return count;
}

bool NavGrid::build(const Scene& scene, const PhysicsWorld& physics, const Config& config) {
    reset();
    config_ = config;
    if (!(config_.cellSize > .2f) || !(config_.agentHeight > 0) ||
        !(config_.agentRadius >= 0) || !(config_.maxStep >= 0) || config_.maxCells == 0) {
        return false;
    }

    // Limites derivados das Parts solidas, nunca de uma constante fixa do mapa.
    bool any = false;
    float minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;
    for (const Part* part : scene.parts()) {
        if (part == nullptr || !part->canCollide()) continue;
        const Vector3 p = part->position();
        const Vector3 half = Vector3Scale(part->size(), .5f);
        const float lo[3] = {p.x - half.x, p.y - half.y, p.z - half.z};
        const float hi[3] = {p.x + half.x, p.y + half.y, p.z + half.z};
        if (!any) {
            minX = lo[0]; maxX = hi[0]; minY = lo[1]; maxY = hi[1]; minZ = lo[2]; maxZ = hi[2];
            any = true;
        } else {
            minX = std::min(minX, lo[0]); maxX = std::max(maxX, hi[0]);
            minY = std::min(minY, lo[1]); maxY = std::max(maxY, hi[1]);
            minZ = std::min(minZ, lo[2]); maxZ = std::max(maxZ, hi[2]);
        }
    }
    if (!any) return false;

    float spanX = maxX - minX, spanZ = maxZ - minZ;
    if (!(spanX > 0) || !(spanZ > 0)) return false;
    float cell = config_.cellSize;
    const double estimate = double(spanX / cell + 1) * double(spanZ / cell + 1);
    if (estimate > double(config_.maxCells)) {
        cell = std::max(1.0f, std::sqrt(double(spanX) * double(spanZ) / double(config_.maxCells)));
    }
    config_.cellSize = cell;
    columns_ = unsigned(std::floor(spanX / cell)) + 1;
    rows_ = unsigned(std::floor(spanZ / cell)) + 1;
    while (std::size_t(columns_) * rows_ > config_.maxCells) {
        if (columns_ >= rows_ && columns_ > 1) --columns_;
        else if (rows_ > 1) --rows_;
        else break;
    }
    if (columns_ == 0 || rows_ == 0) return false;
    originX_ = minX;
    originZ_ = minZ;
    cells_.assign(std::size_t(columns_) * rows_, Cell{});

    const float top = maxY + std::max(2.f, cell);
    const float bottom = minY - std::max(2.f, cell);
    const float down = top - bottom;
    const float probe = cell * .5f + config_.agentRadius * .5f;
    const float knee = std::min(config_.agentHeight * .35f, 1.6f);
    const Vector3 horizontal[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};

    for (unsigned row = 0; row < rows_; ++row) {
        for (unsigned column = 0; column < columns_; ++column) {
            const float x = originX_ + (float(column) + .5f) * cell;
            const float z = originZ_ + (float(row) + .5f) * cell;
            Part* hit = nullptr;
            Vector3 point{}, normal{};
            if (!physics.raycast({x, top, z}, {0, -1, 0}, down, &hit, &point, &normal, true) ||
                hit == nullptr || normal.y < config_.minNormalY) {
                continue;
            }
            // Altura livre: um teto baixo impede a passagem do corpo do NPC.
            Part* ceiling = nullptr;
            if (physics.raycast({x, point.y + .15f, z}, {0, 1, 0},
                                config_.agentHeight - .15f, &ceiling, nullptr, nullptr, true)) {
                continue;
            }
            // Folga lateral: impede rotas que raspam paredes ou quinas.
            bool blocked = false;
            for (const Vector3 direction : horizontal) {
                Part* wall = nullptr;
                if (physics.raycast({x, point.y + knee, z}, direction, probe, &wall,
                                    nullptr, nullptr, true)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) continue;
            auto& target = cells_[std::size_t(row) * columns_ + column];
            target.height = point.y;
            target.walkable = 1;
        }
    }
    ready_ = walkableCells() > 0;
    return ready_;
}

bool NavGrid::nearestCell(Vector3 point, int maxRadius, int& index) const {
    if (!ready_ || config_.cellSize <= 0) return false;
    const int baseColumn = int(std::floor((point.x - originX_) / config_.cellSize));
    const int baseRow = int(std::floor((point.z - originZ_) / config_.cellSize));
    int best = -1;
    float bestDistance = kInfinity;
    for (int radius = 0; radius <= maxRadius; ++radius) {
        for (int dr = -radius; dr <= radius; ++dr) {
            for (int dc = -radius; dc <= radius; ++dc) {
                if (std::max(std::abs(dr), std::abs(dc)) != radius) continue;
                const int column = baseColumn + dc, row = baseRow + dr;
                if (!cellWalkable(column, row)) continue;
                const int candidate = row * int(columns_) + column;
                const float distance = Vector3DistanceSqr(point, cellCenter(candidate));
                if (distance < bestDistance) { bestDistance = distance; best = candidate; }
            }
        }
        if (best >= 0) { index = best; return true; }
    }
    return false;
}

bool NavGrid::lineClear(int start, int end) const {
    if (start == end) return true;
    if (start < 0 || end < 0) return false;
    const Vector3 a = cellCenter(start), b = cellCenter(end);
    const float distance = Vector3Distance(a, b);
    const int steps = std::max(1, int(std::ceil(distance / std::max(.25f, config_.cellSize * .4f))));
    float previous = cellHeight(start);
    for (int i = 1; i <= steps; ++i) {
        const float t = float(i) / float(steps);
        const Vector3 sample{Vector3Lerp(a, b, t)};
        const int column = int(std::floor((sample.x - originX_) / config_.cellSize));
        const int row = int(std::floor((sample.z - originZ_) / config_.cellSize));
        if (!cellWalkable(column, row)) return false;
        const int index = row * int(columns_) + column;
        if (std::fabs(cellHeight(index) - previous) > config_.maxStep) return false;
        previous = cellHeight(index);
    }
    return true;
}

bool NavGrid::findPath(Vector3 from, Vector3 to, NavPath& path) const {
    path.reachable = false;
    path.points.clear();
    if (!ready_) return false;
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(from.z) ||
        !std::isfinite(to.x) || !std::isfinite(to.y) || !std::isfinite(to.z)) {
        return false;
    }
    int start = -1, goal = -1;
    if (!nearestCell(from, 3, start) || !nearestCell(to, 3, goal)) return false;
    path.reachable = true;
    if (start == goal) { path.points.push_back({to.x, cellHeight(goal), to.z}); return true; }

    const std::size_t total = cells_.size();
    std::vector<float> cost(total, kInfinity);
    std::vector<int> parent(total, -1);
    std::vector<unsigned char> closed(total, 0);
    using Entry = std::pair<float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    const auto heuristic = [&](int index) {
        const float dx = float(columnOf(index) - columnOf(goal));
        const float dz = float(rowOf(index) - rowOf(goal));
        const float low = std::min(std::fabs(dx), std::fabs(dz));
        const float high = std::max(std::fabs(dx), std::fabs(dz));
        return (high - low) + low * 1.41421356f;
    };
    cost[std::size_t(start)] = 0;
    open.emplace(heuristic(start), start);
    const std::size_t budget = total * 8 + 64;
    std::size_t expanded = 0;
    bool found = false;
    while (!open.empty()) {
        const int current = open.top().second;
        open.pop();
        if (closed[std::size_t(current)]) continue;
        closed[std::size_t(current)] = 1;
        if (current == goal) { found = true; break; }
        if (++expanded > budget) break;
        const int column = columnOf(current), row = rowOf(current);
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue;
                const int nextColumn = column + dc, nextRow = row + dr;
                if (!cellWalkable(nextColumn, nextRow)) continue;
                const int next = nextRow * int(columns_) + nextColumn;
                const bool diagonal = dr != 0 && dc != 0;
                if (!stepAllowed(current, next, diagonal)) continue;
                const float step = cost[std::size_t(current)] + (diagonal ? 1.41421356f : 1.f);
                if (step >= cost[std::size_t(next)]) continue;
                cost[std::size_t(next)] = step;
                parent[std::size_t(next)] = current;
                open.emplace(step + heuristic(next), next);
            }
        }
    }
    if (!found) { path.reachable = false; return false; }

    std::vector<int> raw;
    for (int index = goal; index >= 0; index = parent[std::size_t(index)]) {
        raw.push_back(index);
        if (index == start) break;
    }
    if (raw.empty() || raw.back() != start) { path.reachable = false; return false; }
    std::reverse(raw.begin(), raw.end());

    // Suavizacao: mantem apenas os waypoints necessarios para seguir o corredor.
    std::size_t anchor = 0;
    while (anchor + 1 < raw.size()) {
        std::size_t next = anchor + 1;
        for (std::size_t candidate = raw.size() - 1; candidate > anchor; --candidate) {
            if (lineClear(raw[anchor], raw[candidate])) { next = candidate; break; }
        }
        if (next == raw.size() - 1) {
            path.points.push_back({to.x, cellHeight(raw[next]), to.z});
        } else {
            path.points.push_back(cellCenter(raw[next]));
        }
        anchor = next;
    }
    if (path.points.empty()) path.points.push_back({to.x, cellHeight(goal), to.z});
    return true;
}

bool NavGrid::walkableAt(Vector3 point) const {
    int index = -1;
    return nearestCell(point, 0, index);
}

float NavGrid::surfaceAt(Vector3 point) const {
    int index = -1;
    return nearestCell(point, 0, index) ? cellHeight(index) : 0.f;
}

}  // namespace loblox

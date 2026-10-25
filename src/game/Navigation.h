#pragma once

#include <cstddef>
#include <vector>

#include "core/Math.h"

namespace loblox {

class Scene;
class PhysicsWorld;

// Caminho calculado pela grade. points nao inclui a posicao de partida; quando
// reachable e falso, points fica vazio.
struct NavPath {
    bool reachable = false;
    std::vector<Vector3> points;
};

// Grade de navegacao XZ amostrada a partir da fisica real. Cada celula guarda
// a altura do piso mais alto e so e caminhavel quando existe superficie
// solida, espaco livre na altura do agente e folga lateral suficiente.
//
// Limitacoes declaradas: nao ha navmesh triangulada, saltos entre plataformas,
// ligacao entre ilhas separadas nem desvio dinamico de outros agentes. A
// movimentacao continua sendo feita pelo corpo Bullet, que nunca atravessa
// paredes; a grade apenas decide por onde tentar passar.
class NavGrid {
public:
    struct Config {
        float cellSize = 2.5f;    // tamanho lateral desejado de cada celula
        float agentRadius = 1.0f; // folga lateral usada no teste de parede
        float agentHeight = 4.5f; // altura livre exigida sobre o piso
        float maxStep = 1.2f;     // degrau maximo entre celulas vizinhas
        float minNormalY = 0.7f;  // inclinacao maxima do piso (cosseno)
        unsigned maxCells = 4096; // limites de custo da grade
    };

    void reset();
    // Constroi a grade a partir das Parts solidas da cena. O PhysicsWorld deve
    // ja conter essas Parts; a chamada e sincrona e limitada por maxCells.
    bool build(const Scene& scene, const PhysicsWorld& physics, const Config& config);
    bool ready() const { return ready_; }

    unsigned columns() const { return columns_; }
    unsigned rows() const { return rows_; }
    unsigned cellCount() const { return columns_ * rows_; }
    float cellSize() const { return config_.cellSize; }
    std::size_t walkableCells() const;

    // A* 8-vizinhos com custo diagonal, sem cortar cantos. Retorna false sem
    // tocar em path quando a grade nao esta pronta.
    bool findPath(Vector3 from, Vector3 to, NavPath& path) const;

    bool walkableAt(Vector3 point) const;
    float surfaceAt(Vector3 point) const;

private:
    struct Cell {
        float height = 0;
        unsigned char walkable = 0;
    };
    bool cellWalkable(int column, int row) const;
    bool nearestCell(Vector3 point, int maxRadius, int& index) const;
    bool stepAllowed(int a, int b, bool diagonal) const;
    bool lineClear(int start, int end) const;
    Vector3 cellCenter(int index) const;
    float cellHeight(int index) const;
    int columnOf(int index) const { return columns_ ? index % int(columns_) : 0; }
    int rowOf(int index) const { return columns_ ? index / int(columns_) : 0; }

    Config config_{};
    bool ready_ = false;
    unsigned columns_ = 0, rows_ = 0;
    float originX_ = 0, originZ_ = 0;
    std::vector<Cell> cells_;
};

}  // namespace loblox

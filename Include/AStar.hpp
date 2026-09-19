#pragma once
#include "Grid.hpp"
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include <utility>

namespace path {

constexpr float SQRT2 = 1.41421356f;

// Heurística octile: exacta para movimiento en 8 direcciones,
// nunca sobreestima, así que A* sigue dando el camino óptimo.
inline float heuristic(sf::Vector2i a, sf::Vector2i b) {
    float dx = static_cast<float>(std::abs(a.x - b.x));
    float dy = static_cast<float>(std::abs(a.y - b.y));
    return (dx + dy) + (SQRT2 - 2.f) * std::min(dx, dy);
}

// Devuelve la lista de celdas desde start hasta goal (ambas incluidas).
// Vacío si no hay ruta.
inline std::vector<sf::Vector2i> astar(const Grid& g,
                                       sf::Vector2i start,
                                       sf::Vector2i goal,
                                       bool diagonales = true)
{
    std::vector<sf::Vector2i> ruta;
    if (!g.walkable(start.x, start.y) || !g.walkable(goal.x, goal.y)) return ruta;
    if (start == goal) { ruta.push_back(start); return ruta; }

    const int N = g.w * g.h;
    auto id = [&](int x, int y) { return y * g.w + x; };

    const float INF = std::numeric_limits<float>::infinity();
    std::vector<float> gScore(N, INF);   // costo real desde start
    std::vector<int>   padre(N, -1);     // para reconstruir la ruta
    std::vector<char>  cerrado(N, 0);

    using Nodo = std::pair<float, int>;  // (fScore, id)
    std::priority_queue<Nodo, std::vector<Nodo>, std::greater<Nodo>> abiertos;

    const int s = id(start.x, start.y);
    gScore[s] = 0.f;
    abiertos.push({ heuristic(start, goal), s });

    static const int DX[8] = { 1, -1, 0,  0,  1,  1, -1, -1 };
    static const int DY[8] = { 0,  0, 1, -1,  1, -1,  1, -1 };

    while (!abiertos.empty()) {
        const int actual = abiertos.top().second;
        abiertos.pop();
        if (cerrado[actual]) continue;   // duplicado obsoleto en la cola
        cerrado[actual] = 1;

        const int cx = actual % g.w;
        const int cy = actual / g.w;

        if (cx == goal.x && cy == goal.y) {
            for (int n = actual; n != -1; n = padre[n])
                ruta.push_back({ n % g.w, n / g.w });
            std::reverse(ruta.begin(), ruta.end());
            return ruta;
        }

        const int dirs = diagonales ? 8 : 4;
        for (int i = 0; i < dirs; ++i) {
            const int nx = cx + DX[i];
            const int ny = cy + DY[i];
            if (!g.walkable(nx, ny)) continue;

            // Evita que el personaje se cuele por la esquina entre dos obstáculos.
            if (i >= 4 && (!g.walkable(cx, ny) || !g.walkable(nx, cy))) continue;

            const int  vecino = id(nx, ny);
            if (cerrado[vecino]) continue;

            const float paso  = (i >= 4) ? SQRT2 : 1.f;
            const float nuevo = gScore[actual] + paso;

            if (nuevo < gScore[vecino]) {
                gScore[vecino] = nuevo;
                padre[vecino]  = actual;
                abiertos.push({ nuevo + heuristic({ nx, ny }, goal), vecino });
            }
        }
    }
    return ruta;   // sin camino
}

// Convierte la ruta de celdas a puntos en píxeles (centros de celda).
inline std::vector<sf::Vector2f> aMundo(const Grid& g,
                                        const std::vector<sf::Vector2i>& celdas)
{
    std::vector<sf::Vector2f> pts;
    pts.reserve(celdas.size());
    for (auto c : celdas) pts.push_back(g.toWorld(c));
    return pts;
}

} // namespace path

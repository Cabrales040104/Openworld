#pragma once
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cmath>
#include <random>

// Mapa de celdas. Guarda qué celdas son sólidas (obstáculos) y convierte
// entre coordenadas de mundo (píxeles) y coordenadas de celda.
struct Grid {
    int   w = 0;
    int   h = 0;
    float tile = 32.f;
    std::vector<char> solid;   // 1 = obstáculo, 0 = libre

    Grid() = default;
    Grid(int w_, int h_, float tile_)
        : w(w_), h(h_), tile(tile_), solid(static_cast<size_t>(w_ * h_), 0) {}

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < w && y < h;
    }

    bool walkable(int x, int y) const {
        return inBounds(x, y) && !solid[static_cast<size_t>(y) * w + x];
    }

    void setSolid(int x, int y, bool v) {
        if (inBounds(x, y)) solid[static_cast<size_t>(y) * w + x] = v ? 1 : 0;
    }

    void toggleSolid(int x, int y) {
        if (inBounds(x, y)) {
            auto i = static_cast<size_t>(y) * w + x;
            solid[i] = solid[i] ? 0 : 1;
        }
    }

    // Centro de la celda, en píxeles.
    sf::Vector2f toWorld(sf::Vector2i c) const {
        return { (c.x + 0.5f) * tile, (c.y + 0.5f) * tile };
    }

    sf::Vector2i toCell(sf::Vector2f p) const {
        return { static_cast<int>(std::floor(p.x / tile)),
                 static_cast<int>(std::floor(p.y / tile)) };
    }

    // Celda transitable al azar (para spawns y patrullas). Reintenta unas
    // pocas veces antes de rendirse y devolver (0,0).
    sf::Vector2i randomWalkableCell(std::mt19937& rng) const {
        std::uniform_int_distribution<int> dx(0, w - 1), dy(0, h - 1);
        for (int intentos = 0; intentos < 200; ++intentos) {
            int x = dx(rng), y = dy(rng);
            if (walkable(x, y)) return { x, y };
        }
        return { 0, 0 };
    }
};

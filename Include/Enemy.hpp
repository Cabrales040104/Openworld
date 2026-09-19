#pragma once
#include "Grid.hpp"
#include "AStar.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>

// Máquina de estados del enemigo:
//
//   Patrulla --(jugador entra al radio de detección)--> Persiguiendo
//   Persiguiendo --(jugador se aleja demasiado)--> Patrulla
//   Persiguiendo --(está en rango de golpe)--> Atacando
//   Atacando --(recibe daño)--> Herido --(pasa el aturdimiento)--> Persiguiendo
//   Cualquiera --(vida llega a 0)--> Muerto
//
// A* decide la ruta tanto en Patrulla (hacia un punto al azar) como en
// Persiguiendo (hacia la posición actual del jugador, recalculando cada
// pocos décimos de segundo porque el jugador se mueve).
class Enemy {
public:
    enum class Estado { Patrulla, Persiguiendo, Atacando, Herido, Muerto };

    Enemy(sf::Vector2f pos, std::mt19937& rng, const sf::Texture* textura = nullptr)
        : m_pos(pos), m_rng(&rng), m_textura(textura) {
        m_forma.setRadius(9.f);
        m_forma.setOrigin({ 9.f, 9.f });
        m_forma.setOutlineThickness(2.f);
        m_forma.setOutlineColor(sf::Color::White);
    }

    bool vivo() const { return m_estado != Estado::Muerto; }
    Estado estado() const { return m_estado; }
    sf::Vector2f pos() const { return m_pos; }
    int vida() const { return m_vida; }
    int danio() const { return m_danio; }

    sf::FloatRect hitbox() const {
        return { { m_pos.x - m_radioColision, m_pos.y - m_radioColision },
                 { m_radioColision * 2.f, m_radioColision * 2.f } };
    }

    void recibirDanio(int dmg) {
        if (!vivo()) return;
        m_vida -= dmg;
        if (m_vida <= 0) { m_vida = 0; cambiarA(Estado::Muerto); return; }
        cambiarA(Estado::Herido);
    }

    // Devuelve true el frame exacto en que conecta un golpe; quien llama
    // es responsable de aplicar el daño al jugador (el enemigo no conoce al jugador).
    bool update(float dt, const Grid& g, sf::Vector2f playerPos, bool playerVivo) {
        bool golpeo = false;
        float distJugador = distancia(m_pos, playerPos);

        switch (m_estado) {
            case Estado::Patrulla:
                patrullar(dt, g);
                if (playerVivo && distJugador < m_radioDeteccion) cambiarA(Estado::Persiguiendo);
                break;

            case Estado::Persiguiendo:
                if (!playerVivo || distJugador > m_radioPerdida) { cambiarA(Estado::Patrulla); break; }
                if (distJugador < m_radioAtaque) { cambiarA(Estado::Atacando); break; }
                perseguir(dt, g, playerPos);
                break;

            case Estado::Atacando:
                if (!playerVivo) { cambiarA(Estado::Patrulla); break; }
                if (distJugador > m_radioAtaque * 1.3f) { cambiarA(Estado::Persiguiendo); break; }
                if (m_cooldownAtaque <= 0.f) {
                    golpeo = true;
                    m_cooldownAtaque = m_cooldownAtaqueMax;
                }
                break;

            case Estado::Herido:
                if (m_tiempoEstado > 0.3f) cambiarA(Estado::Persiguiendo);
                break;

            case Estado::Muerto:
                break;
        }

        if (m_cooldownAtaque > 0.f) m_cooldownAtaque -= dt;
        m_tiempoEstado += dt;
        return golpeo;
    }

    void draw(sf::RenderTarget& rt) const {
        if (!vivo()) return;

        // Ruta pendiente, útil para depurar el A*.
        if (m_indice < m_ruta.size()) {
            sf::VertexArray linea(sf::PrimitiveType::LineStrip, m_ruta.size() - m_indice + 1);
            linea[0] = sf::Vertex{ m_pos, sf::Color(120, 180, 255, 120) };
            for (size_t i = m_indice; i < m_ruta.size(); ++i)
                linea[i - m_indice + 1] = sf::Vertex{ m_ruta[i], sf::Color(120, 180, 255, 120) };
            rt.draw(linea);
        }

        if (m_textura) {
            sf::Sprite auto_(*m_textura);
            auto tam = m_textura->getSize();
            auto_.setOrigin({ tam.x / 2.f, tam.y / 2.f });
            auto_.setScale({ m_anchoSprite / static_cast<float>(tam.x),
                             m_altoSprite / static_cast<float>(tam.y) });
            auto_.setRotation(sf::degrees(m_angulo));
            auto_.setPosition(m_pos);
            if (m_estado == Estado::Herido) auto_.setColor(sf::Color(255, 150, 150));
            rt.draw(auto_);
        } else {
            sf::CircleShape c = m_forma;
            if (m_estado == Estado::Herido) c.setFillColor(sf::Color::White);
            else if (m_estado == Estado::Persiguiendo || m_estado == Estado::Atacando)
                c.setFillColor(sf::Color(220, 60, 60));
            else c.setFillColor(sf::Color(70, 130, 220));
            c.setPosition(m_pos);
            rt.draw(c);
        }

        // Barra de vida.
        float ancho = 26.f, alto = 4.f;
        sf::RectangleShape fondoBarra({ ancho, alto });
        fondoBarra.setFillColor(sf::Color(40, 40, 40));
        fondoBarra.setPosition({ m_pos.x - ancho / 2.f, m_pos.y - 20.f });
        rt.draw(fondoBarra);

        sf::RectangleShape barra({ ancho * (m_vida / static_cast<float>(m_vidaMax)), alto });
        barra.setFillColor(sf::Color(220, 60, 60));
        barra.setPosition({ m_pos.x - ancho / 2.f, m_pos.y - 20.f });
        rt.draw(barra);
    }

private:
    static float distancia(sf::Vector2f a, sf::Vector2f b) {
        float dx = a.x - b.x, dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    void cambiarA(Estado e) {
        if (m_estado == e) return;
        m_estado = e;
        m_tiempoEstado = 0.f;   // punto único de cambio: aquí reiniciarías animaciones
        if (e == Estado::Muerto) { m_ruta.clear(); m_indice = 0; }
    }

    void patrullar(float dt, const Grid& g) {
        if (m_ruta.empty() || m_indice >= m_ruta.size()) {
            sf::Vector2i destino = g.randomWalkableCell(*m_rng);
            auto celdas = path::astar(g, g.toCell(m_pos), destino);
            if (celdas.size() >= 2) { m_ruta = path::aMundo(g, celdas); m_indice = 1; }
            return;
        }
        avanzarPorRuta(dt, g, m_velocidadPatrulla);
    }

    void perseguir(float dt, const Grid& g, sf::Vector2f playerPos) {
        m_tiempoRecalculo -= dt;
        if (m_ruta.empty() || m_indice >= m_ruta.size() || m_tiempoRecalculo <= 0.f) {
            auto celdas = path::astar(g, g.toCell(m_pos), g.toCell(playerPos));
            if (celdas.size() >= 2) { m_ruta = path::aMundo(g, celdas); m_indice = 1; }
            m_tiempoRecalculo = 0.4f;   // no recalcular cada frame, es caro
        }
        avanzarPorRuta(dt, g, m_velocidadPersecucion);
    }

    void avanzarPorRuta(float dt, const Grid& g, float velocidad) {
        if (m_indice >= m_ruta.size()) return;

        // Si pusieron un obstáculo nuevo encima de la ruta, se descarta
        // y el próximo ciclo de patrulla/persecución la recalcula.
        for (size_t i = m_indice; i < m_ruta.size(); ++i) {
            auto c = g.toCell(m_ruta[i]);
            if (!g.walkable(c.x, c.y)) { m_ruta.clear(); m_indice = 0; return; }
        }

        sf::Vector2f obj = m_ruta[m_indice];
        sf::Vector2f d = obj - m_pos;
        float dist = std::sqrt(d.x * d.x + d.y * d.y);
        if (dist < 2.f) { m_pos = obj; ++m_indice; return; }

        sf::Vector2f dir = { d.x / dist, d.y / dist };
        m_angulo = std::atan2(dir.y, dir.x) * 180.f / 3.14159265f;
        float avance = std::min(velocidad * dt, dist);
        m_pos += dir * avance;
    }

    sf::CircleShape m_forma;
    sf::Vector2f    m_pos;
    std::mt19937*   m_rng;

    const sf::Texture* m_textura;
    float m_angulo = 0.f;
    float m_anchoSprite = 30.f;
    float m_altoSprite  = 16.f;

    std::vector<sf::Vector2f> m_ruta;
    size_t m_indice = 0;
    float  m_tiempoRecalculo = 0.f;

    Estado m_estado = Estado::Patrulla;
    float  m_tiempoEstado = 0.f;

    int m_vidaMax = 60;
    int m_vida = 60;

    float m_radioDeteccion = 220.f;   // distancia a la que nota al jugador
    float m_radioPerdida   = 320.f;   // distancia a la que lo pierde (con histéresis)
    float m_radioAtaque    = 34.f;
    float m_radioColision  = 10.f;

    float m_velocidadPatrulla     = 60.f;
    float m_velocidadPersecucion  = 140.f;

    float m_cooldownAtaque    = 0.f;
    float m_cooldownAtaqueMax = 1.f;
    int   m_danio = 8;
};
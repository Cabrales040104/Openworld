#pragma once
#include "Grid.hpp"
#include "AStar.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

// Máquina de estados del jugador (ahora simple, porque el movimiento lo
// controla la persona con las flechas, no un algoritmo):
//
//   Idle <-> Caminando     (según si hay alguna flecha presionada)
//   Cualquiera --(vida llega a 0)--> Muerto  (dibuja el auto destruido)
//
// El "herido" es solo un destello visual con temporizador propio, no un
// estado formal: puede pasar mientras sigues manejando, así que forzarlo
// como una rama más del switch solo complicaría las transiciones sin
// aportar nada al comportamiento.
//
// A* ya NO mueve al jugador. marcarRuta() calcula un camino con A* y lo
// guarda únicamente para dibujarlo en el mapa como referencia visual.
class Player {
public:
    enum class Estado { Idle, Caminando, Muerto };

    Player() {
        m_forma.setRadius(10.f);
        m_forma.setOrigin({ 10.f, 10.f });
        m_forma.setFillColor(sf::Color(230, 80, 60));
        m_forma.setOutlineThickness(2.f);
        m_forma.setOutlineColor(sf::Color::White);
    }

    void setPos(sf::Vector2f p) { m_pos = p; }
    sf::Vector2f pos() const    { return m_pos; }
    Estado estado() const       { return m_estado; }

    void setTextura(const sf::Texture& t)          { m_textura = &t; }
    void setTexturaDestruido(const sf::Texture& t) { m_texturaDestruido = &t; }
    void setTexturaInmune(const sf::Texture& t)    { m_texturaInmune = &t; }

    bool vivo() const     { return m_estado != Estado::Muerto; }
    int  vida() const     { return m_vida; }
    int  vidaMax() const  { return m_vidaMax; }

    sf::FloatRect hitbox() const {
        return { { m_pos.x - m_radioColision, m_pos.y - m_radioColision },
                 { m_radioColision * 2.f, m_radioColision * 2.f } };
    }

    const char* estadoTexto() const {
        if (m_estado == Estado::Muerto) return "Destruido";
        if (m_inmunidad > 0.f) return "Inmune";
        if (m_flashHerido > 0.f) return "Herido";
        return m_estado == Estado::Caminando ? "Manejando" : "Idle";
    }

    void recibirDanio(int dmg) {
        if (!vivo() || m_inmunidad > 0.f) return;   // inmune: el golpe no cuenta
        m_vida -= dmg;
        if (m_vida <= 0) { m_vida = 0; m_estado = Estado::Muerto; return; }
        m_flashHerido = 0.3f;
    }

    // Power-up de inmunidad: mientras dure, recibirDanio() no hace nada.
    void activarInmunidad(float segundos) { m_inmunidad = segundos; }
    bool esInmune() const { return m_inmunidad > 0.f; }

    // Calcula una ruta con A* desde la posición actual hasta 'destino' y la
    // guarda SOLO para dibujarla; no mueve al jugador ni cambia su estado.
    void marcarRuta(const Grid& g, sf::Vector2f destino) {
        auto celdas = path::astar(g, g.toCell(m_pos), g.toCell(destino));
        m_rutaMarcada = path::aMundo(g, celdas);
    }

    void limpiarRuta() { m_rutaMarcada.clear(); }

    // dirInput: vector con lo que está presionando la persona ahora mismo,
    // por ejemplo {1,0} si solo tiene apretada la flecha derecha, o
    // {1,-1} si tiene derecha+arriba (se normaliza, así que la diagonal no
    // corre más rápido que las direcciones rectas).
    void update(float dt, sf::Vector2f dirInput, const Grid& g) {
        if (m_flashHerido > 0.f) m_flashHerido -= dt;
        if (m_inmunidad > 0.f) m_inmunidad -= dt;
        if (m_estado == Estado::Muerto) return;

        float len = std::sqrt(dirInput.x * dirInput.x + dirInput.y * dirInput.y);
        if (len > 0.001f) {
            sf::Vector2f dir = { dirInput.x / len, dirInput.y / len };
            sf::Vector2f nueva = m_pos + dir * m_velocidad * dt;

            // Choca contra edificios: si la celda de destino no es
            // transitable, simplemente no te dejamos avanzar (como
            // estrellarte contra una pared, sin frenado especial).
            auto celda = g.toCell(nueva);
            if (g.walkable(celda.x, celda.y)) {
                m_pos = nueva;
                m_angulo = std::atan2(dir.y, dir.x) * 180.f / 3.14159265f;
            }
            m_estado = Estado::Caminando;
        } else {
            m_estado = Estado::Idle;
        }

        // La ruta marcada desaparece sola al llegar cerca del destino.
        if (!m_rutaMarcada.empty()) {
            sf::Vector2f destino = m_rutaMarcada.back();
            float dx = destino.x - m_pos.x, dy = destino.y - m_pos.y;
            if (std::sqrt(dx * dx + dy * dy) < 14.f) m_rutaMarcada.clear();
        }
    }

    void draw(sf::RenderTarget& rt) const {
        // La ruta marcada se ve incluso si ya estás muerto (referencia visual).
        if (!m_rutaMarcada.empty()) {
            sf::VertexArray linea(sf::PrimitiveType::LineStrip, m_rutaMarcada.size() + 1);
            linea[0] = sf::Vertex{ m_pos, sf::Color(255, 255, 0, 160) };
            for (size_t i = 0; i < m_rutaMarcada.size(); ++i)
                linea[i + 1] = sf::Vertex{ m_rutaMarcada[i], sf::Color(255, 255, 0, 160) };
            rt.draw(linea);
        }

        if (m_estado == Estado::Muerto) {
            if (m_texturaDestruido) {
                sf::Sprite wreck(*m_texturaDestruido);
                auto tam = m_texturaDestruido->getSize();
                wreck.setOrigin({ tam.x / 2.f, tam.y / 2.f });
                wreck.setScale({ m_anchoSprite / static_cast<float>(tam.x),
                                 m_altoSprite / static_cast<float>(tam.y) });
                wreck.setRotation(sf::degrees(m_angulo));
                wreck.setPosition(m_pos);
                rt.draw(wreck);
            }
            return;   // sin barra de vida ni sprite normal: ya no hay nada más que mostrar
        }

        const sf::Texture* texturaAUsar = m_textura;
        if (m_inmunidad > 0.f && m_texturaInmune) texturaAUsar = m_texturaInmune;

        if (texturaAUsar) {
            sf::Sprite auto_(*texturaAUsar);
            auto tam = texturaAUsar->getSize();
            auto_.setOrigin({ tam.x / 2.f, tam.y / 2.f });
            auto_.setScale({ m_anchoSprite / static_cast<float>(tam.x),
                             m_altoSprite / static_cast<float>(tam.y) });
            auto_.setRotation(sf::degrees(m_angulo));
            auto_.setPosition(m_pos);
            if (m_inmunidad <= 0.f && m_flashHerido > 0.f) auto_.setColor(sf::Color(255, 150, 150));
            rt.draw(auto_);
        } else {
            sf::CircleShape c = m_forma;
            if (m_inmunidad > 0.f)        c.setFillColor(sf::Color(60, 120, 230));
            else if (m_flashHerido > 0.f) c.setFillColor(sf::Color::White);
            else                          c.setFillColor(sf::Color(230, 80, 60));
            c.setPosition(m_pos);
            rt.draw(c);
        }

        float ancho = 30.f, alto = 5.f;
        sf::RectangleShape fondoBarra({ ancho, alto });
        fondoBarra.setFillColor(sf::Color(40, 40, 40));
        fondoBarra.setPosition({ m_pos.x - ancho / 2.f, m_pos.y - 22.f });
        rt.draw(fondoBarra);

        sf::RectangleShape barra({ ancho * (m_vida / static_cast<float>(m_vidaMax)), alto });
        barra.setFillColor(sf::Color(80, 220, 90));
        barra.setPosition({ m_pos.x - ancho / 2.f, m_pos.y - 22.f });
        rt.draw(barra);
    }

private:
    sf::CircleShape m_forma;
    sf::Vector2f    m_pos{ 0.f, 0.f };

    std::vector<sf::Vector2f> m_rutaMarcada;

    Estado m_estado = Estado::Idle;
    float  m_flashHerido = 0.f;
    float  m_inmunidad = 0.f;

    const sf::Texture* m_textura = nullptr;
    const sf::Texture* m_texturaDestruido = nullptr;
    const sf::Texture* m_texturaInmune = nullptr;
    float m_angulo = 0.f;
    float m_anchoSprite = 34.f;
    float m_altoSprite  = 18.f;
    float m_radioColision = 12.f;

    int m_vidaMax = 100;
    int m_vida = 100;

    float m_velocidad = 200.f;
};
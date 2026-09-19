#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

// Menú simple de botones apilados verticalmente, con hover y click.
// Se reutiliza para el menú principal, la pausa y la pantalla de derrota:
// solo cambia el título y la lista de opciones.
class Menu {
public:
    Menu(const sf::Font& fuente, const std::string& titulo,
         const std::vector<std::string>& opciones,
         sf::Vector2f centro)
        : m_titulo(fuente)
    {
        m_titulo.setString(titulo);
        m_titulo.setCharacterSize(40);
        m_titulo.setFillColor(sf::Color::White);
        centrarTexto(m_titulo, { centro.x, centro.y - 130.f });

        float y = centro.y - 30.f;
        for (const auto& etiqueta : opciones) {
            Boton b(fuente);
            b.texto.setString(etiqueta);
            b.texto.setCharacterSize(24);
            b.texto.setFillColor(sf::Color::White);

            b.caja.setSize({ 260.f, 54.f });
            b.caja.setOrigin({ 130.f, 27.f });
            b.caja.setPosition({ centro.x, y });
            b.caja.setFillColor(sf::Color(50, 50, 60));
            b.caja.setOutlineThickness(2.f);
            b.caja.setOutlineColor(sf::Color(120, 120, 140));

            centrarTexto(b.texto, { centro.x, y });
            m_botones.push_back(std::move(b));
            y += 68.f;
        }
    }

    // Devuelve el índice del botón bajo el punto p, o -1 si no hay ninguno ahí.
    int botonEn(sf::Vector2f p) const {
        for (size_t i = 0; i < m_botones.size(); ++i)
            if (m_botones[i].caja.getGlobalBounds().contains(p)) return static_cast<int>(i);
        return -1;
    }

    void actualizarHover(sf::Vector2f mouse) {
        for (auto& b : m_botones) {
            bool encima = b.caja.getGlobalBounds().contains(mouse);
            b.caja.setFillColor(encima ? sf::Color(80, 80, 105) : sf::Color(50, 50, 60));
        }
    }

    void draw(sf::RenderTarget& rt) const {
        rt.draw(m_titulo);
        for (const auto& b : m_botones) {
            rt.draw(b.caja);
            rt.draw(b.texto);
        }
    }

private:
    struct Boton {
        sf::RectangleShape caja;
        sf::Text texto;
        explicit Boton(const sf::Font& f) : texto(f) {}
    };

    static void centrarTexto(sf::Text& t, sf::Vector2f centro) {
        auto b = t.getLocalBounds();
        t.setOrigin({ b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f });
        t.setPosition(centro);
    }

    sf::Text m_titulo;
    std::vector<Boton> m_botones;
};
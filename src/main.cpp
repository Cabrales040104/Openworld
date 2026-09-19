#include <SFML/Graphics.hpp>
#include <random>
#include <sstream>
#include <optional>
#include <cmath>
#include "Grid.hpp"
#include "AStar.hpp"
#include "Enemy.hpp"
#include "Player.hpp"
#include "Menu.hpp"

// Estado de la APLICACIÓN completa (una máquina de estados más, por encima
// de los estados internos del jugador y de cada enemigo):
//
//   Menu --(click "Jugar")--> Jugando
//   Jugando --(Escape)--> Pausa --(click "Continuar")--> Jugando
//   Jugando --(el jugador muere)--> GameOver
//   Pausa/GameOver --(click "Menu principal")--> Menu
//
// Cuando no se está en Jugando, se congela la lógica del mundo (no se
// llama a jugador.update() ni a enemigo.update()), pero se sigue dibujando
// para que el menú/pausa/game-over aparezcan sobre el mundo, no en negro.
enum class EstadoApp { Menu, Jugando, Pausa, GameOver };

int main() {
    const int   COLS = 50, ROWS = 34;
    const float TILE = 28.f;

    sf::RenderWindow window(
        sf::VideoMode({ static_cast<unsigned>(COLS * TILE),
                        static_cast<unsigned>(ROWS * TILE) }),
        "Mundo Abierto - A* + maquina de estados");
    window.setFramerateLimit(60);

    std::random_device rd;
    std::mt19937 rng(rd());

    // ---- Recursos ----
    sf::Texture fondoTex;
    bool hayFondo = fondoTex.loadFromFile("assets/imagen/imagen 1.png");
    sf::Sprite fondo(fondoTex);   // válido aunque fondoTex esté vacía; solo se dibuja si hayFondo
    if (hayFondo) {
        auto tam = fondoTex.getSize();
        fondo.setScale({ COLS * TILE / static_cast<float>(tam.x),
                         ROWS * TILE / static_cast<float>(tam.y) });
    }

    sf::Font fuente;
    bool hayFuente = fuente.openFromFile("assets/font/Dead Stock.ttf.ttf");

    sf::Texture chargerTex, chargerDestruidoTex, chargerInmuneTex, policeTex, edificioTex;
    bool hayCharger  = chargerTex.loadFromFile("assets/imagen/charger.png");
    bool hayCharger2 = chargerDestruidoTex.loadFromFile("assets/imagen/charger_destruido.png");
    bool hayCharger3 = chargerInmuneTex.loadFromFile("assets/imagen/charger_inmune.png");
    bool hayPolice   = policeTex.loadFromFile("assets/imagen/policecar.png");
    bool hayEdificio = edificioTex.loadFromFile("assets/imagen/building.png");
    chargerTex.setSmooth(false);
    chargerDestruidoTex.setSmooth(false);
    chargerInmuneTex.setSmooth(false);
    policeTex.setSmooth(false);
    edificioTex.setSmooth(false);

    sf::Sprite edificioSprite(edificioTex);
    if (hayEdificio) {
        auto tam = edificioTex.getSize();
        edificioSprite.setScale({ TILE / static_cast<float>(tam.x),
                                  TILE / static_cast<float>(tam.y) });
    }

    sf::Text hud(fuente);
    hud.setCharacterSize(15);
    hud.setFillColor(sf::Color::White);
    hud.setPosition({ 8.f, 6.f });

    sf::Vector2f centro{ COLS * TILE / 2.f, ROWS * TILE / 2.f };
    Menu menuPrincipal(fuente, "Mundo Abierto", { "Jugar", "Salir" }, centro);
    Menu menuPausa(fuente, "Pausa", { "Continuar", "Menu principal", "Salir" }, centro);
    Menu menuGameOver(fuente, "Has muerto", { "Reintentar", "Menu principal" }, centro);

    // ---- Mundo abierto: obstáculos dispersos al azar ----
    Grid grid(COLS, ROWS, TILE);
    std::uniform_int_distribution<int> distX(0, COLS - 1), distY(0, ROWS - 1);
    std::uniform_int_distribution<int> distTam(2, 5);
    for (int i = 0; i < 22; ++i) {
        int ox = distX(rng), oy = distY(rng);
        int aw = distTam(rng), ah = distTam(rng);
        for (int y = oy; y < oy + ah && y < ROWS; ++y)
            for (int x = ox; x < ox + aw && x < COLS; ++x)
                grid.setSolid(x, y, true);
    }

    Player jugador;
    std::vector<Enemy> enemigos;

    // Power-up de inmunidad: un único punto en el mapa que reaparece solo
    // después de recogerlo.
    sf::Vector2f powerupPos{ 0.f, 0.f };
    bool  powerupActivo = false;
    float powerupRespawn = 0.f;
    const float POWERUP_RESPAWN_SEG = 18.f;
    const float POWERUP_DURACION_SEG = 3.f;
    const float POWERUP_RADIO = 16.f;

    // Oleadas: cada 15s aparecen 2 patrullas más, hasta un tope razonable
    // para que el mapa no se sature.
    float temporizadorOleada = 0.f;
    const float OLEADA_INTERVALO_SEG = 15.f;
    const int   OLEADA_CANTIDAD = 2;
    const size_t MAX_ENEMIGOS = 24;

    // Deja el mundo listo para una partida nueva: reposiciona al jugador,
    // le regresa la vida completa y vuelve a poner a los enemigos vivos y patrullando.
    auto reiniciarPartida = [&]() {
        sf::Vector2i celdaInicial = grid.randomWalkableCell(rng);
        grid.setSolid(celdaInicial.x, celdaInicial.y, false);
        jugador = Player();
        jugador.setPos(grid.toWorld(celdaInicial));
        if (hayCharger)  jugador.setTextura(chargerTex);
        if (hayCharger2) jugador.setTexturaDestruido(chargerDestruidoTex);
        if (hayCharger3) jugador.setTexturaInmune(chargerInmuneTex);

        enemigos.clear();
        for (int i = 0; i < 3; ++i) {
            sf::Vector2i c = grid.randomWalkableCell(rng);
            grid.setSolid(c.x, c.y, false);
            enemigos.emplace_back(grid.toWorld(c), rng, hayPolice ? &policeTex : nullptr);
        }

        temporizadorOleada = 0.f;
        powerupActivo = false;
        powerupRespawn = 6.f;   // el primero tarda un poco menos en aparecer
    };
    reiniciarPartida();   // arma un mundo desde ya, aunque arranquemos en el menú

    EstadoApp estado = EstadoApp::Menu;
    float tiempoTotal = 0.f;

    sf::Clock reloj;
    while (window.isOpen()) {
        float dt = reloj.restart().asSeconds();
        tiempoTotal += dt;
        sf::Vector2f mousePos(sf::Mouse::getPosition(window));

        // ---- Menús: actualizar hover del que esté activo ----
        if (estado == EstadoApp::Menu)     menuPrincipal.actualizarHover(mousePos);
        if (estado == EstadoApp::Pausa)    menuPausa.actualizarHover(mousePos);
        if (estado == EstadoApp::GameOver) menuGameOver.actualizarHover(mousePos);

        // ---- Eventos ----
        while (const std::optional<sf::Event> ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) window.close();

            if (const auto* kp = ev->getIf<sf::Event::KeyPressed>()) {
                if (kp->code == sf::Keyboard::Key::Escape) {
                    if (estado == EstadoApp::Jugando) estado = EstadoApp::Pausa;
                    else if (estado == EstadoApp::Pausa) estado = EstadoApp::Jugando;
                }
            }

            if (const auto* mp = ev->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f p(static_cast<float>(mp->position.x),
                               static_cast<float>(mp->position.y));

                if (estado == EstadoApp::Menu && mp->button == sf::Mouse::Button::Left) {
                    int i = menuPrincipal.botonEn(p);
                    if (i == 0) { reiniciarPartida(); estado = EstadoApp::Jugando; }
                    else if (i == 1) window.close();
                }
                else if (estado == EstadoApp::Pausa && mp->button == sf::Mouse::Button::Left) {
                    int i = menuPausa.botonEn(p);
                    if (i == 0) estado = EstadoApp::Jugando;
                    else if (i == 1) estado = EstadoApp::Menu;
                    else if (i == 2) window.close();
                }
                else if (estado == EstadoApp::GameOver && mp->button == sf::Mouse::Button::Left) {
                    int i = menuGameOver.botonEn(p);
                    if (i == 0) { reiniciarPartida(); estado = EstadoApp::Jugando; }
                    else if (i == 1) estado = EstadoApp::Menu;
                }
                else if (estado == EstadoApp::Jugando) {
                    if (mp->button == sf::Mouse::Button::Left) {
                        // Ya no mueve al jugador: solo calcula y dibuja la ruta con A*,
                        // como referencia de "por dónde se llegaría" a ese punto.
                        jugador.marcarRuta(grid, p);
                    } else if (mp->button == sf::Mouse::Button::Right) {
                        auto c = grid.toCell(p);
                        grid.toggleSolid(c.x, c.y);
                    }
                }
            }
        }

        // ---- Lógica (congelada fuera de Jugando) ----
        if (estado == EstadoApp::Jugando) {
            sf::Vector2f dirInput{ 0.f, 0.f };
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)    || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) dirInput.y -= 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)  || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) dirInput.y += 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)  || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dirInput.x -= 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dirInput.x += 1.f;

            jugador.update(dt, dirInput, grid);

            const int danioChoque = 20;
            for (auto& e : enemigos) {
                bool golpeo = e.update(dt, grid, jugador.pos(), jugador.vivo());
                if (golpeo) jugador.recibirDanio(e.danio());

                // Choque: si el Charger se estrella contra una patrulla, le hace
                // daño. e.estado()!=Herido evita golpearla 60 veces por segundo
                // mientras siguen pegados (el propio aturdimiento de Herido
                // hace de ventana de invulnerabilidad).
                if (e.vivo() && e.estado() != Enemy::Estado::Herido &&
                    jugador.hitbox().findIntersection(e.hitbox())) {
                    e.recibirDanio(danioChoque);
                }
            }

            if (!jugador.vivo()) estado = EstadoApp::GameOver;

            // ---- Power-up de inmunidad ----
            if (!powerupActivo) {
                powerupRespawn -= dt;
                if (powerupRespawn <= 0.f) {
                    powerupPos = grid.toWorld(grid.randomWalkableCell(rng));
                    powerupActivo = true;
                    jugador.marcarRuta(grid, powerupPos);   // A* dibuja el camino hacia él
                }
            } else if (jugador.vivo()) {
                float dx = powerupPos.x - jugador.pos().x, dy = powerupPos.y - jugador.pos().y;
                if (std::sqrt(dx * dx + dy * dy) < POWERUP_RADIO) {
                    jugador.activarInmunidad(POWERUP_DURACION_SEG);
                    jugador.limpiarRuta();
                    powerupActivo = false;
                    powerupRespawn = POWERUP_RESPAWN_SEG;
                }
            }

            // ---- Oleadas: más patrullas con el paso del tiempo ----
            temporizadorOleada += dt;
            if (temporizadorOleada >= OLEADA_INTERVALO_SEG) {
                temporizadorOleada -= OLEADA_INTERVALO_SEG;
                if (enemigos.size() < MAX_ENEMIGOS) {
                    for (int i = 0; i < OLEADA_CANTIDAD; ++i) {
                        sf::Vector2i c = grid.randomWalkableCell(rng);
                        enemigos.emplace_back(grid.toWorld(c), rng, hayPolice ? &policeTex : nullptr);
                    }
                }
            }
        }

        // ---- Dibujo ----
        window.clear(sf::Color(24, 24, 28));
        if (hayFondo && estado == EstadoApp::Menu) window.draw(fondo);

        if (estado != EstadoApp::Menu) {
            sf::RectangleShape celdaRespaldo({ TILE - 1.f, TILE - 1.f });
            celdaRespaldo.setFillColor(sf::Color(50, 70, 50, 230));
            for (int y = 0; y < ROWS; ++y)
                for (int x = 0; x < COLS; ++x)
                    if (!grid.walkable(x, y)) {
                        if (hayEdificio) {
                            edificioSprite.setPosition({ x * TILE, y * TILE });
                            window.draw(edificioSprite);
                        } else {
                            celdaRespaldo.setPosition({ x * TILE, y * TILE });
                            window.draw(celdaRespaldo);
                        }
                    }

            for (auto& e : enemigos) e.draw(window);

            if (powerupActivo) {
                float pulso = 6.f + 2.f * std::sin(tiempoTotal * 4.f);
                sf::CircleShape estrella(POWERUP_RADIO * 0.6f + pulso * 0.3f);
                estrella.setOrigin({ estrella.getRadius(), estrella.getRadius() });
                estrella.setPosition(powerupPos);
                estrella.setFillColor(sf::Color(255, 215, 60, 220));
                estrella.setOutlineThickness(2.f);
                estrella.setOutlineColor(sf::Color(255, 250, 200, 255));
                window.draw(estrella);
            }

            jugador.draw(window);
        }

        if (hayFuente && estado == EstadoApp::Jugando) {
            std::ostringstream ss;
            ss << "Jugador: " << jugador.estadoTexto()
               << " (" << jugador.vida() << "/" << jugador.vidaMax() << ")"
               << "   |  Flechas/WASD: mover   Choca con la patrulla para danarla"
               << "   |  Estrella dorada: 3s de inmunidad   Click: marcar ruta   Esc: pausa";
            hud.setString(ss.str());
            window.draw(hud);
        }

        // Overlay oscuro + menú correspondiente, sobre el mundo congelado.
        if (estado != EstadoApp::Jugando) {
            sf::RectangleShape overlay({ static_cast<float>(COLS * TILE), static_cast<float>(ROWS * TILE) });
            overlay.setFillColor(sf::Color(10, 10, 15, 180));
            window.draw(overlay);

            if (estado == EstadoApp::Menu)     menuPrincipal.draw(window);
            if (estado == EstadoApp::Pausa)    menuPausa.draw(window);
            if (estado == EstadoApp::GameOver) menuGameOver.draw(window);
        }

        window.display();
    }
    return 0;
}
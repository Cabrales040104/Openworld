# OpenWorld

Juego de mundo abierto desarrollado en C++ con SFML, con movimiento del jugador, patrullas enemigas, sistema de A* para rutas, menús de inicio/pausa/game over y power-up de inmunidad.

## Descripción

OpenWorld es un pequeño proyecto de juego top-down en 2D donde el jugador se mueve por un mapa con obstáculos, evita patrullas y intenta sobrevivir a oleadas progresivas de enemigos. El proyecto incluye:

- Movimiento con teclado
- Detección y persecución por parte de los enemigos
- Ruta calculada con algoritmo A*
- Menú principal, pausa y pantalla de derrota
- Power-up de inmunidad temporal
- Generación aleatoria de obstáculos y spawns

## Video

Puedes ver la demostración del proyecto aquí:

https://drive.google.com/file/d/1k1UmOhvI-2SJQ9L7EPpFNKD-_DD4BadR/view?usp=drive_link

## Capturas de pantalla

El juego usa recursos gráficos en la carpeta `assets`, como fondos, fuentes, coches y edificios. Si los archivos no están presentes junto al ejecutable, la aplicación seguirá ejecutándose con recursos por defecto o sin algunos elementos visuales.

## Requisitos

- Windows
- Visual Studio 2022
- SFML configurado para C++ con MSBuild/vcpkg
- Plataforma x64

## Cómo ejecutar

1. Abre la solución `OpenWorld.sln` en Visual Studio.
2. Selecciona la configuración `Debug` o `Release` y la plataforma `x64`.
3. Compila el proyecto.
4. Ejecuta el archivo generado en:

   `bin\x64\Debug\OpenWorld.exe`

   o

   `bin\x64\Release\OpenWorld.exe`

> El proyecto está configurado para copiar automáticamente la carpeta `assets` al directorio de salida tras compilar.

## Controles

- `WASD` o flechas: mover al jugador
- `Click izquierdo`: marcar una ruta visual con A*
- `Click derecho`: alternar celda sólida/obstáculo en el mapa
- `Esc`: abrir/cerrar pausa

## Modo de juego

- En el menú principal puedes iniciar la partida o salir.
- Durante la partida, el jugador tiene vida y puede recibir daño al chocar con patrullas.
- Los enemigos patrullan y se vuelven agresivos cuando detectan al jugador.
- Cada cierto tiempo aparecen más enemigos en oleadas.
- El power-up dorado concede inmunidad durante unos segundos.
- Si la vida llega a cero, se muestra la pantalla de Game Over.

## Estructura del proyecto

```text
OpenWorld/
├── Assets/
│   ├── font/
│   └── imagen/
├── Include/
│   ├── AStar.hpp
│   ├── Enemy.hpp
│   ├── Grid.hpp
│   ├── Menu.hpp
│   └── Player.hpp
├── src/
│   └── main.cpp
├── OpenWorld.sln
├── OpenWorld.vcxproj
├── OpenWorld.vcxproj.user
├── Readme
└── obj/
    └── x64/
```

## Tecnologías utilizadas

- C++17
- SFML
- A* para navegación y rutas
- Máquina de estados para menus y flujo de juego

## Notas importantes

- El juego está pensado como una demo/prototipo de un mundo abierto con IA básica.
- La ruta calculada por A* se usa principalmente como referencia visual para el jugador.
- El movimiento del personaje no está totalmente automatizado por el algoritmo; la lógica del control se maneja por entrada del usuario.

## Autores

Raul Cabrales Carmona

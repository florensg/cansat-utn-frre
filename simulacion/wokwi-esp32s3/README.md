# Simulación Interactiva Aislada (Wokwi — ESP32-S3)

Esta carpeta contiene el entorno de simulación interactivo aislado del software de vuelo funcional (`firmware/`).

## Archivos incluidos

- `wokwi_online_sketch.ino`: Sketch autocontenido con estímulos de prueba (potenciómetro de altitud, botón de lanzamiento, LEDs de estado y perfil automático de vuelo) para usar directamente en Wokwi Online.
- `diagram.json`: Esquema de componentes virtuales para Wokwi.
- `wokwi.toml`: Configuración para la extensión de Wokwi en VS Code.

## Cómo ejecutar en Wokwi Online

1. Abrir [https://wokwi.com/projects/new/esp32-s3](https://wokwi.com/projects/new/esp32-s3).
2. Copiar el contenido de `wokwi_online_sketch.ino` en `sketch.ino`.
3. Copiar el contenido de `diagram.json` en `diagram.json`.
4. Presionar **Start the simulation** (Play).

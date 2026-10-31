# Decisión: controlador principal ESP32-S3 DevKitC-1

- Fecha: 2026-10-31
- Estado: aceptada
- Responsable: Florencia González (ISI 1), en coordinación con Juan Pedro López (IEM 3)

## Contexto

La planificación y la lista de elementos tenían dos alternativas de controlador principal:

- **C-001 — STM32**, marcado como **Descartado** en Elementos v2.
- **C-002 — ESP32-S3**, marcado como **Seleccionado** en Elementos v2.

La actividad **5.0** de la Planificación General v3 asigna la arquitectura de hardware y
software del microcontrolador principal a ISI 1 e IEM 3. El modelo seleccionado es el
**ESP32-S3 DevKitC-1 de 44 pines**, con alimentación de 3.3 V, USB-C y radio WiFi/BLE
integrada.

## Decisión

1. Usar el **ESP32-S3 DevKitC-1 de 44 pines** como controlador principal del OBC.
2. Migrar el entorno de firmware de `ststm32` a `espressif32`, con
   `board = esp32-s3-devkitc-1`.
3. Renombrar `firmware/obc-stm32/` a `firmware/obc-esp32s3/`.
4. Mantener la lógica de vuelo en `flight_core` como C++ puro y probarla en el entorno
   `native`.
5. Usar USB CDC para el monitor serie y reservar las UART físicas para la radio y la
   placa secundaria.
6. Reemplazar el watchdog IWDG del STM32 por el **Task Watchdog Timer (TWDT)** del
   ESP32-S3, con RTC WDT como respaldo.
7. Archivar `elementos-cansat.xlsx` y `planificacion-general-v2.xlsx` para evitar que se
   usen por error.

## Consecuencias

- El firmware compila para ESP32-S3 y conserva los 13 tests unitarios nativos.
- El pinout, la variante de flash/PSRAM y el mapa de buses quedan pendientes de cierre con
  IEM 3.
- El diagrama de arquitectura general debe mostrar el ESP32-S3 como controlador principal
  y las XIAO ESP32S3 Sense como placas de cámara.
- La alimentación de 3.3 V y el consumo del ESP32-S3 se validan con IEM 1.

## Verificación

- `pio run -e obc` compila sin errores para `ESP32-S3-DevKitC-1-N8` (8 MB QD, sin PSRAM):
  7,3 % de RAM y 10,6 % de flash usados.
- `pio test -e native` pasa 13/13 tests.
- La grabación por USB-C (`pio run -e obc -t upload`) queda pendiente de probar con la
  placa física.

## Fuentes

- [`planificacion-general-v3.xlsx`](../planificacion-general-v3.xlsx), hoja `Gantt PDR`,
  fila 16 (actividad 5.0).
- [`elementos-cansat-v2.xlsx`](../elementos-cansat-v2.xlsx), hoja `Componentes`, filas 2 y
  3 (C-001 descartado, C-002 seleccionado).
- [`elementos-cansat-v2.xlsx`](../elementos-cansat-v2.xlsx), hoja `Presupuesto de masa`,
  fila 29: “Incluye ESP32-S3 seleccionado”.

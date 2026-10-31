# Evidencia: compilación del firmware ESP32-S3

- Fecha: 2026-10-31
- Responsable: Florencia González (ISI 1)
- Placa: ESP32-S3 DevKitC-1 de 44 pines (Elementos v2, C-002)
- Entorno: PlatformIO `obc`, `platform = espressif32`, `board = esp32-s3-devkitc-1`

## Comandos ejecutados

```bash
cd firmware/obc-esp32s3
pio run -e obc
pio test -e native
```

## Resultado

- `pio run -e obc`: **SUCCESS**.
- Placa detectada por PlatformIO: `ESP32-S3-DevKitC-1-N8` (8 MB QD, sin PSRAM).
- RAM: 23.912 bytes de 327.680 (7,3 %).
- Flash: 355.942 bytes de 3.342.336 (10,6 %).
- `pio test -e native`: **13/13 tests pasaron**.

## Observaciones

- No se probó la grabación por USB-C porque no había placa física conectada.
- Si la placa entregada es la variante N8R8, hay que habilitar PSRAM en `platformio.ini`.
- El pinout de radio y placa secundaria sigue pendiente de cierre con IEM 3.

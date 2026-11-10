---
schema_version: 1
doc_type: spec
title: Simulación Wokwi alineada a la Propuesta 2 de circuito
created_at: '2026-11-10T02:47:13.611987Z'
updated_at: '2026-11-10T02:47:13.611987Z'
tags:
- spec
- wokwi
- simulacion
- hardware
- esp32-s3
- propuesta-2
- sensores
- telemetria
status: draft
links: []
vault_scope: local
fingerprint: 96f8498e3959c7edb804dd3f16fd595a13cd82df0d5cd1ae51346c16436b9a73
verification_hooks:
- name: Compila firmware OBC ESP32-S3
  command: pio run -e obc
  required: true
  success_criteria: exit code 0
  timeout_seconds: 900
- name: Tests nativos flight_core
  command: pio test -e native
  required: true
  success_criteria: 13/13 tests passed
  timeout_seconds: 900
- name: diagram.json parseable
  command: python -c "import json;json.load(open('diagram.json',encoding='utf-8'));print('OK')"
  required: true
  success_criteria: exit code 0
  timeout_seconds: 60
goal: Reescribir la simulación Wokwi del OBC ESP32-S3 para representar todos los bloques
  de la 'Propuesta 2 de circuito cansat' (ESP32-S3, MPU-6050, BMP280, LIS3MDL, RTC
  RV-3028-C7, INA219, FRAM MB85RC256V, ADS1115 + 4x TSL251RD, GPS NEO-M8N, servo SG90,
  XBee, Raspberry Pi Zero W y 2x XIAO ESP32-S3), con drivers I2C/UART reales, telemetría
  extendida y stand-ins documentados donde Wokwi no tiene el componente nativo.
files_in_scope:
- firmware/obc-esp32s3/diagram.json
- firmware/obc-esp32s3/wokwi_online_sketch.ino
- firmware/obc-esp32s3/include/config.h
- firmware/obc-esp32s3/src/main.cpp
- firmware/obc-esp32s3/lib/flight_core/include/flight_core/sensors.h
- firmware/obc-esp32s3/lib/flight_core/include/flight_core/telemetry.h
- firmware/obc-esp32s3/lib/flight_core/src/sensors.cpp
- firmware/obc-esp32s3/lib/flight_core/src/telemetry.cpp
- firmware/obc-esp32s3/test/test_flight_core/test_flight_core.cpp
- docs/04-pruebas/evidencias/2026-11-04_simulacion-wokwi-online-esp32s3.md
constraints:
- 'Wokwi no tiene RV-3028-C7, MB85RC256V, TSL251RD, XBee, RPi Zero W ni XIAO ESP32-S3:
  usar stand-ins o anotaciones y documentarlo; no fingir que el componente real está
  simulado.'
- El sketch de Wokwi Online debe ser un único .ino sin librerías externas (solo Wire
  y Arduino.h).
- No cambiar el contrato CSV de forma incompatible sin registrar la decisión y avisar
  a ISI 2.
- 'La simulación no reemplaza la validación en hardware: toda calibración es preliminar.'
- Mantener IDs y formato de LEDs, botón y servo para no romper los casos de prueba
  existentes.
- 'El pinout definitivo sigue pendiente de cierre con IEM 3: el mapa de simulación
  queda marcado como borrador.'
acceptance_criteria:
- diagram.json es JSON válido y contiene los bloques de la Propuesta 2 sin IDs duplicados.
- El sketch Wokwi compila en Wokwi Online para ESP32-S3.
- Cada sensor I2C se detecta en su dirección y el firmware sigue emitiendo telemetría
  si alguno falta.
- La altitud del BMP280 reproduce el perfil 0-400 m y dispara apogeo y 2do paracaídas
  con la lógica actual.
- La telemetría CSV incluye los campos nuevos y sigue siendo parseable a 1 Hz.
- Los 13 tests nativos pasan (pio test -e native) y el firmware compila (pio run -e
  obc).
- La evidencia documenta la tabla nativo/stand-in/anotado y la colisión 0x68 resuelta.
---

## Goal

Reescribir la simulación Wokwi del OBC ESP32-S3 para representar todos los bloques de la 'Propuesta 2 de circuito cansat' (ESP32-S3, MPU-6050, BMP280, LIS3MDL, RTC RV-3028-C7, INA219, FRAM MB85RC256V, ADS1115 + 4x TSL251RD, GPS NEO-M8N, servo SG90, XBee, Raspberry Pi Zero W y 2x XIAO ESP32-S3), con drivers I2C/UART reales, telemetría extendida y stand-ins documentados donde Wokwi no tiene el componente nativo.

## Requirements

- R1 — Inventario y mapa de interfaces de cada bloque: bus (I2C/UART/GPIO), dirección o baudios, pines del ESP32-S3 y clasificación nativo / stand-in / anotado.
- R2 — diagram.json: ESP32-S3 DevKitC-1, MPU-6050, BMP280, LIS3MDL, INA219, ADS1115, GPS NEO-6M, servo SG90, LEDs, botón y potenciómetro; DS3231 y 24LC256 como stand-in de RV-3028-C7 y FRAM; LDR/potenciómetros en las 4 entradas del ADS1115 como stand-in de los TSL251RD; XBee, RPi Zero W y XIAO ESP32-S3 representados con sustitutos seriales o anotaciones wokwi-text.
- R3 — Resolver la colisión I2C 0x68 (MPU-6050 vs DS3231) con un segundo bus Wire1 en pines libres del ESP32-S3, manteniendo GPIO 8/9 a 400 kHz para el bus principal.
- R4 — Sketch Wokwi: drivers para MPU-6050 (0x68), BMP280 (0x76), LIS3MDL (0x1C), INA219 (0x40), ADS1115 (0x48), DS3231 (Wire1) y 24LC256 (0x50), GPS NEO-6M por UART a 9600 baudios, con degradación graciosa si un dispositivo no responde.
- R5 — Altitud primaria desde el BMP280 (fórmula barométrica); potenciómetro como estímulo manual y perfil automático de vuelo como modo de prueba.
- R6 — Telemetría CSV extendida con presión, temperatura, campo magnético, tensión/corriente, 4 canales ópticos, hora del RTC y fix/altitud GPS, manteniendo team_id, mission_time, packet_count, estado y modo.
- R7 — Mantener intacta la lógica nuclear: FlightStateMachine, ApogeeDetector (caída >= 5 m sobre 100 m, 2do paracaídas al 80%) y comandos CX/ST/CAL/MEC/SIM/RST.
- R8 — Alinear include/config.h y src/main.cpp con el nuevo mapa de direcciones, pines y campos de telemetría, sin romper los 13 tests nativos.
- R9 — Actualizar la evidencia en docs/04-pruebas/evidencias/ con el mapa de conexiones, la tabla nativo/stand-in/anotado, los casos de prueba y las limitaciones.

## Files in Scope

- `firmware/obc-esp32s3/diagram.json`
- `firmware/obc-esp32s3/wokwi_online_sketch.ino`
- `firmware/obc-esp32s3/include/config.h`
- `firmware/obc-esp32s3/src/main.cpp`
- `firmware/obc-esp32s3/lib/flight_core/include/flight_core/sensors.h`
- `firmware/obc-esp32s3/lib/flight_core/include/flight_core/telemetry.h`
- `firmware/obc-esp32s3/lib/flight_core/src/sensors.cpp`
- `firmware/obc-esp32s3/lib/flight_core/src/telemetry.cpp`
- `firmware/obc-esp32s3/test/test_flight_core/test_flight_core.cpp`
- `docs/04-pruebas/evidencias/2026-11-04_simulacion-wokwi-online-esp32s3.md`

## Constraints

- Wokwi no tiene RV-3028-C7, MB85RC256V, TSL251RD, XBee, RPi Zero W ni XIAO ESP32-S3: usar stand-ins o anotaciones y documentarlo; no fingir que el componente real está simulado.
- El sketch de Wokwi Online debe ser un único .ino sin librerías externas (solo Wire y Arduino.h).
- No cambiar el contrato CSV de forma incompatible sin registrar la decisión y avisar a ISI 2.
- La simulación no reemplaza la validación en hardware: toda calibración es preliminar.
- Mantener IDs y formato de LEDs, botón y servo para no romper los casos de prueba existentes.
- El pinout definitivo sigue pendiente de cierre con IEM 3: el mapa de simulación queda marcado como borrador.

## Acceptance Criteria

- [ ] diagram.json es JSON válido y contiene los bloques de la Propuesta 2 sin IDs duplicados.
- [ ] El sketch Wokwi compila en Wokwi Online para ESP32-S3.
- [ ] Cada sensor I2C se detecta en su dirección y el firmware sigue emitiendo telemetría si alguno falta.
- [ ] La altitud del BMP280 reproduce el perfil 0-400 m y dispara apogeo y 2do paracaídas con la lógica actual.
- [ ] La telemetría CSV incluye los campos nuevos y sigue siendo parseable a 1 Hz.
- [ ] Los 13 tests nativos pasan (pio test -e native) y el firmware compila (pio run -e obc).
- [ ] La evidencia documenta la tabla nativo/stand-in/anotado y la colisión 0x68 resuelta.

## Verification Hooks

Commands that objectively prove the work is done. Run by
`cortex finish-session` (Pluggable Middle, Phase 01).

### Compila firmware OBC ESP32-S3
```bash
pio run -e obc
```

Success: exit code 0 · Timeout: 900s
### Tests nativos flight_core
```bash
pio test -e native
```

Success: 13/13 tests passed · Timeout: 900s
### diagram.json parseable
```bash
python -c "import json;json.load(open('diagram.json',encoding='utf-8'));print('OK')"
```

Success: exit code 0 · Timeout: 60s

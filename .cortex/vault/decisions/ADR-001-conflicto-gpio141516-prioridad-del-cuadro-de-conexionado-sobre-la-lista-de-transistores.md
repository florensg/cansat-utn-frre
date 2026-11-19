---
schema_version: 1
doc_type: adr
title: 'Conflicto GPIO14/15/16: prioridad del cuadro de conexionado sobre la lista
  de transistores'
created_at: '2026-11-19T18:16:01.763379Z'
updated_at: '2026-11-19T18:16:01.763379Z'
tags:
- hardware
- pinmap
- circuito-v2
- obc
status: accepted
links:
- docs/02-arquitectura/pinmap-esp32s3-v2.md
- docs/02-arquitectura/fuentes/README.md
vault_scope: local
fingerprint: a964f9c3212176ec6008560f92327cd98684af72413b4084082ecbb606cdadc3
adr_number: 1
supersedes: []
superseded_by: null
alternatives_considered: []
acceptance_criteria_met: false
---

## Context

El cuadro de conexionado (Cuadro_conexionado_ESP32S3_CanSat_v2.docx) y la lista de transistores (Conexion de transistores.jpeg, 2 minutos mas reciente) asignan GPIO 14/15/16 a funciones distintas.

El cuadro ubica ahi las lineas de UART: RX de la Raspberry Pi en 14, RX del XBee en 15 y TX del XBee en 16. El JPEG ubica en esos mismos pines las habilitaciones EN_NADIR, EN_SERVO y EN_SEC.

Las tres llaves no pueden compartir GPIO con las UART: el pin en reposo de una UART queda en HIGH, asi que los tres rieles estarian siempre energizados. El conflicto bloqueaba el arranque del firmware (checkpoint 8 de la sesion).

## Decision

Manda el cuadro de conexionado, porque es la fuente de lo que esta fisicamente conectado (amarillo = conectado).

- Las tres UART quedan contiguas en GPIO13-18: Serial1 = GPS (RX 17 / TX 18), Serial2 = XBee (RX 15 / TX 16), g_rpi_serial = HardwareSerial(0) remapeada a RX 14 / TX 13.
- EN_NADIR pasa a GPIO38, EN_SERVO a GPIO39 y EN_SEC a GPIO40.
- EN_RPI (11) y EN_CENIT (12) no cambian.
- El arnes se arma directamente en los pines nuevos: no hay hardware existente que recablear, porque las llaves todavia no estaban cableadas.

## Alternatives Considered

(none)

## Consequences

Ventana de riesgo: si alguien cablea las compuertas en 14/15/16 con el mapa viejo, los tres rieles quedan comandados por las UART en reposo en HIGH y se encienden siempre. Hay que verificar el conexionado antes de energizar.
Un test de test_flight_core verifica que ningun EN_* comparta GPIO con una UART, para que el error no vuelva.
Cambian el checklist de armado y los valores esperados de la mascara PWR (001F al arranque, 03FF con la carga util encendida).


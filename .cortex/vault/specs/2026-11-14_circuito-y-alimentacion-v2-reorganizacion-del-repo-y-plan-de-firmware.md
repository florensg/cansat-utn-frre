---
schema_version: 1
doc_type: spec
title: 'Circuito y alimentacion v.2: reorganizacion del repo y plan de firmware'
created_at: '2026-11-14T20:09:14.445643Z'
updated_at: '2026-11-14T20:09:14.445643Z'
tags:
- spec
- hardware
- arquitectura
- reorganizacion
- circuito-v2
- firmware
- power-gating
status: draft
links: []
vault_scope: local
fingerprint: 3f24156977ae8f6b8d3fe613bf024aa91bdaa207fe456660bea48264bcf9eb2f
verification_hooks:
- name: pdf-v2-en-su-lugar
  command: powershell -NoProfile -Command "if (Test-Path 'docs/02-arquitectura/circuito-electronico-y-alimentacion-v2.pdf')
    { exit 0 } else { exit 1 }"
  required: true
  success_criteria: exit code 0
  timeout_seconds: 30
- name: indices-referencian-v2
  command: powershell -NoProfile -Command "if (Select-String -Path 'docs/README.md','README.md'
    -Pattern 'circuito-electronico-y-alimentacion-v2' -Quiet) { exit 0 } else { exit
    1 }"
  required: true
  success_criteria: exit code 0
  timeout_seconds: 30
goal: Adoptar el esquematico 'CIRCUITO ELECTRONICO Y ALIMENTACION v.2' como referencia
  vigente del hardware, reorganizar la documentacion de arquitectura y dejar registrado
  el plan de cambios de firmware derivados del nuevo circuito (power gating, RTC con
  respaldo, bateria 1S, LED de estado, buzzer, XBee-PRO 900HP, ADS1115 + TSL251RD).
files_in_scope:
- CIRCUITO ELECTRONICO Y ALIMENTACION v.2.pdf
- Diagrama.jpeg
- README.md
- docs/README.md
- docs/02-arquitectura/
- firmware/obc-esp32s3/README.md
- firmware/payload-rpi/README.md
- firmware/payload-xiao/README.md
constraints:
- 'No reescribir la logica de vuelo (FSM, apogeo, telemetria, storage) en esta sesion:
  el foco es documentacion y reorganizacion.'
- Mantener compatibilidad de rutas relativas en los READMEs.
- No mover datos personales ni binarios pesados.
acceptance_criteria:
- El PDF v.2 y el diagrama estan en docs/02-arquitectura con nombres canonicos.
- docs/README.md y README.md raiz apuntan a v.2 como vigente.
- Los READMEs de firmware referencian v.2 como fuente vigente.
- Existe una tabla de componentes del esquematico v.2.
---

## Goal

Adoptar el esquematico 'CIRCUITO ELECTRONICO Y ALIMENTACION v.2' como referencia vigente del hardware, reorganizar la documentacion de arquitectura y dejar registrado el plan de cambios de firmware derivados del nuevo circuito (power gating, RTC con respaldo, bateria 1S, LED de estado, buzzer, XBee-PRO 900HP, ADS1115 + TSL251RD).

## Requirements

- Mover el PDF 'CIRCUITO ELECTRONICO Y ALIMENTACION v.2.pdf' y el diagrama 'Diagrama.jpeg' a docs/02-arquitectura/ con nombres canonicos en minusculas y guiones.
- Crear/actualizar docs/02-arquitectura/README.md como indice, marcando v.2 como vigente y la Propuesta 2 / circuito v3 como superseded.
- Actualizar README.md raiz, docs/README.md y los README de firmware (obc-esp32s3, payload-rpi, payload-xiao) para referenciar el v.2.
- Documentar el mapeo de componentes del esquematico v.2 (ESP32-S3, BMP280, MPU-6050, INA219, NEO-M8N, LIS3MDL, ADS1115, TSL251RD x4, FRAM, RPi Zero W, XIAO Cenit/Nadir, SG90, RV-3028-C7 + CR1225, TPS61088, TPS25940A, TPS2553 x4, TPS62132, TPS22946, BMS-1S, SW-ROT-3, 2N7002 x5, AO3401A x5, buzzer 10440, XBee-PRO 900HP).
- Registrar las preguntas abiertas de firmware: GPIO de habilitacion de cada TPS2553/TPS22946/AO3401A, lectura del switch 3 posiciones, baud de XBee-PRO 900HP, politica de ahorro de energia y obligatoriedad de buzzer/LED de estado.

## Files in Scope

- `CIRCUITO ELECTRONICO Y ALIMENTACION v.2.pdf`
- `Diagrama.jpeg`
- `README.md`
- `docs/README.md`
- `docs/02-arquitectura/`
- `firmware/obc-esp32s3/README.md`
- `firmware/payload-rpi/README.md`
- `firmware/payload-xiao/README.md`

## Constraints

- No reescribir la logica de vuelo (FSM, apogeo, telemetria, storage) en esta sesion: el foco es documentacion y reorganizacion.
- Mantener compatibilidad de rutas relativas en los READMEs.
- No mover datos personales ni binarios pesados.

## Acceptance Criteria

- [ ] El PDF v.2 y el diagrama estan en docs/02-arquitectura con nombres canonicos.
- [ ] docs/README.md y README.md raiz apuntan a v.2 como vigente.
- [ ] Los READMEs de firmware referencian v.2 como fuente vigente.
- [ ] Existe una tabla de componentes del esquematico v.2.

## Verification Hooks

Commands that objectively prove the work is done. Run by
`cortex finish-session` (Pluggable Middle, Phase 01).

### pdf-v2-en-su-lugar
```bash
powershell -NoProfile -Command "if (Test-Path 'docs/02-arquitectura/circuito-electronico-y-alimentacion-v2.pdf') { exit 0 } else { exit 1 }"
```

Success: exit code 0 · Timeout: 30s
### indices-referencian-v2
```bash
powershell -NoProfile -Command "if (Select-String -Path 'docs/README.md','README.md' -Pattern 'circuito-electronico-y-alimentacion-v2' -Quiet) { exit 0 } else { exit 1 }"
```

Success: exit code 0 · Timeout: 30s

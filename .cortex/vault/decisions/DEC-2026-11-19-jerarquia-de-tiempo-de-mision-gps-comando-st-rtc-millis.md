---
schema_version: 1
doc_type: decision
title: 'Jerarquia de tiempo de mision: GPS > comando ST > RTC > millis()'
created_at: '2026-11-19T18:16:01.760657Z'
updated_at: '2026-11-19T18:16:01.760657Z'
tags:
- tiempo
- rtc
- obc
- circuito-v2
status: active
links:
- docs/03-subsistemas/software-vuelo/tiempo-mision-v2.md
vault_scope: local
fingerprint: c69ba963506cfa631ca767ba57068c0de64ecf69b401cd52136fd67ade47c03b
reversible_within_days: 0
---

## Context

Sin RTC externo, la hora UTC dependia del GPS o de un ajuste manual y se perdia en cada reinicio. El circuito v.2 reincorpora un RV-3028-C7 con respaldo CR1225.

Con el RTC disponible hay que definir quien manda y, sobre todo, evitar que una actualizacion del GPS revierta un ajuste manual hecho por la estacion terrena.

## Decision

Orden de precedencia: GPS > comando ST > RTC > millis().

- El GPS solo aporta fecha y hora validas con una trama $--RMC en estado A (la GGA trae posicion y hora del dia, pero no fecha).
- El comando ST siempre se aplica y abre una ventana de gracia de 5 s (kCommandTimeHoldMs) durante la cual el GPS no lo revierte.
- El GPS reescribe el RTC una sola vez por encendido, para no castigar la EEPROM del modulo.
- Sin sincronizar, el reloj arranca en 2000-01-01 y publica valid = 0.
- La fuente activa se publica en telemetria como TSRC (G/C/R/M/-) y en la respuesta al comando CX.

## Alternative Rejected



## Reason




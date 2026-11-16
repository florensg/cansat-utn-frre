# Archivo — documentación de software superseded

Material histórico. **No usar como referencia vigente.**

## `guia-software-cansat-v3.html`

Guía didáctica del software del CanSat (cómo piensa el programa principal, mapa
de comunicaciones, etapas de vuelo, apogeo y 80 %, sensores, FRAM y misión
secundaria).

Está escrita sobre la arquitectura **v3**, que quedó reemplazada por el
circuito **v.2**:

- La v3 **no tenía RTC externo** y usaba un reloj por software; la v.2
  reincorpora el `RV-3028-C7` con respaldo `CR1225` y la jerarquía
  **GPS > comando `ST` > RTC > `millis()`**.
- El pinmap de la guía no coincide necesariamente con el conexionado vigente.

Para el estado actual usar:

- [`../README.md`](../README.md) — software de vuelo.
- [`../tiempo-mision-v2.md`](../tiempo-mision-v2.md) — RTC y tiempo de misión.
- [`../indicadores-v2.md`](../indicadores-v2.md) — LED, watchdog y reinicios.
- [`../../02-arquitectura/pinmap-esp32s3-v2.md`](../../../02-arquitectura/pinmap-esp32s3-v2.md) — pines vigentes.
- [`../../02-arquitectura/correcciones-conexionado-y-estado-firmware-v2.md`](../../../02-arquitectura/correcciones-conexionado-y-estado-firmware-v2.md) — cambios y estado del código.

> El nombre del archivo conserva "v3" a propósito: identifica la versión de la
> arquitectura sobre la que fue escrito, no una versión vigente.

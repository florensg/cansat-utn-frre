# Tiempo de misión y RTC (circuito v.2)

Jerarquía de tiempo del OBC: **GPS > comando `ST` > RTC > `millis()`**.
Cubre los requisitos **F2** (tiempo de misión robusto a reinicios) y **F3**
(ajuste de hora por comando con margen de 1 s) del Anexo A v.2.

## 1. Componentes

| Pieza | Archivo | Rol |
|---|---|---|
| Calendario y precedencia (puro) | `lib/flight_core/include/flight_core/mission_time.h` + `src/mission_time.cpp` | Fecha/hora, epoch, BCD, `MissionClock`, resolución de fuentes |
| Driver del RTC | `src/rtc_rv3028.h` + `src/rtc_rv3028.cpp` | Lectura/escritura del RV-3028-C7 en I²C `0x52` |
| Integración | `src/main.cpp` | Sondeo al arrancar, parseo NMEA, comando `ST`, telemetría |

## 2. Jerarquía y reglas

| Fuente | Cuándo gana | Efecto |
|---|---|---|
| `Gps` | Hay fix y la trama `$--RMC` trae estado `A` | Referencia absoluta; además graba el RTC una vez por encendido |
| `Command` | Comando `ST` desde la GCS | Corrección manual explícita; gana incluso sobre GPS |
| `Rtc` | No hay GPS y el RTC no perdió energía (`PORF = 0`) | Restaura fecha/hora tras un corte o reinicio |
| `Millis` | Nunca sincronizado | Solo cuenta desde el arranque; `RTC_VALID = 0` |

Reglas implementadas en `time_source_accepts()`:

- `millis()` **nunca** sobrescribe una fuente real.
- El GPS reemplaza al RTC, pero no al revés.
- El comando `ST` siempre se aplica y abre una **ventana de gracia de 5 s**
  (`kCommandTimeHoldMs`): dentro de esa ventana el GPS no revierte el ajuste, para
  que una corrección de 1 s no se pierda por el desfasaje del receptor. Vencida la
  ventana, el GPS retoma el control.
- El RTC nunca reemplaza una corrección manual.

## 3. Tramas NMEA utilizadas

| Trama | Campos usados | Aporte |
|---|---|---|
| `$--GGA` | hora, calidad de fix, satélites, lat/lon, altitud | Posición y hora del día |
| `$--RMC` | hora, estado (`A`/`V`), fecha `ddmmyy` | **Fecha completa** y validez |

`GGA` no trae fecha, por eso el RTC se sincroniza desde `RMC`. Mientras no llegue
un `RMC` válido, el reloj conserva la fecha que ya tenía (RTC o referencia interna).

## 4. Mapa de registros del RV-3028-C7

| Registro | Contenido |
|---|---|
| `0x00` | Segundos (BCD) |
| `0x01` | Minutos (BCD) |
| `0x02` | Horas (BCD) |
| `0x03` | Día de la semana (1–7) |
| `0x04` | Día del mes (BCD) |
| `0x05` | Mes (BCD) |
| `0x06` | Año (BCD, 00–99 → 2000–2099) |
| `0x0E` | Status — **bit 0 = `PORF`** (se perdió la hora) |
| `0x10` | Control2 — bit 1 = modo 12 h, bit 0 = reset |

> Ojo con el orden: el día de la semana está en `0x03` y el día del mes en `0x04`
> (no al revés). El bloque se lee/escribe de una sola transacción en `0x00`.

Al inicializar, el driver:

1. Sondea el dispositivo leyendo el bloque de 7 bytes. Sin ACK → `present = false`.
2. Fuerza **modo 24 h** (convierte el valor si estaba en 12 h).
3. Lee `Status` para saber si hubo `PORF`.
4. Decodifica y valida el BCD (`bcd_range_ok` + `datetime_is_valid`).
5. Limpia los flags al escribir hora (`PORF` incluido).

## 5. Comandos

| Comando | Efecto |
|---|---|
| `ST HH:MM:SS` | Ajusta la hora del día, conserva la fecha conocida |
| `ST YYYY-MM-DDTHH:MM:SS` | Ajusta fecha y hora completas |
| `CX` | Responde estado con `TIME=hh:mm:ss UTC=<0/1> TSRC=<G\|C\|R\|M>` |

Ambos formatos de `ST` escriben el RTC (si está presente), de modo que el ajuste
sobrevive a un corte de energía. Un `ST` mal formado responde
`ST ERR (usar ST HH:MM:SS o ST YYYY-MM-DDTHH:MM:SS)` y **no** toca el reloj.

## 6. Telemetría

No se alteró el orden de columnas del CSV principal. Se agregó al final de
`OPTIONAL_DATA`:

```
...;PWR:03FF;TSRC:G
```

`TSRC` identifica la fuente activa: `G` GPS, `C` comando, `R` RTC, `M` millis,
`-` sin sincronizar. El arranque imprime además:

```
RTC:OK 2026-11-16 12:34:56 UTC (semana 1)
RTC: presente pero sin hora confiable (PORF=1); se espera GPS o ST
RTC: FAIL (no responde en 0x52)
```

## 7. Pruebas

Tests nativos (`pio test -e native`, `test/test_flight_core/`):

| Test | Qué verifica |
|---|---|
| `test_time_bcd_helpers` | Conversión BCD ↔ binario y validación de nibbles |
| `test_time_calendar_rules` | Años bisiestos y días por mes |
| `test_time_validation` | Rechazo de mes 13, día 32, hora 24; 29/02 solo en bisiesto |
| `test_time_epoch_roundtrip` | Ida y vuelta fecha ↔ segundos desde 2000-01-01 |
| `test_time_weekday_iso` | Día de la semana (1 = lunes … 7 = domingo) |
| `test_time_source_precedence` | GPS > RTC > millis y ventana del comando |
| `test_mission_clock_ticks` | El reloj avanza con `millis()` desde el ancla |
| `test_mission_clock_keeps_gps_over_rtc` | El RTC no pisa una hora de GPS |
| `test_mission_clock_command_hold` | La corrección manual sobrevive 5 s y luego el GPS retoma |

**Verificación ejecutada (2026-11-16):**

- El algoritmo de calendario/epoch/weekday/precedencia se replicó en Python y se
  contrastó contra `datetime` en **28 074 fechas** del rango 2000–2099 y **5 000**
  fechas aleatorias para el día de la semana: sin discrepancias.
- **Compilación real OK**: `pio run -e obc` con PlatformIO 6.2.0,
  `platform-espressif32` 8.0.0 y Arduino-ESP32 3.3.12. Flash 416 386 B (12.5 %),
  RAM estática 24 808 B (7.6 %).
- Los **tests nativos no se ejecutaron** todavía: el entorno de trabajo no tiene
  compilador de host (MinGW) para el env `native`. El build del firmware valida
  que los 9 tests al menos compilan en el árbol del proyecto.

## 8. Pendientes

> Esta entrega es **teórica**: no hay placa disponible y el conexionado físico
> se hace en la etapa de integración. Los puntos siguientes son de esa etapa.

1. **Medir el `PORF`** en la placa: con la pila `CR1225` puesta, un corte de
   alimentación no debe activar `PORF`.
2. **Confirmar en hardware** que el RTC responde en `0x52` sobre el bus 8/9 y que
   el modo 24 h queda grabado.
3. **Verificar el desfasaje** GPS vs RTC: se acepta ≤ 1 s (requisito F3).
4. **Decidir** si se usa el `INT` del RTC (`GPIO10`) para despertar al OBC en vez
   de sondear; hoy no se usa.
5. **Barrido de fecha**: el RTC solo representa 2000–2099; el firmware lo valida y
   descarta valores fuera de rango.

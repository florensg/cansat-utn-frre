# Power gating — Circuito electrónico y alimentación v.2

Diseño e implementación de la capa de conmutación de alimentación del OBC.

- **Fuente de hardware:** [`circuito-electronico-y-alimentacion-v2.pdf`](../../02-arquitectura/circuito-electronico-y-alimentacion-v2.pdf)
- **Mapeo de componentes:** [`mapeo-componentes-v2.md`](../../02-arquitectura/mapeo-componentes-v2.md)
- **Requisitos aplicables:** [`requisitos-competencia-v2.md`](../../01-requisitos/requisitos-competencia-v2.md)
- **Estado:** Fase 1 implementada; habilitación real pendiente del mapa GPIO de IEM 1.

## 1. Principio de hardware

Cada riel conmutado tiene una llave electrónica:

```text
GPIO (ESP32-S3) ──▶ gate 2N7002 ──▶ gate AO3401A (P-MOS) ──▶ riel VCC
                     pull-down 1k       pull-up 100k (riel OFF por defecto)
```

- **Polaridad:** `GPIO = HIGH` enciende el riel; `GPIO = LOW` o flotante lo apaga.
  El pull-up de 100 kΩ garantiza que el riel arranque **apagado**.
- **TPS2553 / TPS22946:** llaves con `EN` lógico y `FAULT` (no se usa `FAULT` en Fase 1).

> ⚠️ **Bloqueante conocido:** el netlist del PDF v.2 no etiqueta qué GPIO habilita cada
> llave. Hasta que IEM 1 entregue la lista, todos los pines valen `0xFF`
> (`kPowerPinUnassigned`) y el gating queda **desactivado**.

## 2. Rieles

| Enum | Nombre | Consumidores | Riel base |
|---|---|---|:---:|
| `Rtc` | `RTC` | RV-3028-C7 + CR1225 | sí |
| `Fram` | `FRAM` | MB85RC256V | sí |
| `Sensors` | `SENS` | MPU-6050, BMP280, INA219, LIS3MDL, ADS1115 | sí |
| `Gps` | `GPS` | NEO-M8N | sí |
| `Radio` | `RADIO` | XBee-PRO 900HP | sí |
| `Rpi` | `RPI` | Raspberry Pi Zero | carga útil |
| `XiaoCenit` | `XIAO_C` | XIAO ESP32-S3 cenit | carga útil |
| `XiaoNadir` | `XIAO_N` | XIAO ESP32-S3 nadir | carga útil |
| `Servo` | `SERVO` | SG90 (segundo paracaídas) | carga útil |
| `Sec` | `SEC` | ADS1115 + sensores solares / misión secundaria | carga útil |

Solo estos **5** rieles tienen llave conmutada (`EN_RPI`, `EN_CENIT`, `EN_NADIR`,
`EN_SERVO`, `EN_SEC`). Los rieles base no tienen GPIO de habilitación en las listas y se
consideran siempre alimentados.

La política vive en `lib/flight_core/src/power.cpp` (C++ puro, testeable en `native`).

## 3. Política por estado

`power_rails_for_state(state, payload_prepared)`:

| Estado | Base | Carga útil | Notas |
|---|:---:|:---:|---|
| `LaunchPad` | ON | OFF | La carga útil espera el comando `RPI` |
| `LaunchPad` + `RPI` | ON | ON | Override de la GCS en T-60 s |
| `Ascent` | ON | ON | Todo encendido |
| `Descent` | ON | ON | Todo encendido |
| `Landed` | ON | OFF | Ahorro para baliza y recuperación; el override se ignora |

- El override `payload_prepared` **solo** aplica fuera de `Landed`.
- Al apagar la carga útil se apagan RPi, ambas XIAO, el servo y `SEC`.

## 4. Comandos nuevos

| Comando | Efecto | Respuesta |
|---|---|---|
| `RPI` | Prepara la carga útil (enciende RPi + XIAO + servo) | `RPI OK (carga util preparada)` |
| `RPIOFF` | Apaga la carga útil | `RPIOFF OK (carga util apagada)` |

Flujo operativo:

```text
Interruptor E4 ──▶ STAND-BY (solo rieles base)
      │
      ├─ GCS envía RPI en T-60 s ──▶ RPi bootea
      │
      └─ Detección de lanzamiento ──▶ Ascent: todo ON + CMD:MISSION_START
```

El firmware **no** infiere el T-60 s: el instante lo decide la estación terrena.

## 5. Telemetría

El campo `OPTIONAL_DATA` incorpora la máquina de rieles:

```text
MAX:<alt>;P2:<0|1>;CMD:<eco>[;RPI:<resumen>];PWR:<máscara hex 4 dígitos>
```

`PWR` es la máscara de 10 bits, un bit por riel en el orden de la tabla anterior
(bit 0 = `RTC`, bit 9 = `SEC`). Ejemplo: `PWR:001F` = solo rieles base encendidos.

> No se altera el orden de las columnas del CSV principal; `PWR` va dentro de
> `OPTIONAL_DATA`.

## 6. Configuración

En `firmware/obc-esp32s3/include/config.h`:

| Constante | Valor actual | Descripción |
|---|---|---|
| `kPowerGatingEnabled` | `true` | Habilita el manejo real de GPIO |
| `kPowerPinRpi` | `11` | `EN_RPI` |
| `kPowerPinXiaoCenit` | `12` | `EN_CENIT` |
| `kPowerPinXiaoNadir` | `38` | `EN_NADIR` (reubicado) |
| `kPowerPinServo` | `39` | `EN_SERVO` (reubicado) |
| `kPowerPinSec` | `40` | `EN_SEC` (reubicado) |
| `kPowerPinRtc/Fram/Sensors/Gps/Radio` | `0xFF` | Rieles base sin llave |
| `kPowerRailSettleMs` | `50` | Espera tras conmutar un riel |
| `kXiaoBootAfterRailMs` | `4000` | Espera para re-sondear las XIAO |

### Cómo se habilitó

1. Se resolvió el conflicto GPIO14/15/16 a favor del **cuadro de conexionado**: las UART
   de RPi y XBee conservan GPIO13–18.
2. `EN_NADIR`/`EN_SERVO`/`EN_SEC` se movieron a **GPIO38/39/40**
   (ver [`pinmap-esp32s3-v2.md`](../../02-arquitectura/pinmap-esp32s3-v2.md) §4–§5).
3. `kPowerGatingEnabled = true`.
4. **Pendiente:** Catriel debe confirmar y cablear 38/39/40, y se debe medir con
   `INA219` el consumo por estado.

Con `kPowerGatingEnabled = false`, `power_manager` no toca ningún pin y reporta todos
los rieles encendidos: el comportamiento es idéntico al firmware anterior.

## 7. Seguridad de la implementación

- Los rieles se conmutan en el `loop()` a partir del estado de la FSM, sin bloqueos
  largos (solo `kPowerRailSettleMs` cuando hay cambio).
- Al encender el riel XIAO se reprograma su re-sondeo (`init_xiao_cameras()`).
- El servo se sigue comandando por PWM; su riel solo decide si tiene energía.
- El estado de los rieles se persiste implícitamente: al reiniciar se recalcula desde
  el estado de la FSM restaurado.

## 8. Pruebas

Tests nativos (`pio test -e native`, `test/test_flight_core/`):

- `test_power_policy_base_rails` — los 5 rieles base están ON en los 4 estados.
- `test_power_payload_policy` — override en `LaunchPad`, ON en vuelo, ignorado en `Landed`.
- `test_power_mask_helpers` — bits, `kPowerAll` y nombres.
- `test_commands_parser` — `RPI` / `RPIOFF` y rechazo de `RPION` / `$RPI,...`.

> Ejecución pendiente: PlatformIO no está instalado en el entorno actual. La lógica
> pura se puede compilar con cualquier toolchain C++17 si se dispone de `unity.h`.

## 9. Pendientes

1. **Cablear `EN_NADIR`/`EN_SERVO`/`EN_SEC` en GPIO38/39/40** (Catriel). El firmware ya
   los maneja con `kPowerGatingEnabled = true`, pero el hardware sigue cableado en
   14/15/16 según la lista original de transistores.
2. **Baudios del UART del XBee-PRO 900HP** — `config.h` usa `115200`; el valor de fábrica
   típico es `9600`, a confirmar en XCTU.
3. **Banda del XBee-PRO 900HP:** configurar **915–928 MHz** (Anexo A v2, X1), no 902–928.
4. **Tiempo de boot de la RPi** con la imagen definitiva, para ajustar el T-60 s.
5. **Fase 5:** LED de estado (E6) y baliza audible (C8/E8/E9). La baliza usa GPIO6 pero
   **no está conectada** en el cuadro (fila sin resaltar).
6. **Medir consumo por estado** con `INA219` para validar la política.

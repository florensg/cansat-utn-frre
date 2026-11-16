# Correcciones de conexionado y estado del firmware (circuito v.2)

Documento de cierre de la etapa teórica. Registra **qué se corrigió en las conexiones**, la **lista completa y nueva de todo lo conectado** y **cómo quedó el código** del OBC tras las fases 1–3.

- Fecha: **2026-11-16**
- Base: [`pinmap-esp32s3-v2.md`](pinmap-esp32s3-v2.md) (guía de pines), [`Cuadro_conexionado_ESP32S3_CanSat_v2.docx`](fuentes/cuadro-conexionado-esp32s3-cansat-v2.docx) (amarillo = conectado), [`Conexion de transistores.jpeg`](fuentes/conexion-transistores.jpeg) y [`circuito-electronico-y-alimentacion-v2.pdf`](circuito-electronico-y-alimentacion-v2.pdf).
- Alcance: **sin placa**. El conexionado físico se arma en la etapa de integración; acá se deja el mapa definitivo que debe seguir el arnés.

---

## 1. Resumen de las correcciones

1. **UARTs separadas de las llaves de alimentación.** Las tres UART (GPS, XBee y Raspberry Pi) quedaron en GPIO13–18 y las llaves `EN_NADIR`/`EN_SERVO`/`EN_SEC` se movieron a GPIO38/39/40.
2. **Raspberry Pi con UART propia.** Antes compartía `Serial2` con el XBee; ahora usa `HardwareSerial(0)` remapeada a RX14/TX13 y cada enlace tiene su propio *polling*.
3. **MPU-6050 INT movido de GPIO14 a GPIO4**, porque GPIO14 pasó a ser el RX de la RPi.
4. **Fallbacks eliminados:** el bus alternativo `Wire1` en 13/14 y el modo SPI del BMP280 (su `CSB` está atado a 3V3, va por I²C en `0x76`).
5. **Power gating activado** (`kPowerGatingEnabled = true`) con 5 rieles conmutables y 5 base.
6. **Tiempo de misión con RTC**: jerarquía GPS > comando `ST` > RTC > `millis()`.
7. **LED de estado y watchdog** implementados; la baliza audible queda fuera del firmware por requisito (batería e interruptor independientes).

---

## 2. Correcciones de conexiones, una por una

| Señal / bloque | Situación anterior | Asignación final | Motivo |
| --- | --- | --- | --- |
| `EN_NADIR` | GPIO14 | **GPIO38** | GPIO14 es RX de la RPi |
| `EN_SERVO` | GPIO15 | **GPIO39** | GPIO15 es RX del XBee |
| `EN_SEC` | GPIO16 | **GPIO40** | GPIO16 es TX del XBee |
| MPU-6050 `INT` | GPIO14 | **GPIO4** | GPIO14 quedó para la UART de la RPi |
| GPS NEO-M8N | desalineado con el cuadro | **RX 17 / TX 18** (`Serial1`, 9600 8N1) | Coincidir con el cuadro de conexionado |
| XBee-PRO 900HP | compartía `Serial2` con la RPi | **RX 15 / TX 16** (`Serial2`) | Enlace de radio independiente |
| Raspberry Pi Zero | compartía `Serial2` con el XBee | **RX 14 / TX 13** (`HardwareSerial(0)` remapeada) | Enlace de carga útil independiente |
| Bus I²C alternativo `Wire1` | existía en GPIO13/14 | **eliminado** | Chocaba con la UART de la RPi |
| BMP280 por SPI | fallback con CS en GPIO4 | **eliminado**, solo I²C `0x76` | `CSB`→3V3 fuerza I²C; GPIO4 es el INT del MPU |
| Baliza (GPIO6) | declarada “por definir” | **reservada y sin uso** | C8/E8/E9 exigen hardware independiente |
| LED de estado | no existía | lógica lista, **pines sin asignar** | Falta cableado (etapa de integración) |

>**Decisión de fondo (2026-11-16):** ante el conflicto GPIO14/15/16 entre el cuadro de conexionado y la lista de transistores, **manda el cuadro**. Las UART se quedan en 13–18 y las tres llaves se reasignan a 38/39/40. Ver §5 del [`pinmap-esp32s3-v2.md`](pinmap-esp32s3-v2.md).

> ⚠️ **No volar con el arnés viejo.** Si las compuertas de `EN_NADIR`/`EN_SERVO`/`EN_SEC` siguen físicamente en GPIO14/15/16, esas líneas quedan en HIGH por las UART y los tres rieles estarían **siempre encendidos**.

---

## 3. Lista completa y nueva de conexiones

### 3.1 Alimentación

**Rieles base (siempre alimentados, sin GPIO de habilitación):**

| Riel | Alimenta |
| --- | --- |
| RTC | RV-3028-C7 + pila CR1225 |
| FRAM | MB85RC256V |
| Sensores | MPU-6050, BMP280, INA219 |
| GPS | NEO-M8N |
| Radio | XBee-PRO 900HP |
**Rieles conmutables (5):**
| Señal | Carga | GPIO | Conexión |
| --- | --- | --- | --- |
| `EN_RPI` | Raspberry Pi Zero 2W | **11** | GPIO → 1 kΩ → gate 2N7002 |
| `EN_CENIT` | XIAO ESP32-S3 Sense cenit | **12** | GPIO → 1 kΩ → gate 2N7002 |
| `EN_NADIR` | XIAO ESP32-S3 Sense nadir | **38** | GPIO → 1 kΩ → gate 2N7002 |
| `EN_SERVO` | Servo SG90 | **39** | GPIO → 1 kΩ → gate 2N7002 |
| `EN_SEC` | ADS1115 + TSL251RD (misión secundaria) | **40** | GPIO → 1 kΩ → gate 2N7002 |

Cada llave: `GPIO → 1 kΩ → gate del 2N7002`; el drenador lleva a GND el gate del `AO3401A` y un pull-up de 100 kΩ mantiene el riel **apagado** por defecto (activo en HIGH).

### 3.2 Bus I²C principal — GPIO 8 (SDA) / GPIO 9 (SCL)

| Dispositivo | Dirección | Alimentación | Notas |
| --- | --- | --- | --- |
| BMP280 | `0x76` | 3V3 | `CSB`→3V3, `SDO`→GND |
| MPU-6050 (GY-521) | `0x68` | 5 V al breakout | `AD0`→GND, `INT`→GPIO4 |
| INA219 | `0x40` | 3V3 | Shunt en serie con la rama de batería |
| FRAM MB85RC256V | `0x50` | 3V3 | `A0..A2`→GND, `WP`→GND |
| RV-3028-C7 | `0x52` | 3V3 + VBACKUP | `INT`→GPIO10 (opcional, no usado) |

### 3.3 UARTs

| Enlace | GPIO ESP32-S3 | Objeto de firmware | Parámetros |
| --- | --- | --- | --- |
| GPS NEO-M8N | RX **17**, TX **18** | `Serial1` | 9600 8N1 |
| XBee-PRO 900HP | RX **15**, TX **16** | `Serial2` | 115200 8N1 (**baud a confirmar**) |
| Raspberry Pi Zero 2W | RX **14**, TX **13** | `g_rpi_serial` (`HardwareSerial(0)`) | 115200 8N1 |
| Monitor USB | nativo | `Serial` (USB-CDC) | 115200 |

### 3.4 GPIO discretos

O | Función | Estado |
| --- | --- | --- |
| GPIO1 | ADC del panel solar acondicionado | Conectado (requiere divisor) |
| GPIO4 | INT del MPU-6050 | Conectado |
| GPIO5 | PPS del NEO-M8N | **No conectado** (opcional) |
| GPIO6 | Baliza audible (vía driver) | **Reservado, sin uso en firmware** |
| GPIO7 | Mecanismo de liberación (vía driver) | Conectado |
| GPIO10 | INT del RV-3028-C7 | Conectado (no usado: se sondea por I²C) |

### 3.5 Pines libres

GPIO **2, 21, 41, 42, 47, 48** (DevKitC-1 N8R8, fuera de flash/PSRAM, USB, UART0 y strapping).

### 3.6 Pines de LED de estado

`kPinStatusLedOk` y `kPinStatusLedFault` valen `kPowerPinUnassigned` (`0xFF`). Al asignarlos en `config.h`, `status_output.cpp` empieza a manejarlos solo: no hay que tocar ninguna otra parte del código.

---

## 4. Cómo quedó el código

### 4.1 Estructura

| Archivo | Rol |
| --- | --- |
| `include/config.h` | **Único** pinmap, constantes de power gating, watchdog y direcciones I²C |
| `lib/flight_core/.../fsm.*` | Máquina de estados (`LaunchPad → Ascent → Descent → Landed`) |
| `lib/flight_core/.../flight.*` | Detección de apogeo y del 80 % |
| `lib/flight_core/.../sensors.*` | `SensorSample` / interfaz de sensores |
| `lib/flight_core/.../power.*` | Política de rieles por estado (puro) |
| `lib/flight_core/.../commands.*` | Parser de comandos |
| `lib/flight_core/.../storage.*` | Estado persistente (FRAM o RAM) |
| `lib/flight_core/.../telemetry.*` | Formato CSV (columnas del contrato) |
| `lib/flight_core/.../mission_time.*` | Calendario, epoch, BCD y `MissionClock` con precedencia de fuentes |
| `lib/flight_core/.../status.*` | `SystemHealth`, `SystemSeverity` y patrones de LED |
| `src/main.cpp` | Integración: sensores, NMEA, comandos, telemetría, lazo |
| `src/power_manager.*` | Actuación de GPIO de las llaves y máscara `PWR` |
| `src/rtc_rv3028.*` | Driver del RTC (registros, 24 h, `PORF`) |
| `src/status_output.*` | `pinMode`/`digitalWrite` de los LED (no-op si no están asignados) |
| `src/watchdog.*` | TWDT de 4 s y causa del reinicio |
| `test/test_flight_core/...` | 30 tests de lógica pura (env `native`) |

### 4.2 Power gating

`power_rails_for_state()` decide la máscara de rieles:

| Estado | Rieles encendidos |
| --- | --- |
| `LaunchPad` | base (RTC, FRAM, sensores, GPS, radio) |
| `Ascent` / `Descent` | base + RPi + XIAO cenit + XIAO nadir + servo + SEC |
| `Landed` | base (se apaga la carga útil para ahorrar batería) |
| Override `RPI` | agrega la carga útil en cualquier estado **salvo** `Landed` |
La máscara se publica en telemetría como `PWR:%04X` (`001F` en el arranque, `03FF` con la carga útil encendida). Tras conmutar un riel se espera `kPowerRailSettleMs` (50 ms) antes de hablar con el dispositivo, y las XIAO tienen una ventana de arranque de 4 s antes de re-sondearlas.

### 4.3 Tiempo de misión

| Fuente | Cuándo gana |
| --- | --- |
| GPS | `$--RMC` con estado `A` (fecha + hora). Escribe el RTC una vez por encendido |
| Comando `ST` | Siempre; abre ventana de gracia de 5 s para que el GPS no la revierta |
| RTC | Sin GPS y con `PORF = 0` |
| `millis()` | Sin sincronizar (arranca en 2000-01-01, `RTC_VALID = 0`) |
`$--GGA` aporta posición y hora del día; la **fecha** solo llega por `$--RMC`. La fuente activa se publica como `TSRC` (`G`/`C`/`R`/`M`/`-`).

### 4.4 LED de estado y watchdog

| Severidad | Condición | LED OK | LED de falla |
| --- | --- | --- | --- |
| `Ok` | todo presente | fijo | apagado |
| `Degraded` | falta IMU/INA219/FRAM/RTC o `PORF` | 1 Hz | apagado |
| `Fault` | falta el **barómetro** | apagado | 1 Hz |

- TWDT: 4 s, `trigger_panic`; se activa al final de `setup()` y se refresca al inicio de cada `loop()`.
- Causa de reinicio: `RST:<P|S|W|I|X|B|E|D|O|?>` en el banner y en `OPTIONAL_DATA`.

### 4.5 Comunicaciones

- Tres enlaces independientes, cada uno con su *polling*: `poll_gps()`, `poll_rpi_serial()`, `poll_radio_commands()`.
- La telemetría sale por **USB, XBee y RPi**.
- `CMD:MISSION_START;STATE:Ascent` se envía por los tres al detectar lanzamiento.
- Las respuestas a comandos (`CX`, `ST`, `RST`) van a **USB y XBee**.
- Las tramas `$RPI,...` solo se aceptan por la UART de la RPi; el parser de comandos las ignora explícitamente.

### 4.6 Telemetría

El **orden de columnas del CSV principal no cambió**. `OPTIONAL_DATA` (192 B) quedó así:

```text
MAX:<m>;P2:<0|1>;CMD:<eco>;[RPI:<resumen>;]PWR:<máscara hex>;TSRC:<G|C|R|M|->;RST:<tag>
```

### 4.7 Comandos

| Comando | Efecto |
| --- | --- |
| `CX` | Estado: `CX OK <STATE> PKT=… BATT=… CURR=… SOLAR=… TIME=hh:mm:ss UTC=<0/1> TSRC=<x>` |
| `ST HH:MM:SS` | Ajusta la hora, conserva la fecha |
| `ST YYYY-MM-DDTHH:MM:SS` | Ajusta fecha y hora; escribe el RTC |
| `CAL` | Calibración de altitud a cero |
| `MEC` | Activa el mecanismo de liberación |
| `RPI` | Enciende la carga útil (override hasta `Landed`) |
| `RPIOFF` | Apaga la carga útil |
| `RST` | Reinicia la máquina de estados |

---

## 5. Verificación de esta etapa

| Ítem | Resultado |
| --- | --- |
| `pio run -e obc` | ✅ **SUCCESS** (PlatformIO 6.2.0, `platform-espressif32` 8.0.0, Arduino-ESP32 3.3.12) |
| Flash | 416 386 B de 3 342 336 B (**12.5 %**) |
| RAM estática | 24 808 B de 327 680 B (**7.6 %**) |
| Algoritmo de tiempo | ✅ réplica en Python vs `datetime`: 28 074 fechas + 5 000 aleatorias sin discrepancias |
| `pio test -e native` | ⏸ no ejecutado (falta compilador de host/MinGW) |
| Validación en placa | ⏸ etapa de integración |

---

## 6. Checklist para la etapa de integración

1. Armar las 5 llaves con el esquema `GPIO → 1 kΩ → 2N7002 → AO3401A`.
2. Verificar que **ningún** `EN_*` comparte GPIO con las UART (hay un test que lo controla).
3. Sin alimentar el ESP32-S3, medir que los rieles conmutados quedan **cortados** (pull-up de 100 kΩ).
4. Al energizar, el banner debe mostrar `PWR:001F`.
5. Enviar `RPI`: `PWR` debe pasar a `03FF` y medirse ~3,7 V en cada riel.
6. Confirmar el RTC en `0x52`: `RTC:OK …` y `PORF = 0` con la CR1225 puesta.
7. Confirmar el desfasaje GPS vs RTC ≤ 1 s (F3).
8. Asignar los pines de LED y verificar los patrones.
9. Provocar un cuelgue y confirmar reinicio por TWDT con `RST:W` en telemetría.
10. Confirmar el baud real del XBee y que opere en 915–928 MHz (X1).

---

## 7. Documentos relacionados

- [`pinmap-esp32s3-v2.md`](pinmap-esp32s3-v2.md) — mapa de pines y conflicto 14/15/16.
- [`mapeo-componentes-v2.md`](mapeo-componentes-v2.md) — componentes del esquemático.
- [`../03-subsistemas/software-vuelo/power-gating-v2.md`](../03-subsistemas/software-vuelo/power-gating-v2.md) — política de rieles.
- [`../03-subsistemas/software-vuelo/tiempo-mision-v2.md`](../03-subsistemas/software-vuelo/tiempo-mision-v2.md) — RTC y jerarquía de tiempo.
- [`../03-subsistemas/software-vuelo/indicadores-v2.md`](../03-subsistemas/software-vuelo/indicadores-v2.md) — LED, watchdog y causa de reinicio.
- [`../01-requisitos/requisitos-competencia-v2.md`](../01-requisitos/requisitos-competencia-v2.md) — trazabilidad C/E/X/F.

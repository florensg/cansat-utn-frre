# Software de vuelo del OBC

Firmware del **ESP32-S3 DevKitC-1** (`U1`) para el CanSat, alineado con el circuito vigente [`circuito-electronico-y-alimentacion-v2.pdf`](../../docs/02-arquitectura/circuito-electronico-y-alimentacion-v2.pdf). Responsable: Florencia González (ISI 1).

## Estructura

```text
firmware/
├── obc-esp32s3/            Software de vuelo del OBC (ESP32-S3 DevKitC-1)
│   ├── platformio.ini      Entornos: obc (vuelo) y native (tests unitarios)
│   ├── include/config.h    Placa, pines, buses I2C/SPI/UART y direcciones v.2
│   ├── src/main.cpp        Drivers v.2, scheduler y enlace con RPi/XIAO/XBee
│   ├── src/power_manager.* Power gating: traducción de rieles a GPIO
│   ├── lib/flight_core/    Lógica pura C++ (FSM, apogeo, telemetría, comandos, power)
│   └── test/               16 tests unitarios nativos
├── payload-rpi/            Software de la Raspberry Pi Zero W (LIS3MDL + ADS1115 + UART)
└── payload-xiao/           Firmware de las 2 cámaras XIAO ESP32-S3 Sense (U8 VIS / U9 NIR)
```

## Mapeo de hardware según `circuito-electronico-y-alimentacion-v2.pdf`

| Designador | Componente | Interfaz en v.2 | Driver en firmware |
|---|---|---|---|
| `U1` | `ESP32-S3 DevKitC-1` (44 pines) | Controlador principal | `firmware/obc-esp32s3/src/main.cpp` |
| `MPU1` | `MPU-6050` | I2C `0x68` + `INT` en `GPIO 4` | `init_mpu6050()` / `read_mpu6050()` |
| `U2` | `BMP280` | I2C `0x76` (`CSB`→3V3, `SDO`→GND) | `init_barometer()` / `read_barometer()` |
| `INA1` | `INA219` + batería 18650 1S `3.7V` | I2C `0x40` | `init_ina219()` / `read_ina219()` |
| `VRAM1` | `FRAM MB85RC256V` | I2C `0x50` (32 KB) | `fram_write_bytes()` / `fram_read_bytes()` (doble copia + CRC) |
| `RV1` | `RV-3028-C7` + `CR1225` | I2C `0x52`, `INT` en `GPIO10` | `src/rtc_rv3028.cpp` + `init_rtc()`: jerarquía GPS > comando `ST` > RTC > `millis()` (ver [`tiempo-mision-v2.md`](../../docs/03-subsistemas/software-vuelo/tiempo-mision-v2.md)) |
| `U3` | `GPS NEO-M8N` | UART1 (`9600 8N1`): RX `GPIO17`, TX `GPIO18` | `poll_gps()` / `parse_nmea_line()` (posición + hora UTC) |
| `M1` | `Servo SG90` | PWM en `GPIO 7` (mecanismo, vía driver) | `pulse_servo()` |
| `XBee-PRO 900HP` | Radio 900 MHz | UART2: RX `GPIO15`, TX `GPIO16` (baud por confirmar) | Transmisión CSV 1 Hz + recepción de comandos |
| `RPI1` | `Raspberry Pi Zero` | I2C (`GPIO 2/3`) + UART: RX `GPIO14`, TX `GPIO13` | `firmware/payload-rpi/src/main.py` (`$RPI,...`) |
| `1` | `LIS3MDL` | I2C de RPi (`0x1C`, `INT` en `GPIO 17`) | Leído en RPi (`RpiI2cSensors`) y enviado al OBC (también soportado en I2C directo del OBC) |
| `ADS1115` + `U4..U7` | `ADS1115` (`0x48`) + 4× `TSL251RD` | `A3..A0` en I2C de RPi (`GPIO 2/3`) | Leído en RPi (`RpiI2cSensors`) y enviado al OBC (también soportado en I2C directo del OBC) |
| `U8` / `U9` | 2× `XIAO ESP32-S3 Sense` | I2C `D4(SDA)` / `D5(SCL)` (`0x30` VIS, `0x31` NIR) | `firmware/payload-xiao/src/main.cpp` + `trigger_xiao_capture()` |

> **Hora UTC (implementado):** jerarquía **GPS > comando `ST` > RTC > `millis()`**.
> El RTC `RV-3028-C7` (I2C `0x52`, respaldo `CR1225`) se sondea en `setup()` y, si
> `PORF = 0`, arranca el reloj de misión con fecha y hora. El GPS escribe el RTC una
> sola vez por encendido desde la trama `$--RMC`; el comando `ST` acepta
> `HH:MM:SS` o `YYYY-MM-DDTHH:MM:SS` y también persiste en el RTC. La fuente activa
> se publica en `OPTIONAL_DATA` como `TSRC` (`G`/`C`/`R`/`M`/`-`).
> Detalle completo: [`tiempo-mision-v2.md`](../../docs/03-subsistemas/software-vuelo/tiempo-mision-v2.md).

> **Power gating:** las 5 llaves conmutables son `EN_RPI` (GPIO11), `EN_CENIT` (GPIO12),
> `EN_NADIR` (GPIO38), `EN_SERVO` (GPIO39) y `EN_SEC` (GPIO40). La política por estado
> está en `lib/flight_core/src/power.cpp` y la actuación en `src/power_manager.cpp`.
> Está **activo** (`kPowerGatingEnabled = true`); Catriel debe cablear 38/39/40.
> Ver [`pinmap-esp32s3-v2.md`](../../docs/02-arquitectura/pinmap-esp32s3-v2.md)
> y [`power-gating-v2.md`](../../docs/03-subsistemas/software-vuelo/power-gating-v2.md).
>
> **UARTs:** `Serial` = USB-CDC; `Serial1` = GPS (RX17/TX18); `Serial2` = XBee
> (RX15/TX16); `g_rpi_serial` = `HardwareSerial(0)` remapeada a RX14/TX13 para la RPi.

> **Indicadores y watchdog:** el LED de estado (E6) usa el semáforo de salud de
> `lib/flight_core/src/status.cpp` (fijo = OK, 1 Hz = degradado, falla = LED de
> falla parpadeando) con actuación en `src/status_output.cpp`. Los pines siguen
> sin asignar hasta el cableado. El TWDT de 4 s (`src/watchdog.cpp`) se activa al
> final de `setup()` y la causa del reinicio viaja en `OPTIONAL_DATA` como
> `RST:<tag>`. La **baliza audible no la controla el OBC** (C8/E8/E9: batería e
> interruptor independientes). Ver
> [`indicadores-v2.md`](../../docs/03-subsistemas/software-vuelo/indicadores-v2.md).

## Compilación y carga

```bash
pio run -e obc
```

### Estado de verificación (2026-11-16)

| Ítem | Resultado |
|---|---|
| `pio run -e obc` | ✅ **SUCCESS** — PlatformIO 6.2.0, `platform-espressif32` 8.0.0, Arduino-ESP32 3.3.12 |
| Flash | 416 386 B de 3 342 336 B (12.5 %) |
| RAM estática | 24 808 B de 327 680 B (7.6 %) |
| `pio test -e native` | ⏸ **no ejecutado** — falta compilador de host (MinGW) |

> La compilación valida la rama de API del TWDT para Arduino-ESP32 3.x/IDF 5.x.
> La validación en placa (RTC en `0x52`, `PORF`, LED, watchdog real) queda para
> la etapa de integración: esta entrega es teórica y no requiere hardware.

## Tests unitarios

```bash
pio test -e native
```

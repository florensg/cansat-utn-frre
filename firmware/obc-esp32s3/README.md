# Software de vuelo del OBC

Firmware del **ESP32-S3 DevKitC-1** (`U1`) para el CanSat, alineado con el circuito definitivo [`PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf`](../../PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf). Responsable: Florencia González (ISI 1).

## Estructura

```text
firmware/
├── obc-esp32s3/            Software de vuelo del OBC (ESP32-S3 DevKitC-1)
│   ├── platformio.ini      Entornos: obc (vuelo) y native (tests unitarios)
│   ├── include/config.h    Placa, pines, buses I2C/SPI/UART y direcciones v3
│   ├── src/main.cpp        Drivers v3, scheduler y enlace con RPi/XIAO/XBee
│   ├── lib/flight_core/    Lógica pura C++ (FSM, apogeo, telemetría, comandos)
│   └── test/               13 tests unitarios nativos
├── payload-rpi/            Software de la Raspberry Pi Zero W (LIS3MDL + ADS1115 + UART)
└── payload-xiao/           Firmware de las 2 cámaras XIAO ESP32-S3 Sense (U8 VIS / U9 NIR)
```

## Mapeo de hardware según `PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf`

| Designador | Componente | Interfaz en v3 | Driver en firmware |
|---|---|---|---|
| `U1` | `ESP32-S3 DevKitC-1` (44 pines) | Controlador principal | `firmware/obc-esp32s3/src/main.cpp` |
| `MPU1` | `MPU-6050` | I2C `0x68` + `INT` en `GPIO 14` | `init_mpu6050()` / `read_mpu6050()` |
| `U2` | `BMP280` | SPI (`SDI=8, SCK=9, CSB=4, SDO=5`) o I2C `0x76/0x77` | `init_barometer()` / `read_barometer()` |
| `INA1` | `INA219` + `BAT1 3.7V` | I2C `0x40` | `init_ina219()` / `read_ina219()` |
| `VRAM1` | `FRAM MB85RC256V` | I2C `0x50` (32 KB) | `fram_write_bytes()` / `fram_read_bytes()` (doble copia + CRC) |
| `U3` | `GPS NEO-M8N` | UART1 (`9600 8N1`) | `poll_gps()` / `parse_nmea_line()` (posición + hora UTC) |
| `M1` | `Servo SG90` | PWM en `GPIO 7` | `pulse_servo()` |
| `XBee S1` | Radio 2.4 GHz (`USART1`, `I1=1A`) | UART2 (`115200 8N1`) | Transmisión CSV 1 Hz + recepción de comandos |
| `RPI1` | `Raspberry Pi Zero W` | I2C (`GPIO 2/3`) + UART (`GPIO 14/15`) | `firmware/payload-rpi/src/main.py` (`$RPI,...`) |
| `1` | `LIS3MDL` | I2C de RPi (`0x1C`, `INT` en `GPIO 17`) | Leído en RPi (`RpiI2cSensors`) y enviado al OBC (también soportado en I2C directo del OBC) |
| `ADS1115` + `U4..U7` | `ADS1115` (`0x48`) + 4× `TSL251RD` | `A3..A0` en I2C de RPi (`GPIO 2/3`) | Leído en RPi (`RpiI2cSensors`) y enviado al OBC (también soportado en I2C directo del OBC) |
| `U8` / `U9` | 2× `XIAO ESP32-S3 Sense` | I2C `D4(SDA)` / `D5(SCL)` (`0x30` VIS, `0x31` NIR) | `firmware/payload-xiao/src/main.cpp` + `trigger_xiao_capture()` |

> **Nota sobre la hora UTC:** En `PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf` se eliminó el módulo RTC externo (`RV1`). El firmware obtiene la hora UTC directamente de las tramas `$GPGGA`/`$GNGGA` del GPS `NEO-M8N` (o del comando `ST HH:MM:SS`) y la mantiene con `millis()`.

## Compilación y carga

```bash
pio run -e obc
```

## Tests unitarios

```bash
pio test -e native
```

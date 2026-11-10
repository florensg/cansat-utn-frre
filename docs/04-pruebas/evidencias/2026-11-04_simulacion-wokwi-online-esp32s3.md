# Registro y guía: Simulación interactiva en Wokwi Online (`ESP32-S3`, Circuito v3)

- **Fecha de creación:** 2026-11-04
- **Última actualización:** 2026-11-10
- **Responsable:** Florencia González (`ISI 1` — Software de vuelo, telemetría y determinación de actitud)
- **Placa simulada:** `ESP32-S3 DevKitC-1` de 44 pines (Elementos v2, `C-002`)
- **Referencia de hardware:** [`PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf`](../../../PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf) y [`Anexo A v2 .pdf`](../../01-requisitos/competencia/v2/Anexo%20A%20v2%20.pdf)
- **Archivos asociados:**
  - [`firmware/obc-esp32s3/diagram.json`](../../../firmware/obc-esp32s3/diagram.json)
  - [`firmware/obc-esp32s3/wokwi_online_sketch.ino`](../../../firmware/obc-esp32s3/wokwi_online_sketch.ino)
  - [`firmware/obc-esp32s3/include/config.h`](../../../firmware/obc-esp32s3/include/config.h)
  - [`firmware/obc-esp32s3/src/main.cpp`](../../../firmware/obc-esp32s3/src/main.cpp)
  - [`firmware/payload-rpi/src/main.py`](../../../firmware/payload-rpi/src/main.py)
  - [`firmware/payload-xiao/src/main.cpp`](../../../firmware/payload-xiao/src/main.cpp)

---

## 1. Cambios clave incorporados desde `PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf`

1. **Eliminación del RTC externo (`RV-3028-C7` / `DS1307`):**
   - El esquemático v3 eliminó el bloque `RV1`.
   - Se retiró `rtc1` (`wokwi-ds1307` en `Wire1`) de [`diagram.json`](../../../firmware/obc-esp32s3/diagram.json).
   - El tiempo UTC (`RTC_H`, `RTC_M`, `RTC_S`, `RTC_VALID`) se mantiene mediante `TimeKeeper`, sincronizado con las sentencias `$GPGGA`/`$GNGGA` del GPS `NEO-M8N` o con el telecomando `ST HH:MM:SS` ($\pm 1\text{ s}$ según requisito **F3** de [`Anexo A v2 .pdf`](../../01-requisitos/competencia/v2/Anexo%20A%20v2%20.pdf)).
2. **Separación estricta Vuelo (`env:obc`) vs. Simulación (`env:sim` / `CANSAT_SIMULATION`):**
   - En [`platformio.ini`](../../../firmware/obc-esp32s3/platformio.ini), el entorno `env:obc` compila sin `CANSAT_SIMULATION`: nunca inyecta tramas NMEA sintéticas, no reemplaza el `INA219` con valores fijos ni usa potenciómetros de Wokwi.
   - El entorno `env:sim` define `-D CANSAT_SIMULATION=1` para pruebas interactivas en Wokwi (`wokwi.toml`).
3. **Drivers completos de `PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf`:**
   - **FRAM `MB85RC256V` (`0x50`):** Driver I2C real con direccionamiento de 16 bits, doble copia redundante (`0x0000` y `0x0040`) y verificación de checksum (`PersistentState`). Persiste `packet_count`, `mission_time_ms`, `altitude_offset_m`, `state` y `second_parachute_deployed` ante reinicios (**F1, F2, F5**).
   - **`BMP280` (`U3`):** Autodetección en bus I2C (`0x76`/`0x77`) y bus SPI en `GPIO 8 (SDI/MOSI)`, `GPIO 9 (SCK)`, `GPIO 4 (CSB)` y `GPIO 5 (SDO/MISO)`. En modo simulación soporta además el stand-in `BMP180` (`0x77`).
   - **`MPU-6050` (`0x68` + `INT` en `GPIO 14`):** Acelerómetro, giroscopio y temperatura.
   - **`INA219` (`0x40`):** Tensión de batería (`BAT1 = 3.7V`) y corriente (`shunt = 0.1 Ω`).
   - **`SG90` (`M1`):** Señal PWM de liberación del segundo paracaídas en `GPIO 7` (esquemático v3) y espejo en `GPIO 15` (compatibilidad con arnés/Wokwi).
   - **`Raspberry Pi Zero W` (`RPI1`):** Lee en su bus I2C1 (`GPIO 2/3`) el magnetómetro `LIS3MDL` (`0x1C`, `INT` en `GPIO 17`) y el `ADS1115` (`0x48`, `U4->A3`, `U5->A2`, `U6->A1`, `U7->A0`), calcula actitud y envía `$RPI,...` por UART (`GPIO 14/15`) al OBC.
   - **Cámaras `XIAO ESP32-S3 Sense` (`U8` en `0x30` y `U9` en `0x31`):** Esclavos I2C en `D4 (SDA)` / `D5 (SCL)` disparados por el OBC en el lanzamiento (`0x01`), apogeo (`0x02`), 80 % (`0x04`) y aterrizaje (`0x03`), grabando secuencias MJPEG a resolución SVGA (`800x600 >= 640x480`, requisitos **SN6, SN7, C6, C7**).

---

## 2. Cobertura del Circuito v3 en Wokwi (`diagram.json`)

| Bloque de `PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf` | Interfaz en v3 | En Wokwi (`diagram.json`) | Implementación en el firmware |
|---|---|---|---|
| `ESP32-S3 DevKitC-1` (`U1`) | — | **Nativo** (`board-esp32-s3-devkitc-1`) | Placa objetivo real |
| `MPU-6050` | I2C `0x68`, `INT` en `GPIO 14` | **Nativo** (`wokwi-mpu6050`) | Driver I2C real |
| `BMP280` (`U3`) | SPI (`GPIO 8/9/4/5`) o I2C (`0x76`) | **Stand-in:** `board-bmp180` (`0x77`) | Driver real `BMP280` (I2C + SPI) + fallback `BMP180` en `CANSAT_SIMULATION` |
| `INA219` (`U6`) | I2C `0x40` | **Sin parte nativa** | Driver I2C real; estímulo `3.70 V / 185 mA` solo en `CANSAT_SIMULATION` |
| `MB85RC256V` (FRAM) | I2C `0x50` | **Sin parte nativa** | Driver I2C de 16 bits con doble copia (`0x0000`/`0x0040`) + fallback RAM |
| `NEO-M8N` (`GPS1`) | UART1 `9600` (`RX 18 / TX 17`) | **Sin parte nativa** | Parser NMEA `$GPGGA`/`$GNGGA` real + sincronización UTC |
| `SG90` (`M1`) | PWM `GPIO 7` (y `GPIO 15`) | **Nativo** (`wokwi-servo`) | Pulso de `544–2400 µs` en `GPIO 7` y `GPIO 15` (`0°` cerrado, `90°` liberado) |
| `RPI1` + `LIS3MDL` + `ADS1115` + `U4..U7` | RPi I2C1 (`GPIO 2/3`) + UART (`GPIO 1/2`) | **Stand-in:** 4 potenciómetros `opt1..opt4` o trama `$RPI,...` por serie | Firmware completo en [`firmware/payload-rpi/src/main.py`](../../../firmware/payload-rpi/src/main.py) + parser `$RPI,...` y driver directo `LIS3MDL`/`ADS1115` en OBC |
| `U8` / `U9` (`2x XIAO ESP32-S3 Sense`) | I2C `0x30` / `0x31` (`D4/D5`) | **Anotadas** | Firmware completo en [`firmware/payload-xiao/src/main.cpp`](../../../firmware/payload-xiao/src/main.cpp) + triggers I2C en OBC |

---

## 3. Contrato de telemetría y telecomandos (`Anexo A v2`)

### 3.1 Trama CSV a 1 Hz (`X5`)

```text
TEAM_ID,MISSION_TIME_MS,PACKET_COUNT,MODE,STATE,ALT_M,PRESS_PA,TEMP_C,BATT_V,CURR_MA,
ACC_X,ACC_Y,ACC_Z,GYR_X,GYR_Y,GYR_Z,LAT,LON,GPS_ALT_M,SATS,
MAG_X_UT,MAG_Y_UT,MAG_Z_UT,OPT1,OPT2,OPT3,OPT4,RTC_H,RTC_M,RTC_S,RTC_VALID,GPS_FIX,
OPTIONAL_DATA
```

### 3.2 Telecomandos soportados (`X3`, `X6`, `F2`, `F3`, `F4`)

- **Automático en separación/despegue (`X3`):** Al detectar el lanzamiento (`Ascent`), el OBC transmite automáticamente `CMD:MISSION_START;STATE:Ascent`.
- **`CX` (`X6`, `F4`):** Devuelve estado de vuelo, conteo de paquetes, estado de batería (`BATT`, `CURR`) y señal de los 4 paneles solares (`SOLAR=opt1,opt2,opt3,opt4`):
  ```text
  CX OK <STATE> PKT=<N> BATT=<V>V CURR=<mA>mA SOLAR=<U4>,<U5>,<U6>,<U7>
  ```
- **`ST HH:MM:SS` (`F3`):** Ajusta la hora UTC del CanSat desde la estación terrena con resolución de $1\text{ s}$.
- **`CAL` (`F2`):** Calibra la altitud barométrica a cero en la rampa de lanzamiento y persiste el offset en FRAM.
- **`MEC`:** Activa el servo `SG90` (`90°`) para liberar el segundo paracaídas y persiste el estado en FRAM.
- **`$RPI,...`:** Inyecta una trama desde la Raspberry Pi Zero W con magnetómetro, fotodiodos y actitud calculada.

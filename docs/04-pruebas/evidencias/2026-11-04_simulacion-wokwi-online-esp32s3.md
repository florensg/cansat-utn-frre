# Registro y guía: Simulación interactiva en Wokwi Online y coordinación con el equipo (ESP32-S3)

- **Fecha:** 2026-11-04
- **Responsable:** Florencia González (`ISI 1` — Software de vuelo, telemetría y determinación de actitud)
- **Placa simulada:** `ESP32-S3 DevKitC-1` de 44 pines (Elementos v2, `C-002`)
- **Archivos asociados:**
  - [`firmware/obc-esp32s3/diagram.json`](../../../firmware/obc-esp32s3/diagram.json)
  - [`firmware/obc-esp32s3/wokwi_online_sketch.ino`](../../../firmware/obc-esp32s3/wokwi_online_sketch.ino)
  - [`firmware/obc-esp32s3/include/config.h`](../../../firmware/obc-esp32s3/include/config.h)
  - [`firmware/obc-esp32s3/src/main.cpp`](../../../firmware/obc-esp32s3/src/main.cpp)

---

## 1. Coordinación con integrantes del equipo (Qué compartir con cada rol)

| Integrante | Rol | Subsistema | Qué compartirle y acordar |
|---|---|---|---|
| **Agustín Brites** | `ISI 2` | Estación terrena (GCS), datos e imágenes | **Par directo de software.** Contrato de la trama CSV a 1 Hz (`TelemetryFrame`), formato del campo `OPTIONAL_DATA` (`MAX:<alt>;P2:<0\|1>;CMD:<eco>`), protocolo de telecomandos (`CX`, `ST`, `CAL`, `MEC`) y reparto de procesamiento con la Raspberry Pi Zero 2 W. |
| **Juan Pedro López** | `IEM 3` | Electrónica, sensores y RF | **Par principal de hardware.** Migración al `ESP32-S3 DevKitC-1`, validación del pinout en `include/config.h`, buses I2C (`GPIO 8/9`), SPI y UART (radio `SX1262` y enlace RPi), y calibración/fusión de sensores de actitud (`MPU-6050`, `LIS3MDL`, celdas solares `BPW34 + ADS1113`, GPS `NEO-M8`). |
| **Maximiliano López** | `IEM 2` | Descenso y recuperación | Lógica de `ApogeeDetector` (detección de apogeo con caída $\ge 5\text{ m}$ sobre $100\text{ m}$ y disparo del 2.º paracaídas al **80 % de la altura máxima**), tiempos de guarda ($10\text{ s}$) y señal de actuación del mecanismo (`GPIO 15`). |
| **Catriel Paz** | `IEM 1` | Sistema de alimentación | Perfiles de consumo por modo de operación, alimentación de $3.3\text{ V}$ del `ESP32-S3` y lectura/umbrales de batería con `INA219`. |
| **Luciana Cáceres** | `IEM 4` | Mecánica y estructura | Alineación de la terna de ejes mecánicos ($X, Y, Z$) del CAD con los ejes del algoritmo de actitud y montaje de cámaras. |

---

## 2. Estado del desarrollo de `ISI 1`

1. **Lógica nuclear (`flight_core` en C++ puro):**
   - Máquina de estados (`FlightStateMachine`): `LaunchPad`, `Ascent`, `Descent`, `Landed` con transiciones validadas.
   - Detector de apogeo y 80 % (`ApogeeDetector`): cálculo de altura máxima, detección de apogeo y disparo único (*latch*) del segundo paracaídas al 80 %.
   - Formateador de telemetría CSV (`format_telemetry_csv`) a 1 Hz y contador acumulativo de paquetes.
   - Parser de telecomandos (`parse_command`): `CX`, `ST`, `CAL`, `MEC` (más comandos de banco `SIM` y `RST`).
   - Persistencia (`PersistentState`): validación con `magic`, `version` y `checksum`.
   - **Verificación unitaria:** 13/13 tests nativos pasando (`pio test -e native`).
2. **Integración en `src/main.cpp` y simulación visual en Wokwi:**
   - Lectura I2C directa de `MPU-6050` (`0x68`) para acelerómetro, giroscopio y temperatura.
   - Control de altitud manual por potenciómetro deslizante ($0\text{ m}$ a $500\text{ m}$) y perfil de vuelo sintético automático (`0 m -> 400 m -> Apogeo -> 80 % -> Landed`).
   - Integración en tiempo real de `FlightStateMachine` y `ApogeeDetector` dentro del `loop()`, actualizando 5 LEDs de estado y el servomotor de liberación del mecanismo.
3. **Payload secundario (`firmware/payload-rpi/src/main.py`):**
   - Esqueleto funcional en Python para recepción UART a 115200 baudios, log en CSV y respuesta `ACK` ante eventos.

---

## 3. Mapa de conexiones del circuito en Wokwi (`diagram.json`)

| Componente en Wokwi | ID | Pin `ESP32-S3` | Función en la simulación |
|---|---|:---:|---|
| `wokwi-mpu6050` | `imu1` | `SDA: GPIO 8`<br>`SCL: GPIO 9` | Sensor IMU I2C (`0x68`). Permite variar aceleración ($X,Y,Z$), giro ($X,Y,Z$) y temperatura con sliders. |
| `wokwi-slide-potentiometer` | `pot_alt` | `GPIO 4` (ADC) | Control manual de altitud ($0\text{ m}$ a $500\text{ m}$) y presión barométrica asociada. |
| `wokwi-pushbutton` (Verde) | `btn_launch` | `GPIO 5` | Inicia o reinicia el vuelo simulado automático (`SIM`). |
| `wokwi-led` (Azul) | `led_pad` | `GPIO 10` | Estado `LaunchPad` (CanSat armado en rampa). |
| `wokwi-led` (Verde) | `led_ascent` | `GPIO 11` | Estado `Ascent` (ascenso detectado al superar $15\text{ m}$). |
| `wokwi-led` (Amarillo) | `led_descent` | `GPIO 12` | Estado `Descent` (apogeo detectado). |
| `wokwi-led` (Rojo) | `led_para2` | `GPIO 13` | Disparo del **2.º paracaídas al 80 %** de la altura máxima o comando `MEC`. |
| `wokwi-led` (Blanco) | `led_landed` | `GPIO 14` | Estado `Landed` (aterrizaje confirmado $\le 5\text{ m}$). |
| `wokwi-servo` | `servo_mec` | `GPIO 15` | Mecanismo de liberación: `0°` cerrado, gira a `90°` al activarse el 2.º paracaídas o `MEC`. |

---

## 4. Procedimiento paso a paso para probar en Wokwi Online

1. Abrir en el navegador: **[https://wokwi.com/projects/new/esp32-s3](https://wokwi.com/projects/new/esp32-s3)**
2. En la pestaña **`sketch.ino`**, reemplazar todo el contenido por el de [`firmware/obc-esp32s3/wokwi_online_sketch.ino`](../../../firmware/obc-esp32s3/wokwi_online_sketch.ino).
3. En la pestaña **`diagram.json`**, reemplazar todo el contenido por el de [`firmware/obc-esp32s3/diagram.json`](../../../firmware/obc-esp32s3/diagram.json).
4. Presionar el botón verde **Play (Start the simulation)**.

### Casos de prueba interactivos

- **Caso 1 — Vuelo automático completo (`SW-02`, `SW-03`, `SW-06`, `SW-07`):**
  - Presionar el **Botón Verde (`LANZAR / SIM`)** en el circuito o enviar `SIM` por el monitor serie.
  - Verificar la secuencia de LEDs: Azul (`LaunchPad`) $\rightarrow$ Verde (`Ascent`, sube hasta $400\text{ m}$) $\rightarrow$ Amarillo (`Descent` al caer $5\text{ m}$ desde el apogeo) $\rightarrow$ Rojo + giro de Servo a `90°` al cruzar $320\text{ m}$ ($80\text{ \%}$ de $400\text{ m}$) $\rightarrow$ Blanco (`Landed` al bajar de $5\text{ m}$).
- **Caso 2 — Control manual de altitud:**
  - Deslizar `pot_alt` con el mouse para subir por encima de $100\text{ m}$, esperar al menos $10\text{ s}$ de tiempo de vuelo y descender gradualmente para comprobar los umbrales de apogeo y del 80 %.
- **Caso 3 — Estímulo de IMU (`MPU-6050`):**
  - Hacer clic sobre el módulo `MPU-6050` y mover los controles de aceleración, velocidad angular y temperatura; verificar su actualización en las columnas de la trama CSV cada 1 segundo.
- **Caso 4 — Telecomandos (`SW-08`):**
  - Enviar desde la consola serie:
    - `CX` $\rightarrow$ devuelve `CX OK <ESTADO> PKT=<N>`.
    - `ST` $\rightarrow$ devuelve `ST OK`.
    - `CAL` $\rightarrow$ calibra el cero de altitud en la posición actual (`CAL OK`).
    - `MEC` $\rightarrow$ activa manualmente el mecanismo, enciende el LED rojo y posiciona el servo en `90°` (`MEC OK`).
    - `RST` $\rightarrow$ reinicia la máquina de estados a `LaunchPad` y vuelve el servo a `0°` (`RST OK`).

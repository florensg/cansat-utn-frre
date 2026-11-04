# Software de vuelo del OBC

Firmware del **ESP32-S3 DevKitC-1** para el CanSat. Responsable: Florencia González
(ISI 1).

La placa está seleccionada como componente **C-002** en
[`elementos-cansat-v2.xlsx`](../../docs/00-gestion/elementos-cansat-v2.xlsx). El STM32
(C-001) figura como descartado. La actividad **5.0** de la
[`planificacion-general-v3.xlsx`](../../docs/00-gestion/planificacion-general-v3.xlsx)
asigna la arquitectura de hardware y software del microcontrolador principal a ISI 1 e
IEM 3.

## Estructura

```text
firmware/obc-esp32s3/
├── platformio.ini          Entornos obc y native
├── include/config.h        Placa, pines, tiempos y parámetros de misión
├── src/main.cpp            Arranque, scheduler y enlace con hardware
├── lib/flight_core/        Lógica pura, sin dependencias de Arduino
└── test/test_flight_core/  Tests unitarios del entorno native
```

## Compilación

```bash
pio run -e obc
```

El entorno `obc` usa `platform = espressif32` y `board = esp32-s3-devkitc-1`. PlatformIO
descarga el toolchain la primera vez. La definición por defecto corresponde a la variante
**N8** (8 MB de flash, sin PSRAM); si la placa entregada es N8R8, ajustar la configuración
de PSRAM antes de usar ese recurso.

## Grabación y monitor

```bash
pio run -e obc -t upload
pio device monitor
```

La placa se conecta por USB-C. El firmware habilita USB CDC (`ARDUINO_USB_CDC_ON_BOOT=1`),
por lo que `Serial` aparece como puerto serie del monitor. Si la carga no arranca
automáticamente, mantener presionado **BOOT**, presionar **RESET** y soltar **BOOT**.

## Simulación sin placa (Wokwi Online y VS Code)

El proyecto incluye el circuito interactivo en [`diagram.json`](diagram.json) (con sensor IMU `MPU-6050`, slider de altitud de 0 a 500 m, botón de lanzamiento/simulación, 5 LEDs de estado y servomotor de liberación del 2.º paracaídas) y una versión unificada en [`wokwi_online_sketch.ino`](wokwi_online_sketch.ino) lista para usar en el navegador.

### Opción A: Wokwi Online (en el navegador, sin instalar nada)

1. Abrir [https://wokwi.com/projects/new/esp32-s3](https://wokwi.com/projects/new/esp32-s3).
2. Pegar el contenido de [`wokwi_online_sketch.ino`](wokwi_online_sketch.ino) en la pestaña `sketch.ino`.
3. Pegar el contenido de [`diagram.json`](diagram.json) en la pestaña `diagram.json`.
4. Presionar **Start the simulation** (Play).
5. Presionar el **Botón Verde (`LANZAR / SIM`)** para ver el vuelo automático completo, mover el slider de altitud o hacer clic en el `MPU-6050`, y probar los comandos `CX`, `ST`, `CAL`, `MEC`, `SIM` y `RST`.

### Opción B: Wokwi for VS Code (local)

1. Instalar la extensión **Wokwi for VS Code**.
2. Compilar el firmware:

   ```bash
   pio run -e obc
   ```

3. Presionar `F1` y elegir **Wokwi: Start Simulator**.
4. Abrir el monitor serie y probar los controles visuales y los comandos `CX`, `ST`, `CAL`, `MEC`, `SIM` y `RST`.

El detalle completo de pines simulados y casos de prueba está registrado en
[`docs/04-pruebas/evidencias/2026-11-04_simulacion-wokwi-online-esp32s3.md`](../../docs/04-pruebas/evidencias/2026-11-04_simulacion-wokwi-online-esp32s3.md).

## Tests

```bash
pio test -e native
```

Los tests cubren:

- Transiciones válidas e inválidas de la FSM.
- Formateo y truncado de la trama de telemetría.
- Detección de apogeo y del 80 % de altura.
- Parser de comandos `CX`, `ST`, `CAL` y `MEC`.
- Persistencia y checksum del estado.

## Integración pendiente

- Reemplazar `SimulatedSensorSource` por los drivers reales de I2C, SPI y UART.
- Reemplazar `MemoryStorage` por el driver de FRAM MB85RC256V.
- Configurar el RTC RV-3028-C7, el Task Watchdog Timer (TWDT) y la causa de reinicio.
- Acordar el pinout definitivo del DevKitC-1 de 44 pines y la trama con IEM 3 e ISI 2.
- Definir la UART de la radio y de la placa secundaria; el USB CDC queda para consola.

La documentación funcional está en
[`docs/03-subsistemas/software-vuelo/README.md`](../../docs/03-subsistemas/software-vuelo/README.md).

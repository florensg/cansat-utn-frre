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

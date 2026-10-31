# Software de vuelo del OBC

Firmware del STM32 para el CanSat. Responsable: Florencia González (ISI 1).

## Estructura

```text
firmware/obc-stm32/
├── platformio.ini          Entornos obc y native
├── include/config.h        Pines, tiempos y parámetros de misión
├── src/main.cpp            Arranque, scheduler y enlace con hardware
├── lib/flight_core/        Lógica pura, sin dependencias de Arduino
└── test/test_flight_core/  Tests unitarios del entorno native
```

## Compilación

```bash
pio run -e obc
```

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
- Configurar el RTC RV-3028-C7, el watchdog IWDG y la causa de reinicio.
- Acordar el pinout y la trama definitiva con IEM 3 e ISI 2.

La documentación funcional está en
[`docs/03-subsistemas/software-vuelo/README.md`](../../docs/03-subsistemas/software-vuelo/README.md).

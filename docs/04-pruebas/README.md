# Plan y matriz de pruebas

Este directorio concentra los planes de ensayo, las matrices de verificación y las
evidencias del proyecto.

## Niveles de prueba

| Nivel | Alcance | Herramienta | Responsable |
|---|---|---|---|
| Unitario | Lógica pura de FSM, telemetría, apogeo y comandos | `pio test -e native` | ISI 1 |
| Simulación | Arranque, USB CDC, I2C, telemetría y comandos | Wokwi (`firmware/obc-esp32s3`) | ISI 1 |
| Banco | Drivers de sensores, buses I2C/SPI/UART y persistencia | Hardware de laboratorio | ISI 1 + IEM 3 |
| Integración | OBC + radio + GCS + alimentación | Banco integrado | Todos |
| Ambiental | Vibración, choque y térmico | Equipamiento del laboratorio | IEM 2 + IEM 4 |
| Vuelo | Ensayo de campaña | CanSat completo | Todos |

## Matriz de verificación

| ID | Objetivo | Condición | Criterio de aceptación | Evidencia |
|---|---|---|---|---|
| SW-01 | Compilación del firmware | `pio run` | Sin errores ni warnings críticos | Log de compilación |
| SW-02 | Transiciones de la FSM | Secuencia de eventos | Solo transiciones permitidas | Test unitario |
| SW-03 | Trama de telemetría | 1 Hz durante 10 minutos | 600 paquetes, sin pérdida local | CSV de ejemplo |
| SW-04 | Persistencia | Corte de alimentación | Contador y estado conservados | Log de reinicio |
| SW-05 | Watchdog | Bloqueo simulado | Reinicio automático y registro de causa | Log de reinicio |
| SW-06 | Detección de apogeo | Perfil de presión grabado | Apogeo detectado sin falsos positivos | Gráfica y log |
| SW-07 | Segundo paracaídas | Descenso simulado al 80 % | Disparo único y latch | Log de evento |
| SW-08 | Comandos | `CX`, `ST`, `CAL`, `MEC` | Respuesta correcta y eco | Log de consola |
| SW-09 | Actitud | Orientaciones conocidas | Error menor al umbral acordado | Tabla comparativa |
| SW-10 | Integración OBC-GCS | Enlace de 1 km | Menos del 5 % de paquetes perdidos | CSV de la GCS |

## Evidencias

- [Compilación del firmware ESP32-S3 (2026-10-31)](evidencias/2026-10-31_compilacion-esp32s3.md)

Guardar en `docs/04-pruebas/evidencias/` con el formato:

```text
AAAA-MM-DD_descripcion.ext
```

Cada evidencia debe indicar:

- Fecha y responsables.
- Configuración de hardware y firmware.
- Condiciones del ensayo.
- Resultado obtenido contra el criterio de aceptación.
- Observaciones y acciones correctivas.

## Regresión

Antes de cada integración se ejecutan:

1. Tests unitarios del entorno nativo.
2. Compilación del firmware ESP32-S3.
3. Prueba de telemetría de 10 minutos.
4. Prueba de persistencia con reinicio forzado.

Los resultados se registran en la matriz y se enlazan en el material de la PDR.

# Software de vuelo — OBC (ISI 1)

Responsable: **Florencia González (ISI 1)**.

Fuente de verdad: **Planificación General v3** (actividad 5.0) y **Elementos del CanSat
v2** (C-002). Las versiones anteriores de esas planillas están archivadas y no deben
usarse para definir el controlador.

Este subsistema cubre la computadora de a bordo (OBC), la máquina de estados de misión, la
telemetría, la persistencia ante reinicios y la lógica de vuelo asociada al descenso.

## Alcance

### Misión principal

- Arquitectura de hardware y software del OBC sobre **ESP32-S3 DevKitC-1 de 44 pines**
  (Elementos v2, C-002). El STM32 C-001 está descartado.
- Controlador principal de 3.3 V con USB-C, WiFi/BLE y buses I2C/SPI/UART.
- Definición de sensores, rangos, buses, muestreo y calibración, junto con IEM 3.
- Máquina de estados de misión y transiciones.
- Persistencia en FRAM, RTC con respaldo y watchdog.
- Telemetría a 1 Hz con IMU de tres ejes.
- Lógica automática al separarse del cohete y recepción de telecomandos.
- Detección de apogeo y del 80 % de la altura máxima para el segundo paracaídas.
- Prototipo crítico e integración con alimentación y radio.

### Misión secundaria

- Sistema de referencia, ejes y modelo de actitud.
- Fusión IMU + magnetómetro + sensores solares.
- Arquitectura del procesador secundario y reparto de tareas.
- Interfaz con la misión principal, formato `OPTIONAL_DATA` y sincronización temporal.
- Apoyo al algoritmo de clasificación de terreno.

### Sistema de alimentación

- Modos y perfiles de consumo por fase de misión.
- Lectura del INA219 y monitoreo de batería.
- Validación del presupuesto energético junto con IEM 1.

## Actividades asignadas en la planificación v3

| Actividad | Descripción | Rol |
|---|---|---|
| 4.0 | Sensores, rangos, buses, muestreo y calibración | Compartida con IEM 3 |
| 5.0 | Microcontrolador principal: arquitectura de hardware y software | Compartida con IEM 3 |
| 6.0 | Lógica y estados internos, persistencia, RTC, watchdog y reinicios | Responsable principal |
| 8.0 | Telemetría 1 Hz, lógica automática de separación y telecomandos | Compartida con ISI 2 |
| 16.0 | Algoritmo de detección del 80 % de la altura máxima | Compartida con IEM 2 |
| 22.0 | Prototipos críticos de sensores, control, radio y GCS | Compartida |
| 24.0 | Integración preliminar hardware + software + alimentación | Compartida |
| Secundaria 8.0 | Sistema de referencia, ejes y modelo de actitud | Compartida con IEM 3 |
| Secundaria 9.0 | Fusión IMU + magnetómetro + sensores solares | Responsable principal |
| Secundaria 10.0 | Prototipo y validación de actitud | Compartida con IEM 3 |
| Secundaria 17.0 | Procesador secundario y reparto de procesamiento | Compartida con ISI 2 |
| Secundaria 18.0 | Almacenamiento, formatos e integración con telemetría | Compartida con ISI 2 |

## Arquitectura de software

```text
src/main.cpp                  Arranque, scheduler no bloqueante y enlace con Arduino-ESP32
lib/flight_core/
  fsm/                        Máquina de estados de misión
  telemetry/                  Armado y formateo de la trama CSV
  storage/                    Estado persistente e interfaz de almacenamiento
  sensors/                    Interfaz de fuentes de datos y calibración
  commands/                   Parser de telecomandos
  flight/                     Detección de apogeo y del 80 % de altura
include/config.h              Placa, pines, constantes y parámetros de misión
```

La lógica de `flight_core` es C++ puro, sin dependencias de Arduino, para poder probarla en
el entorno `native` de PlatformIO. El acceso a hardware queda en `src/main.cpp` y en los
drivers que se agreguen en la capa de Arduino-ESP32.

## Máquina de estados

Estados mínimos alineados con la operación CONAE:

| Estado | Descripción | Evento de salida |
|---|---|---|
| `LaunchPad` | CanSat armado, a la espera del lanzamiento | Detección de lanzamiento o separación |
| `Ascent` | Ascenso y adquisición continua | Detección de apogeo |
| `Descent` | Descenso con paracaídas principal | Detección de aterrizaje |
| `Landed` | Post-aterrizaje, baliza y telemetría de recuperación | Reinicio controlado |

La transición `Ascent → Descent` se dispara con el detector de apogeo. La transición
`Descent → Landed` requiere confirmación de altitud estable y tiempo mínimo. Todas las
transiciones son one-shot y quedan registradas en el estado persistente.

## Trama de telemetría

Orden preliminar de columnas, a confirmar con ISI 2 antes de congelar el contrato:

```text
TEAM_ID,MISSION_TIME_MS,PACKET_COUNT,MODE,STATE,ALT_M,PRESS_PA,TEMP_C,BATT_V,CURR_MA,
ACC_X,ACC_Y,ACC_Z,GYR_X,GYR_Y,GYR_Z,LAT,LON,GPS_ALT_M,SATS,OPTIONAL_DATA
```

Reglas:

- Un paquete por segundo, generado por un temporizador no bloqueante.
- `PACKET_COUNT` es acumulativo y sobrevive a los reinicios.
- `MISSION_TIME_MS` se reconstruye con RTC si el OBC se reinicia.
- `OPTIONAL_DATA` transporta los resultados de la misión secundaria y el eco del último
  comando.
- El formato es ASCII/CSV para que la GCS pueda decodificarlo sin ambigüedad.

## Persistencia y recuperación

Variables que deben sobrevivir a un reinicio:

- Contador de paquetes.
- Calibración de altitud (offset de cero).
- Estado de la máquina de estados.
- Tiempo de misión.
- Configuración de red y parámetros de vuelo.
- Indicador de despliegue del segundo paracaídas.

Mecanismos:

- FRAM MB85RC256V por I2C para el estado persistente.
- RTC RV-3028-C7 con VBAT para mantener el tiempo de misión.
- Task Watchdog Timer (TWDT) del ESP32-S3 con refresco cooperativo y RTC WDT de respaldo.
- Registro de causa de reinicio para diagnóstico post-vuelo.

## Detección de apogeo y del 80 %

Algoritmo base implementado en `lib/flight_core/src/flight.cpp`:

1. Se registra la altitud máxima filtrada y el instante en que ocurrió.
2. Se declara apogeo cuando la altitud cae al menos `apogee_drop_m` respecto del máximo y
   se superó la altitud mínima de armado.
3. Se declara el segundo paracaídas cuando la altitud es menor o igual al 80 % de la
   altitud máxima registrada.
4. Condiciones de seguridad: altitud mínima, tiempo mínimo desde el lanzamiento,
   confirmación de descenso y latch de un solo disparo.
5. La calibración de altitud a cero se recibe por telecomando `CAL`.

## Telecomandos

| Comando | Función | Respuesta esperada |
|---|---|---|
| `CX` | Consultar estado | Eco con estado y contador de paquetes |
| `ST` | Configurar hora | Confirmación con hora aplicada |
| `CAL` | Calibrar altitud a cero | Confirmación con offset aplicado |
| `MEC` | Activar mecanismo | Confirmación y registro del evento |

El parser y las constantes están en `lib/flight_core/include/flight_core/commands.h`.

## Interfaces pendientes de acordar

- **Con IEM 3 (Juan):** pinout definitivo del ESP32-S3 DevKitC-1 de 44 pines, mapa de
  buses I2C/SPI/UART, pines de liberación, alimentación de sensores y conexión con la
  radio SX1262.
- **Con ISI 2 (Agustín):** trama CSV definitiva, unidades, formato de `OPTIONAL_DATA`,
  comandos y acuse de recibo.
- **Con IEM 2 (Maximiliano):** señal eléctrica del mecanismo de liberación, tiempos y
  condiciones de seguridad.
- **Con IEM 1 (Catriel):** perfiles de consumo, umbrales de batería y lectura del INA219.
- **Con IEM 4 (Luciana):** sistema de ejes y referencia mecánica para actitud.

## Hoja de ruta

1. Congelar interfaces con IEM 3 e ISI 2.
2. Completar drivers de sensores y adquisición a 1 Hz.
3. Validar persistencia, RTC y watchdog con ensayos de corte y reinicio.
4. Validar el algoritmo de apogeo y 80 % con perfiles de presión grabados.
5. Implementar el parser de comandos y la lógica automática de separación.
6. Implementar la fusión de actitud y validarla con orientaciones conocidas.
7. Integrar la placa secundaria, el almacenamiento y los modos de bajo consumo.

## Evidencias para la PDR

- Diagrama de la máquina de estados.
- Diagrama de bloques del OBC y mapa de buses.
- Tabla de interfaces con los demás subsistemas.
- Formato de telemetría y ejemplo de CSV.
- Descripción del algoritmo de apogeo y 80 %.
- Resultados de pruebas de persistencia y reinicio.
- Resultados de validación de actitud.
- Presupuesto de consumo por modo de operación.

## Pruebas

```bash
cd firmware/obc-esp32s3
pio test -e native
```

Los tests cubren transiciones de la FSM, formateo de telemetría, detección de apogeo y
parser de comandos. Las pruebas de hardware se documentan en
[`docs/04-pruebas/README.md`](../../04-pruebas/README.md).

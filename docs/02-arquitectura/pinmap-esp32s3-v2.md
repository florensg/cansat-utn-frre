# Pinmap ESP32-S3 — Circuito v.2

Asignación de pines del OBC según la documentación de hardware vigente.

## Fuentes

1. [`Cuadro_conexionado_ESP32S3_CanSat_v2.docx`](fuentes/cuadro-conexionado-esp32s3-cansat-v2.docx)
   — cuadro de conexionado completo. **Amarillo = conectado**.
2. [`Conexion de transistores.jpeg`](fuentes/conexion-transistores.jpeg)
   — habilitación de las llaves de alimentación (EN_*).
3. [`circuito-electronico-y-alimentacion-v2.pdf`](circuito-electronico-y-alimentacion-v2.pdf)
   — esquemático y mapa de componentes.

> El `.docx` se titula "asignación propuesta" y fue generado con `python-docx`; el JPEG de
> transistores es 2 minutos más reciente. Ambos asignaban GPIO 14/15/16 a funciones
> distintas; **se resolvió a favor del cuadro de conexionado** (ver §5).

## 1. Bus I²C principal (GPIO 8 = SDA, GPIO 9 = SCL)

| Dispositivo | Dirección | Alimentación | Notas |
|---|---|---|---|
| BMP280 | `0x76` | 3V3 | `CSB`→3V3 (fuerza I²C), `SDO`→GND |
| MPU-6050 (GY-521) | `0x68` | 5 V al breakout | `AD0`→GND; lógica 3V3; `INT`→GPIO4 |
| INA219 | `0x40` | 3V3 | `VIN+`/`VIN-` en serie con la rama de batería |
| FRAM MB85RC256V | `0x50` | 3V3 | `A0..A2`→GND, `WP`→GND |
| RV-3028-C7 | `0x52` | 3V3 | `VBACKUP` a pila; `INT`→GPIO10 (opcional) |

> **BMP280 por I²C**: el `CSB` atado a 3V3 descarta el modo SPI. El fallback SPI del
> firmware anterior queda obsoleto.

## 2. UARTs (según el cuadro de conexionado)

| Enlace | GPIO ESP32-S3 | Dirección | Nota |
|---|---|---|---|
| GPS NEO-M8N | RX = **17**, TX = **18** | 9600 8N1 | TX del GPS → GPIO17; RX del GPS ← GPIO18 |
| XBee-PRO 900HP | RX = **15**, TX = **16** | por confirmar | DOUT→GPIO15, DIN→GPIO16 |
| Raspberry Pi Zero 2W | RX = **14**, TX = **13** | 115200 8N1 | TXD0→GPIO14, RXD0→GPIO13 |
| GPS PPS (opcional) | GPIO5 | — | No resaltado en el cuadro (no conectado) |

> `Serial` es USB-CDC (no consume UART). `Serial1` = GPS, `Serial2` = XBee y
> `HardwareSerial(0)` remapeada a GPIO14/13 = Raspberry Pi (`g_rpi_serial`).
> Los tres enlaces son independientes y cada uno tiene su propio polling.

## 3. GPIO discretos

| GPIO | Función | Estado |
|---|---|---|
| GPIO1 | ADC del panel solar acondicionado | Conectado; requiere divisor/condicionamiento |
| GPIO4 | INT del MPU-6050 | Conectado |
| GPIO5 | PPS del NEO-M8N | Opcional, **no conectado** |
| GPIO6 | Control de baliza audible (vía driver) | **Por definir / no conectado** |
| GPIO7 | Control del mecanismo de liberación (vía driver) | Conectado |
| GPIO10 | INT del RTC (opcional) | Conectado |

> La baliza (C8/E8/E9) exige driver/MOSFET y batería independiente; el GPIO6 no debe
> alimentarla directamente. El LED de estado (E6) todavía no aparece en el cuadro.

## 4. Llaves de alimentación (lista de transistores)

| Señal | Carga controlada | GPIO | Conexión |
|---|---|---|---|
| `EN_RPI` | Raspberry Pi Zero 2W | **11** | GPIO11 → 1 kΩ → gate 2N7002 |
| `EN_CENIT` | XIAO ESP32-S3 Sense cenit | **12** | GPIO12 → 1 kΩ → gate 2N7002 |
| `EN_NADIR` | XIAO ESP32-S3 Sense nadir | **38** | GPIO38 → 1 kΩ → gate 2N7002 |
| `EN_SERVO` | Servo SG90 | **39** | GPIO39 → 1 kΩ → gate 2N7002 |
| `EN_SEC` | ADS1115 + sensores solares / misión secundaria | **40** | GPIO40 → 1 kΩ → gate 2N7002 |

Solo estos **5** rieles son conmutables. RTC, FRAM, sensores de vuelo, GPS y radio
quedan en rieles base siempre alimentados (sin GPIO de habilitación en las listas).

> **Reasignación de cableado:** la lista de transistores original ubicaba
> `EN_NADIR`/`EN_SERVO`/`EN_SEC` en GPIO14/15/16. Al priorizar el cuadro de conexionado
> (UARTs en 13–18) esas tres señales se movieron a **38/39/40**. El arnés debe armarse ya
> con estos pines: el firmware los maneja con `kPowerGatingEnabled = true`.

### Checklist de armado de las llaves

Cada llave es idéntica: `GPIO → 1 kΩ → gate del 2N7002`; el drenador tira del gate del
`AO3401A` (P-MOSFET) y el `100 kΩ` de pull-up mantiene el riel **apagado** por defecto.

| Paso | Verificación |
|---|---|
| 1 | `EN_NADIR` (GPIO38), `EN_SERVO` (GPIO39), `EN_SEC` (GPIO40) van **al gate del 2N7002**, nunca directo a la carga |
| 2 | Ningún `EN_*` comparte GPIO con las UART de GPS (17/18), XBee (15/16) ni RPi (14/13) — hay un test que lo verifica |
| 3 | Con el ESP32-S3 sin alimentar, medir continuidad: el riel debe quedar **cortado** (pull-up de 100 kΩ) |
| 4 | Al energizar, `PWR` en el arranque debe reportar **`001F`** (5 rieles base; la carga útil arranca apagada) |
| 5 | Tras enviar `RPI` por radio, `PWR` debe pasar a **`03FF`** (10 bits) y medir ~3,7 V en cada riel conmutado |

## 5. ✅ Conflicto GPIO 14/15/16 — resuelto

| GPIO | Cuadro de conexionado | Lista de transistores | **Resolución** |
|---|---|---|---|
| GPIO14 | RX desde Raspberry Pi | `EN_NADIR` | **UART de la RPi** |
| GPIO15 | RX desde XBee | `EN_SERVO` | **UART del XBee** |
| GPIO16 | TX hacia XBee | `EN_SEC` | **UART del XBee** |

Decisión (2026-11-16): **manda el cuadro de conexionado**. Las UART quedan en GPIO13–18
y las tres llaves se reasignaron a GPIO38/39/40, que están libres en el DevKitC-1
(N8R8) — fuera de flash/PSRAM (26–37), USB nativo (19/20), UART0 (43/44) y strapping
(0/3/45/46).

Con esto `kPowerGatingEnabled` pasa a `true`.

> ⚠️ **Mientras el hardware no se recablee**, las compuertas de `EN_NADIR`/`EN_SERVO`/`EN_SEC`
> siguen físicamente en GPIO14/15/16, que ahora son líneas de UART en reposo en HIGH:
> esos tres rieles quedarían **siempre encendidos**. No volar sin recablear a 38/39/40.
>
> Resumen de todos los cambios y del estado del firmware:
> [`correcciones-conexionado-y-estado-firmware-v2.md`](correcciones-conexionado-y-estado-firmware-v2.md).

### GPIO libres restantes (DevKitC-1, N8R8)

**2, 21, 41, 42, 47, 48**.

## 6. Estado en firmware

| Elemento | Estado |
|---|---|
| I²C 8/9 y direcciones | ✅ coincide con el firmware |
| MPU-6050 INT | ✅ movido de GPIO14 a GPIO4 |
| GPS RX/TX | ✅ corregido a 17/18 (`Serial1`) |
| XBee | ✅ UART2 en RX15/TX16 |
| Raspberry Pi | ✅ UART0 remapeada a RX14/TX13 (`g_rpi_serial`) |
| Polling separado RPi / radio | ✅ `poll_rpi_serial()` y `poll_radio_commands()` |
| Fallback Wire1 en 13/14 | ✅ eliminado (chocaba con UART de RPi) |
| Fallback SPI del BMP280 | ✅ eliminado (CSB→3V3) |
| EN de las 5 llaves | ✅ `kPowerGatingEnabled = true` (38/39/40 **sin cablear**) |
| RTC y tiempo de misión | ✅ driver RV-3028-C7 + jerarquía GPS > ST > RTC > millis() |
| LED de estado (E6) | ✅ lógica y patrones ([`indicadores-v2.md`](../03-subsistemas/software-vuelo/indicadores-v2.md)); ⏸ pines sin asignar (cableado Fase 5) |
| Baliza audible (C8/E8/E9) | ⏸ hardware independiente; el OBC **no** la controla |
| Watchdog y causa de reinicio | ✅ TWDT 4 s + `RST:<tag>` en `OPTIONAL_DATA` |

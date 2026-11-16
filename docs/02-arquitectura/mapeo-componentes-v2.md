# Mapeo de componentes — Circuito electrónico y alimentación v.2

Fuente: [`circuito-electronico-y-alimentacion-v2.pdf`](circuito-electronico-y-alimentacion-v2.pdf)
y [`diagrama-circuito-alimentacion-v2.jpeg`](diagramas/diagrama-circuito-alimentacion-v2.jpeg).

## 1. Cadena de alimentación

```text
18650 1S 3.7 V (3000 mAh) → BMS-1S → SW1 (SW-ROT-3: Charger / Off / Mission)

  Posición Charger → CHARGER 18650 (entrada externa 5 V) + LED rojo "Charger"
  Posición Mission → INA1 (INA219, medición de batería)
                   → U10 (TPS61088: BOOST 3.7 V → 5 V)
                   → TPS25940A (eFuse 5 V)
                   → TPS62132 (BUCK 5 V → 3.3 V)
```

Rieles conmutados por llaves electrónicas (**TPS2553**, **TPS22946** + **2N7002**/**AO3401A**):
RTC, FRAM, GPS + sensores, XBee-PRO, Raspberry Pi Zero, XIAO Cenit, XIAO Nadir y servo SG90.

## 2. Componentes

| Designador | Componente | Función | Interfaz / notas |
|---|---|---|---|
| `U1` | ESP32-S3 DevKitC-1 (N8R8) | Controlador de vuelo (OBC) | — |
| `MPU1` | MPU-6050 (GY-521) | Acelerómetro + giroscopio | I2C `0x68`, `INT` |
| `U2` | BMP280 | Presión y temperatura | SPI o I2C `0x76/0x77` |
| `INA1` | INA219 | Medición de batería (V/I) | I2C `0x40`, shunt |
| `VRAM1` | FRAM MB85RC256V | Estado persistente | I2C `0x50`, 32 KB |
| `RV1` | RV-3028-C7 + CR1225 | RTC con respaldo | I2C `0x52`, cristal 32.768 kHz — driver en `firmware/obc-esp32s3/src/rtc_rv3028.cpp` |
| `U3` | NEO-M8N (GY-GPSV3) | GPS | UART 9600, respaldo ML1220 |
| — | LIS3MDL | Magnetómetro 3 ejes | I2C `0x1C` |
| — | ADS1115 | ADC 16 bits | I2C `0x48` |
| `U4`..`U7` | TSL251RD ×4 | Luz → tensión (reemplazan los fotodiodos BPW34) | Analógico → `A0..A3` del ADS1115 |
| `RPI1` | Raspberry Pi Zero W (Zero 2W según diagrama) | Misión secundaria / actitud | I2C1 + UART |
| `U8` | XIAO ESP32-S3 Sense "Cenit" | Cámara cenital | I2C `0x30` |
| `U9` | XIAO ESP32-S3 Sense "Nadir" | Cámara nadir | I2C `0x31` |
| `M1` | Servo SG90 | Liberación / 2.º paracaídas | PWM |
| — | XBee-PRO 900HP | Radio | UART |
| — | Buzzer pasivo + 10440 | Avisos sonoros | GPIO |
| `SW1` | SW-ROT-3 | Selector Charger / Off / Mission | Potencia (no lógico) |
| — | 18650 1S + BMS-1S | Batería | 3.7 V, 3000 mAh |
| `U10` | TPS61088 | Boost 3.7 → 5 V | — |
| — | TPS25940A | eFuse 5 V | — |
| — | TPS62132 | Buck 5 → 3.3 V | — |
| — | TPS2553 ×4 | Load switch con `ILIM`/`FAULT` | `EN` por GPIO |
| `U11` | TPS22946 | Load switch | `EN` por GPIO |
| `Q1`..`Q6` | 2N7002 ×5 | N-MOSFET de nivel | Driver de compuerta |
| `Q2`, `Q8`..`Q11` | AO3401A ×5 | P-MOSFET de corte alto | Llave de riel |
| `L1` | 2 µH | Inductor del boost | — |
| `R1`..`R5` | 1 kΩ | Pull-down de compuerta | — |
| `R7`..`R11` | 100 kΩ | Pull-up de compuerta | — |

## 3. LEDs

- LED rojo **"Charger"** y verde **"Mission"** en el selector 3P (indican la posición).
- LED de **estado del software**: rojo = falla, verde = operativo (controlado por `AO3401A`).

## 4. Decisiones y preguntas abiertas para firmware

### Resueltas (2026-11-14, respuestas de IEM 1 + Anexo A v2)

- **SW1 (SW-ROT-3):** solo conmuta corriente (Charger / Off / Mission). **No se comunica
  con el ESP32**; el firmware no lo lee.
- **Frecuencia de radio:** la banda autorizada por el Anexo A v2 (X1) es **915–928 MHz**
  (más 2400–2483.5 MHz). El XBee-PRO 900HP debe configurarse dentro de 915–928 MHz.
- **Ahorro de energía:** sí; se corta y activa la alimentación **según el estado del
  CanSat**. La Raspberry Pi se enciende con el comando `RPI` de la GCS ~1 min antes del
  lanzamiento.
- **Buzzer y LED de estado:** **obligatorios** (Anexo A v2: C8, E6, E8, E9).
- **Interruptor de encendido:** **obligatorio** (E4) y compatible con el comando `RPI`.

### Resueltas recientemente

- **GPIO14/15/16 (2026-11-16):** manda el cuadro de conexionado. Las UART de RPi y XBee
  conservan GPIO13–18 y `EN_NADIR`/`EN_SERVO`/`EN_SEC` se movieron a **GPIO38/39/40**
  (pendiente de recablear). `kPowerGatingEnabled = true`. Detalle en
  [`pinmap-esp32s3-v2.md`](pinmap-esp32s3-v2.md) §5.

### Abiertas

- **Baudios del UART del XBee-PRO 900HP:** el PDF no los indica; el valor de fábrica suele
  ser `9600`.
- **Banda del XBee-PRO 900HP:** configurar **915–928 MHz** (Anexo A v2, X1).
- **Baliza audible (GPIO6):** figura en el cuadro pero **no está resaltada** (no conectada).
- **LED de estado (E6):** todavía no aparece en el cuadro de conexionado.

Los rieles conmutables ya están identificados: `EN_RPI` (GPIO11), `EN_CENIT` (GPIO12),
`EN_NADIR` (GPIO38), `EN_SERVO` (GPIO39) y `EN_SEC` (GPIO40). Los rieles base (RTC, FRAM,
sensores de vuelo, GPS y radio) no tienen GPIO de habilitación.

Ver el diseño implementado en
[`docs/03-subsistemas/software-vuelo/power-gating-v2.md`](../03-subsistemas/software-vuelo/power-gating-v2.md)
y los requisitos en
[`docs/01-requisitos/requisitos-competencia-v2.md`](../01-requisitos/requisitos-competencia-v2.md).

# Software de Misión Secundaria — Raspberry Pi Zero 2W (`RPI1`, Circuito v3)

Software del procesador exclusivo de la **Misión Secundaria** del CanSat, alineado con [`PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf`](../../PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf), [`Arquitectura General Hardware.drawio`](../../docs/02-arquitectura/diagramas/Arquitectura%20General%20Hardware.drawio), [`docs/03-subsistemas/secundaria/README.md`](../../docs/03-subsistemas/secundaria/README.md) y [`docs/03-subsistemas/actitud/README.md`](../../docs/03-subsistemas/actitud/README.md).
Responsable: Florencia González (`ISI 1`).

## Hardware de la Misión Secundaria conectado a `RPI1`

- **Bus I2C1 (`GPIO 2` SDA / `GPIO 3` SCL):**
  - **`LIS3MDL` (`U1`, `0x1C` / `0x1E`):** Magnetómetro de 3 ejes ($\pm 4\text{ gauss}$, $80\text{ Hz}$). Pin `INT` en `GPIO 17`.
  - **`ADS1115` (`0x48`):** Conversor ADC de 16 bits para los **4 fotodiodos solares `BPW34`** (representados como `U4..U7` `TSL251RD` en el esquemático v3):
    - `U4` (`BPW34` +X) $\rightarrow$ `AIN3` (`A3`)
    - `U5` (`BPW34` -X) $\rightarrow$ `AIN2` (`A2`)
    - `U6` (`BPW34` +Y) $\rightarrow$ `AIN1` (`A1`)
    - `U7` (`BPW34` -Y) $\rightarrow$ `AIN0` (`A0`)
- **Puerto CSI de Cámara:**
  - **Cámara Nadir Infrarroja (`IMX219 NoIR`):** Captura imágenes en infrarrojo cercano (NIR) durante el descenso para el cálculo de índices multiespectrales y clasificación de terreno.
- **UART (`GPIO 14` TXD / `GPIO 15` RXD):**
  - Enlace serie a `115200 8N1` con el OBC (`ESP32-S3 DevKitC-1`).

## Funciones en la Misión Secundaria

1. **Determinación de Actitud 3D (`roll`, `pitch`, `yaw` y cuaternión):**
   - Lee `LIS3MDL` (`mag_x_ut`, `mag_y_ut`, `mag_z_ut`) y `ADS1115` (`opt1`..`opt4`) en el bus I2C1 de la Raspberry Pi.
   - Fusiona el acelerómetro del OBC + magnetómetro + vector solar 2D (`sun_azimuth_deg`) para obtener el cuaternión `(qw, qx, qy, qz)` y los ángulos de Euler `(roll, pitch, yaw)`.
2. **Captura NIR (`IMX219 NoIR`), Clasificación de Terreno y Detección de Zonas de Aterrizaje:**
   - Captura imágenes sin pérdida (`PNG`) durante el vuelo y las asocia en la microSD (`payload_log.csv`) con tiempo de misión, coordenadas GPS, altitud y actitud 3D.
   - Clasifica el tipo de terreno observado (`PASTIZAL_ABIERTO`, `SUELO_FIRME`, `VEG_DENSA_OBSTACULOS`, `AGUA_O_SOMBRA`) y calcula un índice de aptitud de aterrizaje (`landing_apt`: `APTA` / `NO_APTA` y `suitability_score`), cumpliendo con la Sección 2.2 de [`propuesta-etapa-2-borrador.docx`](../../docs/01-requisitos/propuesta-etapa-2-borrador.docx).
3. **Reporte al OBC (`$RPI`):**
   - Envía al `ESP32-S3` tramas `$RPI` cada segundo para integrar magnetómetro, fotodiodos, actitud, capturas NIR y aptitud de aterrizaje en la telemetría de $1\text{ Hz}$ y en `OPTIONAL_DATA`.

## Ejecución en Raspberry Pi OS

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python src/main.py --port /dev/ttyAMA0 --baud 115200 --i2c-bus 1 --output payload_log.csv --images-dir nir_images
```

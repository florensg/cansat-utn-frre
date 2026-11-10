# Firmware de Cámaras de Misión Principal — 2x XIAO ESP32-S3 Sense (`U8` Cenit y `U9` Nadir)

Firmware para los dos módulos **Seeed Studio XIAO ESP32-S3 Sense** (`U8` y `U9`) definidos en [`PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf`](../../PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf) y en [`Arquitectura General Hardware.drawio`](../../docs/02-arquitectura/diagramas/Arquitectura%20General%20Hardware.drawio).

## Rol de cada cámara en el CanSat

Según el diagrama de arquitectura de hardware y los requisitos de video de la Misión Principal:

| Designador | Rol | Sensor | Dirección I2C esclava | Entorno PlatformIO |
|---|---|---|:---:|---|
| `U8` | **Cámara Cenit** (apunta hacia arriba: separación del cohete y despliegue de los 2 paracaídas) | `OV2640` (`800x600` color) | `0x30` | `pio run -e xiao_vis` |
| `U9` | **Cámara Nadir Visible** (apunta hacia abajo: superficie terrestre en espectro visible) | `OV2640` (`800x600` color) | `0x31` | `pio run -e xiao_nir` |

*(Nota: La tercera cámara del CanSat, la **IMX219 NoIR** de espectro infrarrojo cercano para la Misión Secundaria, va conectada directamente a la **Raspberry Pi Zero 2W**).*

## Funcionamiento

1. Al iniciar, cada XIAO inicializa su sensor **OV2640** (`SVGA 800x600 JPEG`, cumpliendo el mínimo de `640x480` color del reglamento), monta su tarjeta **microSD** (`CS = GPIO 21`) y se registra como esclavo I2C en `D4` (`SDA`, `GPIO 5`) y `D5` (`SCL`, `GPIO 6`).
2. Cuando el OBC (`ESP32-S3 DevKitC-1`) detecta transiciones de vuelo, envía por I2C el comando de modo al registro `0x01`:
   - `0x01` (`Ascent`): inicio de adquisición en ascenso.
   - `0x02` (`Descent` / Apogeo): captura inmediata + ráfaga continua durante el descenso.
   - `0x03` (`SecondParachuteTriggered` al 80 %): captura sincronizada con la apertura del segundo paracaídas y continuación de ráfaga.
   - `0x00` (`Landed`): detiene la adquisición y cierra archivos en la microSD.

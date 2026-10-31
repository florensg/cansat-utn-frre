# Payload secundario — Raspberry Pi Zero 2 W

Software de la placa secundaria del CanSat. Responsable: Florencia González (ISI 1).

## Función

- Recibir telemetría y eventos del OBC por UART.
- Capturar imágenes visibles y NIR.
- Registrar metadatos sincronizados con tiempo, GPS, altitud y actitud.
- Ejecutar el procesamiento multiespectral y la clasificación de terreno.
- Guardar resultados en microSD y devolver un resumen al OBC.

## Instalación

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

En Raspberry Pi OS:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Ejecución

```bash
python src/main.py --port /dev/ttyAMA0 --baud 115200 --output payload_log.csv
```

En Windows, para pruebas de banco:

```powershell
python src/main.py --port COM3 --baud 115200 --output payload_log.csv
```

## Protocolo

- UART a 115200 baudios.
- Tramas ASCII terminadas en salto de línea.
- El OBC envía telemetría y eventos.
- La placa secundaria responde con acuse y actitud.
- El contrato definitivo se documenta en
  [`docs/03-subsistemas/secundaria/README.md`](../../docs/03-subsistemas/secundaria/README.md).

## Salidas

- `payload_log.csv`: telemetría recibida con marca de tiempo local.
- `captures/`: imágenes visibles y NIR.
- `results/`: índices multiespectrales y clasificación.

## Estado

Esqueleto funcional de recepción y registro. La captura de cámara, el procesamiento de
imágenes y la clasificación se integran en las fases siguientes.

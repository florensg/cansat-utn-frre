# Estación terrena (GCS) — ISI 2

Responsable: **Agustín Brites (ISI 2)**. Apoyo: Juan Pedro López (IEM 3) para radio y
antena, y Florencia González (ISI 1) para el contrato de telemetría.

## Alcance

- Hardware receptor y enlace con la radio SX1262.
- Software de recepción, decodificación y visualización en tiempo real.
- Gráficas simultáneas de altitud, aceleración y velocidad angular.
- Generación de CSV y respaldo de datos.
- Detección de paquetes perdidos.
- Envío y verificación de telecomandos `CX`, `ST`, `CAL` y `MEC`.

## Contrato de datos

La GCS consume la trama definida por el software de vuelo:

```text
TEAM_ID,MISSION_TIME_MS,PACKET_COUNT,MODE,STATE,ALT_M,PRESS_PA,TEMP_C,BATT_V,CURR_MA,
ACC_X,ACC_Y,ACC_Z,GYR_X,GYR_Y,GYR_Z,LAT,LON,GPS_ALT_M,SATS,OPTIONAL_DATA
```

Reglas:

- Un paquete por segundo.
- Todos los campos se muestran sin necesidad de cambiar de pestaña.
- El CSV se escribe en disco a medida que llegan los paquetes.
- La pérdida de paquetes se detecta por el salto en `PACKET_COUNT`.
- Los comandos se registran con hora de envío y respuesta.

## Estructura sugerida

```text
gcs/
├── README.md
├── requirements.txt
├── src/
│   ├── main.py            Recepción y orquestación
│   ├── decoder.py         Parseo de la trama CSV
│   ├── gui.py             Visualización en tiempo real
│   └── storage.py         CSV y respaldo
└── docs/
    └── protocolo.md       Contrato de comandos y respuestas
```

## Requisitos de la competencia

- Estación portátil, sin alimentación de red en el sitio.
- Computadora, radio y antena de mesa o portátil.
- Visualización simultánea de todas las variables.
- Archivo CSV como evidencia para los jueces.

## Pendientes

- Confirmar el formato final de la trama con ISI 1.
- Definir el protocolo de acuse de comandos.
- Medir consumo y autonomía de la GCS junto con IEM 1.
- Documentar el procedimiento de operación en campaña.

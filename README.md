# CanSat Universitario 2026-2027 — UTN FRRe

Repositorio de ingeniería del equipo de la **UTN Facultad Regional Resistencia** para el
**CanSat Universitario 2026-2027** organizado por CONAE.

El proyecto integra una misión principal (descenso controlado, telemetría a 1 Hz y
recuperación) y una misión secundaria (determinación de actitud magneto-solar-inercial y
teledetección multiespectral VIS/NIR).

## Equipo y roles

| Integrante | Carrera | Rol | Responsabilidad principal |
|---|---|---|---|
| Florencia González | ISI | ISI 1 | Software de vuelo, telemetría y determinación de actitud |
| Agustín Brites | ISI | ISI 2 | Estación terrena, datos y procesamiento de imágenes |
| Juan Pedro López | IEM | IEM 3 | Electrónica, sensores y RF |
| Catriel Paz | IEM | IEM 1 | Sistema de alimentación |
| Maximiliano López | IEM | IEM 2 | Descenso y recuperación |
| Luciana Cáceres | IEM | IEM 4 | Mecánica y estructura |

Detalle completo en [`docs/00-gestion/participantes.md`](docs/00-gestion/participantes.md).

## Mapa del repositorio

```text
.
├── docs/                       Documentación técnica y de gestión
│   ├── 00-gestion/             Planificación, roles y listas de elementos
│   ├── 01-requisitos/          Bases, reglamento, Anexo A y requisitos
│   ├── 02-arquitectura/        Diagramas, interfaces y propuesta de circuito
│   ├── 03-subsistemas/         Software de vuelo, actitud, secundaria, RF
│   ├── 04-pruebas/             Planes, matrices y evidencias de ensayos
│   └── 05-pdr/                 Material y checklist de la PDR
├── firmware/
│   ├── obc-stm32/              Software de vuelo del OBC (PlatformIO, C++)
│   └── payload-rpi/            Software de la placa secundaria (Raspberry Pi)
├── gcs/                        Estación terrena (recepción, GUI y CSV)
├── hardware/                   CAD, Proteus y documentación de hardware
└── datasheets/                 Hojas de datos de los componentes
```

## Puesta en marcha

### Software de vuelo (OBC)

Requisitos: [PlatformIO Core](https://platformio.org/install/cli) y toolchain STM32.

```bash
cd firmware/obc-stm32
pio run                 # compila el firmware para el STM32
pio run -t upload       # graba el firmware por ST-Link
pio device monitor      # monitor serie a 115200 baudios
```

Tests de la lógica pura (FSM, telemetría, apogeo y comandos):

```bash
pio test -e native
```

### Payload secundario (Raspberry Pi)

```bash
cd firmware/payload-rpi
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python src/main.py --port COM3 --output payload_log.csv
```

### Estación terrena

La GCS está a cargo de ISI 2. El contrato de datos y los requisitos de interfaz están en
[`gcs/README.md`](gcs/README.md).

## Flujo de trabajo con Git

1. `main` se mantiene estable y siempre compilable.
2. Cada cambio se hace en una rama corta: `feat/isi1-fsm`, `fix/isi1-watchdog`,
   `docs/isi1-interfaces`.
3. Antes de abrir un Pull Request: compilar, correr tests y revisar que no se agreguen
   datos personales ni binarios pesados.
4. Los PR necesitan al menos una revisión de otro integrante antes de mergear a `main`.
5. Los mensajes de commit siguen el formato `tipo(area): descripción breve`, por ejemplo
   `feat(obc): agregar transición de apogeo a descenso`.

## Archivos que NO se versionan

Por privacidad y tamaño, quedan fuera del repositorio:

- `Documentación/`: DNIs, certificados y documentación de identidad del equipo.
- `Datos Participantes.xlsx`: contiene DNI, fecha de nacimiento y correos.
- `Ignorar (Basura)/`: material descartado.
- `Librerias Proteus/Proteus 8.13 SP0 - Samdey Microelectronic.rar`: instalador de 462 MB.
- Backups y workspaces de Proteus.

Estos archivos deben conservarse en el Drive del equipo o en almacenamiento local.

## Estado del proyecto

- Repositorio creado y estructura documental inicial.
- Software de vuelo: esqueleto de FSM, telemetría, persistencia y detección de apogeo.
- Pendiente de acordar con IEM 3 e ISI 2: pinout definitivo, trama de telemetría y
  protocolo entre el OBC y la placa secundaria.

La planificación vigente es la **Planificación General Actualizada v3**, disponible en
[`docs/00-gestion/`](docs/00-gestion/).

# Hardware

Este directorio reúne el diseño mecánico, los proyectos de Proteus y la documentación de
hardware del CanSat.

## Contenido

```text
hardware/
├── cad/                    Piezas y ensamblaje de SolidWorks
│   ├── Compartimento 1.SLDPRT
│   ├── Ensamblaje 1.SLDASM
│   ├── Esqueleto 1.SLDPRT
│   └── Tapa 1.SLDPRT
└── proteus/                Proyectos de simulación electrónica
    ├── CanSat/             Proyectos .pdsprj y librerías
    └── LIBRARY.zip         Librería de componentes
```

## Archivos excluidos

El instalador `Proteus 8.13 SP0 - Samdey Microelectronic.rar` (462 MB) no se versiona por
tamaño. Se conserva en el drive del equipo o en almacenamiento local.

Tampoco se versionan los backups automáticos ni los archivos `.workspace.xml` de Proteus.

## Responsables

- Diseño mecánico y CAD: Luciana Cáceres (IEM 4).
- Electrónica, sensores y RF: Juan Pedro López (IEM 3).
- Alimentación: Catriel Paz (IEM 1).
- Descenso y recuperación: Maximiliano López (IEM 2).

## Convenciones

- Las piezas se nombran en español y conservan el nombre usado en SolidWorks.
- Los cambios de CAD se documentan en el mensaje de commit.
- Las exportaciones para revisión se guardan en `docs/02-arquitectura/` o en
  `docs/05-pdr/`, no en este directorio.

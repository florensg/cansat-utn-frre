# Documentación del proyecto CanSat

Índice de la documentación técnica y de gestión del equipo UTN FRRe para el CanSat
Universitario 2026-2027.

## Estructura

### 00-gestion

Planificación, roles y listas de elementos.

- [Participantes y roles](00-gestion/participantes.md)
- [Decisión: repositorio único privado](00-gestion/decision-repo-bootstrap.md)
- [Decisión: controlador principal ESP32-S3](00-gestion/decision-controlador-esp32s3.md)
- [Planificación General v3](00-gestion/planificacion-general-v3.xlsx) — **vigente**
- [Elementos del CanSat v2](00-gestion/elementos-cansat-v2.xlsx) — **vigente**
- [Versiones anteriores archivadas](00-gestion/archivo/) — solo histórico, no usar

### 01-requisitos

Bases del certamen, reglamento, Anexo A y requisitos técnicos.

- [Propuesta Etapa 2 (borrador)](01-requisitos/propuesta-etapa-2-borrador.docx)
- [Requisitos técnicos organizados](01-requisitos/requisitos-tecnicos-organizado.docx)
- [Requisitos de competencia — síntesis operativa (Anexo A v2)](01-requisitos/requisitos-competencia-v2.md)
- [Documentos de la competencia](01-requisitos/competencia/)

### 02-arquitectura

Arquitectura general, diagramas de sistemas y circuito vigente.

- [Índice de arquitectura, circuito y alimentación](02-arquitectura/README.md)
- [Circuito electrónico y alimentación v.2](02-arquitectura/circuito-electronico-y-alimentacion-v2.pdf) — **vigente**
- [Diagrama del circuito v.2](02-arquitectura/diagramas/diagrama-circuito-alimentacion-v2.jpeg)
- [Fuentes de hardware](02-arquitectura/fuentes/) — cuadro de conexionado y conexión de transistores
- [Mapeo de componentes v.2](02-arquitectura/mapeo-componentes-v2.md)
- [Pinmap ESP32-S3 v.2](02-arquitectura/pinmap-esp32s3-v2.md)
- [Correcciones de conexionado y estado del firmware v.2](02-arquitectura/correcciones-conexionado-y-estado-firmware-v2.md)
- [Diagramas de arquitectura y estados](02-arquitectura/diagramas/)
- [Histórico de circuitos (archivo)](02-arquitectura/archivo/) — Propuesta 2 y v3, no usar

### 03-subsistemas

Documentación por subsistema y responsabilidad.

- [Software de vuelo — ISI 1](03-subsistemas/software-vuelo/README.md)
- [Power gating del OBC (circuito v.2)](03-subsistemas/software-vuelo/power-gating-v2.md)
- [Tiempo de misión y RTC](03-subsistemas/software-vuelo/tiempo-mision-v2.md)
- [Indicadores, watchdog y reinicios](03-subsistemas/software-vuelo/indicadores-v2.md)
- [Determinación de actitud — ISI 1](03-subsistemas/actitud/README.md)
- [Software de misión secundaria — ISI 1](03-subsistemas/secundaria/README.md)
- [Comunicaciones — IEM 3 / ISI 2](03-subsistemas/comunicaciones/)
- [Resumen de Ingeniería de Software (borrador)](03-subsistemas/software-vuelo/resumen-ingenieria-software-borrador.docx)

### 04-pruebas

Planes de ensayo, matrices de verificación y evidencias.

- [Plan y matriz de pruebas](04-pruebas/README.md)

### 05-pdr

Material de la Preliminary Design Review.

- [Checklist y evidencias PDR](05-pdr/README.md)

## Convenciones

- Los documentos se nombran en minúsculas, sin acentos y con guiones.
- Las versiones nuevas de un documento de planificación se agregan como archivo nuevo y se
  indica cuál es la vigente en este índice.
- Las evidencias de ensayos se guardan en `04-pruebas/evidencias/` con el formato
  `AAAA-MM-DD_descripcion.ext`.
- Los datos personales (DNI, fecha de nacimiento, correos) no se versionan en este
  repositorio.

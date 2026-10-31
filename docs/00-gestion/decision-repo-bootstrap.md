# Decisión: repositorio único privado y exclusión de datos personales

- Fecha: 2026-10-31
- Estado: aceptada
- Responsable: Florencia González (ISI 1)

## Contexto

El equipo necesitaba un lugar común para documentación, firmware, estación terrena,
hardware y datasheets. El material estaba disperso en carpetas locales, con documentos
duplicados, nombres inconsistentes y archivos muy pesados. Además, la carpeta
`Documentación/` y el archivo `Datos Participantes.xlsx` contienen DNIs, fechas de
nacimiento y correos, que no deben versionarse.

## Decisión

1. Usar un **monorepo privado** en GitHub bajo la cuenta `florensg`, con el nombre
   `cansat-utn-frre`.
2. Organizar el contenido en `docs/`, `firmware/`, `gcs/`, `hardware/` y `datasheets/`.
3. Excluir del repositorio:
   - `Documentación/` y `Datos Participantes.xlsx` por datos personales.
   - `Ignorar (Basura)/` por material descartado.
   - El RAR de Proteus de 462 MB por tamaño.
   - Backups y workspaces de Proteus.
4. No usar Git LFS en esta etapa: el contenido versionable ronda los 63 MB y ningún
   archivo supera el límite de 100 MB de GitHub.
5. Mantener los originales fuera del repositorio en el Drive del equipo o en
   almacenamiento local.

## Consecuencias

- El equipo tiene un único punto de acceso al proyecto.
- Los documentos de identidad no quedan expuestos en el historial de Git.
- El repositorio se mantiene liviano y clonable sin instalar Git LFS.
- Los archivos binarios de CAD y Proteus se versionan directamente, sin historial
  diferencial, lo cual es aceptable para la etapa PDR.
- Si en el futuro se necesita versionar imágenes o binarios más pesados, se evalúa
  migrar esos formatos a Git LFS.

## Alternativas descartadas

- **Repositorio solo de software:** no centraliza el proyecto y obliga a mantener dos
  lugares.
- **Monorepo con Git LFS:** consume la cuota gratuita y agrega complejidad para todo el
  equipo sin beneficio proporcional en esta etapa.

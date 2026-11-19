---
schema_version: 1
doc_type: spec
title: 'Presentacion PDR: bloque de software de vuelo de Flor Gonzalez (slides 69-71
  + FSW-A)'
created_at: '2026-11-19T18:35:53.679407Z'
updated_at: '2026-11-19T18:35:53.679407Z'
tags:
- spec
- pdr
- presentacion
- fsw
- obc
- flor-gonzalez
status: draft
links: []
vault_scope: local
fingerprint: f80fdcf626d2a414f047c962588e0b7e6aec72485e2c0201dd1d272fb6ca11e8
verification_hooks:
- name: guia-existe
  command: powershell -NoProfile -Command "if (Test-Path 'docs/05-pdr/presentacion-flor-gonzalez.md')
    { exit 0 } else { exit 1 }"
  required: true
  success_criteria: exit code 0
  timeout_seconds: 30
- name: guia-cubre-las-4-slides
  command: powershell -NoProfile -Command "if ((Select-String -Path 'docs/05-pdr/presentacion-flor-gonzalez.md'
    -Pattern 'Slide 69','Slide 70','Slide 71','FSW-A' -Quiet) -eq $true) { exit 0
    } else { exit 1 }"
  required: true
  success_criteria: exit code 0
  timeout_seconds: 30
- name: pdf-pdr-no-versionados
  command: powershell -NoProfile -Command "if (Select-String -Path '.gitignore' -Pattern
    'docs/05-pdr/*.pdf' -SimpleMatch -Quiet) { exit 0 } else { exit 1 }"
  required: true
  success_criteria: exit code 0
  timeout_seconds: 30
goal: 'Preparar el contenido del bloque de software de vuelo de la PDR: que va en
  cada diapositiva y que se explica oralmente. Alcance limitado a lo asignado a Flor
  Gonzalez en la guia de armado (slides 69, 70 y 71) mas la slide adicional FSW-A
  de la Scoresheet. La fuente tecnica es el firmware real del OBC.'
files_in_scope:
- docs/05-pdr/presentacion-flor-gonzalez.md
- docs/05-pdr/README.md
- docs/03-subsistemas/software-vuelo
- docs/02-arquitectura
- firmware/obc-esp32s3
constraints:
- 'Los PDF de la PDR no se versionan: docs/05-pdr/*.pdf esta en .gitignore. La guia
  es markdown y si se versiona.'
- 'El firmware no se modifica: se lee como fuente de evidencia.'
- El deck de la PDR esta en ingles; los titulos y el texto de las slides van en ingles,
  la guia y las notas orales en espanol.
- 'Respetar el reparto de la guia de armado: no invadir slides de otros integrantes.'
- Si un dato no esta verificado en el repo, marcarlo como pendiente en vez de estimarlo.
acceptance_criteria:
- 'La guia tiene una tabla por slide con: numero, titulo, layout, contenido exacto,
  cifras, mensaje oral y evidencia de respaldo.'
- Incluye el diagrama de estados de la FSM y el diagrama de flujo del software en
  Mermaid.
- Incluye una matriz criterio de la Scoresheet -> slide -> evidencia -> estado.
- Incluye lista de pendientes y de lo que NO entra en el bloque.
- Todas las cifras citadas coinciden con config.h o con el resultado de build registrado.
---

## Goal

Preparar el contenido del bloque de software de vuelo de la PDR: que va en cada diapositiva y que se explica oralmente. Alcance limitado a lo asignado a Flor Gonzalez en la guia de armado (slides 69, 70 y 71) mas la slide adicional FSW-A de la Scoresheet. La fuente tecnica es el firmware real del OBC.

## Requirements

- Cubrir solo el bloque de Flor Gonzalez: slides 69 (FSW Overview), 70 (Payload FSW State Diagram), 71 (Software Development Plan) y la adicional FSW-A (Simulation Mode Software).
- No redactar slides de otros integrantes: 57-59 (Juan Lopez), 72-77 (Agustin Brites), 60-68 (Catriel Paz). Solo senalar las interfaces que cruzan con software.
- Slide 69: arquitectura por capas, modulos, lenguaje y framework, tareas del lazo, watchdog, FRAM y RTC; incluir diagrama de flujo de arranque y de una vuelta de loop().
- Slide 70: FSM con estados, transiciones, umbrales numericos, tasas de muestreo, comunicaciones, almacenamiento, activacion del mecanismo, power management por estado y recuperacion tras reset.
- Slide 71: prototipos, secuencia de desarrollo, responsables y metodologia de pruebas con criterio de aceptacion.
- Slide FSW-A: modo simulacion y como se inyectan perfiles de altitud, aceleracion, separacion, 80% de Hmax y aterrizaje.
- Cada afirmacion tecnica debe apoyarse en un archivo del repo o en un documento, citado en la guia.
- Ningun dato cuantitativo sin verificar contra config.h, el codigo o el build registrado.

## Files in Scope

- `docs/05-pdr/presentacion-flor-gonzalez.md`
- `docs/05-pdr/README.md`
- `docs/03-subsistemas/software-vuelo`
- `docs/02-arquitectura`
- `firmware/obc-esp32s3`

## Constraints

- Los PDF de la PDR no se versionan: docs/05-pdr/*.pdf esta en .gitignore. La guia es markdown y si se versiona.
- El firmware no se modifica: se lee como fuente de evidencia.
- El deck de la PDR esta en ingles; los titulos y el texto de las slides van en ingles, la guia y las notas orales en espanol.
- Respetar el reparto de la guia de armado: no invadir slides de otros integrantes.
- Si un dato no esta verificado en el repo, marcarlo como pendiente en vez de estimarlo.

## Acceptance Criteria

- [ ] La guia tiene una tabla por slide con: numero, titulo, layout, contenido exacto, cifras, mensaje oral y evidencia de respaldo.
- [ ] Incluye el diagrama de estados de la FSM y el diagrama de flujo del software en Mermaid.
- [ ] Incluye una matriz criterio de la Scoresheet -> slide -> evidencia -> estado.
- [ ] Incluye lista de pendientes y de lo que NO entra en el bloque.
- [ ] Todas las cifras citadas coinciden con config.h o con el resultado de build registrado.

## Verification Hooks

Commands that objectively prove the work is done. Run by
`cortex finish-session` (Pluggable Middle, Phase 01).

### guia-existe
```bash
powershell -NoProfile -Command "if (Test-Path 'docs/05-pdr/presentacion-flor-gonzalez.md') { exit 0 } else { exit 1 }"
```

Success: exit code 0 · Timeout: 30s
### guia-cubre-las-4-slides
```bash
powershell -NoProfile -Command "if ((Select-String -Path 'docs/05-pdr/presentacion-flor-gonzalez.md' -Pattern 'Slide 69','Slide 70','Slide 71','FSW-A' -Quiet) -eq $true) { exit 0 } else { exit 1 }"
```

Success: exit code 0 · Timeout: 30s
### pdf-pdr-no-versionados
```bash
powershell -NoProfile -Command "if (Select-String -Path '.gitignore' -Pattern 'docs/05-pdr/*.pdf' -SimpleMatch -Quiet) { exit 0 } else { exit 1 }"
```

Success: exit code 0 · Timeout: 30s

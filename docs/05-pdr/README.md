# Preliminary Design Review (PDR)

Material, checklist y evidencias para la revisión preliminar de diseño del CanSat
Universitario 2026-2027.

## Documentos

Los documentos de la PDR **no se versionan** (son material del equipo y se
distribuyen por el Drive). Se dejan las rutas locales de referencia:

| Archivo | Qué es |
|---|---|
| `pdr-yvysat-v3.pdf` | PDR del equipo **YVYSAT**, versión 3 — documento de la revisión |
| `guia-armado-pdr-yvysat.pdf` | Guía de armado de la PDR: orden de secciones y qué evidencia va en cada una |
| [`presentacion-flor-gonzalez.md`](presentacion-flor-gonzalez.md) | Guía del bloque de software de vuelo (slides 69-71 + FSW-A): contenido por slide, guion oral y evidencia |
| [`PDR Template - V2.pptx`](../01-requisitos/competencia/Modelos/PDR%20Template%20-%20V2.pptx) | Plantilla oficial de la competencia (esta sí se versiona) |

> Los dos PDF están en `.gitignore` (`docs/05-pdr/*.pdf`). Si no los tenés en
> local, pedilos al Drive del equipo: sin ellos el checklist de abajo no se puede
> completar.

## Checklist por subsistema

### Software de vuelo — ISI 1

- [ ] Diagrama de bloques del OBC y mapa de buses.
- [ ] Diagrama de la máquina de estados con eventos y condiciones.
- [ ] Tabla de interfaces con sensores, radio, alimentación y placa secundaria.
- [ ] Formato de telemetría con unidades y ejemplo de CSV.
- [ ] Descripción del algoritmo de apogeo y 80 % con condiciones de seguridad.
- [ ] Estrategia de persistencia, RTC y watchdog.
- [ ] Resultados de tests unitarios y de banco.
- [ ] Presupuesto de consumo por modo de operación.

### Determinación de actitud — ISI 1

- [ ] Sistema de ejes y referencia mecánica.
- [ ] Diagrama de flujo de la fusión sensorial.
- [ ] Curvas de calibración de magnetómetro y sensores solares.
- [ ] Resultados de validación con orientaciones conocidas.
- [ ] Error de roll, pitch y yaw.

### Misión secundaria — ISI 1 / ISI 2

- [ ] Arquitectura del procesador secundario.
- [ ] Contrato de protocolo UART.
- [ ] Formato de `OPTIONAL_DATA`.
- [ ] Ejemplo de registro sincronizado imagen + metadatos.
- [ ] Consumo medido por modo.

### Integración

- [ ] Presupuesto de masa, energía, costo y riesgo.
- [ ] Cronograma actualizado.
- [ ] Demostración de conceptos críticos.
- [ ] Riesgos abiertos con mitigación.

## Evidencias

Las evidencias se enlazan desde este checklist hacia:

- `docs/04-pruebas/evidencias/` para logs y capturas.
- `docs/02-arquitectura/` para diagramas.
- `docs/03-subsistemas/` para documentación técnica.

## Estado

La PDR se considera lista cuando todos los ítems obligatorios tienen evidencia verificable
y los riesgos abiertos tienen responsable y mitigación.

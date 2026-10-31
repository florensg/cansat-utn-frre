# Preliminary Design Review (PDR)

Material, checklist y evidencias para la revisión preliminar de diseño del CanSat
Universitario 2026-2027.

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

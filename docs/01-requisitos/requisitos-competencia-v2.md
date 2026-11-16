# Requisitos de competencia — síntesis operativa (Anexo A v2)

Requisitos del **CanSat Universitario 2026-2027 (CONAE)** que impactan en el diseño de
hardware y firmware del equipo UTN FRRe. Extraídos del **Anexo A v2** y de las **Bases y
condiciones** vigentes.

- **Fuente primaria:** [`competencia/v2/Anexo A v2 .pdf`](competencia/v2/Anexo%20A%20v2%20.pdf)
- **Bases:** [`competencia/v2/bases_y_condiciones_-_cansat_universitario_2026-2027.pdf`](competencia/v2/bases_y_condiciones_-_cansat_universitario_2026-2027.pdf)
- **Estado:** verificado por extracción de texto del PDF (no por memoria).

> Este documento es una **síntesis de trabajo**. Ante cualquier duda de redacción prevalece
> el PDF oficial.

## 1. Requisitos de misión (apéndice C)

| ID | Requisito | Implicancia |
|---|---|---|
| **C5** | Telemetría a **1 Hz**. | El scheduler ya emite una trama por segundo. |
| **C6** | Video de todo el descenso apuntando a **cenit**. | Cámara XIAO cenit + almacenamiento. |
| **C7** | Video de todo el descenso apuntando a **nadir**. | Cámara XIAO nadir. |
| **C8** | **Baliza audible** que encienda por separado y sea **independiente de la batería y de la electrónica** del CanSat. | Buzzer + batería 10440 + interruptor propio. **Obligatorio.** |

## 2. Requisitos eléctricos y de contenedor (apéndice E)

| ID | Requisito | Implicancia |
|---|---|---|
| **E1** | No se permiten baterías de polímero de litio; se admiten alcalina, Ni-Cd, Ni-MH o litio; se permiten celdas tipo **18650**. | La 18650 1S del v.2 cumple. |
| **E4** | **Interruptor de encendido** fácilmente accesible a través del contenedor para poner la carga útil en **STAND-BY**. | `SW1` (SW-ROT-3). **Obligatorio**: no es opcional ni reemplazable por comando. |
| **E5** | Orificios de acceso para los interruptores de **≤ 10 mm**. | Diseño mecánico (IEM 4). |
| **E6** | **Indicador de encendido**. | LED de estado (Fase 5). **Obligatorio.** |
| **E7** | El CanSat debe funcionar **≥ 2 horas** integrado en el cohete. | Justifica apagar la carga útil hasta T-60 s. |
| **E8** | La baliza de audio debe funcionar con **batería independiente**. | Batería propia del buzzer. |
| **E9** | La baliza de audio debe tener **interruptor propio** accesible desde el contenedor. | Interruptor independiente del de la carga útil. |

## 3. Requisitos de radiofrecuencia (apéndice X)

| ID | Requisito | Implicancia |
|---|---|---|
| **X1** | Bandas autorizadas: **915–928 MHz** y **2400–2483.5 MHz**. | El XBee-PRO 900HP cubre 902–928 MHz: **configurar para operar dentro de 915–928 MHz**. |
| **X3** | La telemetría debe enviar **un comando automáticamente** cuando el CanSat se separa del cohete (comienzo de misión). | Implementado como `CMD:MISSION_START;STATE:Ascent` al detectar lanzamiento. |

> ⚠️ **Corrección respecto de la información verbal:** la banda autorizada por X1 es
> **915–928 MHz**, no 902–928 MHz. El extremo 902–915 MHz no debe usarse en vuelo.

## 4. Requisitos de operación (apéndice F)

| ID | Requisito | Implicancia |
|---|---|---|
| **F2** | Mantener el **tiempo de misión** ante reinicios o pérdida de energía. | FRAM + RTC; hoy hay hueco de reboot (ver sesión de Cortex). |
| **F3** | Ajuste de hora por comando con margen de **1 s**. | Comando `ST`. |
| **F4** | Comandos adicionales **documentados en el manual**. | `RPI`, `RPIOFF`, `CAL`, `MEC` deben figurar en el manual de operación. |

## 5. Checklist de cumplimiento

- [x] Batería 18650 1S (E1).
- [x] Interruptor de encendido accesible (E4) — `SW1` en el v.2.
- [ ] Orificios ≤ 10 mm (E5) — verificar en mecánica.
- [ ] Indicador de encendido (E6) — software y patrones listos ([`indicadores-v2.md`](../03-subsistemas/software-vuelo/indicadores-v2.md)); falta cablear el LED.
- [ ] Baliza audible independiente + interruptor propio (C8/E8/E9) — hardware independiente del OBC.
- [x] Telemetría 1 Hz (C5).
- [x] Comando automático al separarse (X3).
- [ ] XBee configurado en 915–928 MHz (X1).
- [x] Tiempo de misión robusto a reinicios (F2) — RTC `RV-3028-C7` implementado; falta validar en placa (ver [`tiempo-mision-v2.md`](../03-subsistemas/software-vuelo/tiempo-mision-v2.md)).
- [x] Hora por comando `ST` (F3).
- [ ] Documentar `RPI`/`RPIOFF` en el manual (F4).

## 6. Requisitos que NO están en el Anexo A pero condicionan el diseño

- **Peso y dimensiones** del contenedor: ver Bases y condiciones.
- **Protocolo de telecomando** y formato de trama: a acordar con ISI 2.
- **Frecuencia de muestreo de sensores**: la misión exige 1 Hz de telemetría; el
  muestreo interno puede ser mayor.

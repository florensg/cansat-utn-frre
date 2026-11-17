---
title: Ubiquitous Language Guide
tags: [glossary, domain, cortex-context]
created: 2026-05-13
project: CanSat
---

# Ubiquitous Language (CONTEXT.md)

Este archivo define el **vocabulario canonico del dominio** de `CanSat`.
Los skills `cortex-sync` y `cortex-documenter` lo leen al boot para usar
terminos consistentes. **No es una capa de retrieval** — es material de
prompt para los agentes.

## Como extenderlo

Cuando descubras un termino del dominio nuevo:

1. Agregalo a la tabla de abajo con su definicion canonica.
2. Lista sinonimos prohibidos (que NO se deben usar en docs).
3. Si entra en conflicto con uso previo, crea un ADR de rename.

## Terminos

| Termino canonico | Definicion | Sinonimos prohibidos |
|------------------|------------|----------------------|
| `PACKET_COUNT` | Contador monotónico de paquetes transmitidos por el OBC (persistido en FRAM, requisito F1). | `RX_COUNT`, `paquetes recibidos` |
| `RX_COUNT` | Conteo independiente de paquetes recibidos y validados con éxito en la Estación Terrena (requisito G14). | `PACKET_COUNT`, `paquetes transmitidos` |
| `OPTIONAL_DATA` | Columna 32 (índice base 0) de la trama CSV de 33 campos que transporta `MAX`, `P2`, `CMD` y el resumen `$RPI`. | `extra_data`, `payload_tail` |
| `Ventana Silenciosa UART` | Ventana de ~800 ms posterior a la recepción del paquete de 1 Hz (con guarda de 60 ms) usada por la GCS para enviar telecomandos sin colisión RF. | `envío asíncrono libre` |

## Reglas de uso

1. Usa SIEMPRE el termino canonico en session notes y specs.
2. Si descubris un nuevo concepto del dominio, registralo aqui ANTES de usarlo.
3. Si un concepto entra en conflicto con uso previo, crea un ADR de rename.

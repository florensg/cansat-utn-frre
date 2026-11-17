---
title: Architecture Decision Records (ADRs)
tags: [adr, architecture, decisions]
created: 2026-04-13
---

# Architecture Decision Records

> Log of significant technical decisions and their rationale.

## ADR-001: Initialize Cortex Memory System

- **Status**: Accepted
- **Date**: 2026-04-13
- **Context**: Need persistent memory for AI agents and CI/CD pipelines.
- **Decision**: Adopt Cortex hybrid memory system (episodic + semantic).
- **Consequences**:
  - CI/CD pipeline results are stored as episodic memories
  - Documentation auto-generated into vault
  - Hybrid RRF search enables context-aware retrieval for agents

---

## ADR-002: Contrato ICD de 33 Columnas, Ventana Anti-Colisión y Pipeline Post-Vuelo NIR en GCS (ISI 2)

- **Status**: Accepted
- **Date**: 2026-11-16
- **Context**: El firmware del OBC (`telemetry.cpp`) emite una trama CSV extendida de 33 columnas a 1 Hz y el reglamento CONAE (`Anexo A v2.pdf`, requisitos G1–G15 y X1–X6) exige visualización simultánea sin pestañas (G13), tema claro de alto contraste con fuente >= 14 pt para luz solar directa (G12), unidades SI (G6), conteo `RX_COUNT` independiente de `PACKET_COUNT` (G14), respaldo en tiempo real en disco secundario/USB (G2) y análisis real de píxeles NIR.
- **Decision**:
  1. Congelar el contrato ICD de 33 columnas en `gcs/docs/protocolo.md` y validarlo estrictamente en `gcs/src/decoder.py`.
  2. Implementar `TelemetrySessionController` (`gcs/src/serial_manager.py`) con ventana anti-colisión (guarda de 60 ms + ventana de 800 ms tras el paquete de 1 Hz) y bloqueo de seguridad de `MEC` en estado `Ascent`.
  3. Diseñar `CanSatMainWindow` (`gcs/src/gui.py`) en una única pantalla sin `QTabWidget`, con 4 gráficos `PyQtGraph` en tiempo real y hoja de estilos de alto contraste solar (>= 14 pt).
  4. Implementar `gcs/src/post_flight_nir.py` decodificando la matriz 2D real de píxeles de las capturas `NIR_*.png` para segmentar agua/sombra, suelo firme, pastizal abierto y vegetación densa/obstáculos.
- **Consequences**:
  - Cumplimiento trazable de los requisitos G1–G15 y X1–X6 de CONAE sin modificar `firmware/` ni `hardware/`.
  - Compatibilidad multiplataforma en macOS y Windows con modo `--simulate` y `--headless` para pruebas continuas.

---

## Template: New ADR

Copy this template for each new decision:

### ADR-NNN: Title

- **Status**: Proposed | Accepted | Deprecated | Superseded
- **Date**: YYYY-MM-DD
- **Context**: What is the issue/decision we're facing?
- **Decision**: What did we decide?
- **Consequences**: What are the trade-offs and impacts?

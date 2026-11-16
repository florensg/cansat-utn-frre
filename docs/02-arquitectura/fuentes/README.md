# Fuentes de hardware (circuito v.2)

Documentos originales de los que sale el pinmap. Son la **fuente de verdad** del
conexionado; el resto de la documentación los resume.

| Archivo | Qué es | Cómo leerlo |
|---|---|---|
| [`cuadro-conexionado-esp32s3-cansat-v2.docx`](cuadro-conexionado-esp32s3-cansat-v2.docx) | Cuadro de conexionado completo del ESP32-S3 | **Amarillo = conectado**. Las celdas sin resaltar son propuestas no cableadas |
| [`conexion-transistores.jpeg`](conexion-transistores.jpeg) | Lista de transistores de las llaves de alimentación (`EN_*`) | Más reciente que el `.docx`; su asignación de GPIO 14/15/16 quedó **reemplazada** por la del cuadro |

> **Conflicto resuelto (2026-11-16):** ante la discrepancia de GPIO 14/15/16 entre
> ambos documentos, manda el cuadro de conexionado. Las UART se quedan en
> GPIO13–18 y `EN_NADIR`/`EN_SERVO`/`EN_SEC` pasan a GPIO38/39/40. Ver §5 de
> [`../pinmap-esp32s3-v2.md`](../pinmap-esp32s3-v2.md) y el resumen de cambios en
> [`../correcciones-conexionado-y-estado-firmware-v2.md`](../correcciones-conexionado-y-estado-firmware-v2.md).

Los nombres de archivo se normalizaron (minúsculas, sin espacios) según las
convenciones de [`../../README.md`](../../README.md); el contenido es el original.

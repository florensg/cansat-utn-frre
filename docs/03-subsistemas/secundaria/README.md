# Software de misión secundaria (ISI 1)

Responsable: **Florencia González (ISI 1)**. Apoyo: Agustín Brites (ISI 2) y Juan Pedro
López (IEM 3).

## Alcance

- Arquitectura de software del procesador secundario Raspberry Pi Zero 2 W.
- Gestión de cámara visible y cámara NIR IMX219 NoIR.
- Sincronización de imágenes con tiempo, GPS, altitud y actitud.
- Almacenamiento en microSD y respaldo de resultados.
- Reparto de procesamiento entre la placa secundaria y el post-proceso.
- Interfaz UART con el OBC y formato `OPTIONAL_DATA`.
- Modos de consumo y apagado controlado.

## Arquitectura

```text
firmware/payload-rpi/
├── src/main.py            Lectura UART, registro y orquestación
├── requirements.txt       Dependencia de comunicación serie
└── README.md              Contrato de datos y operación
```

La placa secundaria recibe del OBC las líneas de telemetría y los eventos de misión. Cada
registro se guarda con marca de tiempo y se asocia a la imagen capturada en la misma
ventana temporal.

## Protocolo con el OBC

- Enlace UART a 115200 baudios.
- Tramas ASCII terminadas en salto de línea.
- El OBC envía telemetría y eventos; la placa secundaria responde con acuse y actitud.
- Cada mensaje incluye versión de protocolo, tipo, tiempo de misión y carga útil.
- El detalle de campos se congela junto con ISI 2 antes de la integración.

## Productos de datos

- Imágenes visibles y NIR en formato sin pérdida.
- CSV de metadatos con tiempo, GPS, altitud, actitud y nombre de archivo.
- Resultados de índices multiespectrales y clasificación.
- Registro de errores y estado de la placa.

## Modos de operación

| Modo | Descripción | Consumo esperado |
|---|---|---|
| Reposo | Espera de separación | Mínimo, cámara apagada |
| Adquisición | Captura y registro | Medio |
| Procesamiento | Cálculo de índices y clasificación | Alto |
| Apagado | Cierre ordenado y desenergización | Nulo |

## Pendientes de integración

- Formato exacto de `OPTIONAL_DATA` con ISI 2.
- Asignación de pines UART y niveles de tensión con IEM 3.
- Ubicación física y disipación térmica con IEM 4.
- Consumo real por modo con IEM 1.

## Evidencias para la PDR

- Diagrama de arquitectura del procesador secundario.
- Contrato de protocolo UART.
- Ejemplo de registro sincronizado imagen + metadatos.
- Medición de consumo por modo.
- Prueba de captura y almacenamiento continuo.

# Determinación de actitud — misión secundaria (ISI 1)

Responsable: **Florencia González (ISI 1)**. Apoyo: Juan Pedro López (IEM 3).

Objetivo: estimar la orientación tridimensional del CanSat durante el descenso y entregar
roll, pitch y yaw junto con la marca de tiempo, la posición GPS y la altitud.

## Sensores

| Sensor | Variable | Bus | Uso |
|---|---|---|---|
| MPU-6050 / GY-521 | Aceleración y velocidad angular 3 ejes | I2C | Actitud inercial |
| LIS3MDL | Campo magnético 3 ejes | I2C | Corrección de yaw |
| BPW34 + ADS1113 | Intensidad solar direccional | I2C | Vector solar |
| NEO-M8 | Posición y tiempo | UART | Referencia temporal y geográfica |

## Sistema de referencia

- Eje X: longitudinal del CanSat, positivo hacia la tapa superior.
- Eje Y: transversal.
- Eje Z: completar terna derecha.
- La referencia mecánica definitiva se acuerda con IEM 4 para que coincida con el CAD y el
  montaje de la cámara nadir.
- El marco de navegación es NED (North-East-Down) y la actitud se expresa como cuaternión y
  como ángulos de Euler para telemetría.

## Algoritmo

1. Calibración de acelerómetro y giroscopio: bias y escala.
2. Calibración de magnetómetro: corrección de hierro duro y blando.
3. Calibración de sensores solares: respuesta angular y saturación.
4. Filtro complementario o Madgwick para fusionar giroscopio, acelerómetro y magnetómetro.
5. Corrección con el vector solar cuando hay iluminación suficiente.
6. Salida: cuaternión, roll, pitch, yaw, indicador de validez y residuales.

## Datos generados

```text
timestamp_ms,qw,qx,qy,qz,roll_deg,pitch_deg,yaw_deg,sun_valid,mag_valid,residual
```

Los resultados se registran en la microSD y se transmiten resumidos en `OPTIONAL_DATA`.

## Validación

- Orientaciones conocidas con el CanSat estático.
- Rotaciones controladas en banco.
- Comparación contra una referencia externa.
- Repetibilidad y deriva durante al menos 10 minutos.
- Ensayo con iluminación y sombra para validar el vector solar.

## Interfaz con el OBC

- El OBC entrega tiempo de misión, GPS y altitud.
- La placa secundaria devuelve actitud y validez por UART.
- El OBC incluye el resultado en `OPTIONAL_DATA` y en el registro de la microSD.
- El protocolo exacto se define junto con ISI 2 y queda documentado en
  [`docs/03-subsistemas/secundaria/README.md`](../secundaria/README.md).

## Evidencias para la PDR

- Diagrama de ejes y montaje.
- Curvas de calibración.
- Resultados de validación con orientaciones conocidas.
- Error cuadrático medio de roll, pitch y yaw.
- Registro de una prueba de 10 minutos.

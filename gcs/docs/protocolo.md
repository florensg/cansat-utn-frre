# Interface Control Document (ICD) — Protocolo de Telemetría y Telecomandos (OBC ↔ GCS)

- **Proyecto:** CanSat Universitario 2026-2027 — UTN Facultad Regional Resistencia (CONAE)
- **Versión del contrato:** 2.0 (Alineado a `PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf` y `Anexo A v2.pdf`)
- **Responsables:** Florencia González (`ISI 1` — Software de Vuelo OBC) y Agustín Brites (`ISI 2` — Estación Terrena GCS)
- **Estado:** Congelado

---

## 1. Enlace Físico y Transporte

| Parámetro | Valor Oficial | Requisito CONAE |
|---|---|---|
| **Módulo RF** | XBee S1 (2.4 GHz ISM) / Enlace Serie Transparente | **X1, X2** (2400–2483.5 MHz, direccionamiento punto a punto sin broadcast) |
| **Interfaz Serie** | UART `115200` baudios, `8N1`, sin control de flujo por hardware | — |
| **Frecuencia de Telemetría** | `1 Hz` (`1000 ms` por trama) | **C5, X4** |
| **Codificación** | ASCII UTF-8, líneas terminadas en `\n` (`LF` o `CRLF`) | **G5** |

---

## 2. Trama de Telemetría de Bajada (Downlink: OBC → GCS)

El software de vuelo (`firmware/obc-esp32s3/lib/flight_core/src/telemetry.cpp`) emite exactamente **33 campos separados por coma (`,`)** una vez por segundo:

```text
TEAM_ID,MISSION_TIME_MS,PACKET_COUNT,MODE,STATE,ALT_M,PRESS_PA,TEMP_C,BATT_V,CURR_MA,ACC_X,ACC_Y,ACC_Z,GYR_X,GYR_Y,GYR_Z,LAT,LON,GPS_ALT_M,SATS,MAG_X_UT,MAG_Y_UT,MAG_Z_UT,OPT1,OPT2,OPT3,OPT4,RTC_H,RTC_M,RTC_S,RTC_VALID,GPS_FIX,OPTIONAL_DATA
```

### 2.1 Tabla Detallada de las 33 Columnas

| Índice | Nombre de Columna | Tipo C++ / Python | Formato `snprintf` | Unidad en Trama | Unidad SI en GUI (G6) | Descripción y Requisito |
|:---:|---|---|---|---|---|---|
| `0` | `TEAM_ID` | `uint16` / `int` | `%u` | Adimensional | — | Identificador del equipo (`1000`). |
| `1` | `MISSION_TIME_MS` | `uint32` / `int` | `%lu` | `ms` | `s` (`hh:mm:ss`) | Tiempo acumulado de misión (**G3, F2**). |
| `2` | `PACKET_COUNT` | `uint32` / `int` | `%lu` | Paquetes | Paquetes | Contador monotónico de paquetes transmitidos persistido en FRAM (**F1, G14**). |
| `3` | `MODE` | `uint8` / `int` | `%u` | `0=Vuelo`, `1=Sim` | — | Modo operativo del OBC. |
| `4` | `STATE` | `string` | `%s` | Enum | — | Estado FSM: `LaunchPad`, `Ascent`, `Descent`, `Landed` (**G8, F5**). |
| `5` | `ALT_M` | `float` | `%.2f` | `m` | `m` | Altitud barométrica relativa a la rampa (**SN1, G7**). |
| `6` | `PRESS_PA` | `float` | `%.2f` | `Pa` | `Pa` / `kPa` | Presión atmosférica absoluta del `BMP280` (**X5**). |
| `7` | `TEMP_C` | `float` | `%.2f` | `°C` | `°C` | Temperatura interna (`MPU-6050` / `BMP280`) (**SN2, X5, G8**). |
| `8` | `BATT_V` | `float` | `%.2f` | `V` | `V` | Tensión del bus de batería vía `INA219` (**SN3, X6, G7**). |
| `9` | `CURR_MA` | `float` | `%.2f` | `mA` | `A` (`mA / 1000`) | Corriente instantánea de batería vía `INA219` (**SN8, G7**). |
| `10` | `ACC_X` | `float` | `%.3f` | `g` | `m/s²` (`g × 9.80665`) | Aceleración eje X (`MPU-6050`) (**SN5, X5, G7**). |
| `11` | `ACC_Y` | `float` | `%.3f` | `g` | `m/s²` (`g × 9.80665`) | Aceleración eje Y (`MPU-6050`) (**SN5, X5, G7**). |
| `12` | `ACC_Z` | `float` | `%.3f` | `g` | `m/s²` (`g × 9.80665`) | Aceleración eje Z (`MPU-6050`) (**SN5, X5, G7**). |
| `13` | `GYR_X` | `float` | `%.2f` | `deg/s` (`°/s`) | `rad/s` / `°/s` | Velocidad angular Roll X (`MPU-6050`) (**SN5, X5, G7**). |
| `14` | `GYR_Y` | `float` | `%.2f` | `deg/s` (`°/s`) | `rad/s` / `°/s` | Velocidad angular Pitch Y (`MPU-6050`) (**SN5, X5, G7**). |
| `15` | `GYR_Z` | `float` | `%.2f` | `deg/s` (`°/s`) | `rad/s` / `°/s` | Velocidad angular Yaw Z (`MPU-6050`) (**SN5, X5, G7**). |
| `16` | `LAT` | `float` | `%.6f` | `deg` (`°`) | `°` | Latitud GPS `NEO-M8N` (**SN4, X5, G8**). |
| `17` | `LON` | `float` | `%.6f` | `deg` (`°`) | `°` | Longitud GPS `NEO-M8N` (**SN4, X5, G8**). |
| `18` | `GPS_ALT_M` | `float` | `%.2f` | `m` | `m` | Altitud sobre el nivel del mar reportada por GPS (**X5**). |
| `19` | `SATS` | `uint8` / `int` | `%u` | Satélites | Satélites | Número de satélites rastreados (**X5**). |
| `20` | `MAG_X_UT` | `float` | `%.1f` | `µT` | `µT` | Campo magnético eje X (`LIS3MDL`). |
| `21` | `MAG_Y_UT` | `float` | `%.1f` | `µT` | `µT` | Campo magnético eje Y (`LIS3MDL`). |
| `22` | `MAG_Z_UT` | `float` | `%.1f` | `µT` | `µT` | Campo magnético eje Z (`LIS3MDL`). |
| `23` | `OPT1` | `uint16` / `int` | `%u` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U4` (+X, canal `AIN3` del `ADS1115`) (**E3, X6**). |
| `24` | `OPT2` | `uint16` / `int` | `%u` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U5` (-X, canal `AIN2` del `ADS1115`) (**E3, X6**). |
| `25` | `OPT3` | `uint16` / `int` | `%u` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U6` (+Y, canal `AIN1` del `ADS1115`) (**E3, X6**). |
| `26` | `OPT4` | `uint16` / `int` | `%u` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U7` (-Y, canal `AIN0` del `ADS1115`) (**E3, X6**). |
| `27` | `RTC_H` | `uint8` / `int` | `%u` | Horas (`0–23`) | `hh:mm:ss UTC` | Hora UTC sincronizada por GPS o comando `ST` (**F3**). |
| `28` | `RTC_M` | `uint8` / `int` | `%u` | Minutos (`0–59`) | `hh:mm:ss UTC` | Minuto UTC (**F3**). |
| `29` | `RTC_S` | `uint8` / `int` | `%u` | Segundos (`0–59`) | `hh:mm:ss UTC` | Segundo UTC (**F3**). |
| `30` | `RTC_VALID` | `uint8` / `int` | `%u` | `0` o `1` | Booleano | `1` si el reloj UTC fue sincronizado por GPS o `ST`. |
| `31` | `GPS_FIX` | `uint8` / `int` | `%u` | `0` o `1` | Booleano | `1` si el GPS posee fix válido (`fix_quality > 0`). |
| `32` | `OPTIONAL_DATA` | `string` | `%s` | Clave-valor | — | Resumen de eventos de vuelo, eco de comando y misión secundaria (`RPI`). |

---

## 3. Estructura de `OPTIONAL_DATA` (Columna 32)

El campo `OPTIONAL_DATA` codifica pares `CLAVE:VALOR` separados por punto y coma (`;`), sin comas internas para preservar las 33 columnas del CSV:

```text
MAX:<max_alt_m>;P2:<0|1>;CMD:<last_cmd_echo>;RPI:YAW:<yaw>|SUN:<sun_az>|NIR:<count>|APT:<APTA|NO_APTA|PENDIENTE>(<score>)
```

Ejemplo real emitido por `src/main.cpp`:
```text
MAX:400.0;P2:1;CMD:CAL;RPI:YAW:182|SUN:135|NIR:12|APT:APTA(0.84)
```

| Subcampo | Significado |
|---|---|
| `MAX` | Altitud máxima registrada por `ApogeeDetector` en metros (`float`). |
| `P2` | `1` si se disparó el segundo paracaídas (automático al 80 % o por comando `MEC`), `0` en caso contrario. |
| `CMD` | Eco del último telecomando ejecutado por el OBC (`NONE`, `CX`, `ST`, `CAL`, `MEC`, `RST`, `START`). |
| `RPI` | Resumen reportado por la Raspberry Pi Zero 2W con `YAW` (grados), `SUN` (acimut solar en grados), `NIR` (cantidad de capturas IMX219 NoIR) y `APT` (clasificación de aptitud de aterrizaje y puntaje `0.00–1.00`). |

---

## 4. Mensajes Asíncronos de Evento (OBC → GCS)

Además de la trama CSV de `1 Hz`, el OBC puede emitir líneas de evento o respuestas directas a comandos:

- **Notificación automática de inicio de misión (Requisito X3):**
  ```text
  CMD:MISSION_START;STATE:Ascent
  ```
- **Eventos de transición:**
  - `EVENT: LAUNCH_DETECTED -> Ascent`
  - `EVENT: APOGEE_DETECTED -> Descent (Max Alt: <H> m)`
  - `EVENT: SECOND_PARACHUTE_80_PERCENT -> Servo 90 deg (Alt: <H> m)`
  - `EVENT: LANDED_DETECTED -> Landed`

---

## 5. Telecomandos (Uplink: GCS → OBC) y Ventana Anti-Colisión

### 5.1 Catálogo de Telecomandos Oficiales (`commands.cpp`)

| Comando Enviado | Argumentos | Propósito y Requisito | Respuesta Directa del OBC | Eco en `OPTIONAL_DATA` |
|---|---|---|---|---|
| `CX\n` | Ninguno | Consulta estado de batería, corriente y señal de los 4 sensores solares (**X6, F4, G15**). | `CX OK <STATE> PKT=<N> BATT=<V>V CURR=<mA>mA SOLAR=<U4>,<U5>,<U6>,<U7>` | `CMD:CX` |
| `ST HH:MM:SS\n` | Hora UTC (`HH:MM:SS` o ISO-8601) | Sincroniza el reloj UTC del OBC con margen $< 1\text{ s}$ antes del despegue (**F3, G15**). | `ST OK` | `CMD:ST` |
| `CAL\n` | Ninguno | Calibra la altitud barométrica a `0.00 m` en la plataforma de lanzamiento y persiste en FRAM (**G1, F5, G15**). | `CAL OK` | `CMD:CAL` |
| `MEC\n` | Ninguno | Activa manualmente el servo `SG90` (`90°`) para liberar el 2.º paracaídas (requiere doble confirmación en GUI y bloqueo en ascenso). | `MEC OK (Servo liberado -> 90 deg)` | `CMD:MEC` |
| `RST\n` | Ninguno | Reinicia la FSM a `LaunchPad` (solo permitido en banco/rampa). | `RST OK` | `CMD:RST` |

### 5.2 Temporización Anti-Colisión en Enlace Serie Transparente

Dado que el enlace XBee S1 opera en modo transparente half-duplex sobre el aire:
1. El OBC transmite la trama CSV cada `1000 ms` (duración en aire/UART $\approx 25\text{--}40\text{ ms}$).
2. Cuando el operador solicita enviar un telecomando en la GCS, el `SerialManager` lo encola y espera inmediatamente al fin de recepción del paquete periódico de `1 Hz` (con un retardo de guarda de `60 ms` tras el `\n`), transmitiendo el comando dentro de la **ventana silenciosa de ~800 ms** del OBC.
3. Si no se recibe telemetría (por ejemplo, antes de iniciar o tras pérdida temporal de enlace), el comando se transmite de forma inmediata tras un timeout de espera de `1100 ms`.
4. Un comando pasa de estado `ENVIADO` a `CONFIRMADO` en cuanto se recibe la línea `<CMD> OK` o el campo `CMD:<CMD>` dentro de `OPTIONAL_DATA`. Si transcurren `3.0 s` sin acuse, se marca como `SIN CONFIRMACIÓN`.

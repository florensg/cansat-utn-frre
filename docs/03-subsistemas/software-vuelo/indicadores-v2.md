# Indicadores de estado, watchdog y causa de reinicio (circuito v.2)

Cubre el **indicador de encendido (E6)** y la robustez ante cuelgues. La
**baliza audible (C8/E8/E9)** queda deliberadamente fuera del firmware.

## 1. LED de estado (E6)

Requisito: indicador de encendido visible a través del contenedor.

### Semáforo de salud

`status_severity()` (`lib/flight_core/src/status.cpp`) resume el estado del OBC
en tres niveles:

| Nivel | Condición | LED OK | LED de falla |
|---|---|---|---|
| `Ok` | barómetro, IMU, INA219, FRAM y RTC presentes | fijo encendido | apagado |
| `Degraded` | falta IMU / INA219 / FRAM / RTC, o el RTC perdió la hora (`PORF`) | parpadea 1 Hz | apagado |
| `Fault` | falta el **barómetro** | apagado | parpadea 1 Hz |

Por qué el barómetro es la única falla crítica: la altitud que alimenta la
detección de apogeo y del segundo paracaídas sale de `read_barometer()`. Sin
barómetro el lazo de vuelo no puede cumplir su función, aunque la telemetría
siga saliendo.

El parpadeo a 1 Hz (500 ms encendido / 500 ms apagado) también actúa como
**latido**: si el firmware se cuelga, el patrón se congela y se ve a simple
vista, algo que un LED fijo no mostraría.

### Cableado

| Señal | Constante en `config.h` | Estado |
|---|---|---|
| LED OK | `kPinStatusLedOk` | **sin asignar** (`kPowerPinUnassigned`) |
| LED de falla | `kPinStatusLedFault` | **sin asignar** (`kPowerPinUnassigned`) |

`src/status_output.cpp` hace `pinMode()`/`digitalWrite()` solo si el pin está
asignado; con el valor actual no toca ningún GPIO. Para activarlo alcanza con
definir los dos pines en `config.h` cuando IEM 4 cierre el cableado.

> El GPIO no debe alimentar el LED directamente si se busca el mínimo consumo
> (E7): usar un MOSFET o un LED de bajo consumo con resistencia serie. El
> consumo del indicador entra en el presupuesto de la 18650.

## 2. Baliza audible (C8/E8/E9): NO la controla el OBC

Los requisitos piden una baliza **independiente de la batería y de la
electrónica** del CanSat, con **interruptor propio**. Por eso el firmware **no**
la maneja:

- Si el OBC se cuelga o se queda sin energía, la baliza debe seguir sonando.
- `kPinBuzzer` (GPIO6) queda reservado en `config.h` pero **no se usa** en
  `main.cpp`. El GPIO6 solo podría controlar un driver/MOSFET, nunca alimentar
  el buzzer.

Conclusión: C8/E8/E9 se cumplen en el arnés (buzzer 10440 + interruptor), no en
software. Este documento lo deja explícito para que nadie agregue un
`digitalWrite(kPinBuzzer, ...)` pensando que hace falta.

## 3. Task Watchdog Timer (TWDT)

| Parámetro | Valor |
|---|---|
| Timeout | `kWatchdogTimeoutMs` = 4000 ms |
| Acción | `trigger_panic = true` → reinicio del OBC |
| Suscripción | tarea de `loop()` (`esp_task_wdt_add(nullptr)`) |
| Refresco | `watchdog_feed()` al inicio de cada `loop()` |
| Inicialización | **al final de `setup()`** |

Se inicializa al final del `setup()` a propósito: el sondeo de sensores, el RTC
y el arranque de las XIAO pueden tardar más que el timeout. Una vez que el lazo
principal está corriendo, cada vuelta debe completarse en menos de 4 s.

Compatibilidad: `src/watchdog.cpp` usa la API de estructura en Arduino‑ESP32
3.x/IDF 5.x y la de `timeout_s + panic` en 2.x. Si el TWDT ya venía
inicializado, `ESP_ERR_INVALID_STATE` se trata como éxito y se respeta la
configuración vigente.

## 4. Causa del reinicio

`reset_reason_tag()` traduce `esp_reset_reason()` a un carácter que se publica
en el arranque y en **cada** trama de telemetría:

| Tag | Causa |
|---|---|
| `P` | power-on (encendido normal) |
| `S` | reset por software |
| `W` | task watchdog (cuelgue del lazo) |
| `I` | interrupt watchdog |
| `X` | panic / excepción |
| `B` | brownout (caída de tensión) |
| `E` | reset externo |
| `D` | deep sleep |
| `O` | SDIO |
| `?` | desconocida |

Banner de arranque:

```text
RTC:OK 2026-11-16 12:34:56 UTC (semana 1)
RTC:OK TSRC:R RST:P (power-on)
WDT:ON timeout=4000 ms
```

Telemetría (`OPTIONAL_DATA`, sin tocar el orden de columnas):

```text
...;PWR:03FF;TSRC:R;RST:P
```

Un `RST:W` en vuelo es la señal de que el lazo se colgó y el watchdog rescató
el OBC: hay que revisar el log de la GCS y el instante del reinicio.

## 5. Pruebas

Tests nativos (`pio test -e native`):

| Test | Qué verifica |
|---|---|
| `test_status_severity_ok` | Todos los subsistemas presentes → `Ok` |
| `test_status_severity_degraded` | IMU/RTC faltante o `PORF` → `Degraded` |
| `test_status_severity_fault` | Barómetro ausente → `Fault` |
| `test_status_leds_patterns` | LED fijo en `Ok`, 1 Hz en `Degraded`/`Fault`, y OK apagado en falla |

**Verificación ejecutada (2026‑11‑16):**

- **Compilación real OK**: `pio run -e obc` con PlatformIO 6.2.0 y
  Arduino-ESP32 3.3.12, que es la versión que ejercita la rama de API del TWDT
  para 3.x/IDF 5.x. Flash 416 386 B (12.5 %), RAM estática 24 808 B (7.6 %).
- Análisis estático: balance de llaves/paréntesis en los archivos tocados.
- Los **tests nativos no se ejecutaron**: falta un compilador de host (MinGW)
  para el env `native`. La lógica de severidad y patrones queda cubierta por
  los tests escritos, pendientes de correr.

## 6. Pendientes

> Esta entrega es **teórica**: el cableado de LED, baliza y llaves se hace en la
> etapa de integración. Lo que sigue es de esa etapa.

1. **Cablear** los LED y asignar `kPinStatusLedOk` / `kPinStatusLedFault`.
2. **Medir** el consumo del LED (presupuesto E7 de 2 h).
3. **Provocar un cuelgue** controlado para confirmar que el TWDT reinicia y que
   el `RST:W` aparece en telemetría.
4. **Confirmar** que el arranque de las XIAO no supera los 4 s con el watchdog
   ya activo (se activa después de `init_xiao_cameras()`, así que el riesgo es
   en una re-sonda diferida; si molesta, subir `kWatchdogTimeoutMs`).
5. **Verificar brownout**: con la 18650 y el TPS25940A no debería aparecer
   `RST:B`; si aparece, revisar el orden de encendido de los rieles.

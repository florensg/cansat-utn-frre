# Estación Terrena (GCS) y Procesamiento Post-Vuelo — CanSat YVYSAT (UTN FRRe)

- **Certamen:** CanSat Universitario 2026–2027 (CONAE)
- **Equipo:** YVYSAT — Universidad Tecnológica Nacional, Facultad Regional Resistencia (`TEAM_ID = 1000`)
- **Módulo:** Subsistema de Estación Terrena, Gestión de Datos y Procesamiento Multiespectral Post-Vuelo (**ISI 2**)
- **Responsable:** Agustín Brites (`ISI 2`)
- **Interfaces asociadas:** Florencia González (`ISI 1` — Software de Vuelo OBC y Misión Secundaria) y Juan Pedro López (`IEM 3` — Electrónica, Sensores y RF)

---

## Tabla de Contenidos

1. [Descripción General y Arquitectura del Subsistema (Cumplimiento CONAE G1–G15)](#1-descripción-general-y-arquitectura-del-subsistema-cumplimiento-conae-g1g15)
2. [Hardware Necesario, Antenas y Conexiones (Obligatorio vs. Opcional)](#2-hardware-necesario-antenas-y-conexiones-obligatorio-vs-opcional)
3. [Instalación e Inicialización Paso a Paso (macOS y Windows)](#3-instalación-e-inicialización-paso-a-paso-macos-y-windows)
4. [Cómo Ejecutar Pruebas (Sin Hardware y Unitarias)](#4-cómo-ejecutar-pruebas-sin-hardware-y-unitarias)
5. [Protocolo Operativo para el Día de la Competencia (Checklist Pre-Vuelo, Vuelo y Post-Vuelo)](#5-protocolo-operativo-para-el-día-de-la-competencia-checklist-pre-vuelo-vuelo-y-post-vuelo)
6. [Guía Visual de la Interfaz (Qué Significa Cada Indicador, Botón y Gráfico)](#6-guía-visual-de-la-interfaz-qué-significa-cada-indicador-botón-y-gráfico)
7. [Archivos Generados, Ubicación y Formato de Datos (Contrato de 33 Columnas)](#7-archivos-generados-ubicación-y-formato-de-datos-contrato-de-33-columnas)
8. [Pipeline Post-Vuelo de Imágenes NIR/VIS (`src/post_flight_nir.py`)](#8-pipeline-post-vuelo-de-imágenes-nirvis-srcpost_flight_nirpy)

---

## 1. Descripción General y Arquitectura del Subsistema (Cumplimiento CONAE G1–G15)

La **Estación Terrena (Ground Control Station — GCS)** del proyecto **YVYSAT** (`TEAM_ID = 1000`) es el centro de comando, recepción telemétrica a `1 Hz`, visualización científica en tiempo real, persistencia redundante de datos y análisis multiespectral post-vuelo.

### 1.1 Principios de Ingeniería y Diseño

1. **Arquitectura Multihilo No Bloqueante (`PySide6` + `PyQtGraph` + `pyserial`):**
   - El hilo principal de la interfaz gráfica (`CanSatMainWindow` en [`src/gui.py`](src/gui.py)) se dedica exclusivamente al renderizado fluido de las 4 gráficas simultáneas y la interacción del operador.
   - La lectura/escritura del puerto serie y el simulador de vuelo corren en un hilo dedicado (`TelemetrySessionController` en [`src/serial_manager.py`](src/serial_manager.py)) comunicado mediante señales thread-safe (`_UiBridge`).
   - La replicación periódica a memoria externa USB se ejecuta en un hilo demonio independiente (`GCS-BackupSync` en [`src/storage.py`](src/storage.py)) cada `5.0 s`.
2. **Pantalla Única sin Pestañas y Alto Contraste Solar (Requisitos G12 y G13):**
   - **Prohibición estricta de `QTabWidget` (G13):** Todos los paneles numéricos, alarmas, contadores, telecomandos, consola cruda y los 4 gráficos en tiempo real están visibles simultáneamente en una única ventana.
   - **Legibilidad bajo luz solar intensa (G12):** Tema claro obligatorio (`#F8FAFC` / `#FFFFFF`) con texto oscuro (`#0F172A`), tipografía mínima de **`14 pt`** (`16–18 pt` en cabeceras y estados), ejes en negrita, trazos de gráficos de **`2.8 px`** e indicadores redundantes que combinan **color + símbolo + borde + texto explícito**.
3. **Tolerancia a Fallos y Cero Pérdida de Datos (Requisito G2):**
   - Cada línea entrante se registra primero en crudo (`raw_rx.log`) antes de validarse.
   - Cada trama válida de 33 columnas se escribe en `Flight_1000_<YYYYMMDD_HHMMSS>.csv` con `flush()` y `os.fsync()` inmediatos para garantizar cero pérdida ante un eventual corte eléctrico o cierre inesperado.

### 1.2 Diagrama de Flujo de Datos del Sistema YVYSAT

```text
+-----------------------------------------------------------------------------------+
| SEGMENTO DE VUELO — CANSAT YVYSAT (TEAM_ID = 1000)                                |
|                                                                                   |
|  [RPi Zero 2W (RPI1)] --(UART $RPI: LIS3MDL + ADS1115/BPW34 + IMX219 NoIR)-->     |
|                                                                               \   |
|  [Sensores I2C/SPI/UART] --> [OBC ESP32-S3 DevKitC-1] --> Trama CSV 33 cols (1 Hz)|
|  [Cámaras XIAO U8/U9]   <--(Disparo I2C 0x30/0x31)--                              |
+-----------------------------------------------------------------------------------+
                                         |
                                         | UART (9600 / 115200 bps, 8N1)
                                         v
+-----------------------------------------------------------------------------------+
| ENLACE DE RADIOFRECUENCIA (RF)                                                    |
|  Transceptor Digi XBee-PRO 900HP (XBP9B, 915–928 MHz ISM, FHSS, Modo Transparente)|
|  Downlink: Telemetría CSV 1 Hz + Eventos  |  Uplink: Telecomandos (CAL,ST,CX,MEC) |
+-----------------------------------------------------------------------------------+
                                         |
                                         | Antena Yagi 915 MHz -> XBee Receptor
                                         | Adaptador USB-Serie (FTDI / CP2102)
                                         v
+-----------------------------------------------------------------------------------+
| SEGMENTO TERRENO — ESTACIÓN TERRENA GCS (macOS / Windows)                         |
|                                                                                   |
|  [serial_manager.py] ---> [storage.py] ---> 1. raw_rx.log (Tramas crudas)         |
|   (Hilo Serie + Cola       (Flush + Sync)   2. Flight_1000_<TS>.csv (33 columnas) |
|    Anti-Colisión UART)          |           3. events.log (Bitácora de eventos)   |
|          |                      +---------> 4. Respaldo Automático USB (cada 5 s) |
|          v                                                                        |
|  [decoder.py + packet_monitor.py]                                                 |
|   (Validador 33 cols + Conversión SI + Conteo RX_COUNT vs PACKET_COUNT)           |
|          |                                                                        |
|          v                                                                        |
|  [gui.py (PySide6 + PyQtGraph)]                                                   |
|   (Pantalla Única Solar >=14pt + 4 Gráficos en Vivo + Alarmas + Telecomandos)     |
+-----------------------------------------------------------------------------------+
                                         |
                                         | Post-Recuperación (Extracción microSD)
                                         v
+-----------------------------------------------------------------------------------+
| PIPELINE POST-VUELO MULTIESPECTRAL (src/post_flight_nir.py)                       |
|  Entrada: Capturas NIR_*.png / VIS_*.jpg + payload_log.csv                        |
|  Salida:  Imágenes segmentadas (*_segmented.png) + post_flight_report.csv         |
|           + Gráfico resumen de aptitud (post_flight_summary.png)                  |
+-----------------------------------------------------------------------------------+
```

### 1.3 Matriz de Trazabilidad Reglamentaria CONAE (`Anexo A v2.pdf`)

| Código | Requisito Oficial CONAE | Módulo que lo Cumple en `gcs/` |
|:---:|---|---|
| **G1** | Enviar comando para calibrar la altitud a cero en plataforma de lanzamiento | [`src/gui.py`](src/gui.py) (`🎯 CAL (Altitud 0 m)`) y [`src/serial_manager.py`](src/serial_manager.py). |
| **G2** | Generar archivos CSV con todos los sensores en la PC y en disco de respaldo | [`src/storage.py`](src/storage.py) (`Flight_1000_<TS>.csv` + replicación automática USB cada 5 s). |
| **G3** | Incluir el tiempo de misión con resolución de 1 segundo | [`src/decoder.py`](src/decoder.py) (`mission_time_s`, `mission_time_hms`) y cabecera de [`src/gui.py`](src/gui.py). |
| **G4** | Desarrollo propio de estación terrena por el equipo | Código íntegro desarrollado por el equipo en [`gcs/src/`](src/). |
| **G5** | Mostrar toda la telemetría en tiempo real en formato texto durante ascenso y descenso | Consola inferior en vivo y paneles numéricos de [`src/gui.py`](src/gui.py). |
| **G6** | Mostrar telemetría en el Sistema Internacional de Unidades (SI) indicando unidades | Propiedades SI en [`src/decoder.py`](src/decoder.py) (`m`, `Pa`/`kPa`, `°C`, `V`, `A`, `m/s²`, `rad/s`, `µT`). |
| **G7** | Graficar altitud, voltaje, corriente, acelerómetro y tasas de rotación en tiempo real | 4 paneles `PyQtGraph` simultáneos en [`src/gui.py`](src/gui.py). |
| **G8** | Mostrar tiempo de misión, temperatura, GPS, paquetes recibidos, perdidos y estado FSM | Cabecera, panel de contadores y panel de sensores en [`src/gui.py`](src/gui.py). |
| **G12** | Diseño para luz solar intensa: fuente mínimo 14 pt, trazos y ejes gruesos, fondo claro | `SUNLIGHT_STYLESHEET` y `GUI_THEME` en [`src/config.py`](src/config.py) y [`src/gui.py`](src/gui.py). |
| **G13** | Todos los datos simultáneamente en la GUI. **No se permiten pestañas** | Layout matricial único sin `QTabWidget` en [`src/gui.py`](src/gui.py). |
| **G14** | Contar paquetes recibidos con éxito en tierra de forma independiente a los transmitidos | [`src/packet_monitor.py`](src/packet_monitor.py) (`RX_COUNT` independiente de `PACKET_COUNT`). |
| **G15 / X6** | Enviar telecomandos y consultar estado de batería y paneles solares | Botones `CAL`, `CX`, `ST`, `MEC` con ventana anti-colisión en [`src/serial_manager.py`](src/serial_manager.py). |

---

## 2. Hardware Necesario, Antenas y Conexiones (Obligatorio vs. Opcional)

### 2.1 Equipamiento Obligatorio para Operación en Vuelo (Campaña de Lanzamiento)

1. **Computadora Portátil (Estación Terrena):**
   - Sistema operativo **macOS** o **Windows 10/11**.
   - Autonomía de batería verificada $\ge 2\text{ horas}$ (Requisito **G9, G10** — no hay red de CA en la línea de vuelo) y brillo de pantalla al 100 %.
2. **Transceptor de Radiofrecuencia Terreno:**
   - Módulo **Digi XBee-PRO 900HP (`XBP9B`)** operando en la banda ISM autorizada por ENACOM de **915–928 MHz** con salto de frecuencia **FHSS** (o XBee S1 2.4 GHz según configuración de banco).
   - Configurado en **Modo Transparente (`AP = 0`)**, velocidad serie de **`9600 bps` (`8N1`)** (o `115200 bps` según perfil de enlace activo), mismo `PAN ID (ID)` y `Preamble ID (HP)` que el CanSat, y direccionamiento unicast punto a punto (`DH`/`DL` cruzados, **prohibido modo broadcast** según Requisito **X2**).
3. **Interfaz USB-UART:**
   - Placa adaptadora **SparkFun XBee Explorer USB** (con conversor FTDI `FT231X` o Silicon Labs `CP2102`) y cable USB con filtro de ferrita y conexión firme.
4. **Sistema de Antena Terrena (Requisito G11):**
   - **Antena direccional Yagi de 915 MHz** de alta ganancia para seguimiento durante el ascenso y descenso a larga distancia (o antena omnidireccional colineal de mesa/trípode con conector `SMA` / `RP-SMA`).
   - > **ADVERTENCIA CRÍTICA DE HARDWARE:** **SIEMPRE conectar y enroscar la antena al módulo XBee-PRO ANTES de enchufar el cable USB a la computadora.** Energizar el transmisor de radio sin antena conectada puede dañar irreversiblemente la etapa de salida del amplificador de RF por reflexión de onda estacionaria (ROE).

### 2.2 Equipamiento Obligatorio para Pruebas en Banco (Sin RF)

- **Opción A (Con placa física OBC):** Cable **USB-C de datos** conectado directamente al puerto nativo del **ESP32-S3 DevKitC-1** (`115200 bps`).
- **Opción B (Sin hardware):** Ninguno. La GCS incluye un simulador de misión completo (`src/simulator.py`) que reproduce toda la física de vuelo a `1 Hz`.

### 2.3 Equipamiento Opcional Altamente Recomendado

- **Pendrive / Unidad Flash USB externa (Requisito G2):** Formateada en `FAT32` o `exFAT` para que `src/storage.py` replique automáticamente el CSV oficial y los logs cada `5 segundos`.
- **Parasol / visera antirreflejo para pantalla de laptop** y batería portátil (`powerbank` USB-C PD) de respaldo.

---

## 3. Instalación e Inicialización Paso a Paso (macOS y Windows)

El código de `gcs/` es **100 % multiplataforma**: utiliza `pathlib.Path` para todas las rutas de archivos y `serial.tools.list_ports` para detectar automáticamente los puertos serie del sistema operativo. **No hay que modificar ni una sola línea de código para pasar de macOS a Windows.**

### 3.1 Requisitos Previos

- **Python 3.11, 3.12 o 3.13** instalado.
- En **Windows**, tener instalado el driver **FTDI VCP** o **Silicon Labs CP210x** si el adaptador XBee Explorer no es reconocido automáticamente en el Administrador de Dispositivos.

### 3.2 Instalación en macOS / Linux

Abrir la aplicación **Terminal** y ejecutar:

```bash
cd /ruta/al/repositorio/Cansat/gcs

# 1. Crear el entorno virtual .venv
python3 -m venv .venv

# 2. Activar el entorno virtual
source .venv/bin/activate

# 3. Actualizar pip e instalar todas las dependencias
pip install --upgrade pip
pip install -r requirements.txt
```

- **Nombres de puerto en macOS:** Al enchufar el XBee Explorer o el ESP32-S3, aparecerán en el desplegable de la GUI como `/dev/cu.usbserial-XXXX` o `/dev/cu.usbmodemXXXX` (el sistema prioriza automáticamente los dispositivos `/dev/cu.*`).

### 3.3 Instalación en Windows (PowerShell o CMD)

Abrir **PowerShell** en la carpeta del proyecto y ejecutar:

```powershell
cd C:\ruta\al\repositorio\Cansat\gcs

# 1. Crear el entorno virtual .venv
python -m venv .venv

# 2. Activar el entorno virtual (en PowerShell)
.\.venv\Scripts\Activate.ps1
# (Si usas CMD clásico en lugar de PowerShell, ejecuta: .venv\Scripts\activate.bat)

# 3. Actualizar pip e instalar todas las dependencias
python -m pip install --upgrade pip
pip install -r requirements.txt
```

- **Nombres de puerto en Windows:** Aparecerán automáticamente en el desplegable de la GUI como `COM3`, `COM4`, `COM5`, etc. junto con su descripción (`USB Serial Port`).

---

## 4. Cómo Ejecutar Pruebas (Sin Hardware y Unitarias)

### 4.1 Ejecutar la Interfaz Gráfica en Modo Simulación 1 Hz (Sin Hardware)

Con el entorno virtual activo, podés iniciar la GUI tanto invocando el módulo como ejecutando `src/main.py`:

```bash
# Abre la GUI y arranca inmediatamente el vuelo simulado a 1 Hz:
python src/main.py --simulate

# O abre la GUI en espera para iniciar manualmente desde el botón en pantalla:
python src/main.py
```
*(En macOS, si tenés Anaconda `(base)` activo simultáneamente, podés invocar directamente `./.venv/bin/python src/main.py --simulate`).*

**Qué verificar durante la simulación:**
1. Presionar el botón **`⚙ Modo Simulación (1 Hz)`** (si no se inició con `--simulate`).
2. **En Rampa (`t = 0..4 s`, `ESTADO: LaunchPad`):** Probar los botones **`🎯 CAL (Altitud 0 m)`**, **`📊 CX (Batería y Solar)`** y **`🕒 ST (Sincronizar UTC)`**. Verificar que la etiqueta inferior muestre `✔ COMANDO '...': CONFIRMADO | ECO OBC: CMD:<COMANDO>`.
3. **Despegue y Ascenso (`t = 5..19 s`, `ESTADO: Ascent`):** Al superar los `15 m`, el simulador emite el comando automático `CMD:MISSION_START;STATE:Ascent` (Requisito **X3**), sube hasta `400.0 m` y bloquea automáticamente el botón `⚠ MEC` por seguridad de vuelo.
4. **Apogeo y 2.º Paracaídas (`t = 20..25 s`, `ESTADO: Descent`):** En `t = 20 s` detecta una caída de `6 m` desde los `400 m` y pasa a `Descent` (`~15 m/s`). En `t = 25 s`, al cruzar el **80 % de la altura máxima (`<= 320 m`)**, despliega el 2.º paracaídas (`[✔ 2.º PARACAÍDAS: DESPLEGADO (90°)]`) y reduce la velocidad de descenso a `~5 m/s`.
5. **Alarma de Batería y Aterrizaje (`t = 88 s+`, `ESTADO: Landed`):** Verificar el cambio de la alarma de batería a **Advertencia (`< 3.60 V`, amarillo/ámbar)** y la estabilización del gráfico del giroscopio en el rango mínimo `[-0.5, 0.5] rad/s` al tocar tierra (`Altitud Baro: 0.00 m`).

### 4.2 Ejecutar la Batería de Pruebas Unitarias (`pytest`)

Dentro de la carpeta `gcs/`, ejecutar:

```bash
pytest -v
```
*(O `./.venv/bin/python -m pytest -v`).*

La suite ejecuta **16 tests automatizados** distribuidos en 3 archivos dentro de [`gcs/tests/`](tests/):
- **[`tests/test_decoder.py`](tests/test_decoder.py) (9 tests):** Valida las 33 columnas exactas del contrato CSV, conversiones al Sistema Internacional (`kPa`, `A`, `m/s²`, `rad/s`), parseo de `OPTIONAL_DATA` (`MAX`, `P2`, `CMD`, `RPI`), rechazo de tramas con menos/más columnas, rechazo de `TEAM_ID` incorrecto, rechazo de `NaN`/`Inf`, umbrales de batería 1S (`3.60 V` / `3.40 V`) y autocompletado UTC del comando `ST`.
- **[`tests/test_packet_monitor.py`](tests/test_packet_monitor.py) (6 tests):** Verifica el conteo independiente `RX_COUNT` vs. `PACKET_COUNT` (Requisito **G14**), detección exacta de pérdida de paquetes por saltos (`delta - 1`), manejo de duplicados, rotación automática de archivo CSV y reinicio de contadores cuando `PACKET_COUNT` vuelve a `1`, bloqueo de `MEC` en `Ascent`, sincronización de `ECO OBC` y respaldo en unidad USB.
- **[`tests/test_post_flight_nir.py`](tests/test_post_flight_nir.py) (1 test integral multi-escenario):** Genera imágenes sintéticas de pastizal abierto (`APTA`), agua/sombra (`NO_APTA`) y vegetación densa con obstáculos (`NO_APTA`), verifica la decodificación real de píxeles 2D, la compatibilidad de columnas largas (`altitude_m`, `latitude_deg`, `longitude_deg`) y cortas (`alt_m`, `lat`, `lon`) en `payload_log.csv`, y la exportación de `post_flight_report.csv`, `post_flight_summary.png` e imágenes segmentadas.

---

## 5. Protocolo Operativo para el Día de la Competencia (Checklist Pre-Vuelo, Vuelo y Post-Vuelo)

### 5.1 Fase 1: Checklist Pre-Vuelo (En Rampa de Lanzamiento)

- [ ] **1. Antena RF primero:** Enroscar firmemente la antena de `915 MHz` al conector del módulo **XBee-PRO 900HP** antes de conectarlo al puerto USB.
- [ ] **2. Conexión USB y Pendrive:** Conectar el adaptador XBee Explorer a un puerto USB de la laptop y enchufar un pendrive USB vacío para el respaldo secundario (Requisito **G2**).
- [ ] **3. Iniciar la GCS:** Ejecutar `python src/main.py` (o `./.venv/bin/python src/main.py`).
- [ ] **4. Seleccionar Puerto y Conectar:**
  - Presionar **`↻ Puertos`**, elegir el puerto del XBee (`/dev/cu.usbserial-*` en macOS o `COM*` en Windows).
  - Si el enlace RF está configurado a `9600 bps`, iniciar con `python src/main.py --baud 9600` (o `115200` según configuración del módulo) y presionar **`▶ Conectar Serie`**.
- [ ] **5. Configurar Respaldo USB:** Presionar **`💾 Carpeta/USB Respaldo`**, elegir la carpeta dentro del pendrive USB y verificar que en la barra superior aparezca `RESPALDO USB OK (<HH:MM:SS>)`.
- [ ] **6. Verificar Salud del Satélite en Rampa:**
  - Confirmar badge **`[✔ ENLACE SERIAL ACTIVO]`** y **`ESTADO: LaunchPad`**.
  - Confirmar **`[✔ BATERÍA OK: > 3.70 V]`** y **`[✔ GPS FIX OK: >= 4 SATS]`**.
  - Presionar **`📊 CX (Batería y Solar)`** y verificar que `ECO OBC:` cambie a `CMD:CX` y se actualicen las lecturas de los 4 fotodiodos solares (`U4..U7`).
- [ ] **7. Sincronizar Hora UTC (Requisito F3):**
  - Dejar vacío el campo `HH:MM:SS` (para usar la hora UTC exacta del sistema) o escribir la hora UTC oficial de los jueces y presionar **`🕒 ST (Sincronizar UTC)`**.
  - Confirmar en cabecera **`UTC: HH:MM:SS [SINCR: SÍ]`** y `ECO OBC: CMD:ST`.
- [ ] **8. Calibrar Altitud Barométrica a Cero (Requisito G1):**
  - Con el CanSat quieto en la plataforma de lanzamiento, presionar **`🎯 CAL (Altitud 0 m)`**.
  - Confirmar que **`Altitud Baro:`** marque **`0.00 m (±0.3 m)`** y que `ECO OBC:` muestre **`CMD:CAL`**.

### 5.2 Fase 2: Durante el Vuelo (Ascenso y Descenso)

- **Separación / Inicio de Misión:** Al detectar el lanzamiento (`>= 15 m`), el OBC envía automáticamente `CMD:MISSION_START;STATE:Ascent` (Requisito **X3**) y el badge principal cambia a **`ESTADO: Ascent`** (verde).
- **Apogeo y Etapa 1 de Descenso (`~15 m/s`):** Al superar el apogeo, el estado cambia a **`ESTADO: Descent`** (ámbar) y se registra la altura pico en **`Altitud Máx:`**.
- **Apertura del 2.º Paracaídas al 80 % (`~5 m/s`):** Al descender al `80 %` de la altitud máxima, el badge de paracaídas cambia automáticamente a **`[✔ 2.º PARACAÍDAS: DESPLEGADO (90°)]`**.
- **Uso de Emergencia de `⚠ MEC`:** El botón `MEC` permanece bloqueado durante `Ascent`. Solo durante `Descent`, si por orden de seguridad o contingencia se requiere accionar manualmente el servo, presionar **`⚠ MEC (2.º Paracaídas)`** y aceptar las **dos ventanas de confirmación consecutivas**.

### 5.3 Fase 3: Aterrizaje y Recuperación

1. **Confirmar Aterrizaje:** Verificar que el badge cambie a **`ESTADO: Landed`** (púrpura), la `Altitud Baro` se estabilice cerca de `0.00 m`, la aceleración Z vuelva a `~9.81 m/s²` y las curvas del giroscopio permanezcan planas dentro de `[-0.5, 0.5] rad/s`.
2. **Guiar al Equipo de Rescate:** Dictar al equipo de campo las últimas coordenadas válidas del panel **`Posición GPS:`** (`Lat: -27.XXXXXX° | Lon: -58.XXXXXX°`).
3. **Cierre Seguro de Sesión:** Presionar **`⏹ Desconectar Serie`**, verificar el mensaje `RESPALDO USB OK` en la barra superior, cerrar la ventana y retirar el pendrive con el archivo `Flight_1000_<YYYYMMDD_HHMMSS>.csv` listo para entregar al jurado.

---

## 6. Guía Visual de la Interfaz (Qué Significa Cada Indicador, Botón y Gráfico)

La pantalla única de [`src/gui.py`](src/gui.py) se divide en **5 bloques operativos**:

### 6.1 Franja Superior — Control de Enlace y Estado de Misión
- **`Puerto` + `↻ Puertos`:** Lista desplegable con autodetección de puertos serie (`COM*` / `/dev/cu.*`).
- **`▶ Conectar Serie` / `⏹ Desconectar Serie`:** Abre o cierra el enlace serie real, limpiando gráficos y creando un nuevo archivo CSV de sesión limpia.
- **`⚙ Modo Simulación (1 Hz)`:** Inicia o detiene el simulador de vuelo interno, reiniciando curvas y contadores.
- **`ESTADO: <STATE>`:** Badge principal de alto contraste que indica la fase de la máquina de estados del OBC (`LaunchPad`, `Ascent`, `Descent`, `Landed`) y resalta alertas de batería si ocurren.
- **`T. MISIÓN: HH:MM:SS (N s)`:** Tiempo transcurrido de misión con resolución de `1 segundo` (Requisito **G3**).
- **`UTC: HH:MM:SS [SINCR: SÍ/NO]`:** Hora UTC mantenida por el OBC y estado de sincronización por GPS o comando `ST`.

### 6.2 Barra de Salud, Alarmas y Almacenamiento
- **`[ENLACE RF]`:** Verde (`ACTIVO`) si llegan tramas cada `< 3.0 s`; Rojo (`⚠ PÉRDIDA DE ENLACE RF`) si pasan más de `3.0 s` sin datos.
- **`[BATERÍA]`:** Configurada para celda Li-Ion 1S:
  - **Verde (`✔ BATERÍA OK`):** `>= 3.60 V`.
  - **Amarillo/Ámbar (`⚠ ADVERTENCIA BATERÍA`):** `< 3.60 V`.
  - **Rojo (`🚨 ALARMA CRÍTICA BATERÍA`):** `< 3.40 V`.
- **`[GPS]`:** Verde (`✔ GPS FIX OK`) cuando `GPS_FIX == 1` y `SATS >= 4`; Ámbar (`⚠ GPS SIN FIX`) en caso contrario.
- **`[2.º PARACAÍDAS]`:** Gris (`CERRADO 0°`) o Verde (`✔ DESPLEGADO 90°`).
- **`CSV (...) | RESPALDO USB`** + botón **`💾 Carpeta/USB Respaldo`:** Muestra la cantidad de filas grabadas en el CSV activo y el último horario de sincronización con el pendrive USB.

### 6.3 Columna Izquierda — Contadores, Sensores SI, Misión Secundaria y Telecomandos
1. **Contadores de Paquetes (Requisitos G8 y G14):**
   - `PACKET_COUNT (TX OBC)`: Número de secuencia transmitido por el satélite.
   - `RX_COUNT (Recibidos GCS)`: Cantidad real de paquetes válidos recibidos por la estación terrena.
   - `PERDIDOS`: Paquetes no recibidos calculados por saltos en `PACKET_COUNT`, junto con su porcentaje (`%`).
   - `CORRUPTOS`: Líneas descartadas por error de formato o truncamiento en el aire.
2. **Sensores en Sistema Internacional (SI — Requisitos G5, G6, G8):**
   - **`Altitud Baro` (m):** Altura relativa sobre la rampa de lanzamiento (**AGL — Above Ground Level**), calculada a partir de la presión atmosférica del `BMP280` respecto a la referencia calibrada con `CAL`.
   - **`Altitud Máx` (m):** Pico máximo de altitud barométrica registrado en vuelo por el `ApogeeDetector` del OBC.
   - **`Presión Atm` (Pa / kPa) y `Temperatura` (°C):** Lecturas directas del `BMP280` / `MPU-6050`.
   - **`Tensión Batería` (V) y `Corriente Bat` (A / mA):** Medición de alta precisión del sensor `INA219`.
   - **`Aceleración (SI)` (`m/s²`) y `Giroscopio (SI)` (`rad/s`):** Tres ejes convertidos automáticamente desde `g` y `°/s` al Sistema Internacional.
3. **Posición GPS y Misión Secundaria:**
   - **`Posición GPS` (`Lat`, `Lon`) y `Alt GPS / Sats`:** La altitud GPS corresponde a la altura sobre el nivel medio del mar (**MSL — Mean Sea Level**), por lo que difiere de `Altitud Baro` (AGL) en la elevación topográfica del terreno (~52 m en Resistencia).
   - **`Magnetómetro` (`µT`):** Tres ejes del sensor `LIS3MDL`.
   - **`Fotodiodos / Sol` (`U4..U7` y `Az`) y `Actitud (RPI)` (`Yaw`):** Lecturas crudas de los 4 sensores ópticos del `ADS1115` (`0–32767`), acimut solar estimado y ángulo de guiñada (`Yaw`) fusionado por la Raspberry Pi.
   - **`Terreno / NIR`:** Contador de capturas de la cámara infrarroja `IMX219 NoIR` y clasificación en tiempo real de la superficie (`APTA` / `NO_APTA` + puntaje `0.00–1.00`).
4. **Panel de Telecomandos con Ventana Anti-Colisión:**
   - Botones **`🎯 CAL`**, **`📊 CX`**, campo **`HH:MM:SS`** + **`🕒 ST`**, y **`⚠ MEC`** (con doble confirmación).
   - La barra inferior muestra el ciclo de vida del comando: `⏳ PENDIENTE/ENVIADO` $\rightarrow$ `✔ CONFIRMADO` (o `✖ SIN CONFIRMACIÓN` tras `3.0 s`) y el **`ECO OBC: CMD:<COMANDO>`** sincronizado inmediatamente.

### 6.4 Columna Derecha — 4 Gráficos Simultáneos en Tiempo Real (`PyQtGraph`, Requisito G7)
1. **Gráfico 1 — Altitud Barométrica y GPS (`m` vs. `s`):** Curva azul continua para altitud barométrica (AGL) y curva verde punteada para altitud GPS (MSL).
2. **Gráfico 2 — Tensión (`V`) y Corriente (`A`) de Batería (`V / A` vs. `s`):** Permite monitorear el consumo nominal y el pico de corriente del servo `SG90` en la apertura del 2.º paracaídas.
3. **Gráfico 3 — Acelerómetro 3 Ejes en SI (`m/s²` vs. `s`):** Trazos independientes para `Acc X`, `Acc Y` y `Acc Z`.
4. **Gráfico 4 — Tasas de Rotación / Giroscopio 3 Ejes en SI (`rad/s` vs. `s`):** Trazos `Gyr X`, `Gyr Y` y `Gyr Z` con rango mínimo fijado en `[-0.5, 0.5] rad/s` para evitar que el auto-zoom amplifique ruido residual cuando el CanSat está en reposo.

### 6.5 Consola Inferior — Flujo de Telemetría en Texto (Requisito G5)
Muestra en vivo las últimas 250 líneas ASCII crudas recibidas (tramas CSV de 33 columnas, acuses `OK` y eventos `EVENT:` del OBC).

---

## 7. Archivos Generados, Ubicación y Formato de Datos (Contrato de 33 Columnas)

### 7.1 Ubicación y Política de No-Sobrescritura

Todos los registros se guardan automáticamente dentro de la carpeta **`gcs/mission_logs/`** (y se replican cada 5 segundos en la carpeta USB de respaldo si fue seleccionada):

| Archivo | Propósito | Política de Rotación |
|---|---|---|
| `Flight_1000_<YYYYMMDD_HHMMSS>.csv` | Archivo CSV oficial de 33 columnas con todas las muestras válidas del vuelo (Requisito **G2**). | **Crea un archivo nuevo con timestamp único** en cada inicio de sesión, reconexión o reinicio de `PACKET_COUNT = 1`. **Nunca sobrescribe vuelos previos.** |
| `events.log` | Bitácora cronológica con timestamp UTC de inicio de sesión, telecomandos encolados/enviados/confirmados, eventos del OBC y errores de formato. | Append continuo con marca de tiempo ISO-8601 UTC (`YYYY-MM-DDTHH:MM:SS.mmmZ`). |
| `raw_rx.log` | Registro forense de cada línea ASCII cruda recibida por el puerto serie antes del decodificador. | Append continuo con marca de tiempo ISO-8601 UTC. |

### 7.2 Contrato Oficial de las 33 Columnas CSV (`OBC ↔ GCS`)

```text
TEAM_ID,MISSION_TIME_MS,PACKET_COUNT,MODE,STATE,ALT_M,PRESS_PA,TEMP_C,BATT_V,CURR_MA,ACC_X,ACC_Y,ACC_Z,GYR_X,GYR_Y,GYR_Z,LAT,LON,GPS_ALT_M,SATS,MAG_X_UT,MAG_Y_UT,MAG_Z_UT,OPT1,OPT2,OPT3,OPT4,RTC_H,RTC_M,RTC_S,RTC_VALID,GPS_FIX,OPTIONAL_DATA
```

| # | Columna | Tipo | Unidad en CSV | Unidad SI en GUI | Descripción |
|:---:|---|---|---|---|---|
| 0 | `TEAM_ID` | `int` | — | — | ID del equipo (`1000`). |
| 1 | `MISSION_TIME_MS` | `int` | `ms` | `s` (`HH:MM:SS`) | Tiempo acumulado de misión desde el encendido. |
| 2 | `PACKET_COUNT` | `int` | Paquetes | Paquetes | Contador monotónico de paquetes transmitidos por el OBC (persistido en FRAM). |
| 3 | `MODE` | `int` | `0` / `1` | — | `0` = Vuelo real, `1` = Simulación. |
| 4 | `STATE` | `str` | Texto | — | Estado FSM: `LaunchPad`, `Ascent`, `Descent`, `Landed`. |
| 5 | `ALT_M` | `float` | `m` | `m` | Altitud barométrica AGL relativa a la rampa. |
| 6 | `PRESS_PA` | `float` | `Pa` | `Pa` / `kPa` | Presión atmosférica absoluta (`BMP280`). |
| 7 | `TEMP_C` | `float` | `°C` | `°C` | Temperatura interna del CanSat. |
| 8 | `BATT_V` | `float` | `V` | `V` | Tensión del bus de batería (`INA219`). |
| 9 | `CURR_MA` | `float` | `mA` | `A` (`mA / 1000`) | Corriente instantánea de batería (`INA219`). |
| 10 | `ACC_X` | `float` | `g` | `m/s²` (`× 9.80665`) | Aceleración eje X (`MPU-6050`). |
| 11 | `ACC_Y` | `float` | `g` | `m/s²` (`× 9.80665`) | Aceleración eje Y (`MPU-6050`). |
| 12 | `ACC_Z` | `float` | `g` | `m/s²` (`× 9.80665`) | Aceleración eje Z (`MPU-6050`). |
| 13 | `GYR_X` | `float` | `deg/s` | `rad/s` | Velocidad angular eje X (`MPU-6050`). |
| 14 | `GYR_Y` | `float` | `deg/s` | `rad/s` | Velocidad angular eje Y (`MPU-6050`). |
| 15 | `GYR_Z` | `float` | `deg/s` | `rad/s` | Velocidad angular eje Z (`MPU-6050`). |
| 16 | `LAT` | `float` | `deg` (`°`) | `°` | Latitud GPS (`NEO-M8N`). |
| 17 | `LON` | `float` | `deg` (`°`) | `°` | Longitud GPS (`NEO-M8N`). |
| 18 | `GPS_ALT_M` | `float` | `m` | `m` | Altitud GPS sobre el nivel del mar (MSL). |
| 19 | `SATS` | `int` | Satélites | Satélites | Satélites GPS en seguimiento. |
| 20 | `MAG_X_UT` | `float` | `µT` | `µT` | Campo magnético eje X (`LIS3MDL`). |
| 21 | `MAG_Y_UT` | `float` | `µT` | `µT` | Campo magnético eje Y (`LIS3MDL`). |
| 22 | `MAG_Z_UT` | `float` | `µT` | `µT` | Campo magnético eje Z (`LIS3MDL`). |
| 23 | `OPT1` | `int` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U4` (+X, canal `AIN3` del `ADS1115`). |
| 24 | `OPT2` | `int` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U5` (-X, canal `AIN2` del `ADS1115`). |
| 25 | `OPT3` | `int` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U6` (+Y, canal `AIN1` del `ADS1115`). |
| 26 | `OPT4` | `int` | Cuentas ADC | Cuentas (`0–32767`) | Fotodiodo solar `U7` (-Y, canal `AIN0` del `ADS1115`). |
| 27 | `RTC_H` | `int` | Horas (`0–23`) | `HH:MM:SS UTC` | Hora UTC del reloj interno sincronizado. |
| 28 | `RTC_M` | `int` | Minutos (`0–59`) | `HH:MM:SS UTC` | Minuto UTC. |
| 29 | `RTC_S` | `int` | Segundos (`0–59`)| `HH:MM:SS UTC` | Segundo UTC. |
| 30 | `RTC_VALID` | `int` | `0` o `1` | Booleano | `1` si el reloj UTC está sincronizado. |
| 31 | `GPS_FIX` | `int` | `0` o `1` | Booleano | `1` si el GPS posee fix 3D válido. |
| 32 | `OPTIONAL_DATA` | `str` | Clave-valor | — | `MAX:<m>;P2:<0|1>;CMD:<echo>;RPI:YAW:<°>|SUN:<°>|NIR:<n>|APT:<clase>(<score>)`. |

*(El detalle completo de bajo nivel se encuentra en [`docs/protocolo.md`](docs/protocolo.md)).*

---

## 8. Pipeline Post-Vuelo de Imágenes NIR/VIS (`src/post_flight_nir.py`)

Una vez recuperado el CanSat tras el aterrizaje, se extrae la tarjeta microSD de la **Raspberry Pi Zero 2W** (que contiene la carpeta `nir_images/` con las capturas `NIR_<id>_<mission_time_ms>ms.png` de la cámara **IMX219 NoIR** y el archivo `payload_log.csv`) y, opcionalmente, las tarjetas microSD de las cámaras **XIAO ESP32-S3 Sense** (`VIS_*.jpg` / `NIR_*.jpg`).

### 8.1 Comando de Ejecución Post-Vuelo

Dentro de `gcs/`, ejecutar:

```bash
python -m src.post_flight_nir \
  --images-dir ../firmware/payload-rpi/nir_images \
  --payload-csv ../firmware/payload-rpi/payload_log.csv \
  --output-dir ./post_flight_output
```

*(Para probarlo rápidamente con las imágenes sintéticas de demostración incluidas en `demo_nir/`, ejecutar:)*
```bash
python -m src.post_flight_nir \
  --images-dir ./demo_nir \
  --payload-csv ./demo_nir/payload_log.csv \
  --output-dir ./post_flight_output
```

### 8.2 Criterios Físicos y Radiométricos Evaluados

A diferencia del estimador rápido en vuelo, [`src/post_flight_nir.py`](src/post_flight_nir.py) decodifica la **matriz 2D real de píxeles** de cada imagen (`OpenCV` / `Pillow` / decodificador nativo) y calcula:

1. **Reflectancia Media NIR (`mean_nir`, `0–255`):** En el infrarrojo cercano, el agua absorbe fuertemente la radiación (`< 65`), el suelo desnudo presenta reflectancia intermedia (`65–114`) y la vegetación sana presenta alta reflectancia por dispersión celular en el mesófilo (`>= 115`, efecto *Red-Edge*).
2. **Rugosidad Global (`std_roughness`) y Contraste Espacial Local (`local_contrast`):** Evalúa la desviación estándar y el gradiente espacial 2D entre píxeles vecinos para distinguir **pastizales abiertos y llanos** (baja textura) de **arboledas densas u obstáculos topográficos** (alta textura y sombras entrelazadas).
3. **Segmentación Píxel a Píxel en 4 Clases:**
   - **Agua o sombra profunda (`water_shadow_pct`):** Píxeles `< 65` (coloreados en **azul**).
   - **Vegetación densa / obstáculos (`dense_veg_pct`):** Píxeles `> 165` o con gradiente local `> 28` (coloreados en **rojo/naranja**).
   - **Pastizal abierto uniforme (`open_pasture_pct`):** Píxeles `115..165` suaves (coloreados en **verde**).
   - **Suelo firme plano (`bare_soil_pct`):** Píxeles `65..114` suaves (coloreados en **verde lima**).
4. **Penalización por Actitud 3D (`roll_deg`, `pitch_deg`):** Cruza cada imagen con `payload_log.csv` (aceptando tanto `altitude_m, latitude_deg, longitude_deg` como `alt_m, lat, lon`), calcula la inclinación nadir $\text{tilt} = \sqrt{\text{roll}^2 + \text{pitch}^2}$ y declara la zona como **`APTA`** únicamente si **`suitability_score >= 0.60`** y **`tilt_deg <= 35.0°`**.

### 8.3 Productos Entregados en `post_flight_output/`

1. **`post_flight_report.csv`:** Tabla completa de 24 columnas con metadatos GPS/actitud, métricas radiométricas, porcentajes de cobertura de suelo, clase dominante (`PASTIZAL_ABIERTO`, `SUELO_FIRME`, `VEG_DENSA_OBSTACULOS`, `AGUA_O_SOMBRA`), aptitud (`APTA` / `NO_APTA`) y puntaje `0.00–1.00`.
2. **`post_flight_summary.png`:** Gráfico de barras de alta resolución titulado *"Clasificación Post-Vuelo de Terreno NIR — YVYSAT (UTN FRRe)"* con barras verdes (`#2e7d32`, `APTA`) y rojas (`#c62828`, `NO_APTA`), línea punteada de umbral en `0.60` (`"Umbral Zona Apta (0.60)"`) y valores anotados sobre cada barra.
3. **`annotated/*_segmented.png`:** Imágenes individuales superpuestas con el mapa de calor de zonas aptas vs. obstáculos, retícula nadir central y marco perimetral verde (`APTA`) o rojo (`NO_APTA`).

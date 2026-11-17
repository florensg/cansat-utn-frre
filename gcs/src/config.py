"""Configuración central, constantes, umbrales de alarma y contrato de 33 columnas de la GCS.

Alineado con:
- firmware/obc-esp32s3/include/config.h
- firmware/obc-esp32s3/lib/flight_core/src/telemetry.cpp
- firmware/obc-esp32s3/lib/flight_core/src/commands.cpp
- docs/01-requisitos/competencia/v2/Anexo A v2 .pdf (Requisitos G1-G15, X1-X6)
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Tuple

# Identificador oficial del equipo (coincide con kTeamId en config.h del OBC)
TEAM_ID: int = 1000

# Parámetros de enlace serie por defecto
DEFAULT_BAUDRATE: int = 115200
SERIAL_TIMEOUT_S: float = 0.15

# Ventana anti-colisión para envío de telecomandos tras recibir paquete de 1 Hz
CMD_GUARD_DELAY_MS: int = 60
CMD_SILENT_WINDOW_MS: int = 800
CMD_MAX_WAIT_FOR_SLOT_MS: int = 1100
CMD_ACK_TIMEOUT_S: float = 3.0

# Intervalo de replicación automática a disco/pendrive de respaldo (Requisito G2)
BACKUP_SYNC_INTERVAL_S: float = 5.0

# Constante gravitatoria estándar para conversión de g -> m/s^2 (Requisito G6: SI)
STANDARD_GRAVITY_MS2: float = 9.80665

# Estados válidos de la máquina de estados del OBC (fsm.cpp)
VALID_FLIGHT_STATES: Tuple[str, ...] = (
    "LaunchPad",
    "Ascent",
    "Descent",
    "Landed",
)

# Telecomandos oficiales soportados por el OBC (commands.cpp + main.cpp)
VALID_COMMANDS: Tuple[str, ...] = ("CX", "ST", "CAL", "MEC", "RST")

# Definición exacta de las 33 columnas emitidas por format_telemetry_csv() en telemetry.cpp
TELEMETRY_COLUMNS: Tuple[str, ...] = (
    "TEAM_ID",          # 0
    "MISSION_TIME_MS",  # 1
    "PACKET_COUNT",     # 2
    "MODE",             # 3
    "STATE",            # 4
    "ALT_M",            # 5
    "PRESS_PA",         # 6
    "TEMP_C",           # 7
    "BATT_V",           # 8
    "CURR_MA",          # 9
    "ACC_X",            # 10
    "ACC_Y",            # 11
    "ACC_Z",            # 12
    "GYR_X",            # 13
    "GYR_Y",            # 14
    "GYR_Z",            # 15
    "LAT",              # 16
    "LON",              # 17
    "GPS_ALT_M",        # 18
    "SATS",             # 19
    "MAG_X_UT",         # 20
    "MAG_Y_UT",         # 21
    "MAG_Z_UT",         # 22
    "OPT1",             # 23
    "OPT2",             # 24
    "OPT3",             # 25
    "OPT4",             # 26
    "RTC_H",            # 27
    "RTC_M",            # 28
    "RTC_S",            # 29
    "RTC_VALID",        # 30
    "GPS_FIX",          # 31
    "OPTIONAL_DATA",    # 32
)

EXPECTED_COLUMN_COUNT: int = len(TELEMETRY_COLUMNS)  # 33


@dataclass(frozen=True)
class AlarmThresholds:
    """Umbrales operativos para las alarmas visuales de la estación terrena (celda Li-Ion 1S)."""

    battery_low_v: float = 3.60       # < 3.60 V -> Advertencia (amarillo/ámbar)
    battery_critical_v: float = 3.40  # < 3.40 V -> Alarma Crítica (rojo)
    current_high_ma: float = 1200.0
    link_loss_timeout_s: float = 3.0
    min_gps_satellites: int = 4
    temp_high_c: float = 55.0
    max_descent_rate_stage1_ms: float = 18.0  # 15 m/s +/- 3 m/s (C3)
    max_descent_rate_stage2_ms: float = 8.0   # 5 m/s +/- 3 m/s (C4)


ALARM_THRESHOLDS = AlarmThresholds()


@dataclass(frozen=True)
class GuiTheme:
    """Paleta de alto contraste para lectura bajo luz solar directa (Requisito G12)."""

    bg_main: str = "#F8FAFC"
    bg_panel: str = "#FFFFFF"
    bg_header: str = "#E2E8F0"
    border_normal: str = "#64748B"
    border_strong: str = "#0F172A"
    text_primary: str = "#0F172A"
    text_secondary: str = "#334155"
    accent_blue: str = "#0369A1"
    accent_green: str = "#15803D"
    accent_amber: str = "#B45309"
    accent_red: str = "#B91C1C"
    accent_purple: str = "#6D28D9"
    font_size_base_pt: int = 14
    font_size_title_pt: int = 16
    font_size_hero_pt: int = 18
    plot_line_width: float = 2.8


GUI_THEME = GuiTheme()

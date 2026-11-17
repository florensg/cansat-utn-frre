"""Parser y validador estricto de las 33 columnas de telemetría del OBC y OPTIONAL_DATA.

Cumple con:
- Contrato exacto de firmware/obc-esp32s3/lib/flight_core/src/telemetry.cpp (33 campos)
- Requisito G3 (tiempo de misión con resolución de 1 s)
- Requisito G6 (conversiones al Sistema Internacional de Unidades SI)
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from .config import (
    EXPECTED_COLUMN_COUNT,
    STANDARD_GRAVITY_MS2,
    TEAM_ID,
    VALID_FLIGHT_STATES,
)


class TelemetryDecodeError(ValueError):
    """Excepción lanzada cuando una trama CSV no cumple el contrato de 33 columnas."""


@dataclass(frozen=True)
class OptionalDataParsed:
    """Datos estructurados extraídos de la columna 32 (OPTIONAL_DATA)."""

    raw: str = ""
    max_altitude_m: float = 0.0
    second_parachute_deployed: bool = False
    cmd_echo: str = "NONE"
    rpi_raw: str = ""
    rpi_yaw_deg: Optional[float] = None
    rpi_sun_azimuth_deg: Optional[float] = None
    rpi_nir_captures: int = 0
    rpi_landing_apt: str = "PENDIENTE"
    rpi_suitability_score: float = 0.0
    extra_tags: Dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True)
class TelemetryPacket:
    """Paquete de telemetría validado con campos crudos (CSV) y derivados en unidades SI (G6)."""

    # --- 33 columnas oficiales del contrato CSV ---
    team_id: int
    mission_time_ms: int
    packet_count: int
    mode: int
    state: str
    alt_m: float
    press_pa: float
    temp_c: float
    batt_v: float
    curr_ma: float
    acc_x_g: float
    acc_y_g: float
    acc_z_g: float
    gyr_x_dps: float
    gyr_y_dps: float
    gyr_z_dps: float
    lat_deg: float
    lon_deg: float
    gps_alt_m: float
    sats: int
    mag_x_ut: float
    mag_y_ut: float
    mag_z_ut: float
    opt1_raw: int
    opt2_raw: int
    opt3_raw: int
    opt4_raw: int
    rtc_h: int
    rtc_m: int
    rtc_s: int
    rtc_valid: int
    gps_fix: int
    optional_data: str

    # --- Datos parseados de OPTIONAL_DATA ---
    optional: OptionalDataParsed = field(default_factory=OptionalDataParsed)

    # --- Línea cruda y lista de 33 celdas para guardado CSV exacto ---
    raw_line: str = ""
    raw_fields: Tuple[str, ...] = field(default_factory=tuple)

    # --- Propiedades convertidas al Sistema Internacional (SI) para GUI (Requisito G6) ---
    @property
    def mission_time_s(self) -> int:
        """Tiempo de misión con resolución de 1 segundo (Requisito G3)."""
        return self.mission_time_ms // 1000

    @property
    def mission_time_hms(self) -> str:
        """Tiempo de misión formateado como HH:MM:SS (Requisito G3)."""
        total_s = max(0, self.mission_time_s)
        hh = total_s // 3600
        mm = (total_s % 3600) // 60
        ss = total_s % 60
        return f"{hh:02d}:{mm:02d}:{ss:02d}"

    @property
    def rtc_hms(self) -> str:
        """Hora UTC reportada por el OBC en formato HH:MM:SS."""
        return f"{self.rtc_h:02d}:{self.rtc_m:02d}:{self.rtc_s:02d}"

    @property
    def press_kpa(self) -> float:
        """Presión atmosférica en kilopascales (kPa)."""
        return self.press_pa / 1000.0

    @property
    def curr_a(self) -> float:
        """Corriente de batería en amperios (A) para cumplimiento SI."""
        return self.curr_ma / 1000.0

    @property
    def acc_x_ms2(self) -> float:
        """Aceleración eje X en m/s² (SI)."""
        return self.acc_x_g * STANDARD_GRAVITY_MS2

    @property
    def acc_y_ms2(self) -> float:
        """Aceleración eje Y en m/s² (SI)."""
        return self.acc_y_g * STANDARD_GRAVITY_MS2

    @property
    def acc_z_ms2(self) -> float:
        """Aceleración eje Z en m/s² (SI)."""
        return self.acc_z_g * STANDARD_GRAVITY_MS2

    @property
    def acc_norm_ms2(self) -> float:
        """Magnitud total de aceleración en m/s²."""
        return math.sqrt(
            self.acc_x_ms2**2 + self.acc_y_ms2**2 + self.acc_z_ms2**2
        )

    @property
    def gyr_x_rads(self) -> float:
        """Velocidad angular eje X en rad/s (SI)."""
        return math.radians(self.gyr_x_dps)

    @property
    def gyr_y_rads(self) -> float:
        """Velocidad angular eje Y en rad/s (SI)."""
        return math.radians(self.gyr_y_dps)

    @property
    def gyr_z_rads(self) -> float:
        """Velocidad angular eje Z en rad/s (SI)."""
        return math.radians(self.gyr_z_dps)

    @property
    def mag_norm_ut(self) -> float:
        """Magnitud del campo magnético en µT."""
        return math.sqrt(
            self.mag_x_ut**2 + self.mag_y_ut**2 + self.mag_z_ut**2
        )

    @property
    def solar_azimuth_deg(self) -> float:
        """Acimut solar 2D en grados derivado de RPI o de los 4 fotodiodos OPT1..OPT4."""
        if self.optional.rpi_sun_azimuth_deg is not None:
            return self.optional.rpi_sun_azimuth_deg
        sx = float(self.opt1_raw - self.opt2_raw)
        sy = float(self.opt3_raw - self.opt4_raw)
        if abs(sx) > 10 or abs(sy) > 10:
            return math.degrees(math.atan2(sy, sx)) % 360.0
        return 0.0

    def to_csv_row(self) -> List[str]:
        """Devuelve la fila de 33 valores lista para escribir en el archivo CSV oficial."""
        if len(self.raw_fields) == EXPECTED_COLUMN_COUNT:
            return list(self.raw_fields)
        return [
            str(self.team_id),
            str(self.mission_time_ms),
            str(self.packet_count),
            str(self.mode),
            self.state,
            f"{self.alt_m:.2f}",
            f"{self.press_pa:.2f}",
            f"{self.temp_c:.2f}",
            f"{self.batt_v:.2f}",
            f"{self.curr_ma:.2f}",
            f"{self.acc_x_g:.3f}",
            f"{self.acc_y_g:.3f}",
            f"{self.acc_z_g:.3f}",
            f"{self.gyr_x_dps:.2f}",
            f"{self.gyr_y_dps:.2f}",
            f"{self.gyr_z_dps:.2f}",
            f"{self.lat_deg:.6f}",
            f"{self.lon_deg:.6f}",
            f"{self.gps_alt_m:.2f}",
            str(self.sats),
            f"{self.mag_x_ut:.1f}",
            f"{self.mag_y_ut:.1f}",
            f"{self.mag_z_ut:.1f}",
            str(self.opt1_raw),
            str(self.opt2_raw),
            str(self.opt3_raw),
            str(self.opt4_raw),
            str(self.rtc_h),
            str(self.rtc_m),
            str(self.rtc_s),
            str(self.rtc_valid),
            str(self.gps_fix),
            self.optional_data,
        ]


_APT_PATTERN = re.compile(r"^([A-Za-z_]+)(?:\(([0-9.]+)\))?$")


def parse_optional_data(raw_opt: str) -> OptionalDataParsed:
    """Parsea el campo OPTIONAL_DATA generado por el OBC y la Raspberry Pi Zero 2W.

    Soporta formatos como:
    - 'MAX:400.0;P2:1;CMD:CAL;RPI:YAW:182|SUN:135|NIR:12|APT:APTA(0.84)'
    - 'MAX:0.0;P2:0;CMD:NONE'
    - 'ATT=OK;CMD_ECHO=CX'
    """
    text = (raw_opt or "").strip()
    if not text:
        return OptionalDataParsed(raw="")

    max_alt = 0.0
    p2_deployed = False
    cmd_echo = "NONE"
    rpi_raw = ""
    rpi_yaw: Optional[float] = None
    rpi_sun: Optional[float] = None
    rpi_nir = 0
    rpi_apt = "PENDIENTE"
    rpi_score = 0.0
    extra: Dict[str, str] = {}

    segments = [seg.strip() for seg in text.split(";") if seg.strip()]
    for seg in segments:
        if seg.startswith("RPI:"):
            rpi_raw = seg[4:].strip()
            for sub in rpi_raw.split("|"):
                if ":" not in sub:
                    continue
                k, v = sub.split(":", 1)
                k = k.strip().upper()
                v = v.strip()
                if k == "YAW":
                    try:
                        rpi_yaw = float(v)
                    except ValueError:
                        pass
                elif k == "SUN":
                    try:
                        rpi_sun = float(v)
                    except ValueError:
                        pass
                elif k == "NIR":
                    try:
                        rpi_nir = int(float(v))
                    except ValueError:
                        pass
                elif k == "APT":
                    match = _APT_PATTERN.match(v)
                    if match:
                        rpi_apt = match.group(1).upper()
                        if match.group(2) is not None:
                            try:
                                rpi_score = float(match.group(2))
                            except ValueError:
                                pass
                    else:
                        rpi_apt = v
            continue

        sep = ":" if ":" in seg else ("=" if "=" in seg else None)
        if sep is None:
            extra[seg] = ""
            continue
        key, val = seg.split(sep, 1)
        key_u = key.strip().upper()
        val_s = val.strip()

        if key_u == "MAX":
            try:
                max_alt = float(val_s)
            except ValueError:
                pass
        elif key_u == "P2":
            p2_deployed = val_s in ("1", "TRUE", "true", "SI", "OK")
        elif key_u in ("CMD", "CMD_ECHO"):
            cmd_echo = val_s or "NONE"
        else:
            extra[key_u] = val_s

    return OptionalDataParsed(
        raw=text,
        max_altitude_m=max_alt,
        second_parachute_deployed=p2_deployed,
        cmd_echo=cmd_echo,
        rpi_raw=rpi_raw,
        rpi_yaw_deg=rpi_yaw,
        rpi_sun_azimuth_deg=rpi_sun,
        rpi_nir_captures=rpi_nir,
        rpi_landing_apt=rpi_apt,
        rpi_suitability_score=rpi_score,
        extra_tags=extra,
    )


def _parse_finite_float(value: str, field_name: str) -> float:
    try:
        num = float(value)
    except ValueError as exc:
        raise TelemetryDecodeError(
            f"Campo '{field_name}' no es un float válido: '{value}'"
        ) from exc
    if not math.isfinite(num):
        raise TelemetryDecodeError(
            f"Campo '{field_name}' contiene NaN/Inf no permitido: '{value}'"
        )
    return num


def _parse_int(value: str, field_name: str, min_val: Optional[int] = None, max_val: Optional[int] = None) -> int:
    try:
        num = int(value)
    except ValueError as exc:
        raise TelemetryDecodeError(
            f"Campo '{field_name}' no es un entero válido: '{value}'"
        ) from exc
    if min_val is not None and num < min_val:
        raise TelemetryDecodeError(
            f"Campo '{field_name}' ({num}) es menor que el mínimo permitido ({min_val})"
        )
    if max_val is not None and num > max_val:
        raise TelemetryDecodeError(
            f"Campo '{field_name}' ({num}) supera el máximo permitido ({max_val})"
        )
    return num


def decode_telemetry_line(
    line: str,
    expected_team_id: Optional[int] = TEAM_ID,
    strict_state: bool = True,
) -> TelemetryPacket:
    """Valida y decodifica una línea CSV de 33 columnas emitida por el OBC.

    Lanza TelemetryDecodeError si la línea está vacía, no tiene 33 columnas,
    tiene tipos inválidos o el TEAM_ID no coincide.
    """
    if line is None:
        raise TelemetryDecodeError("La línea de telemetría es None")

    cleaned = line.strip()
    if not cleaned:
        raise TelemetryDecodeError("Línea de telemetría vacía")

    # Separar las primeras 32 comas (permitiendo que OPTIONAL_DATA sea la columna 32)
    parts = cleaned.split(",")
    if len(parts) != EXPECTED_COLUMN_COUNT:
        raise TelemetryDecodeError(
            f"Cantidad de columnas inválida: se esperaban {EXPECTED_COLUMN_COUNT}, "
            f"se recibieron {len(parts)}"
        )

    team_id = _parse_int(parts[0].strip(), "TEAM_ID", min_val=0, max_val=65535)
    if expected_team_id is not None and team_id != expected_team_id:
        raise TelemetryDecodeError(
            f"TEAM_ID inválido: recibido {team_id}, esperado {expected_team_id}"
        )

    mission_time_ms = _parse_int(parts[1].strip(), "MISSION_TIME_MS", min_val=0)
    packet_count = _parse_int(parts[2].strip(), "PACKET_COUNT", min_val=0)
    mode = _parse_int(parts[3].strip(), "MODE", min_val=0, max_val=255)

    state = parts[4].strip()
    if strict_state and state not in VALID_FLIGHT_STATES:
        raise TelemetryDecodeError(
            f"STATE desconocido: '{state}'. Valores válidos: {VALID_FLIGHT_STATES}"
        )

    alt_m = _parse_finite_float(parts[5].strip(), "ALT_M")
    press_pa = _parse_finite_float(parts[6].strip(), "PRESS_PA")
    temp_c = _parse_finite_float(parts[7].strip(), "TEMP_C")
    batt_v = _parse_finite_float(parts[8].strip(), "BATT_V")
    curr_ma = _parse_finite_float(parts[9].strip(), "CURR_MA")

    acc_x_g = _parse_finite_float(parts[10].strip(), "ACC_X")
    acc_y_g = _parse_finite_float(parts[11].strip(), "ACC_Y")
    acc_z_g = _parse_finite_float(parts[12].strip(), "ACC_Z")

    gyr_x_dps = _parse_finite_float(parts[13].strip(), "GYR_X")
    gyr_y_dps = _parse_finite_float(parts[14].strip(), "GYR_Y")
    gyr_z_dps = _parse_finite_float(parts[15].strip(), "GYR_Z")

    lat_deg = _parse_finite_float(parts[16].strip(), "LAT")
    lon_deg = _parse_finite_float(parts[17].strip(), "LON")
    gps_alt_m = _parse_finite_float(parts[18].strip(), "GPS_ALT_M")
    sats = _parse_int(parts[19].strip(), "SATS", min_val=0, max_val=255)

    mag_x_ut = _parse_finite_float(parts[20].strip(), "MAG_X_UT")
    mag_y_ut = _parse_finite_float(parts[21].strip(), "MAG_Y_UT")
    mag_z_ut = _parse_finite_float(parts[22].strip(), "MAG_Z_UT")

    opt1_raw = _parse_int(parts[23].strip(), "OPT1", min_val=0, max_val=65535)
    opt2_raw = _parse_int(parts[24].strip(), "OPT2", min_val=0, max_val=65535)
    opt3_raw = _parse_int(parts[25].strip(), "OPT3", min_val=0, max_val=65535)
    opt4_raw = _parse_int(parts[26].strip(), "OPT4", min_val=0, max_val=65535)

    rtc_h = _parse_int(parts[27].strip(), "RTC_H", min_val=0, max_val=23)
    rtc_m = _parse_int(parts[28].strip(), "RTC_M", min_val=0, max_val=59)
    rtc_s = _parse_int(parts[29].strip(), "RTC_S", min_val=0, max_val=59)
    rtc_valid = _parse_int(parts[30].strip(), "RTC_VALID", min_val=0, max_val=1)
    gps_fix = _parse_int(parts[31].strip(), "GPS_FIX", min_val=0, max_val=1)

    optional_str = parts[32].strip()
    optional_parsed = parse_optional_data(optional_str)

    return TelemetryPacket(
        team_id=team_id,
        mission_time_ms=mission_time_ms,
        packet_count=packet_count,
        mode=mode,
        state=state,
        alt_m=alt_m,
        press_pa=press_pa,
        temp_c=temp_c,
        batt_v=batt_v,
        curr_ma=curr_ma,
        acc_x_g=acc_x_g,
        acc_y_g=acc_y_g,
        acc_z_g=acc_z_g,
        gyr_x_dps=gyr_x_dps,
        gyr_y_dps=gyr_y_dps,
        gyr_z_dps=gyr_z_dps,
        lat_deg=lat_deg,
        lon_deg=lon_deg,
        gps_alt_m=gps_alt_m,
        sats=sats,
        mag_x_ut=mag_x_ut,
        mag_y_ut=mag_y_ut,
        mag_z_ut=mag_z_ut,
        opt1_raw=opt1_raw,
        opt2_raw=opt2_raw,
        opt3_raw=opt3_raw,
        opt4_raw=opt4_raw,
        rtc_h=rtc_h,
        rtc_m=rtc_m,
        rtc_s=rtc_s,
        rtc_valid=rtc_valid,
        gps_fix=gps_fix,
        optional_data=optional_str,
        optional=optional_parsed,
        raw_line=cleaned,
        raw_fields=tuple(p.strip() for p in parts),
    )


def extract_command_ack(line: str) -> Optional[str]:
    """Detecta si una línea no-CSV del OBC corresponde a la confirmación directa de un comando.

    Retorna el nombre canónico del comando ('CX', 'ST', 'CAL', 'MEC', 'RST', 'MISSION_START')
    o None si no es un acuse.
    """
    text = (line or "").strip()
    if not text:
        return None
    for cmd in ("CX", "ST", "CAL", "MEC", "RST"):
        if text.startswith(f"{cmd} OK"):
            return cmd
    if text.startswith("CMD:MISSION_START"):
        return "MISSION_START"
    return None

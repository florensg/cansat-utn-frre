"""Simulador de vuelo completo a 1 Hz para pruebas de la GCS sin hardware conectado.

Reproduce fielmente el comportamiento de:
- firmware/obc-esp32s3/src/main.cpp
- firmware/obc-esp32s3/lib/flight_core/src/telemetry.cpp
- firmware/payload-rpi/src/main.py (resumen $RPI en OPTIONAL_DATA)

Secuencia de vuelo simulada:
- t = 0..4 s:   LaunchPad (0 m, armado en rampa)
- t = 5..19 s:  Ascent (0 m -> 400 m) + emisión de CMD:MISSION_START;STATE:Ascent (Requisito X3)
- t = 20 s:     Apogeo detectado (~394 m <= 400 - 5 m) -> transición a Descent (15 m/s)
- t = 25 s:     Cruza el 80 % de 400 m (<= 320 m) -> apertura 2.º paracaídas (P2:1, descenso a ~5 m/s)
- t = 88 s+:    Aterrizaje (alt <= 5 m) -> transición a Landed (0 m)
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import List, Tuple

from .config import TEAM_ID


@dataclass
class FlightSimulator:
    """Generador determinista/interactivo de tramas CSV de 33 columnas y respuestas a comandos."""

    team_id: int = TEAM_ID
    base_lat_deg: float = -27.451120   # UTN FRRe, Resistencia, Chaco
    base_lon_deg: float = -58.986510
    base_gps_alt_m: float = 52.0
    baseline_press_pa: float = 101325.0

    def __post_init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        """Reinicia la simulación al estado inicial en plataforma de lanzamiento."""
        self.tick_Index: int = 0
        self.packet_count: int = 0
        self.mission_time_ms: int = 0
        self.state: str = "LaunchPad"
        self.max_altitude_m: float = 0.0
        self.apogee_detected: bool = False
        self.second_parachute_deployed: bool = False
        self.altitude_offset_m: float = 0.0
        self.last_cmd_echo: str = "NONE"
        self.nir_capture_count: int = 0

        now_utc = datetime.now(timezone.utc)
        self.rtc_base_seconds: int = (
            now_utc.hour * 3600 + now_utc.minute * 60 + now_utc.second
        )
        self.rtc_valid: int = 1
        self._pending_lines: List[str] = []

    def _raw_profile_altitude(self, t_s: int) -> float:
        """Curva de altitud física antes de aplicar el offset de calibración CAL."""
        if t_s <= 4:
            return 0.4 * math.sin(t_s * 0.9)
        if t_s <= 19:
            # Ascenso hasta 400.0 m en t = 19 s
            frac = (t_s - 4) / 15.0
            return 400.0 * frac
        if t_s == 20:
            # Caída de 6 m respecto a 400 m -> dispara ApogeeDetected (>= 5 m)
            return 394.0
        if t_s <= 25:
            # Etapa 1 de descenso (~15 m/s según requisito C3): de 394 m a 319 m
            return 394.0 - 15.0 * (t_s - 20)
        # Etapa 2 de descenso tras liberar 2.º paracaídas al 80 % (<= 320 m):
        # desciende de forma suave a razón de ~5 m/s (requisito C4)
        alt = 319.0 - 5.2 * (t_s - 25)
        return max(0.0, alt)

    def handle_command(self, cmd_line: str) -> List[str]:
        """Procesa un telecomando enviado desde la GCS y devuelve las líneas de respuesta."""
        clean = (cmd_line or "").strip()
        if not clean:
            return []

        tokens = clean.split()
        verb = tokens[0].upper()
        responses: List[str] = []

        if verb == "CX":
            self.last_cmd_echo = "CX"
            responses.append(
                f"CX OK {self.state} PKT={self.packet_count} "
                f"BATT=3.92V CURR=245.0mA SOLAR=14200,4800,11300,5200"
            )
        elif verb == "ST":
            self.last_cmd_echo = "ST"
            if len(tokens) >= 2:
                time_part = tokens[1].split("T")[-1]
                hms = time_part.split(":")
                if len(hms) == 3:
                    try:
                        h, m, s = int(hms[0]), int(hms[1]), int(hms[2])
                        if 0 <= h < 24 and 0 <= m < 60 and 0 <= s < 60:
                            elapsed_s = self.mission_time_ms // 1000
                            self.rtc_base_seconds = (
                                h * 3600 + m * 60 + s - elapsed_s
                            ) % 86400
                            self.rtc_valid = 1
                    except ValueError:
                        pass
            responses.append("ST OK")
        elif verb == "CAL":
            raw_alt = self._raw_profile_altitude(self.tick_Index)
            self.altitude_offset_m = raw_alt
            self.last_cmd_echo = "CAL"
            responses.append("CAL OK")
        elif verb == "MEC":
            self.second_parachute_deployed = True
            self.last_cmd_echo = "MEC"
            responses.append("MEC OK (Servo liberado -> 90 deg)")
        elif verb == "RST":
            self.reset()
            self.last_cmd_echo = "RST"
            responses.append("RST OK")

        self._pending_lines.extend(responses)
        return responses

    def step(self) -> Tuple[List[str], str]:
        """Avanza 1 segundo la simulación.

        Retorna (lineas_de_evento_previas, linea_csv_33_campos).
        """
        events: List[str] = list(self._pending_lines)
        self._pending_lines.clear()

        t_s = self.tick_Index
        self.tick_Index += 1
        self.mission_time_ms = t_s * 1000
        self.packet_count += 1

        raw_alt = self._raw_profile_altitude(t_s)
        alt_m = max(0.0, raw_alt - self.altitude_offset_m)

        # Máquina de estados y detector de apogeo idénticos a update_flight_logic()
        if self.state == "LaunchPad" and alt_m >= 15.0:
            self.state = "Ascent"
            self.last_cmd_echo = "START"
            events.append("CMD:MISSION_START;STATE:Ascent")
            events.append("EVENT: LAUNCH_DETECTED -> Ascent")

        if self.state in ("Ascent", "Descent"):
            if alt_m > self.max_altitude_m:
                self.max_altitude_m = alt_m

            if (
                not self.apogee_detected
                and self.max_altitude_m >= 100.0
                and (self.max_altitude_m - alt_m) >= 5.0
            ):
                self.apogee_detected = True
                self.state = "Descent"
                events.append(
                    f"EVENT: APOGEE_DETECTED -> Descent (Max Alt: {self.max_altitude_m:.1f} m)"
                )
            elif (
                self.apogee_detected
                and not self.second_parachute_deployed
                and alt_m <= (self.max_altitude_m * 0.80)
            ):
                self.second_parachute_deployed = True
                events.append(
                    f"EVENT: SECOND_PARACHUTE_80_PERCENT -> Servo 90 deg (Alt: {alt_m:.1f} m)"
                )

        if self.state == "Descent" and alt_m <= 5.0 and t_s > 25:
            self.state = "Landed"
            events.append("EVENT: LANDED_DETECTED -> Landed")

        # Variables físicas coherentes con la altitud y la fase de misión
        press_pa = self.baseline_press_pa * math.pow(
            1.0 - (alt_m / 44330.0), 5.255
        )
        temp_c = 26.5 - (alt_m * 0.0065)
        batt_v = max(3.55, 4.12 - (t_s * 0.002))
        curr_ma = (
            620.0
            if (self.second_parachute_deployed and 25 <= t_s <= 27)
            else (310.0 if self.state in ("Ascent", "Descent") else 185.0)
        )

        # Dinámica IMU
        if self.state == "Ascent":
            acc_x = 0.12 * math.sin(t_s * 0.7)
            acc_y = 0.10 * math.cos(t_s * 0.7)
            acc_z = 2.45 + 0.25 * math.sin(t_s)
            gyr_x = 12.0 * math.sin(t_s * 0.5)
            gyr_y = 9.5 * math.cos(t_s * 0.5)
            gyr_z = 45.0
        elif self.state == "Descent":
            damping = 0.45 if self.second_parachute_deployed else 1.0
            acc_x = 0.08 * damping * math.sin(t_s * 0.6)
            acc_y = 0.08 * damping * math.cos(t_s * 0.6)
            acc_z = 1.02 + 0.05 * math.sin(t_s * 0.8)
            gyr_x = 18.0 * damping * math.sin(t_s * 0.4)
            gyr_y = 15.0 * damping * math.cos(t_s * 0.4)
            gyr_z = 28.0 * damping
        else:
            acc_x = 0.01
            acc_y = -0.01
            acc_z = 1.00
            gyr_x = 0.1
            gyr_y = -0.1
            gyr_z = 0.0

        # Deriva horizontal por viento durante el vuelo
        drift_lat = (0.000025 * t_s) if self.state != "LaunchPad" else 0.0
        drift_lon = (0.000032 * t_s) if self.state != "LaunchPad" else 0.0
        lat_deg = self.base_lat_deg + drift_lat
        lon_deg = self.base_lon_deg + drift_lon
        gps_alt_m = self.base_gps_alt_m + alt_m
        sats = 9
        gps_fix = 1

        # Magnetómetro LIS3MDL y 4 fotodiodos solares BPW34 (ADS1115)
        yaw_angle_rad = (t_s * 0.22) % (2.0 * math.pi)
        mag_x_ut = 22.5 * math.cos(yaw_angle_rad)
        mag_y_ut = -22.5 * math.sin(yaw_angle_rad)
        mag_z_ut = -14.8

        sun_angle_rad = (yaw_angle_rad + 0.6) % (2.0 * math.pi)
        opt1 = int(12000 + 8000 * max(0.0, math.cos(sun_angle_rad)))
        opt2 = int(12000 + 8000 * max(0.0, -math.cos(sun_angle_rad)))
        opt3 = int(12000 + 8000 * max(0.0, math.sin(sun_angle_rad)))
        opt4 = int(12000 + 8000 * max(0.0, -math.sin(sun_angle_rad)))

        # Reloj UTC
        total_utc_s = (self.rtc_base_seconds + t_s) % 86400
        rtc_h = total_utc_s // 3600
        rtc_m = (total_utc_s % 3600) // 60
        rtc_s = total_utc_s % 60

        # Resumen de Misión Secundaria (Raspberry Pi Zero 2W)
        if self.state in ("Ascent", "Descent") and (t_s % 2 == 0):
            self.nir_capture_count += 1
        yaw_deg = math.degrees(yaw_angle_rad) % 360.0
        sun_deg = math.degrees(sun_angle_rad) % 360.0
        apt_label = "APTA" if self.second_parachute_deployed or self.state == "Landed" else "PENDIENTE"
        apt_score = 0.86 if apt_label == "APTA" else 0.52
        p2_flag = 1 if self.second_parachute_deployed else 0

        optional_data = (
            f"MAX:{self.max_altitude_m:.1f};P2:{p2_flag};CMD:{self.last_cmd_echo};"
            f"RPI:YAW:{yaw_deg:.0f}|SUN:{sun_deg:.0f}|NIR:{self.nir_capture_count}|"
            f"APT:{apt_label}({apt_score:.2f})"
        )

        csv_line = (
            f"{self.team_id},{self.mission_time_ms},{self.packet_count},1,{self.state},"
            f"{alt_m:.2f},{press_pa:.2f},{temp_c:.2f},{batt_v:.2f},{curr_ma:.2f},"
            f"{acc_x:.3f},{acc_y:.3f},{acc_z:.3f},{gyr_x:.2f},{gyr_y:.2f},{gyr_z:.2f},"
            f"{lat_deg:.6f},{lon_deg:.6f},{gps_alt_m:.2f},{sats},"
            f"{mag_x_ut:.1f},{mag_y_ut:.1f},{mag_z_ut:.1f},"
            f"{opt1},{opt2},{opt3},{opt4},"
            f"{rtc_h},{rtc_m},{rtc_s},{self.rtc_valid},{gps_fix},{optional_data}"
        )
        return events, csv_line

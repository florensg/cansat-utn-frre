"""Tests unitarios para validar el parseo estricto de las 33 columnas y OPTIONAL_DATA."""

from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from src.config import (
    ALARM_THRESHOLDS,
    EXPECTED_COLUMN_COUNT,
    STANDARD_GRAVITY_MS2,
    TEAM_ID,
)
from src.decoder import (
    TelemetryDecodeError,
    decode_telemetry_line,
    extract_command_ack,
    parse_optional_data,
)
from src.serial_manager import format_command_string


SAMPLE_VALID_33_LINE = (
    "1000,15000,15,0,Ascent,120.50,99850.25,24.80,3.95,310.00,"
    "0.050,-0.020,1.850,4.50,-2.10,30.00,-27.451120,-58.986510,172.50,9,"
    "21.4,-8.2,-14.6,14500,4200,11800,5100,14,32,10,1,1,"
    "MAX:120.5;P2:0;CMD:CAL;RPI:YAW:182|SUN:135|NIR:6|APT:APTA(0.84)"
)


class TestTelemetryDecoder(unittest.TestCase):
    """Suite de pruebas del decodificador de 33 columnas."""

    def test_expected_column_count_is_33(self) -> None:
        self.assertEqual(EXPECTED_COLUMN_COUNT, 33)

    def test_decode_valid_33_column_line(self) -> None:
        pkt = decode_telemetry_line(SAMPLE_VALID_33_LINE)
        self.assertEqual(pkt.team_id, TEAM_ID)
        self.assertEqual(pkt.mission_time_ms, 15000)
        self.assertEqual(pkt.mission_time_s, 15)
        self.assertEqual(pkt.mission_time_hms, "00:00:15")
        self.assertEqual(pkt.packet_count, 15)
        self.assertEqual(pkt.mode, 0)
        self.assertEqual(pkt.state, "Ascent")
        self.assertAlmostEqual(pkt.alt_m, 120.50)
        self.assertAlmostEqual(pkt.press_pa, 99850.25)
        self.assertAlmostEqual(pkt.press_kpa, 99.85025)
        self.assertAlmostEqual(pkt.temp_c, 24.80)
        self.assertAlmostEqual(pkt.batt_v, 3.95)
        self.assertAlmostEqual(pkt.curr_ma, 310.00)
        self.assertAlmostEqual(pkt.curr_a, 0.310)
        self.assertAlmostEqual(pkt.acc_z_g, 1.850)
        self.assertAlmostEqual(pkt.acc_z_ms2, 1.850 * STANDARD_GRAVITY_MS2, places=4)
        self.assertAlmostEqual(pkt.gyr_z_dps, 30.00)
        self.assertAlmostEqual(pkt.gyr_z_rads, math.radians(30.00), places=5)
        self.assertAlmostEqual(pkt.lat_deg, -27.451120)
        self.assertAlmostEqual(pkt.lon_deg, -58.986510)
        self.assertAlmostEqual(pkt.gps_alt_m, 172.50)
        self.assertEqual(pkt.sats, 9)
        self.assertAlmostEqual(pkt.mag_x_ut, 21.4)
        self.assertAlmostEqual(pkt.mag_y_ut, -8.2)
        self.assertAlmostEqual(pkt.mag_z_ut, -14.6)
        self.assertEqual(pkt.opt1_raw, 14500)
        self.assertEqual(pkt.opt2_raw, 4200)
        self.assertEqual(pkt.opt3_raw, 11800)
        self.assertEqual(pkt.opt4_raw, 5100)
        self.assertEqual(pkt.rtc_hms, "14:32:10")
        self.assertEqual(pkt.rtc_valid, 1)
        self.assertEqual(pkt.gps_fix, 1)
        self.assertEqual(len(pkt.to_csv_row()), 33)

    def test_optional_data_parsing(self) -> None:
        opt = parse_optional_data(
            "MAX:398.4;P2:1;CMD:MEC;RPI:YAW:270|SUN:90|NIR:14|APT:APTA(0.91)"
        )
        self.assertAlmostEqual(opt.max_altitude_m, 398.4)
        self.assertTrue(opt.second_parachute_deployed)
        self.assertEqual(opt.cmd_echo, "MEC")
        self.assertAlmostEqual(opt.rpi_yaw_deg or 0.0, 270.0)
        self.assertAlmostEqual(opt.rpi_sun_azimuth_deg or 0.0, 90.0)
        self.assertEqual(opt.rpi_nir_captures, 14)
        self.assertEqual(opt.rpi_landing_apt, "APTA")
        self.assertAlmostEqual(opt.rpi_suitability_score, 0.91)

    def test_rejects_wrong_column_count(self) -> None:
        with self.assertRaises(TelemetryDecodeError):
            decode_telemetry_line("1000,1000,1,0,LaunchPad,0.0,101325.0")

    def test_rejects_wrong_team_id(self) -> None:
        bad_team = SAMPLE_VALID_33_LINE.replace("1000,", "9999,", 1)
        with self.assertRaises(TelemetryDecodeError):
            decode_telemetry_line(bad_team, expected_team_id=1000)

    def test_rejects_invalid_state(self) -> None:
        bad_state = SAMPLE_VALID_33_LINE.replace(",Ascent,", ",FreeFall,", 1)
        with self.assertRaises(TelemetryDecodeError):
            decode_telemetry_line(bad_state)

    def test_rejects_nan_or_non_numeric(self) -> None:
        bad_float = SAMPLE_VALID_33_LINE.replace(",120.50,", ",NaN,", 1)
        with self.assertRaises(TelemetryDecodeError):
            decode_telemetry_line(bad_float)

    def test_extract_command_ack(self) -> None:
        self.assertEqual(
            extract_command_ack("CX OK Ascent PKT=12 BATT=3.9V CURR=200mA SOLAR=1,2,3,4"),
            "CX",
        )
        self.assertEqual(extract_command_ack("CAL OK"), "CAL")
        self.assertEqual(extract_command_ack("ST OK"), "ST")
        self.assertEqual(extract_command_ack("MEC OK (Servo liberado -> 90 deg)"), "MEC")
        self.assertEqual(extract_command_ack("CMD:MISSION_START;STATE:Ascent"), "MISSION_START")
        self.assertIsNone(extract_command_ack(SAMPLE_VALID_33_LINE))

    def test_1s_battery_alarm_thresholds_and_st_auto_utc(self) -> None:
        self.assertAlmostEqual(ALARM_THRESHOLDS.battery_low_v, 3.60)
        self.assertAlmostEqual(ALARM_THRESHOLDS.battery_critical_v, 3.40)

        st_cmd = format_command_string("ST", "")
        self.assertTrue(st_cmd.startswith("ST "))
        self.assertEqual(len(st_cmd.split()[1].split(":")), 3)


if __name__ == "__main__":
    unittest.main()

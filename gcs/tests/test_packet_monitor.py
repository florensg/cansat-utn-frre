"""Tests unitarios para el monitor de paquetes, almacenamiento USB, simulador y anti-colisión."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from src.decoder import decode_telemetry_line
from src.packet_monitor import PacketMonitor
from src.serial_manager import TelemetrySessionController
from src.simulator import FlightSimulator
from src.storage import TelemetryStorage


class TestPacketMonitor(unittest.TestCase):
    """Verifica el cumplimiento del requisito CONAE G14 (RX_COUNT vs PACKET_COUNT)."""

    def test_contiguous_sequence_has_zero_loss(self) -> None:
        mon = PacketMonitor()
        for pkt_id in (1, 2, 3, 4, 5):
            stats = mon.register_valid_packet(pkt_id)
        self.assertEqual(stats.rx_count, 5)
        self.assertEqual(stats.last_tx_packet_count, 5)
        self.assertEqual(stats.lost_count, 0)
        self.assertAlmostEqual(stats.packet_loss_percent, 0.0)

    def test_detects_single_and_multiple_packet_gaps(self) -> None:
        mon = PacketMonitor()
        mon.register_valid_packet(10)
        mon.register_valid_packet(11)
        # Salta el 12 y el 13 (2 perdidos)
        stats = mon.register_valid_packet(14)
        self.assertEqual(stats.rx_count, 3)
        self.assertEqual(stats.lost_count, 2)
        # Salta del 14 al 20 (5 perdidos adicionales -> total 7 perdidos)
        stats = mon.register_valid_packet(20)
        self.assertEqual(stats.rx_count, 4)
        self.assertEqual(stats.lost_count, 7)
        self.assertAlmostEqual(stats.packet_loss_percent, (7 / 11) * 100.0, places=3)

    def test_handles_duplicate_and_reboot_without_negative_loss(self) -> None:
        mon = PacketMonitor()
        mon.register_valid_packet(50)
        mon.register_valid_packet(51)
        # Duplicado del 51
        stats = mon.register_valid_packet(51)
        self.assertEqual(stats.lost_count, 0)
        self.assertEqual(stats.duplicate_or_reordered_count, 1)

        # Reinicio de OBC sin FRAM (vuelve a 1)
        stats = mon.register_valid_packet(1)
        self.assertEqual(stats.reboot_count, 1)
        self.assertEqual(stats.lost_count, 0)
        self.assertEqual(stats.last_tx_packet_count, 1)

    def test_malformed_counter(self) -> None:
        mon = PacketMonitor()
        mon.register_malformed_packet()
        stats = mon.register_malformed_packet()
        self.assertEqual(stats.malformed_count, 2)
        self.assertEqual(stats.rx_count, 0)

    def test_full_flight_simulator_and_usb_backup(self) -> None:
        sim = FlightSimulator()
        seen_states = set()
        seen_p2 = False

        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp) / "local"
            usb = Path(tmp) / "usb_drive"
            storage = TelemetryStorage(
                base_dir=base, backup_dir=usb, sync_interval_s=0.0
            )
            controller = TelemetrySessionController(storage=storage)

            for step_idx in range(95):
                events, csv_line = sim.step()
                for ev in events:
                    controller.process_incoming_line(ev, now_mono=float(step_idx))
                pkt = controller.process_incoming_line(csv_line, now_mono=float(step_idx))
                self.assertIsNotNone(pkt)
                assert pkt is not None
                seen_states.add(pkt.state)
                if pkt.optional.second_parachute_deployed:
                    seen_p2 = True

                # En ascenso, verificar que MEC está bloqueado por seguridad
                if pkt.state == "Ascent":
                    with self.assertRaises(PermissionError):
                        controller.queue_command("MEC")

            self.assertEqual(
                seen_states, {"LaunchPad", "Ascent", "Descent", "Landed"}
            )
            self.assertTrue(seen_p2)
            self.assertEqual(controller.monitor.stats.rx_count, 95)
            self.assertEqual(controller.monitor.stats.lost_count, 0)

            self.assertTrue(storage.sync_to_backup_now())
            self.assertTrue((usb / storage.csv_filename).exists())
            self.assertTrue((usb / "raw_rx.log").exists())
            self.assertTrue((usb / "events.log").exists())
            storage.close()

    def test_session_reset_and_command_echo_sync(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp) / "local"
            storage = TelemetryStorage(base_dir=base, sync_interval_s=0.0)
            controller = TelemetrySessionController(storage=storage)
            sim = FlightSimulator()

            first_csv = storage.csv_filename
            for i in range(3):
                _, line = sim.step()
                controller.process_incoming_line(line, now_mono=float(i))
            self.assertEqual(controller.monitor.stats.rx_count, 3)

            # Simular envío y confirmación inmediata de MEC en LaunchPad
            rec = controller.queue_command("MEC")
            rec.status = "ENVIADO"
            controller._active_command = rec
            controller.process_incoming_line("MEC OK (Servo liberado -> 90 deg)", now_mono=3.1)
            self.assertEqual(rec.status, "CONFIRMADO")
            self.assertEqual(rec.confirmed_echo, "CMD:MEC")

            # Reiniciar vuelo a PACKET_COUNT == 1 -> debe rotar CSV y reiniciar RX_COUNT a 1
            sim.reset()
            _, line_restart = sim.step()
            controller.process_incoming_line(line_restart, now_mono=4.0)
            self.assertEqual(controller.monitor.stats.rx_count, 1)
            self.assertEqual(controller.monitor.stats.lost_count, 0)
            self.assertNotEqual(storage.csv_filename, first_csv)
            storage.close()


if __name__ == "__main__":
    unittest.main()

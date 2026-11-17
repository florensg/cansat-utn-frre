"""Punto de entrada principal de la Estación Terrena (GCS) — CanSat UTN FRRe (ISI 2).

Soporta:
- Modo GUI completo con PySide6 + PyQtGraph (`python -m src.main` o `python -m src.main --simulate`)
- Modo serie real (`python -m src.main --port /dev/cu.usbserial-0001` o `--port COM3`)
- Modo consola/headless (`python -m src.main --simulate --headless --duration 10`) para
  verificación automática o ejecución en entornos sin servidor gráfico.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import List, Optional

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    __package__ = "src"

from .config import DEFAULT_BAUDRATE, TEAM_ID
from .decoder import TelemetryPacket
from .gui import PYSIDE6_AVAILABLE
from .packet_monitor import PacketStats
from .serial_manager import TelemetrySessionController
from .storage import TelemetryStorage


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Estación Terrena (GCS) — CanSat Universitario 2026-2027 (UTN FRRe)"
    )
    parser.add_argument(
        "--port",
        type=str,
        default=None,
        help="Puerto serie del receptor XBee / OBC (ej. COM3 en Windows o /dev/cu.usbserial en macOS)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=DEFAULT_BAUDRATE,
        help=f"Velocidad en baudios (default: {DEFAULT_BAUDRATE})",
    )
    parser.add_argument(
        "--simulate",
        action="store_true",
        help="Inicia automáticamente el simulador de vuelo integrado a 1 Hz",
    )
    parser.add_argument(
        "--data-dir",
        type=str,
        default="mission_logs",
        help="Carpeta local donde se guardan Flight_<TEAM_ID>.csv, raw_rx.log y events.log",
    )
    parser.add_argument(
        "--backup-dir",
        type=str,
        default=None,
        help="Carpeta o unidad USB/pendrive para replicación automática cada 5 s (Requisito G2)",
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        help="Ejecuta en modo consola sin abrir ventana gráfica (útil para CI y pruebas rápidas)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="Duración máxima en segundos antes de cerrar automáticamente (0 = ilimitado)",
    )
    return parser


def _run_headless_console(
    controller: TelemetrySessionController,
    simulate: bool,
    port: Optional[str],
    baud: int,
    duration_s: float,
) -> int:
    def _on_pkt(pkt: TelemetryPacket, stats: PacketStats) -> None:
        print(
            f"[T={pkt.mission_time_hms}] STATE={pkt.state:<9} "
            f"PKT_TX={pkt.packet_count:<4} RX={stats.rx_count:<4} LOST={stats.lost_count:<2} | "
            f"ALT={pkt.alt_m:6.2f} m | P={pkt.press_kpa:6.2f} kPa | T={pkt.temp_c:5.2f} °C | "
            f"BATT={pkt.batt_v:4.2f} V ({pkt.curr_a:5.3f} A) | "
            f"ACC=({pkt.acc_x_ms2:+.2f},{pkt.acc_y_ms2:+.2f},{pkt.acc_z_ms2:+.2f}) m/s² | "
            f"OPT={pkt.optional_data}"
        )

    def _on_ev(cat: str, msg: str) -> None:
        print(f"  >> [{cat}] {msg}")

    controller.on_packet = _on_pkt
    controller.on_event = _on_ev

    if simulate:
        controller.start_simulation()
    elif port:
        controller.start_serial(port, baud)
    else:
        print(
            "[INFO] Sin --port ni --simulate especificados. Iniciando modo simulación por defecto."
        )
        controller.start_simulation()

    start_mono = time.monotonic()
    try:
        while controller.is_running:
            if duration_s > 0.0 and (time.monotonic() - start_mono) >= duration_s:
                break
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        controller.stop()
        controller.storage.close()
        print(
            f"[OK] Sesión finalizada. Paquetes guardados en CSV: {controller.storage.packets_written} "
            f"({controller.storage.csv_path})"
        )
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    args = build_arg_parser().parse_args(argv)

    storage = TelemetryStorage(
        base_dir=Path(args.data_dir),
        team_id=TEAM_ID,
        backup_dir=Path(args.backup_dir) if args.backup_dir else None,
    )
    controller = TelemetrySessionController(storage=storage, team_id=TEAM_ID)

    if args.headless or not PYSIDE6_AVAILABLE:
        if not args.headless and not PYSIDE6_AVAILABLE:
            print(
                "[AVISO] PySide6/PyQtGraph no están instalados en este entorno Python. "
                "Ejecutando en modo consola en tiempo real (--headless).",
                file=sys.stderr,
            )
        return _run_headless_console(
            controller=controller,
            simulate=args.simulate,
            port=args.port,
            baud=args.baud,
            duration_s=args.duration,
        )

    from PySide6.QtCore import QTimer
    from PySide6.QtWidgets import QApplication
    from .gui import CanSatMainWindow

    app = QApplication(sys.argv)
    window = CanSatMainWindow(controller=controller)
    window.show()

    if args.simulate:
        window._toggle_simulation()
    elif args.port:
        controller.start_serial(args.port, args.baud)
        window.btn_connect_serial.setText("⏹ Desconectar Serie")
        window.btn_simulate.setEnabled(False)

    if args.duration > 0.0:
        QTimer.singleShot(int(args.duration * 1000), window.close)

    exit_code = app.exec()
    controller.on_packet = None
    controller.on_raw_line = None
    controller.on_command_status = None
    controller.on_event = None
    controller.stop()
    storage.close()
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())

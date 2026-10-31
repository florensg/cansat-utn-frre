#!/usr/bin/env python3
"""Registro de telemetria del payload secundario.

Lee lineas ASCII del OBC por UART y las guarda con marca de tiempo local.
"""

from __future__ import annotations

import argparse
import csv
import sys
import time
from pathlib import Path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Registra la telemetria recibida por el puerto serie."
    )
    parser.add_argument("--port", required=True, help="Puerto serie, por ejemplo COM3")
    parser.add_argument("--baud", type=int, default=115200, help="Velocidad en baudios")
    parser.add_argument(
        "--output",
        default="payload_log.csv",
        help="Archivo CSV de salida",
    )
    return parser


def run(port: str, baud: int, output: str) -> int:
    try:
        import serial
    except ImportError:
        print(
            "Falta pyserial. Instalar con: pip install -r requirements.txt",
            file=sys.stderr,
        )
        return 2

    output_path = Path(output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with serial.Serial(port, baud, timeout=1) as link:
        with output_path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["received_at", "raw_line"])
            while True:
                raw = link.readline().decode("utf-8", errors="replace").strip()
                if not raw:
                    continue
                stamp = time.strftime("%Y-%m-%dT%H:%M:%S")
                writer.writerow([stamp, raw])
                handle.flush()
                print(raw)
    return 0


def main() -> int:
    args = build_parser().parse_args()
    try:
        return run(args.port, args.baud, args.output)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    raise SystemExit(main())

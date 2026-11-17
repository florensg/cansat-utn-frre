"""Almacenamiento resiliente en tiempo real y replicación automática a USB/respaldo (Requisito G2).

Gestiona:
1. raw_rx.log: Cada línea cruda recibida por el puerto serie antes de parsear.
2. Flight_<TEAM_ID>_<YYYYMMDD_HHMMSS>.csv: Las 33 columnas validadas con flush() inmediato.
3. events.log: Registro con marca de tiempo de telecomandos enviados, acuses, alarmas y errores.
4. Replicación no bloqueante en segundo plano cada 5 s hacia un directorio/pendrive de respaldo.
"""

from __future__ import annotations

import csv
import os
import shutil
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional, TextIO

from .config import BACKUP_SYNC_INTERVAL_S, TEAM_ID, TELEMETRY_COLUMNS
from .decoder import TelemetryPacket


class TelemetryStorage:
    """Administrador de archivos de misión y respaldo periódico en segundo plano."""

    def __init__(
        self,
        base_dir: Path | str,
        team_id: int = TEAM_ID,
        backup_dir: Optional[Path | str] = None,
        sync_interval_s: float = BACKUP_SYNC_INTERVAL_S,
        session_timestamp: Optional[str] = None,
    ) -> None:
        self.base_dir = Path(base_dir).resolve()
        self.base_dir.mkdir(parents=True, exist_ok=True)
        self.team_id = team_id
        self.sync_interval_s = sync_interval_s

        stamp = session_timestamp or datetime.now().strftime("%Y%m%d_%H%M%S")
        self.csv_filename = f"Flight_{self.team_id}_{stamp}.csv"
        self.csv_path = self.base_dir / self.csv_filename
        self.raw_log_path = self.base_dir / "raw_rx.log"
        self.events_log_path = self.base_dir / "events.log"

        self._lock = threading.Lock()
        self._backup_dir: Optional[Path] = (
            Path(backup_dir).resolve() if backup_dir else None
        )
        self._last_backup_ok: bool = False
        self._last_backup_time_str: str = "NUNCA"
        self._last_backup_error: str = ""
        self._packets_written: int = 0

        # Abrir descriptores de archivo y escribir cabecera CSV si es nuevo
        self._raw_file: TextIO = self.raw_log_path.open("a", encoding="utf-8")
        self._events_file: TextIO = self.events_log_path.open("a", encoding="utf-8")
        csv_exists = self.csv_path.exists() and self.csv_path.stat().st_size > 0
        self._csv_file: TextIO = self.csv_path.open(
            "a", newline="", encoding="utf-8"
        )
        self._csv_writer = csv.writer(self._csv_file)
        if not csv_exists:
            self._csv_writer.writerow(list(TELEMETRY_COLUMNS))
            self._csv_file.flush()

        # Hilo demonio para replicación periódica sin bloquear la GUI
        self._stop_event = threading.Event()
        self._sync_thread: Optional[threading.Thread] = None
        if self.sync_interval_s > 0:
            self._sync_thread = threading.Thread(
                target=self._backup_loop,
                name="GCS-BackupSync",
                daemon=True,
            )
            self._sync_thread.start()

        self.log_event(
            "SESSION_START",
            f"Archivos inicializados en {self.base_dir} ({self.csv_filename})",
        )

    @property
    def backup_dir(self) -> Optional[Path]:
        with self._lock:
            return self._backup_dir

    @property
    def packets_written(self) -> int:
        with self._lock:
            return self._packets_written

    @property
    def backup_status_summary(self) -> str:
        """Devuelve un resumen legible para mostrar en el panel de estado de la GUI."""
        with self._lock:
            if self._backup_dir is None:
                return "SIN UNIDAD USB CONFIGURADA"
            if self._last_backup_ok:
                return f"RESPALDO USB OK ({self._last_backup_time_str}) -> {self._backup_dir.name}"
            if self._last_backup_error:
                return f"ERROR RESPALDO USB: {self._last_backup_error}"
            return f"PENDIENTE -> {self._backup_dir}"

    def set_backup_dir(self, backup_dir: Optional[Path | str]) -> bool:
        """Configura o cambia en caliente la carpeta/pendrive de respaldo y fuerza una copia."""
        with self._lock:
            if backup_dir is None or str(backup_dir).strip() == "":
                self._backup_dir = None
                self._last_backup_ok = False
                self._last_backup_error = ""
                return False
            self._backup_dir = Path(backup_dir).resolve()
        self.log_event("BACKUP_CONFIG", f"Destino de respaldo: {self._backup_dir}")
        return self.sync_to_backup_now()

    def start_new_session_csv(self, session_timestamp: Optional[str] = None) -> Path:
        """Crea un nuevo archivo CSV de sesión limpia cuando se reinicia Simulación/Serie o el vuelo.

        Si el archivo CSV actual aún tiene 0 paquetes escritos, lo reutiliza sin crear archivos vacíos.
        """
        with self._lock:
            if self._packets_written == 0 and not self._csv_file.closed:
                return self.csv_path

            try:
                if not self._csv_file.closed:
                    self._csv_file.flush()
                    self._csv_file.close()
            except Exception:
                pass

            stamp = session_timestamp or datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:19]
            self.csv_filename = f"Flight_{self.team_id}_{stamp}.csv"
            self.csv_path = self.base_dir / self.csv_filename
            self._packets_written = 0

            csv_exists = self.csv_path.exists() and self.csv_path.stat().st_size > 0
            self._csv_file = self.csv_path.open("a", newline="", encoding="utf-8")
            self._csv_writer = csv.writer(self._csv_file)
            if not csv_exists:
                self._csv_writer.writerow(list(TELEMETRY_COLUMNS))
                self._csv_file.flush()

        self.log_event(
            "SESSION_NEW_CSV",
            f"Nuevo archivo CSV de sesión iniciado: {self.csv_filename}",
        )
        return self.csv_path

    def write_raw_line(self, raw_line: str) -> None:
        """Guarda inmediatamente la línea cruda en raw_rx.log antes de parsearla."""
        ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"
        clean = raw_line.rstrip("\r\n")
        with self._lock:
            if not self._raw_file.closed:
                self._raw_file.write(f"[{ts}] {clean}\n")
                self._raw_file.flush()

    def write_packet(self, packet: TelemetryPacket) -> None:
        """Escribe un paquete validado en Flight_<TEAM_ID>.csv con flush() inmediato."""
        row = packet.to_csv_row()
        with self._lock:
            if not self._csv_file.closed:
                self._csv_writer.writerow(row)
                self._csv_file.flush()
                try:
                    os.fsync(self._csv_file.fileno())
                except OSError:
                    pass
                self._packets_written += 1

    def log_event(self, category: str, message: str) -> None:
        """Registra un evento operativo, comando o error en events.log con flush inmediato."""
        ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"
        line = f"[{ts}] [{category}] {message}\n"
        with self._lock:
            if not self._events_file.closed:
                self._events_file.write(line)
                self._events_file.flush()

    def sync_to_backup_now(self) -> bool:
        """Copia de forma atómica los archivos de sesión hacia la unidad USB/carpeta de respaldo."""
        with self._lock:
            target_dir = self._backup_dir
            if target_dir is None:
                return False
            try:
                self._csv_file.flush()
                self._raw_file.flush()
                self._events_file.flush()
            except Exception:
                pass

        try:
            target_dir.mkdir(parents=True, exist_ok=True)
            for src_path in (self.csv_path, self.raw_log_path, self.events_log_path):
                if src_path.exists():
                    shutil.copy2(src_path, target_dir / src_path.name)
            now_str = time.strftime("%H:%M:%S")
            with self._lock:
                self._last_backup_ok = True
                self._last_backup_time_str = now_str
                self._last_backup_error = ""
            return True
        except Exception as exc:
            with self._lock:
                self._last_backup_ok = False
                self._last_backup_error = str(exc)
            return False

    def _backup_loop(self) -> None:
        while not self._stop_event.wait(self.sync_interval_s):
            if self.backup_dir is not None:
                self.sync_to_backup_now()

    def close(self) -> None:
        """Detiene el hilo de respaldo, realiza una última sincronización y cierra archivos."""
        self._stop_event.set()
        if self._sync_thread is not None and self._sync_thread.is_alive():
            self._sync_thread.join(timeout=1.5)
        self.sync_to_backup_now()
        with self._lock:
            for handle in (self._csv_file, self._raw_file, self._events_file):
                try:
                    if not handle.closed:
                        handle.flush()
                        handle.close()
                except Exception:
                    pass

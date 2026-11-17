"""Gestor de comunicaciones serie multiplataforma (macOS/Windows) con ventana anti-colisión.

Características principales:
- Autodetección de puertos COM (Windows) y /dev/cu.* o /dev/tty.* (macOS/Linux).
- Encolado de telecomandos (CX, ST, CAL, MEC, RST) y despacho sincronizado dentro de la
  ventana silenciosa (~800 ms) posterior a la recepción del paquete de 1 Hz para evitar
  colisiones UART en radio XBee transparente.
- Seguimiento de estado de cada comando: ENVIADO -> CONFIRMADO / SIN CONFIRMACIÓN.
- Soporta tanto enlace serie real (pyserial) como modo Simulación integrado a 1 Hz.
"""

from __future__ import annotations

import queue
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Callable, List, Optional

from .config import (
    CMD_ACK_TIMEOUT_S,
    CMD_GUARD_DELAY_MS,
    CMD_MAX_WAIT_FOR_SLOT_MS,
    CMD_SILENT_WINDOW_MS,
    DEFAULT_BAUDRATE,
    SERIAL_TIMEOUT_S,
    TEAM_ID,
)
from .decoder import (
    TelemetryDecodeError,
    TelemetryPacket,
    decode_telemetry_line,
    extract_command_ack,
)
from .packet_monitor import PacketMonitor, PacketStats
from .simulator import FlightSimulator
from .storage import TelemetryStorage


@dataclass
class PortInfo:
    """Información descriptiva de un puerto serie detectado en el sistema."""

    device: str
    description: str
    hwid: str = ""

    @property
    def display_label(self) -> str:
        if self.description and self.description != "n/a":
            return f"{self.device} — {self.description}"
        return self.device


@dataclass
class CommandStatusRecord:
    """Estado de seguimiento de un telecomando enviado al CanSat."""

    command_text: str
    verb: str
    status: str  # "PENDIENTE", "ENVIADO", "CONFIRMADO", "SIN CONFIRMACIÓN"
    queued_at_mono: float
    sent_at_mono: Optional[float] = None
    confirmed_at_mono: Optional[float] = None
    confirmed_echo: str = "NONE"


def list_available_serial_ports() -> List[PortInfo]:
    """Detecta todos los puertos serie disponibles en macOS, Windows y Linux."""
    ports: List[PortInfo] = []
    try:
        from serial.tools import list_ports  # type: ignore

        for p in list_ports.comports():
            # En macOS priorizar /dev/cu.* para evitar bloqueo en DCD de /dev/tty.*
            ports.append(
                PortInfo(
                    device=p.device,
                    description=p.description or "",
                    hwid=p.hwid or "",
                )
            )
    except Exception:
        pass

    # Ordenar priorizando adaptadores USB-Serie (CP210x, FTDI, CH340, USB JTAG/serial)
    def _priority(info: PortInfo) -> tuple[int, str]:
        desc_u = (info.description + " " + info.device).upper()
        is_usb = any(
            k in desc_u
            for k in ("USB", "UART", "SLAB", "CP210", "FTDI", "CH340", "MODEM", "COM")
        )
        is_cu = info.device.startswith("/dev/cu.") or info.device.upper().startswith("COM")
        return (0 if (is_usb and is_cu) else (1 if is_usb else 2), info.device)

    ports.sort(key=_priority)
    return ports


def format_command_string(verb: str, argument: Optional[str] = None) -> str:
    """Construye la cadena ASCII canónica del telecomando para el OBC."""
    v = verb.strip().upper()
    if v == "ST":
        if not argument or not argument.strip():
            now_utc = datetime.now(timezone.utc)
            argument = now_utc.strftime("%H:%M:%S")
        return f"ST {argument.strip()}"
    if v in ("CX", "CAL", "MEC", "RST"):
        return v
    raise ValueError(f"Telecomando no reconocido: '{verb}'")


class TelemetrySessionController:
    """Orquestador thread-safe de recepción, almacenamiento, ventana anti-colisión y simulación."""

    def __init__(
        self,
        storage: TelemetryStorage,
        monitor: Optional[PacketMonitor] = None,
        team_id: int = TEAM_ID,
        on_packet: Optional[Callable[[TelemetryPacket, PacketStats], None]] = None,
        on_raw_line: Optional[Callable[[str], None]] = None,
        on_command_status: Optional[Callable[[CommandStatusRecord], None]] = None,
        on_event: Optional[Callable[[str, str], None]] = None,
    ) -> None:
        self.storage = storage
        self.monitor = monitor or PacketMonitor()
        self.team_id = team_id

        self.on_packet = on_packet
        self.on_raw_line = on_raw_line
        self.on_command_status = on_command_status
        self.on_event = on_event

        self._cmd_queue: "queue.Queue[CommandStatusRecord]" = queue.Queue()
        self._lock = threading.Lock()
        self._running = threading.Event()
        self._worker_thread: Optional[threading.Thread] = None

        self._mode: str = "IDLE"  # "IDLE" | "SERIAL" | "SIMULATION"
        self._port_name: str = ""
        self._baudrate: int = DEFAULT_BAUDRATE
        self._last_rx_mono: float = 0.0
        self._last_packet_rx_mono: float = 0.0
        self._active_command: Optional[CommandStatusRecord] = None
        self._last_state: str = "LaunchPad"
        self._simulator = FlightSimulator(team_id=self.team_id)

    @property
    def is_running(self) -> bool:
        return self._running.is_set()

    @property
    def mode(self) -> str:
        with self._lock:
            return self._mode

    @property
    def last_rx_monotonic(self) -> float:
        with self._lock:
            return self._last_rx_mono

    @property
    def seconds_since_last_rx(self) -> float:
        with self._lock:
            if self._last_rx_mono <= 0.0:
                return float("inf")
            return max(0.0, time.monotonic() - self._last_rx_mono)

    @property
    def current_flight_state(self) -> str:
        with self._lock:
            return self._last_state

    @property
    def active_command(self) -> Optional[CommandStatusRecord]:
        with self._lock:
            return self._active_command

    def queue_command(
        self,
        verb: str,
        argument: Optional[str] = None,
        allow_mec_override: bool = False,
    ) -> CommandStatusRecord:
        """Encola un telecomando para ser transmitido en la próxima ventana silenciosa."""
        v = verb.strip().upper()
        with self._lock:
            current_state = self._last_state

        # Bloqueo de seguridad en ascenso para MEC salvo override explícito de emergencia
        if v == "MEC" and current_state == "Ascent" and not allow_mec_override:
            raise PermissionError(
                "Seguridad de vuelo: el comando MEC está bloqueado durante Ascent."
            )

        cmd_str = format_command_string(v, argument)
        record = CommandStatusRecord(
            command_text=cmd_str,
            verb=v,
            status="PENDIENTE",
            queued_at_mono=time.monotonic(),
        )
        self._cmd_queue.put(record)
        self.storage.log_event("CMD_QUEUED", f"Comando encolado: {cmd_str}")
        if self.on_command_status:
            self.on_command_status(record)
        return record

    def can_transmit_command_now(self, now_mono: Optional[float] = None) -> bool:
        """Evalúa si estamos dentro de la ventana anti-colisión posterior al paquete de 1 Hz."""
        now = now_mono if now_mono is not None else time.monotonic()
        with self._lock:
            last_pkt = self._last_packet_rx_mono

        if last_pkt <= 0.0:
            # Aún no se recibieron paquetes; permitir transmisión inmediata
            return True

        elapsed_ms = (now - last_pkt) * 1000.0
        if CMD_GUARD_DELAY_MS <= elapsed_ms <= (CMD_GUARD_DELAY_MS + CMD_SILENT_WINDOW_MS):
            return True
        if elapsed_ms >= CMD_MAX_WAIT_FOR_SLOT_MS:
            # Si pasó más de 1.1 s sin paquete nuevo, abrir ventana por timeout
            return True
        return False

    def process_incoming_line(self, raw_line: str, now_mono: Optional[float] = None) -> Optional[TelemetryPacket]:
        """Procesa una línea entrante (de puerto serie o simulador)."""
        now = now_mono if now_mono is not None else time.monotonic()
        clean = raw_line.strip()
        if not clean:
            return None

        with self._lock:
            self._last_rx_mono = now

        # 1. Guardar siempre en raw_rx.log antes de parsear (Requisito G2)
        self.storage.write_raw_line(clean)
        if self.on_raw_line:
            self.on_raw_line(clean)

        # 2. Verificar si es un acuse directo de telecomando (ej. 'CAL OK', 'CX OK ...')
        ack_verb = extract_command_ack(clean)
        if ack_verb is not None:
            if ack_verb == "MISSION_START":
                self.storage.log_event(
                    "MISSION_START_X3",
                    f"Inicio automático de misión recibido del OBC: {clean}",
                )
                if self.on_event:
                    self.on_event("MISSION_START", clean)
            else:
                self._mark_command_confirmed(ack_verb, now, source=clean)
            return None

        # 3. Verificar si es una línea de evento textual del OBC ('EVENT: ...')
        if clean.startswith("EVENT:") or clean.startswith("CanSat ") or clean.startswith("==="):
            self.storage.log_event("OBC_EVENT", clean)
            if self.on_event:
                self.on_event("OBC_EVENT", clean)
            return None

        # 4. Decodificar como trama CSV de 33 columnas
        try:
            packet = decode_telemetry_line(clean, expected_team_id=self.team_id)
        except TelemetryDecodeError as exc:
            stats = self.monitor.register_malformed_packet()
            self.storage.log_event("DECODE_ERROR", f"{exc} | Línea: {clean}")
            if self.on_event:
                self.on_event("DECODE_ERROR", f"{exc} ({stats.malformed_count} err)")
            return None

        # 5. Paquete válido: detectar si el vuelo se reinició a PACKET_COUNT == 1
        prev_pkt_count = self.monitor.stats.last_tx_packet_count
        if prev_pkt_count is not None and prev_pkt_count > 1 and packet.packet_count == 1:
            self.storage.start_new_session_csv()

        with self._lock:
            self._last_packet_rx_mono = now
            self._last_state = packet.state

        stats = self.monitor.register_valid_packet(packet.packet_count)
        self.storage.write_packet(packet)

        # 6. Verificar confirmación por eco en OPTIONAL_DATA (CMD:<verb>)
        if packet.optional.cmd_echo and packet.optional.cmd_echo != "NONE":
            self._mark_command_confirmed(
                packet.optional.cmd_echo, now, source=f"OPTIONAL_DATA(CMD:{packet.optional.cmd_echo})"
            )

        if self.on_packet:
            self.on_packet(packet, stats)
        return packet

    def _mark_command_confirmed(self, verb: str, now_mono: float, source: str) -> None:
        v = verb.strip().upper()
        updated: Optional[CommandStatusRecord] = None
        with self._lock:
            if (
                self._active_command is not None
                and self._active_command.verb == v
                and self._active_command.status != "CONFIRMADO"
            ):
                self._active_command.status = "CONFIRMADO"
                self._active_command.confirmed_at_mono = now_mono
                self._active_command.confirmed_echo = f"CMD:{v}"
                updated = self._active_command

        if updated is not None:
            self.storage.log_event(
                "CMD_CONFIRMED",
                f"Comando '{updated.command_text}' CONFIRMADO vía {source}",
            )
            if self.on_command_status:
                self.on_command_status(updated)

    def check_command_timeout(self, now_mono: Optional[float] = None) -> None:
        """Marca como SIN CONFIRMACIÓN un comando enviado que superó CMD_ACK_TIMEOUT_S."""
        now = now_mono if now_mono is not None else time.monotonic()
        timed_out: Optional[CommandStatusRecord] = None
        with self._lock:
            cmd = self._active_command
            if (
                cmd is not None
                and cmd.status == "ENVIADO"
                and cmd.sent_at_mono is not None
                and (now - cmd.sent_at_mono) >= CMD_ACK_TIMEOUT_S
            ):
                cmd.status = "SIN CONFIRMACIÓN"
                timed_out = cmd

        if timed_out is not None:
            self.storage.log_event(
                "CMD_TIMEOUT",
                f"Comando '{timed_out.command_text}' SIN CONFIRMACIÓN tras {CMD_ACK_TIMEOUT_S:.1f}s",
            )
            if self.on_command_status:
                self.on_command_status(timed_out)

    def reset_session(self) -> None:
        """Reinicia contadores, cola de comandos y rota a un nuevo CSV de sesión limpia."""
        while not self._cmd_queue.empty():
            try:
                self._cmd_queue.get_nowait()
            except queue.Empty:
                break
        self.monitor.reset()
        self.storage.start_new_session_csv()
        with self._lock:
            self._last_rx_mono = 0.0
            self._last_packet_rx_mono = 0.0
            self._active_command = None
            self._last_state = "LaunchPad"

    def start_simulation(self) -> None:
        """Inicia la recepción simulada en tiempo real a 1 Hz en un hilo de fondo."""
        self.stop()
        self.reset_session()
        self._simulator.reset()
        with self._lock:
            self._mode = "SIMULATION"
        self._running.set()
        self.storage.log_event("SIM_START", "Modo Simulación iniciado a 1 Hz")
        self._worker_thread = threading.Thread(
            target=self._simulation_loop, name="GCS-SimWorker", daemon=True
        )
        self._worker_thread.start()

    def start_serial(self, port: str, baudrate: int = DEFAULT_BAUDRATE) -> None:
        """Abre el puerto serie real e inicia el bucle no bloqueante de lectura/escritura."""
        self.stop()
        self.reset_session()
        with self._lock:
            self._mode = "SERIAL"
            self._port_name = port
            self._baudrate = baudrate
        self._running.set()
        self.storage.log_event(
            "SERIAL_CONNECT", f"Conectando a {port} @ {baudrate} bps"
        )
        self._worker_thread = threading.Thread(
            target=self._serial_loop, name="GCS-SerialWorker", daemon=True
        )
        self._worker_thread.start()

    def stop(self) -> None:
        """Detiene de forma limpia cualquier hilo activo (serie o simulación)."""
        self._running.clear()
        if (
            self._worker_thread is not None
            and self._worker_thread.is_alive()
            and threading.current_thread() is not self._worker_thread
        ):
            self._worker_thread.join(timeout=1.5)
        self._worker_thread = None
        with self._lock:
            self._mode = "IDLE"

    def _simulation_loop(self) -> None:
        next_tick = time.monotonic()
        while self._running.is_set():
            now = time.monotonic()
            # 1. Despachar comandos encolados dentro de la ventana silenciosa
            if not self._cmd_queue.empty() and self.can_transmit_command_now(now):
                try:
                    record = self._cmd_queue.get_nowait()
                    record.status = "ENVIADO"
                    record.sent_at_mono = time.monotonic()
                    with self._lock:
                        self._active_command = record
                    self.storage.log_event(
                        "CMD_SENT", f"[SIM] TX -> {record.command_text}"
                    )
                    if self.on_command_status:
                        self.on_command_status(record)
                    responses = self._simulator.handle_command(record.command_text)
                    for resp in responses:
                        self.process_incoming_line(resp)
                except queue.Empty:
                    pass

            self.check_command_timeout(now)

            # 2. Emitir paquete de 1 Hz
            if now >= next_tick:
                events, csv_line = self._simulator.step()
                for ev in events:
                    self.process_incoming_line(ev)
                self.process_incoming_line(csv_line)
                next_tick = now + 1.0

            time.sleep(0.03)

    def _serial_loop(self) -> None:
        try:
            import serial  # type: ignore
        except ImportError as exc:
            msg = f"pyserial no está instalado: {exc}"
            self.storage.log_event("SERIAL_ERROR", msg)
            if self.on_event:
                self.on_event("SERIAL_ERROR", msg)
            with self._lock:
                self._mode = "IDLE"
            self._running.clear()
            return

        try:
            with serial.Serial(
                self._port_name, self._baudrate, timeout=SERIAL_TIMEOUT_S
            ) as ser:
                self.storage.log_event(
                    "SERIAL_OPEN",
                    f"Puerto serie abierto exitosamente: {self._port_name}",
                )
                while self._running.is_set():
                    raw_bytes = ser.readline()
                    if raw_bytes:
                        line = raw_bytes.decode("utf-8", errors="replace")
                        self.process_incoming_line(line)

                    now = time.monotonic()
                    if not self._cmd_queue.empty() and self.can_transmit_command_now(now):
                        try:
                            record = self._cmd_queue.get_nowait()
                            payload = (record.command_text + "\n").encode("ascii", errors="ignore")
                            ser.write(payload)
                            ser.flush()
                            record.status = "ENVIADO"
                            record.sent_at_mono = time.monotonic()
                            with self._lock:
                                self._active_command = record
                            self.storage.log_event(
                                "CMD_SENT",
                                f"[UART] TX -> {record.command_text}",
                            )
                            if self.on_command_status:
                                self.on_command_status(record)
                        except queue.Empty:
                            pass

                    self.check_command_timeout(now)
        except Exception as exc:
            msg = f"Error en puerto serie {self._port_name}: {exc}"
            self.storage.log_event("SERIAL_ERROR", msg)
            if self.on_event:
                self.on_event("SERIAL_ERROR", msg)
        finally:
            with self._lock:
                self._mode = "IDLE"
            self._running.clear()

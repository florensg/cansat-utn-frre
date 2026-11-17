"""Interfaz Gráfica Reglamentaria CONAE (PySide6 + PyQtGraph) para la Estación Terrena (GCS).

Cumple estrictamente con:
- Requisito G13: Prohibido usar pestañas (QTabWidget). Todos los datos, gráficos, alarmas y
  telecomandos se visualizan simultáneamente en una única pantalla.
- Requisito G12: Diseño para luz solar directa: tema claro (#F8FAFC / #FFFFFF), texto oscuro
  (#0F172A), tipografía >= 14 pt en todas las lecturas y botones, ejes en negrita y trazos de
  gráficos gruesos (>= 2.8 px), e indicadores con texto + símbolo + borde.
- Requisito G5, G6, G7, G8, G14, G15: Telemetría en texto en vivo, unidades SI explícitas,
  4 gráficos simultáneos en tiempo real, contadores PACKET_COUNT vs RX_COUNT y telecomandos
  con doble confirmación de seguridad para MEC.
"""

from __future__ import annotations

import math
from collections import deque
from datetime import datetime, timezone
from pathlib import Path
from typing import Deque, Dict, Optional

from .config import ALARM_THRESHOLDS, DEFAULT_BAUDRATE, GUI_THEME, TEAM_ID
from .decoder import TelemetryPacket
from .packet_monitor import PacketStats
from .serial_manager import (
    CommandStatusRecord,
    TelemetrySessionController,
    list_available_serial_ports,
)

try:
    import os
    import stat
    import PySide6  # type: ignore

    _pyside_pkg_dir = Path(PySide6.__file__).resolve().parent
    _qt_plugins_dir = _pyside_pkg_dir / "Qt" / "plugins"
    if not _qt_plugins_dir.is_dir():
        _qt_plugins_dir = _pyside_pkg_dir / "plugins"
    if _qt_plugins_dir.is_dir():
        # En macOS, si el proyecto está en Desktop con iCloud Drive, el sistema marca
        # los archivos dentro de `.venv` con el flag UF_HIDDEN y Qt QFactoryLoader
        # ignora los plugins `.dylib` ocultos. Limpiamos UF_HIDDEN automáticamente:
        if hasattr(os, "chflags") and hasattr(stat, "UF_HIDDEN"):
            for _root, _dirs, _files in os.walk(_qt_plugins_dir):
                _root_p = Path(_root)
                try:
                    _st = _root_p.stat()
                    if _st.st_flags & stat.UF_HIDDEN:
                        os.chflags(_root_p, _st.st_flags & ~stat.UF_HIDDEN)
                except OSError:
                    pass
                for _fn in _files:
                    _fp = _root_p / _fn
                    try:
                        _fst = _fp.stat()
                        if _fst.st_flags & stat.UF_HIDDEN:
                            os.chflags(_fp, _fst.st_flags & ~stat.UF_HIDDEN)
                    except OSError:
                        pass

        os.environ["QT_PLUGIN_PATH"] = str(_qt_plugins_dir)
        _platforms_dir = _qt_plugins_dir / "platforms"
        if _platforms_dir.is_dir():
            os.environ["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(_platforms_dir)

    from PySide6.QtCore import QCoreApplication, Qt, QTimer, Signal, QObject

    if _qt_plugins_dir.is_dir():
        QCoreApplication.setLibraryPaths([str(_qt_plugins_dir)])

    from PySide6.QtGui import QFont
    from PySide6.QtWidgets import (
        QApplication,
        QComboBox,
        QFileDialog,
        QFrame,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPlainTextEdit,
        QPushButton,
        QSizePolicy,
        QVBoxLayout,
        QWidget,
    )
    import pyqtgraph as pg  # type: ignore

    PYSIDE6_AVAILABLE = True
except ImportError:
    PYSIDE6_AVAILABLE = False


SUNLIGHT_STYLESHEET = f"""
QMainWindow, QWidget {{
    background-color: {GUI_THEME.bg_main};
    color: {GUI_THEME.text_primary};
    font-family: "Helvetica Neue", "Segoe UI", Arial, sans-serif;
    font-size: {GUI_THEME.font_size_base_pt}pt;
}}
QGroupBox {{
    background-color: {GUI_THEME.bg_panel};
    border: 2px solid {GUI_THEME.border_normal};
    border-radius: 6px;
    margin-top: 14px;
    padding-top: 10px;
    font-size: {GUI_THEME.font_size_title_pt}pt;
    font-weight: bold;
    color: {GUI_THEME.text_primary};
}}
QGroupBox::title {{
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 6px;
    background-color: {GUI_THEME.bg_panel};
}}
QLabel {{
    font-size: {GUI_THEME.font_size_base_pt}pt;
    color: {GUI_THEME.text_primary};
}}
QPushButton {{
    background-color: {GUI_THEME.bg_header};
    color: {GUI_THEME.text_primary};
    border: 2px solid {GUI_THEME.border_strong};
    border-radius: 5px;
    padding: 6px 12px;
    font-size: {GUI_THEME.font_size_base_pt}pt;
    font-weight: bold;
}}
QPushButton:hover {{
    background-color: #CBD5E1;
}}
QPushButton:disabled {{
    background-color: #E2E8F0;
    color: #94A3B8;
    border-color: #94A3B8;
}}
QComboBox, QLineEdit {{
    background-color: #FFFFFF;
    color: {GUI_THEME.text_primary};
    border: 2px solid {GUI_THEME.border_strong};
    border-radius: 4px;
    padding: 4px 8px;
    font-size: {GUI_THEME.font_size_base_pt}pt;
    font-weight: bold;
}}
QPlainTextEdit {{
    background-color: #FFFFFF;
    color: {GUI_THEME.text_primary};
    border: 2px solid {GUI_THEME.border_strong};
    font-family: "Menlo", "Consolas", "Courier New", monospace;
    font-size: {GUI_THEME.font_size_base_pt}pt;
    font-weight: bold;
}}
"""


if PYSIDE6_AVAILABLE:

    class _UiBridge(QObject):
        """Puente de señales Qt para transferir eventos de hilos de fondo al hilo GUI."""

        packet_received = Signal(object, object)
        raw_line_received = Signal(str)
        command_status_changed = Signal(object)
        event_logged = Signal(str, str)

    class CanSatMainWindow(QMainWindow):
        """Ventana única sin pestañas (G13) y de alto contraste solar (G12) para la GCS."""

        def __init__(
            self,
            controller: TelemetrySessionController,
            max_history_points: int = 300,
        ) -> None:
            super().__init__()
            self.controller = controller
            self.max_history_points = max_history_points

            # Buffers circulares para las 4 gráficas en tiempo real
            self._t_buf: Deque[float] = deque(maxlen=max_history_points)
            self._alt_baro_buf: Deque[float] = deque(maxlen=max_history_points)
            self._alt_gps_buf: Deque[float] = deque(maxlen=max_history_points)
            self._batt_v_buf: Deque[float] = deque(maxlen=max_history_points)
            self._curr_a_buf: Deque[float] = deque(maxlen=max_history_points)
            self._acc_x_buf: Deque[float] = deque(maxlen=max_history_points)
            self._acc_y_buf: Deque[float] = deque(maxlen=max_history_points)
            self._acc_z_buf: Deque[float] = deque(maxlen=max_history_points)
            self._gyr_x_buf: Deque[float] = deque(maxlen=max_history_points)
            self._gyr_y_buf: Deque[float] = deque(maxlen=max_history_points)
            self._gyr_z_buf: Deque[float] = deque(maxlen=max_history_points)

            self._last_packet: Optional[TelemetryPacket] = None

            self.setWindowTitle(
                f"Estación Terrena CanSat UTN FRRe — Equipo #{TEAM_ID} (CONAE 2026-2027)"
            )
            self.resize(1680, 980)
            self.setStyleSheet(SUNLIGHT_STYLESHEET)

            # Configurar PyQtGraph en modo claro de alto contraste (G12)
            pg.setConfigOption("background", GUI_THEME.bg_panel)
            pg.setConfigOption("foreground", GUI_THEME.text_primary)
            pg.setConfigOptions(antialias=True)

            # Conectar callbacks del controlador mediante señales thread-safe
            self._bridge = _UiBridge()
            self._bridge.packet_received.connect(self._on_packet_ui)
            self._bridge.raw_line_received.connect(self._on_raw_line_ui)
            self._bridge.command_status_changed.connect(self._on_command_status_ui)
            self._bridge.event_logged.connect(self._on_event_ui)

            self.controller.on_packet = self._bridge.packet_received.emit
            self.controller.on_raw_line = self._bridge.raw_line_received.emit
            self.controller.on_command_status = self._bridge.command_status_changed.emit
            self.controller.on_event = self._bridge.event_logged.emit

            self._build_single_screen_layout()
            self.refresh_serial_ports()

            # Timer de supervisión a 2 Hz para vigilar enlace RF, reloj y estado USB
            self._watchdog_timer = QTimer(self)
            self._watchdog_timer.timeout.connect(self._update_supervision_indicators)
            self._watchdog_timer.start(500)

        def _build_single_screen_layout(self) -> None:
            """Construye la pantalla única (PROHIBIDO usar QTabWidget según G13)."""
            central = QWidget(self)
            self.setCentralWidget(central)
            root_layout = QVBoxLayout(central)
            root_layout.setContentsMargins(8, 8, 8, 8)
            root_layout.setSpacing(6)

            # 1. FILA SUPERIOR: CABECERA DE CONEXIÓN, ESTADO FSM Y TIEMPO DE MISIÓN
            root_layout.addWidget(self._create_header_panel())

            # 2. FILA DE ALARMAS VISUALES Y ESTADO DE RESPALDO USB (G2, G12)
            root_layout.addWidget(self._create_alarms_and_storage_bar())

            # 3. CUERPO CENTRAL EN GRILLA ÚNICA:
            #    Izquierda: Contadores + Sensores SI + GPS + Actitud/Secundaria + Telecomandos
            #    Derecha: 4 Gráficos simultáneos en tiempo real (G7)
            body_grid = QGridLayout()
            body_grid.setSpacing(8)

            left_column = QVBoxLayout()
            left_column.setSpacing(6)
            left_column.addWidget(self._create_counters_panel())
            left_column.addWidget(self._create_telemetry_si_panel())
            left_column.addWidget(self._create_gps_and_secondary_panel())
            left_column.addWidget(self._create_telecommands_panel())

            right_plots_grid = QGridLayout()
            right_plots_grid.setSpacing(6)
            self._build_four_realtime_plots(right_plots_grid)

            body_grid.addLayout(left_column, 0, 0)
            body_grid.addLayout(right_plots_grid, 0, 1)
            body_grid.setColumnStretch(0, 42)
            body_grid.setColumnStretch(1, 58)
            root_layout.addLayout(body_grid, stretch=5)

            # 4. FILA INFERIOR: TELEMETRÍA EN TEXTO EN TIEMPO REAL (Requisito G5)
            root_layout.addWidget(self._create_raw_text_console_panel(), stretch=1)

        def _create_header_panel(self) -> QGroupBox:
            box = QGroupBox("CONTROL DE ENLACE Y ESTADO DE MISIÓN")
            layout = QHBoxLayout(box)
            layout.setSpacing(10)

            layout.addWidget(QLabel("Puerto:"))
            self.port_combo = QComboBox()
            self.port_combo.setMinimumWidth(220)
            layout.addWidget(self.port_combo)

            self.btn_refresh_ports = QPushButton("↻ Puertos")
            self.btn_refresh_ports.clicked.connect(self.refresh_serial_ports)
            layout.addWidget(self.btn_refresh_ports)

            self.btn_connect_serial = QPushButton("▶ Conectar Serie")
            self.btn_connect_serial.clicked.connect(self._toggle_serial_connection)
            layout.addWidget(self.btn_connect_serial)

            self.btn_simulate = QPushButton("⚙ Modo Simulación (1 Hz)")
            self.btn_simulate.clicked.connect(self._toggle_simulation)
            layout.addWidget(self.btn_simulate)

            layout.addSpacing(12)

            self.lbl_state_badge = QLabel("ESTADO: LaunchPad")
            self.lbl_state_badge.setAlignment(Qt.AlignmentFlag.AlignCenter)
            self.lbl_state_badge.setStyleSheet(
                f"background-color: #DBEAFE; color: {GUI_THEME.accent_blue}; "
                f"border: 3px solid {GUI_THEME.accent_blue}; border-radius: 6px; "
                f"padding: 6px 14px; font-size: {GUI_THEME.font_size_hero_pt}pt; font-weight: bold;"
            )
            layout.addWidget(self.lbl_state_badge)

            self.lbl_mission_time = QLabel("T. MISIÓN: 00:00:00 (0 s)")
            self.lbl_mission_time.setStyleSheet(
                f"background-color: #FFFFFF; border: 2px solid {GUI_THEME.border_strong}; "
                f"border-radius: 6px; padding: 6px 12px; font-size: {GUI_THEME.font_size_title_pt}pt; font-weight: bold;"
            )
            layout.addWidget(self.lbl_mission_time)

            self.lbl_utc_clock = QLabel("UTC: 00:00:00 [SINCR: NO]")
            self.lbl_utc_clock.setStyleSheet(
                f"background-color: #FFFFFF; border: 2px solid {GUI_THEME.border_normal}; "
                f"border-radius: 6px; padding: 6px 12px; font-size: {GUI_THEME.font_size_base_pt}pt; font-weight: bold;"
            )
            layout.addWidget(self.lbl_utc_clock)

            return box

        def _create_alarms_and_storage_bar(self) -> QFrame:
            frame = QFrame()
            frame.setStyleSheet(
                f"background-color: {GUI_THEME.bg_panel}; border: 2px solid {GUI_THEME.border_strong}; border-radius: 6px;"
            )
            layout = QHBoxLayout(frame)
            layout.setContentsMargins(8, 4, 8, 4)
            layout.setSpacing(10)

            self.lbl_alarm_link = QLabel("[● ENLACE RF: EN ESPERA]")
            self.lbl_alarm_batt = QLabel("[● BATERÍA: SIN DATOS]")
            self.lbl_alarm_gps = QLabel("[● GPS: SIN FIX]")
            self.lbl_alarm_p2 = QLabel("[● 2.º PARACAÍDAS: CERRADO (0°)]")

            for badge in (
                self.lbl_alarm_link,
                self.lbl_alarm_batt,
                self.lbl_alarm_gps,
                self.lbl_alarm_p2,
            ):
                badge.setAlignment(Qt.AlignmentFlag.AlignCenter)
                badge.setStyleSheet(
                    "background-color: #F1F5F9; color: #0F172A; border: 2px solid #475569; "
                    "border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
                layout.addWidget(badge)

            self.lbl_storage_status = QLabel(
                f"CSV: {self.controller.storage.csv_filename} | {self.controller.storage.backup_status_summary}"
            )
            self.lbl_storage_status.setStyleSheet("font-weight: bold; color: #0F172A;")
            layout.addWidget(self.lbl_storage_status, stretch=1)

            self.btn_select_usb = QPushButton("💾 Carpeta/USB Respaldo")
            self.btn_select_usb.clicked.connect(self._choose_backup_directory)
            layout.addWidget(self.btn_select_usb)

            return frame

        def _create_counters_panel(self) -> QGroupBox:
            box = QGroupBox("CONTADORES DE PAQUETES (REQUISITOS G8 y G14)")
            grid = QGridLayout(box)
            grid.setSpacing(8)

            self.lbl_pkt_tx = QLabel("PACKET_COUNT (TX OBC): 0")
            self.lbl_pkt_rx = QLabel("RX_COUNT (Recibidos GCS): 0")
            self.lbl_pkt_lost = QLabel("PERDIDOS: 0 (0.0 %)")
            self.lbl_pkt_err = QLabel("CORRUPTOS: 0")

            for lbl in (
                self.lbl_pkt_tx,
                self.lbl_pkt_rx,
                self.lbl_pkt_lost,
                self.lbl_pkt_err,
            ):
                lbl.setStyleSheet(
                    "background-color: #F8FAFC; border: 2px solid #334155; "
                    "border-radius: 4px; padding: 5px 8px; font-weight: bold;"
                )

            grid.addWidget(self.lbl_pkt_tx, 0, 0)
            grid.addWidget(self.lbl_pkt_rx, 0, 1)
            grid.addWidget(self.lbl_pkt_lost, 1, 0)
            grid.addWidget(self.lbl_pkt_err, 1, 1)
            return box

        def _create_telemetry_si_panel(self) -> QGroupBox:
            box = QGroupBox("SENSORES EN SISTEMA INTERNACIONAL (SI — REQUISITOS G5, G6, G8)")
            grid = QGridLayout(box)
            grid.setSpacing(6)

            self._val_labels: Dict[str, QLabel] = {}
            fields = [
                ("alt", "Altitud Baro:", "0.00 m"),
                ("max_alt", "Altitud Máx:", "0.00 m"),
                ("press", "Presión Atm:", "101325 Pa (101.33 kPa)"),
                ("temp", "Temperatura:", "0.00 °C"),
                ("batt", "Tensión Batería:", "0.00 V"),
                ("curr", "Corriente Bat:", "0.000 A (0.0 mA)"),
                ("acc", "Aceleración (SI):", "X: 0.00 | Y: 0.00 | Z: 9.81 m/s²"),
                ("gyr", "Giroscopio (SI):", "X: 0.00 | Y: 0.00 | Z: 0.00 rad/s"),
            ]

            for idx, (key, title, default_val) in enumerate(fields):
                row = idx // 2
                col = (idx % 2) * 2
                lbl_t = QLabel(title)
                lbl_t.setStyleSheet("font-weight: bold; color: #334155;")
                lbl_v = QLabel(default_val)
                lbl_v.setStyleSheet(
                    "background-color: #F8FAFC; border: 1px solid #64748B; "
                    "border-radius: 4px; padding: 4px 6px; font-weight: bold; color: #0F172A;"
                )
                grid.addWidget(lbl_t, row, col)
                grid.addWidget(lbl_v, row, col + 1)
                self._val_labels[key] = lbl_v

            return box

        def _create_gps_and_secondary_panel(self) -> QGroupBox:
            box = QGroupBox("POSICIÓN GPS Y MISIÓN SECUNDARIA (ACTITUD + TERRENO NIR)")
            grid = QGridLayout(box)
            grid.setSpacing(6)

            sec_fields = [
                ("gps_pos", "Posición GPS:", "Lat: 0.000000° | Lon: 0.000000°"),
                ("gps_alt_sats", "Alt GPS / Sats:", "0.00 m | Sats: 0 [NO FIX]"),
                ("mag", "Magnetómetro:", "X: 0.0 | Y: 0.0 | Z: 0.0 µT"),
                ("solar", "Fotodiodos / Sol:", "U4..U7: 0,0,0,0 | Az: 0.0°"),
                ("attitude", "Actitud (RPI):", "Yaw: 0.0° | Sol: 0.0°"),
                ("nir_apt", "Terreno / NIR:", "NIR: 0 fotos | Zona: PENDIENTE (0.00)"),
            ]

            for idx, (key, title, default_val) in enumerate(sec_fields):
                row = idx // 2
                col = (idx % 2) * 2
                lbl_t = QLabel(title)
                lbl_t.setStyleSheet("font-weight: bold; color: #334155;")
                lbl_v = QLabel(default_val)
                lbl_v.setStyleSheet(
                    "background-color: #F8FAFC; border: 1px solid #64748B; "
                    "border-radius: 4px; padding: 4px 6px; font-weight: bold; color: #0F172A;"
                )
                grid.addWidget(lbl_t, row, col)
                grid.addWidget(lbl_v, row, col + 1)
                self._val_labels[key] = lbl_v

            return box

        def _create_telecommands_panel(self) -> QGroupBox:
            box = QGroupBox("TELECOMANDOS CON VENTANA ANTI-COLISIÓN (REQUISITOS G1, G15, F3, X6)")
            vlayout = QVBoxLayout(box)
            vlayout.setSpacing(6)

            btn_row = QHBoxLayout()
            self.btn_cmd_cal = QPushButton("🎯 CAL (Altitud 0 m)")
            self.btn_cmd_cal.clicked.connect(lambda: self._send_command("CAL"))
            btn_row.addWidget(self.btn_cmd_cal)

            self.btn_cmd_cx = QPushButton("📊 CX (Batería y Solar)")
            self.btn_cmd_cx.clicked.connect(lambda: self._send_command("CX"))
            btn_row.addWidget(self.btn_cmd_cx)

            self.edit_st_time = QLineEdit()
            self.edit_st_time.setPlaceholderText("HH:MM:SS")
            self.edit_st_time.setToolTip(
                "Formato HH:MM:SS UTC. Si se deja vacío, envía automáticamente la hora UTC actual del sistema."
            )
            self.edit_st_time.setMinimumWidth(120)
            self.edit_st_time.setMaximumWidth(170)
            btn_row.addWidget(self.edit_st_time)

            self.btn_cmd_st = QPushButton("🕒 ST (Sincronizar UTC)")
            self.btn_cmd_st.clicked.connect(self._send_st_command)
            btn_row.addWidget(self.btn_cmd_st)

            self.btn_cmd_mec = QPushButton("⚠ MEC (2.º Paracaídas)")
            self.btn_cmd_mec.setStyleSheet(
                f"background-color: #FEE2E2; color: {GUI_THEME.accent_red}; "
                f"border: 3px solid {GUI_THEME.accent_red}; font-weight: bold;"
            )
            self.btn_cmd_mec.clicked.connect(self._confirm_and_send_mec)
            btn_row.addWidget(self.btn_cmd_mec)

            vlayout.addLayout(btn_row)

            self.lbl_cmd_status = QLabel(
                "ESTADO DE COMANDO: NINGUNO ENVIADO | ECO OBC: NONE"
            )
            self.lbl_cmd_status.setStyleSheet(
                "background-color: #F1F5F9; border: 2px solid #0F172A; "
                "border-radius: 4px; padding: 6px 10px; font-weight: bold;"
            )
            vlayout.addWidget(self.lbl_cmd_status)
            return box

        def _build_four_realtime_plots(self, grid: QGridLayout) -> None:
            """Crea las 4 gráficas simultáneas reglamentarias (G7, G12)."""
            axis_font = QFont("Helvetica Neue", GUI_THEME.font_size_base_pt, QFont.Weight.Bold)
            pen_w = GUI_THEME.plot_line_width

            def _style_plot(pw: pg.PlotWidget, title: str, y_label: str, y_units: str) -> None:
                pw.setTitle(
                    f"<span style='color:{GUI_THEME.text_primary}; font-size:{GUI_THEME.font_size_base_pt}pt; font-weight:bold'>{title}</span>"
                )
                pw.setLabel(
                    "left",
                    f"<span style='color:{GUI_THEME.text_primary}; font-size:{GUI_THEME.font_size_base_pt}pt; font-weight:bold'>{y_label} ({y_units})</span>",
                )
                pw.setLabel(
                    "bottom",
                    f"<span style='color:{GUI_THEME.text_primary}; font-size:{GUI_THEME.font_size_base_pt}pt; font-weight:bold'>Tiempo de Misión (s)</span>",
                )
                pw.showGrid(x=True, y=True, alpha=0.35)
                pw.addLegend(offset=(10, 10))
                for ax_name in ("left", "bottom"):
                    ax = pw.getAxis(ax_name)
                    ax.setPen(pg.mkPen(color=GUI_THEME.text_primary, width=2.2))
                    ax.setTextPen(pg.mkPen(color=GUI_THEME.text_primary))
                    ax.setStyle(tickFont=axis_font)

            # 1. Altitud Barométrica y GPS vs. Tiempo
            self.plot_alt = pg.PlotWidget()
            _style_plot(self.plot_alt, "1. Altitud Barométrica y GPS", "Altitud", "m")
            self.curve_alt_baro = self.plot_alt.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_blue, width=pen_w),
                name="Altitud Baro (m)",
            )
            self.curve_alt_gps = self.plot_alt.plot(
                pen=pg.mkPen(
                    color=GUI_THEME.accent_green,
                    width=pen_w,
                    style=Qt.PenStyle.DashLine,
                ),
                name="Altitud GPS (m)",
            )

            # 2. Tensión (V) y Corriente (A) de Batería vs. Tiempo
            self.plot_power = pg.PlotWidget()
            _style_plot(
                self.plot_power,
                "2. Tensión (V) y Corriente (A) de Batería",
                "Magnitud Eléctrica",
                "V / A",
            )
            self.curve_batt_v = self.plot_power.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_green, width=pen_w),
                name="Tensión Batería (V)",
            )
            self.curve_curr_a = self.plot_power.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_amber, width=pen_w),
                name="Corriente Batería (A)",
            )

            # 3. Acelerómetro 3 ejes en m/s² (SI)
            self.plot_acc = pg.PlotWidget()
            _style_plot(
                self.plot_acc,
                "3. Acelerómetro 3 Ejes (SI)",
                "Aceleración",
                "m/s²",
            )
            self.curve_acc_x = self.plot_acc.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_red, width=pen_w),
                name="Acc X (m/s²)",
            )
            self.curve_acc_y = self.plot_acc.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_green, width=pen_w),
                name="Acc Y (m/s²)",
            )
            self.curve_acc_z = self.plot_acc.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_blue, width=pen_w),
                name="Acc Z (m/s²)",
            )

            # 4. Giroscopio 3 ejes en rad/s (SI) con escala mínima [-0.5, 0.5] rad/s
            self.plot_gyr = pg.PlotWidget()
            _style_plot(
                self.plot_gyr,
                "4. Tasas de Rotación / Giroscopio 3 Ejes (SI)",
                "Vel. Angular",
                "rad/s",
            )
            self.plot_gyr.getViewBox().setLimits(minYRange=1.0)
            self.plot_gyr.setYRange(-0.5, 0.5, padding=0.05)
            self.curve_gyr_x = self.plot_gyr.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_red, width=pen_w),
                name="Gyr X (rad/s)",
            )
            self.curve_gyr_y = self.plot_gyr.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_green, width=pen_w),
                name="Gyr Y (rad/s)",
            )
            self.curve_gyr_z = self.plot_gyr.plot(
                pen=pg.mkPen(color=GUI_THEME.accent_purple, width=pen_w),
                name="Gyr Z (rad/s)",
            )

            grid.addWidget(self.plot_alt, 0, 0)
            grid.addWidget(self.plot_power, 0, 1)
            grid.addWidget(self.plot_acc, 1, 0)
            grid.addWidget(self.plot_gyr, 1, 1)

        def _create_raw_text_console_panel(self) -> QGroupBox:
            box = QGroupBox(
                "FLUJO DE TELEMETRÍA EN TEXTO Y EVENTOS EN TIEMPO REAL (REQUISITO G5)"
            )
            layout = QVBoxLayout(box)
            self.raw_console = QPlainTextEdit()
            self.raw_console.setReadOnly(True)
            self.raw_console.setMaximumBlockCount(250)
            self.raw_console.setMinimumHeight(105)
            layout.addWidget(self.raw_console)
            return box

        # ------------------------------------------------------------------
        # Acciones del Operador
        # ------------------------------------------------------------------
        def refresh_serial_ports(self) -> None:
            ports = list_available_serial_ports()
            self.port_combo.clear()
            if not ports:
                self.port_combo.addItem("Sin puertos detectados", "")
            else:
                for p in ports:
                    self.port_combo.addItem(p.display_label, p.device)

        def _reset_plots_and_counters_ui(self) -> None:
            """Limpia los buffers de los 4 gráficos y reinicia contadores e indicadores visuales."""
            for buf in (
                self._t_buf,
                self._alt_baro_buf,
                self._alt_gps_buf,
                self._batt_v_buf,
                self._curr_a_buf,
                self._acc_x_buf,
                self._acc_y_buf,
                self._acc_z_buf,
                self._gyr_x_buf,
                self._gyr_y_buf,
                self._gyr_z_buf,
            ):
                buf.clear()

            for curve in (
                self.curve_alt_baro,
                self.curve_alt_gps,
                self.curve_batt_v,
                self.curve_curr_a,
                self.curve_acc_x,
                self.curve_acc_y,
                self.curve_acc_z,
                self.curve_gyr_x,
                self.curve_gyr_y,
                self.curve_gyr_z,
            ):
                curve.setData([], [])

            self.plot_gyr.setYRange(-0.5, 0.5, padding=0.05)
            self._last_packet = None
            self.lbl_pkt_tx.setText("PACKET_COUNT (TX OBC): 0")
            self.lbl_pkt_rx.setText("RX_COUNT (Recibidos GCS): 0")
            self.lbl_pkt_lost.setText("PERDIDOS: 0 (0.0 %)")
            self.lbl_pkt_err.setText("CORRUPTOS: 0")
            self.lbl_cmd_status.setText(
                "ESTADO DE COMANDO: NINGUNO ENVIADO | ECO OBC: NONE"
            )
            self.lbl_cmd_status.setStyleSheet(
                "background-color: #F1F5F9; border: 2px solid #0F172A; "
                "border-radius: 4px; padding: 6px 10px; font-weight: bold;"
            )
            self._update_supervision_indicators()

        def _toggle_serial_connection(self) -> None:
            if self.controller.mode == "SERIAL":
                self.controller.stop()
                self.btn_connect_serial.setText("▶ Conectar Serie")
                self.btn_simulate.setEnabled(True)
                return

            port_dev = self.port_combo.currentData()
            if not port_dev:
                port_dev = self.port_combo.currentText().split("—")[0].strip()
            if not port_dev or "Sin puertos" in port_dev:
                QMessageBox.warning(
                    self,
                    "Puerto Serie",
                    "Seleccione un puerto serie válido o inicie el Modo Simulación.",
                )
                return

            self._reset_plots_and_counters_ui()
            self.controller.start_serial(port_dev, DEFAULT_BAUDRATE)
            self.btn_connect_serial.setText("⏹ Desconectar Serie")
            self.btn_simulate.setEnabled(False)

        def _toggle_simulation(self) -> None:
            if self.controller.mode == "SIMULATION":
                self.controller.stop()
                self.btn_simulate.setText("⚙ Modo Simulación (1 Hz)")
                self.btn_connect_serial.setEnabled(True)
            else:
                self._reset_plots_and_counters_ui()
                self.controller.start_simulation()
                self.btn_simulate.setText("⏹ Detener Simulación")
                self.btn_connect_serial.setEnabled(False)

        def _choose_backup_directory(self) -> None:
            folder = QFileDialog.getExistingDirectory(
                self,
                "Seleccionar Unidad USB o Carpeta de Respaldo (Requisito G2)",
                str(Path.home()),
            )
            if folder:
                self.controller.storage.set_backup_dir(folder)
                self._update_supervision_indicators()

        def _send_command(self, verb: str, arg: Optional[str] = None, allow_override: bool = False) -> None:
            try:
                self.controller.queue_command(
                    verb, argument=arg, allow_mec_override=allow_override
                )
            except Exception as exc:
                QMessageBox.critical(
                    self, "Bloqueo de Telecomando", str(exc)
                )

        def _send_st_command(self) -> None:
            user_time = self.edit_st_time.text().strip()
            if not user_time:
                user_time = datetime.now(timezone.utc).strftime("%H:%M:%S")
            self._send_command("ST", arg=user_time)

        def _confirm_and_send_mec(self) -> None:
            state = self.controller.current_flight_state
            if state == "Ascent":
                QMessageBox.critical(
                    self,
                    "MEC Bloqueado en Ascenso",
                    "Por seguridad de misión, la liberación manual del mecanismo (MEC) "
                    "está bloqueada mientras el CanSat se encuentra en estado Ascent.",
                )
                return

            ans1 = QMessageBox.question(
                self,
                "Confirmación 1/2 — Accionar Mecanismo (MEC)",
                f"Estado actual: {state}.\n\n"
                "¿Está seguro de que desea enviar el telecomando MEC para girar el servo SG90 a 90°?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if ans1 != QMessageBox.StandardButton.Yes:
                return

            ans2 = QMessageBox.warning(
                self,
                "Confirmación Final 2/2 — Liberación de 2.º Paracaídas",
                "CONFIRMACIÓN DEFINITIVA:\n"
                "Esta acción liberará físicamente el segundo paracaídas. ¿Confirmar envío de MEC?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if ans2 == QMessageBox.StandardButton.Yes:
                self._send_command("MEC")

        # ------------------------------------------------------------------
        # Actualización en Tiempo Real desde el Hilo Receptor
        # ------------------------------------------------------------------
        def _on_raw_line_ui(self, line: str) -> None:
            self.raw_console.appendPlainText(line)

        def _on_event_ui(self, category: str, message: str) -> None:
            self.raw_console.appendPlainText(f"[{category}] {message}")

        def _on_command_status_ui(self, record: CommandStatusRecord) -> None:
            if record.status == "CONFIRMADO":
                bg, fg, icon = "#DCFCE7", GUI_THEME.accent_green, "✔"
                echo = (
                    record.confirmed_echo
                    if record.confirmed_echo and record.confirmed_echo != "NONE"
                    else f"CMD:{record.verb}"
                )
            elif record.status == "SIN CONFIRMACIÓN":
                bg, fg, icon = "#FEE2E2", GUI_THEME.accent_red, "✖"
                echo = (
                    f"CMD:{self._last_packet.optional.cmd_echo}"
                    if (
                        self._last_packet is not None
                        and self._last_packet.optional.cmd_echo != "NONE"
                    )
                    else "NONE"
                )
            else:
                bg, fg, icon = "#FEF3C7", GUI_THEME.accent_amber, "⏳"
                echo = (
                    f"CMD:{self._last_packet.optional.cmd_echo}"
                    if (
                        self._last_packet is not None
                        and self._last_packet.optional.cmd_echo != "NONE"
                    )
                    else "NONE"
                )

            self.lbl_cmd_status.setText(
                f"{icon} COMANDO '{record.command_text}': {record.status} | ECO OBC: {echo}"
            )
            self.lbl_cmd_status.setStyleSheet(
                f"background-color: {bg}; color: {fg}; border: 2px solid {fg}; "
                "border-radius: 4px; padding: 6px 10px; font-weight: bold;"
            )

        def _on_packet_ui(self, pkt: TelemetryPacket, stats: PacketStats) -> None:
            # Si el paquete entrante reinicia la cuenta a PACKET_COUNT == 1 tras un vuelo anterior,
            # limpiar los buffers de los 4 gráficos para no mezclar curvas ni tiempos anteriores
            if (
                self._last_packet is not None
                and self._last_packet.packet_count > 1
                and pkt.packet_count == 1
            ):
                self._reset_plots_and_counters_ui()

            self._last_packet = pkt

            # Sincronizar inmediatamente la etiqueta ECO OBC con el eco del paquete actual
            active_cmd = self.controller.active_command
            current_echo_str = (
                f"CMD:{pkt.optional.cmd_echo}"
                if pkt.optional.cmd_echo and pkt.optional.cmd_echo != "NONE"
                else "NONE"
            )
            if active_cmd is not None:
                if active_cmd.status == "CONFIRMADO":
                    icon = "✔"
                elif active_cmd.status == "SIN CONFIRMACIÓN":
                    icon = "✖"
                else:
                    icon = "⏳"
                self.lbl_cmd_status.setText(
                    f"{icon} COMANDO '{active_cmd.command_text}': {active_cmd.status} | ECO OBC: {current_echo_str}"
                )
            else:
                self.lbl_cmd_status.setText(
                    f"ESTADO DE COMANDO: NINGUNO ENVIADO | ECO OBC: {current_echo_str}"
                )

            # 1. Actualizar Cabecera (Estado, Tiempo de Misión y Hora UTC)
            state_styles = {
                "LaunchPad": ("#DBEAFE", GUI_THEME.accent_blue),
                "Ascent": ("#DCFCE7", GUI_THEME.accent_green),
                "Descent": ("#FEF3C7", GUI_THEME.accent_amber),
                "Landed": ("#F3E8FF", GUI_THEME.accent_purple),
            }
            bg_s, fg_s = state_styles.get(pkt.state, ("#E2E8F0", "#0F172A"))
            self.lbl_state_badge.setText(f"ESTADO: {pkt.state}")
            self.lbl_state_badge.setStyleSheet(
                f"background-color: {bg_s}; color: {fg_s}; border: 3px solid {fg_s}; "
                f"border-radius: 6px; padding: 6px 14px; font-size: {GUI_THEME.font_size_hero_pt}pt; font-weight: bold;"
            )
            self.lbl_mission_time.setText(
                f"T. MISIÓN: {pkt.mission_time_hms} ({pkt.mission_time_s} s)"
            )
            sync_txt = "SÍ" if pkt.rtc_valid else "NO"
            self.lbl_utc_clock.setText(f"UTC: {pkt.rtc_hms} [SINCR: {sync_txt}]")

            # Bloqueo visual del botón MEC durante Ascent
            self.btn_cmd_mec.setEnabled(pkt.state != "Ascent")

            # 2. Actualizar Contadores de Paquetes (G8, G14)
            self.lbl_pkt_tx.setText(f"PACKET_COUNT (TX OBC): {pkt.packet_count}")
            self.lbl_pkt_rx.setText(f"RX_COUNT (Recibidos GCS): {stats.rx_count}")
            self.lbl_pkt_lost.setText(
                f"PERDIDOS: {stats.lost_count} ({stats.packet_loss_percent:.1f} %)"
            )
            self.lbl_pkt_err.setText(f"CORRUPTOS: {stats.malformed_count}")

            # 3. Actualizar Lecturas en Unidades SI (G6)
            self._val_labels["alt"].setText(f"{pkt.alt_m:.2f} m")
            self._val_labels["max_alt"].setText(
                f"{pkt.optional.max_altitude_m:.2f} m"
            )
            self._val_labels["press"].setText(
                f"{pkt.press_pa:.0f} Pa ({pkt.press_kpa:.2f} kPa)"
            )
            self._val_labels["temp"].setText(f"{pkt.temp_c:.2f} °C")
            self._val_labels["batt"].setText(f"{pkt.batt_v:.2f} V")
            self._val_labels["curr"].setText(
                f"{pkt.curr_a:.3f} A ({pkt.curr_ma:.1f} mA)"
            )
            self._val_labels["acc"].setText(
                f"X:{pkt.acc_x_ms2:+.2f} | Y:{pkt.acc_y_ms2:+.2f} | Z:{pkt.acc_z_ms2:+.2f} m/s²"
            )
            self._val_labels["gyr"].setText(
                f"X:{pkt.gyr_x_rads:+.2f} | Y:{pkt.gyr_y_rads:+.2f} | Z:{pkt.gyr_z_rads:+.2f} rad/s"
            )

            # 4. Actualizar GPS y Misión Secundaria
            fix_str = "FIX OK" if pkt.gps_fix else "SIN FIX"
            self._val_labels["gps_pos"].setText(
                f"Lat: {pkt.lat_deg:.6f}° | Lon: {pkt.lon_deg:.6f}°"
            )
            self._val_labels["gps_alt_sats"].setText(
                f"{pkt.gps_alt_m:.2f} m | Sats: {pkt.sats} [{fix_str}]"
            )
            self._val_labels["mag"].setText(
                f"X:{pkt.mag_x_ut:+.1f} | Y:{pkt.mag_y_ut:+.1f} | Z:{pkt.mag_z_ut:+.1f} µT"
            )
            self._val_labels["solar"].setText(
                f"U4..U7: {pkt.opt1_raw},{pkt.opt2_raw},{pkt.opt3_raw},{pkt.opt4_raw} | Az: {pkt.solar_azimuth_deg:.1f}°"
            )
            yaw_val = (
                pkt.optional.rpi_yaw_deg
                if pkt.optional.rpi_yaw_deg is not None
                else (math.degrees(math.atan2(-pkt.mag_y_ut, pkt.mag_x_ut)) % 360.0)
            )
            self._val_labels["attitude"].setText(
                f"Yaw: {yaw_val:.1f}° | Sol: {pkt.solar_azimuth_deg:.1f}°"
            )
            self._val_labels["nir_apt"].setText(
                f"NIR: {pkt.optional.rpi_nir_captures} fotos | "
                f"Zona: {pkt.optional.rpi_landing_apt} ({pkt.optional.rpi_suitability_score:.2f})"
            )

            # 5. Actualizar Alarmas de Batería (1S Li-Ion: <3.60V Advertencia, <3.40V Crítica), GPS y 2.º Paracaídas
            if pkt.batt_v < ALARM_THRESHOLDS.battery_critical_v:
                self.lbl_alarm_batt.setText(
                    f"[🚨 ALARMA CRÍTICA BATERÍA: {pkt.batt_v:.2f} V (< {ALARM_THRESHOLDS.battery_critical_v:.2f} V)]"
                )
                self.lbl_alarm_batt.setStyleSheet(
                    f"background-color: #FEE2E2; color: {GUI_THEME.accent_red}; "
                    f"border: 3px solid {GUI_THEME.accent_red}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
                self._val_labels["batt"].setText(f"🚨 {pkt.batt_v:.2f} V [CRÍTICA]")
                self._val_labels["batt"].setStyleSheet(
                    f"background-color: #FEE2E2; color: {GUI_THEME.accent_red}; "
                    f"border: 2px solid {GUI_THEME.accent_red}; border-radius: 4px; padding: 4px 6px; font-weight: bold;"
                )
                self.lbl_state_badge.setText(f"ESTADO: {pkt.state} [🚨 BAT CRÍTICA]")
                self.lbl_state_badge.setStyleSheet(
                    f"background-color: #FEE2E2; color: {GUI_THEME.accent_red}; border: 3px solid {GUI_THEME.accent_red}; "
                    f"border-radius: 6px; padding: 6px 14px; font-size: {GUI_THEME.font_size_hero_pt}pt; font-weight: bold;"
                )
            elif pkt.batt_v < ALARM_THRESHOLDS.battery_low_v:
                self.lbl_alarm_batt.setText(
                    f"[⚠ ADVERTENCIA BATERÍA: {pkt.batt_v:.2f} V (< {ALARM_THRESHOLDS.battery_low_v:.2f} V)]"
                )
                self.lbl_alarm_batt.setStyleSheet(
                    f"background-color: #FEF3C7; color: {GUI_THEME.accent_amber}; "
                    f"border: 3px solid {GUI_THEME.accent_amber}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
                self._val_labels["batt"].setText(f"⚠ {pkt.batt_v:.2f} V [ADVERTENCIA]")
                self._val_labels["batt"].setStyleSheet(
                    f"background-color: #FEF3C7; color: {GUI_THEME.accent_amber}; "
                    f"border: 2px solid {GUI_THEME.accent_amber}; border-radius: 4px; padding: 4px 6px; font-weight: bold;"
                )
                self.lbl_state_badge.setText(f"ESTADO: {pkt.state} [⚠ BAT BAJA]")
                self.lbl_state_badge.setStyleSheet(
                    f"background-color: #FEF3C7; color: {GUI_THEME.accent_amber}; border: 3px solid {GUI_THEME.accent_amber}; "
                    f"border-radius: 6px; padding: 6px 14px; font-size: {GUI_THEME.font_size_hero_pt}pt; font-weight: bold;"
                )
            else:
                self.lbl_alarm_batt.setText(f"[✔ BATERÍA OK: {pkt.batt_v:.2f} V]")
                self.lbl_alarm_batt.setStyleSheet(
                    f"background-color: #DCFCE7; color: {GUI_THEME.accent_green}; "
                    f"border: 2px solid {GUI_THEME.accent_green}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
                self._val_labels["batt"].setText(f"✔ {pkt.batt_v:.2f} V [OK]")
                self._val_labels["batt"].setStyleSheet(
                    "background-color: #F8FAFC; border: 1px solid #64748B; "
                    "border-radius: 4px; padding: 4px 6px; font-weight: bold; color: #0F172A;"
                )

            if pkt.gps_fix and pkt.sats >= ALARM_THRESHOLDS.min_gps_satellites:
                self.lbl_alarm_gps.setText(f"[✔ GPS FIX OK: {pkt.sats} SATS]")
                self.lbl_alarm_gps.setStyleSheet(
                    f"background-color: #DCFCE7; color: {GUI_THEME.accent_green}; "
                    f"border: 2px solid {GUI_THEME.accent_green}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
            else:
                self.lbl_alarm_gps.setText(f"[⚠ GPS SIN FIX ({pkt.sats} SATS)]")
                self.lbl_alarm_gps.setStyleSheet(
                    f"background-color: #FEF3C7; color: {GUI_THEME.accent_amber}; "
                    f"border: 2px solid {GUI_THEME.accent_amber}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )

            if pkt.optional.second_parachute_deployed:
                self.lbl_alarm_p2.setText("[✔ 2.º PARACAÍDAS: DESPLEGADO (90°)]")
                self.lbl_alarm_p2.setStyleSheet(
                    f"background-color: #DCFCE7; color: {GUI_THEME.accent_green}; "
                    f"border: 2px solid {GUI_THEME.accent_green}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
            else:
                self.lbl_alarm_p2.setText("[● 2.º PARACAÍDAS: CERRADO (0°)]")
                self.lbl_alarm_p2.setStyleSheet(
                    "background-color: #F1F5F9; color: #0F172A; border: 2px solid #475569; "
                    "border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )

            # 6. Agregar muestra a las 4 gráficas en tiempo real (G7)
            t_sec = pkt.mission_time_ms / 1000.0
            self._t_buf.append(t_sec)
            self._alt_baro_buf.append(pkt.alt_m)
            self._alt_gps_buf.append(pkt.gps_alt_m)
            self._batt_v_buf.append(pkt.batt_v)
            self._curr_a_buf.append(pkt.curr_a)
            self._acc_x_buf.append(pkt.acc_x_ms2)
            self._acc_y_buf.append(pkt.acc_y_ms2)
            self._acc_z_buf.append(pkt.acc_z_ms2)
            self._gyr_x_buf.append(pkt.gyr_x_rads)
            self._gyr_y_buf.append(pkt.gyr_y_rads)
            self._gyr_z_buf.append(pkt.gyr_z_rads)

            t_list = list(self._t_buf)
            self.curve_alt_baro.setData(t_list, list(self._alt_baro_buf))
            self.curve_alt_gps.setData(t_list, list(self._alt_gps_buf))
            self.curve_batt_v.setData(t_list, list(self._batt_v_buf))
            self.curve_curr_a.setData(t_list, list(self._curr_a_buf))
            self.curve_acc_x.setData(t_list, list(self._acc_x_buf))
            self.curve_acc_y.setData(t_list, list(self._acc_y_buf))
            self.curve_acc_z.setData(t_list, list(self._acc_z_buf))
            self.curve_gyr_x.setData(t_list, list(self._gyr_x_buf))
            self.curve_gyr_y.setData(t_list, list(self._gyr_y_buf))
            self.curve_gyr_z.setData(t_list, list(self._gyr_z_buf))

            # Mantener rango mínimo de [-0.5, 0.5] rad/s en el giroscopio para no amplificar
            # ruido residual de 0.001 rad/s cuando el CanSat está detenido en Landed
            peak_gyr = max(
                0.5,
                max((abs(v) for v in self._gyr_x_buf), default=0.0),
                max((abs(v) for v in self._gyr_y_buf), default=0.0),
                max((abs(v) for v in self._gyr_z_buf), default=0.0),
            )
            y_bound = max(0.5, peak_gyr * 1.12)
            self.plot_gyr.setYRange(-y_bound, y_bound, padding=0.0)

        def _update_supervision_indicators(self) -> None:
            """Vigila pérdida de enlace (> 3 s) y estado de grabación CSV / USB."""
            sec = self.controller.seconds_since_last_rx
            if not self.controller.is_running:
                self.lbl_alarm_link.setText("[● ENLACE RF: DETENIDO]")
                self.lbl_alarm_link.setStyleSheet(
                    "background-color: #F1F5F9; color: #0F172A; border: 2px solid #475569; "
                    "border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
            elif sec > ALARM_THRESHOLDS.link_loss_timeout_s:
                self.lbl_alarm_link.setText(
                    f"[⚠ PÉRDIDA DE ENLACE RF: {sec:.1f} s SIN DATOS]"
                )
                self.lbl_alarm_link.setStyleSheet(
                    f"background-color: #FEE2E2; color: {GUI_THEME.accent_red}; "
                    f"border: 3px solid {GUI_THEME.accent_red}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )
            else:
                self.lbl_alarm_link.setText(
                    f"[✔ ENLACE {self.controller.mode} ACTIVO ({sec:.1f} s)]"
                )
                self.lbl_alarm_link.setStyleSheet(
                    f"background-color: #DCFCE7; color: {GUI_THEME.accent_green}; "
                    f"border: 2px solid {GUI_THEME.accent_green}; border-radius: 4px; padding: 4px 10px; font-weight: bold;"
                )

            self.lbl_storage_status.setText(
                f"CSV ({self.controller.storage.packets_written} filas): "
                f"{self.controller.storage.csv_filename} | {self.controller.storage.backup_status_summary}"
            )

        def closeEvent(self, event) -> None:  # type: ignore[override]
            self._watchdog_timer.stop()
            self.controller.on_packet = None
            self.controller.on_raw_line = None
            self.controller.on_command_status = None
            self.controller.on_event = None
            self.controller.stop()
            self.controller.storage.close()
            super().closeEvent(event)

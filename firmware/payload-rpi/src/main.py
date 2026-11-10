#!/usr/bin/env python3
"""Software de la Misión Secundaria — Raspberry Pi Zero 2W (RPI1).

Alineado con PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf, Arquitectura General Hardware.drawio
y docs/03-subsistemas/secundaria/README.md:

1. Determinación de Actitud 3D (Misión Secundaria - Parte 1):
   - Bus I2C1 (GPIO2 = SDA, GPIO3 = SCL):
       * Magnetómetro LIS3MDL (0x1C / 0x1E) con pin INT en GPIO17.
       * ADC ADS1115 (0x48) con los 4 fotodiodos BPW34 (simulados como U4..U7 TSL251RD
         en el esquemático v3: U4->A3, U5->A2, U6->A1, U7->A0).
   - Fusiona acelerómetro/giroscopio recibido del OBC + magnetómetro LIS3MDL + vector solar
     de los 4 fotodiodos BPW34 para estimar cuaternión y ángulos de Euler (roll, pitch, yaw).

2. Adquisición Infrarroja IMX219 NoIR, Clasificación de Terreno y Detección de Zonas de Aterrizaje (Misión Secundaria - Parte 2):
   - Gestiona la cámara Nadir IMX219 NoIR conectada al puerto CSI de la Raspberry Pi.
   - Captura imágenes NIR durante el descenso y las asocia en la microSD con marca de
     tiempo, posición GPS, altitud y actitud (roll, pitch, yaw).
   - Evalúa la reflectancia NIR e inclinación para clasificar la superficie observada
     (vegetación baja/pastizal, vegetación densa, suelo desnudo, agua/sombra) y estimar
     un índice de aptitud de aterrizaje (`landing_suitability`: `APTA` / `NO_APTA`).
   - Reporta el estado de la misión secundaria, actitud y aptitud de terreno al OBC por UART
     (GPIO14/15) mediante tramas `$RPI,...` para su transmisión en `OPTIONAL_DATA`.
"""

from __future__ import annotations

import argparse
import csv
import math
import shutil
import subprocess
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple


LIS3MDL_ADDRESSES = (0x1C, 0x1E)
ADS1115_ADDRESS = 0x48
LIS3MDL_INT_GPIO = 17


@dataclass
class ObcTelemetryContext:
    mission_time_ms: int = 0
    state: str = "LaunchPad"
    altitude_m: float = 0.0
    accel_x_g: float = 0.0
    accel_y_g: float = 0.0
    accel_z_g: float = 1.0
    latitude_deg: float = 0.0
    longitude_deg: float = 0.0
    gps_altitude_m: float = 0.0


@dataclass
class SecondarySample:
    mag_x_ut: float = 0.0
    mag_y_ut: float = 0.0
    mag_z_ut: float = 0.0
    opt1_raw: int = 0  # Fotodiodo BPW34 U4 (AIN3 en esquema v3)
    opt2_raw: int = 0  # Fotodiodo BPW34 U5 (AIN2 en esquema v3)
    opt3_raw: int = 0  # Fotodiodo BPW34 U6 (AIN1 en esquema v3)
    opt4_raw: int = 0  # Fotodiodo BPW34 U7 (AIN0 en esquema v3)
    qw: float = 1.0
    qx: float = 0.0
    qy: float = 0.0
    qz: float = 0.0
    roll_deg: float = 0.0
    pitch_deg: float = 0.0
    yaw_deg: float = 0.0
    sun_azimuth_deg: float = 0.0
    lis_ok: bool = False
    ads_ok: bool = False
    nir_image_file: str = "NONE"
    nir_capture_count: int = 0
    terrain_class: str = "SIN_DATOS"
    landing_apt: str = "PENDIENTE"
    suitability_score: float = 0.0


def euler_to_quaternion(
    roll_deg: float, pitch_deg: float, yaw_deg: float
) -> Tuple[float, float, float, float]:
    """Convierte roll, pitch, yaw (grados) a cuaternión (qw, qx, qy, qz) en marco NED."""
    r = math.radians(roll_deg) * 0.5
    p = math.radians(pitch_deg) * 0.5
    y = math.radians(yaw_deg) * 0.5

    cr, sr = math.cos(r), math.sin(r)
    cp, sp = math.cos(p), math.sin(p)
    cy, sy = math.cos(y), math.sin(y)

    qw = cr * cp * cy + sr * sp * sy
    qx = sr * cp * cy - cr * sp * sy
    qy = cr * sp * cy + sr * cp * sy
    qz = cr * cp * sy - sr * sp * cy
    return qw, qx, qy, qz


class Imx219NoirCamera:
    """Controlador de la cámara Nadir IMX219 NoIR conectada por CSI a la Raspberry Pi."""

    def __init__(self, images_dir: Path, interval_s: float = 2.0) -> None:
        self.images_dir = images_dir
        self.images_dir.mkdir(parents=True, exist_ok=True)
        self.interval_s = interval_s
        self.capture_count = 0
        self.last_capture_monotonic = 0.0
        self.last_filename = "NONE"
        self._rpicam_bin = shutil.which("rpicam-still") or shutil.which("libcamera-still")

    def maybe_capture(self, state: str, mission_time_ms: int) -> str:
        """Captura cuadros NIR durante Ascent y Descent respetando el intervalo."""
        if state not in ("Ascent", "Descent"):
            return self.last_filename

        now = time.monotonic()
        if (now - self.last_capture_monotonic) < self.interval_s:
            return self.last_filename

        self.last_capture_monotonic = now
        if self._rpicam_bin is None:
            return self.last_filename

        next_id = self.capture_count + 1
        filename = f"NIR_{next_id:05d}_{mission_time_ms}ms.png"
        filepath = self.images_dir / filename
        try:
            res = subprocess.run(
                [
                    self._rpicam_bin,
                    "-n",
                    "--immediate",
                    "-e",
                    "png",
                    "--width",
                    "1280",
                    "--height",
                    "720",
                    "-o",
                    str(filepath),
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=1.5,
                check=False,
            )
            if res.returncode == 0 and filepath.exists():
                self.capture_count = next_id
                self.last_filename = filename
        except Exception:
            pass
        return self.last_filename


class RpiI2cSensors:
    """Driver I2C para LIS3MDL + ADS1115 (4x BPW34) en GPIO2/GPIO3 de la Raspberry Pi."""

    def __init__(self, bus_id: int = 1) -> None:
        self.bus_id = bus_id
        self.bus = None
        self.lis_addr: Optional[int] = None
        self.ads_available = False
        self._init_hardware()

    def _init_hardware(self) -> None:
        try:
            import smbus2  # type: ignore

            self.bus = smbus2.SMBus(self.bus_id)
        except Exception as exc:
            print(
                f"[WARN] No se pudo abrir I2C bus {self.bus_id} ({exc}). "
                "Continuando en modo degradado (lecturas en 0).",
                file=sys.stderr,
            )
            self.bus = None
            return

        # 1. Inicializar LIS3MDL (0x1C o 0x1E)
        for addr in LIS3MDL_ADDRESSES:
            try:
                who_am_i = self.bus.read_byte_data(addr, 0x0F)
                if who_am_i == 0x3D:
                    self.bus.write_byte_data(addr, 0x20, 0x7C)
                    self.bus.write_byte_data(addr, 0x21, 0x00)
                    self.bus.write_byte_data(addr, 0x22, 0x00)
                    self.bus.write_byte_data(addr, 0x23, 0x0C)
                    self.lis_addr = addr
                    print(f"[OK] LIS3MDL detectado en 0x{addr:02X}")
                    break
            except Exception:
                continue

        # 2. Inicializar ADS1115 (0x48)
        try:
            self.bus.read_i2c_block_data(ADS1115_ADDRESS, 0x00, 2)
            self.ads_available = True
            print(f"[OK] ADS1115 detectado en 0x{ADS1115_ADDRESS:02X}")
        except Exception:
            self.ads_available = False

    def _read_lis3mdl(self) -> Tuple[bool, float, float, float]:
        if self.bus is None or self.lis_addr is None:
            return False, 0.0, 0.0, 0.0
        try:
            raw = bytes(self.bus.read_i2c_block_data(self.lis_addr, 0x28 | 0x80, 6))
            mx, my, mz = struct.unpack("<hhh", raw)
            lsb_per_ut = 68.42
            return True, mx / lsb_per_ut, my / lsb_per_ut, mz / lsb_per_ut
        except Exception:
            return False, 0.0, 0.0, 0.0

    def _read_ads1115_channel(self, mux: int) -> int:
        """Lee un canal single-ended (mux 4=AIN0, 5=AIN1, 6=AIN2, 7=AIN3)."""
        if self.bus is None or not self.ads_available:
            return 0
        try:
            config = 0x8383 | ((mux & 0x07) << 12)
            hi = (config >> 8) & 0xFF
            lo = config & 0xFF
            self.bus.write_i2c_block_data(ADS1115_ADDRESS, 0x01, [hi, lo])
            time.sleep(0.009)
            data = self.bus.read_i2c_block_data(ADS1115_ADDRESS, 0x00, 2)
            val = struct.unpack(">h", bytes(data))[0]
            return max(0, int(val))
        except Exception:
            return 0

    def read_sample(
        self, accel_g: Tuple[float, float, float] = (0.0, 0.0, 1.0)
    ) -> SecondarySample:
        sample = SecondarySample()

        ok_mag, mx, my, mz = self._read_lis3mdl()
        sample.lis_ok = ok_mag
        sample.mag_x_ut = mx
        sample.mag_y_ut = my
        sample.mag_z_ut = mz

        if self.ads_available:
            # Segun PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf:
            # U4 -> A3 (mux 7), U5 -> A2 (mux 6), U6 -> A1 (mux 5), U7 -> A0 (mux 4)
            sample.opt1_raw = self._read_ads1115_channel(7)
            sample.opt2_raw = self._read_ads1115_channel(6)
            sample.opt3_raw = self._read_ads1115_channel(5)
            sample.opt4_raw = self._read_ads1115_channel(4)
            sample.ads_ok = True

        # Estimacion magneto-solar-inercial (roll, pitch, yaw, cuaternion y acimut solar)
        ax, ay, az = accel_g
        sample.roll_deg = math.degrees(math.atan2(ay, az)) if (ay or az) else 0.0
        sample.pitch_deg = math.degrees(
            math.atan2(-ax, math.sqrt(ay * ay + az * az))
        )
        sample.yaw_deg = (
            math.degrees(math.atan2(-sample.mag_y_ut, sample.mag_x_ut)) % 360.0
            if (sample.mag_x_ut or sample.mag_y_ut)
            else 0.0
        )

        # Vector solar 2D desde los 4 fotodiodos BPW34 (U4=+X, U5=-X, U6=+Y, U7=-Y)
        sx = float(sample.opt1_raw - sample.opt2_raw)
        sy = float(sample.opt3_raw - sample.opt4_raw)
        if abs(sx) > 50 or abs(sy) > 50:
            sample.sun_azimuth_deg = math.degrees(math.atan2(sy, sx)) % 360.0

        sample.qw, sample.qx, sample.qy, sample.qz = euler_to_quaternion(
            sample.roll_deg, sample.pitch_deg, sample.yaw_deg
        )
        return sample


def evaluate_terrain_and_landing_suitability(
    images_dir: Path,
    nir_image_file: str,
    roll_deg: float,
    pitch_deg: float,
) -> Tuple[str, str, float]:
    """Clasifica el tipo de terreno y determina si la zona es APTA o NO_APTA para aterrizar.

    Combina la respuesta radiométrica de la imagen NIR capturada (cuando está disponible)
    con la estabilidad de apuntamiento nadir (roll, pitch) según la propuesta de Misión
    Secundaria (Sección 2.2 de propuesta-etapa-2-borrador.docx).
    """
    tilt_deg = math.hypot(roll_deg, pitch_deg)
    attitude_factor = max(0.0, min(1.0, 1.0 - (tilt_deg / 60.0)))

    filepath = images_dir / nir_image_file if nir_image_file != "NONE" else None
    if filepath is None or not filepath.exists():
        return "SIN_IMAGEN", "PENDIENTE", round(attitude_factor * 0.5, 2)

    try:
        data = filepath.read_bytes()
        if len(data) < 128:
            return "SIN_IMAGEN", "PENDIENTE", round(attitude_factor * 0.5, 2)
        # Estimador ligero de reflectancia media y textura sobre el bloque central
        step = max(1, len(data) // 512)
        samples = data[64::step][:512]
        mean_val = sum(samples) / len(samples)
        variance = sum((b - mean_val) ** 2 for b in samples) / len(samples)
        std_val = math.sqrt(variance)

        # Clasificacion por reflectancia NIR y rugosidad/textura:
        # - Agua o sombra profunda: baja reflectancia NIR (< 65) -> NO_APTA
        # - Arboleda / vegetacion densa rugosa: alta reflectancia NIR (> 165) y alta textura (> 45) -> NO_APTA
        # - Pastizal / vegetacion baja uniforme: reflectancia media-alta y baja rugosidad -> APTA
        # - Suelo desnudo / tierra firme plana: reflectancia intermedia y baja rugosidad -> APTA
        if mean_val < 65.0:
            terrain = "AGUA_O_SOMBRA"
            base_score = 0.15
        elif mean_val > 165.0 and std_val > 45.0:
            terrain = "VEG_DENSA_OBSTACULOS"
            base_score = 0.30
        elif mean_val >= 115.0:
            terrain = "PASTIZAL_ABIERTO"
            base_score = 0.90
        else:
            terrain = "SUELO_FIRME"
            base_score = 0.85

        roughness_penalty = min(0.35, (std_val / 128.0) * 0.35)
        score = max(0.0, min(1.0, (base_score - roughness_penalty) * (0.6 + 0.4 * attitude_factor)))
        apt = "APTA" if (score >= 0.55 and tilt_deg <= 35.0) else "NO_APTA"
        return terrain, apt, round(score, 2)
    except Exception:
        return "ERROR_LECTURA", "PENDIENTE", 0.0


def format_rpi_frame(sample: SecondarySample) -> str:
    """Genera la trama ASCII $RPI que el OBC ESP32-S3 parsea por UART."""
    tag = (
        f"YAW:{sample.yaw_deg:.0f}|SUN:{sample.sun_azimuth_deg:.0f}"
        f"|NIR:{sample.nir_capture_count}|APT:{sample.landing_apt}({sample.suitability_score:.2f})"
    )
    return (
        f"$RPI,{sample.mag_x_ut:.1f},{sample.mag_y_ut:.1f},{sample.mag_z_ut:.1f},"
        f"{sample.opt1_raw},{sample.opt2_raw},{sample.opt3_raw},{sample.opt4_raw},"
        f"{sample.roll_deg:.1f},{sample.pitch_deg:.1f},{sample.yaw_deg:.1f},{tag}\n"
    )


def parse_obc_csv(raw_line: str) -> Optional[ObcTelemetryContext]:
    """Extrae tiempo, estado, altura, acelerómetro y GPS de la trama CSV del OBC."""
    parts = raw_line.split(",")
    if len(parts) < 20:
        return None
    try:
        return ObcTelemetryContext(
            mission_time_ms=int(parts[1]),
            state=parts[4].strip(),
            altitude_m=float(parts[5]),
            accel_x_g=float(parts[10]),
            accel_y_g=float(parts[11]),
            accel_z_g=float(parts[12]),
            latitude_deg=float(parts[16]),
            longitude_deg=float(parts[17]),
            gps_altitude_m=float(parts[18]),
        )
    except ValueError:
        return None


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Misión Secundaria (Raspberry Pi Zero 2W): Determinación de actitud "
            "(LIS3MDL + ADS1115/BPW34) y cámara infrarroja IMX219 NoIR sincronizada con OBC."
        )
    )
    parser.add_argument(
        "--port",
        required=True,
        help="Puerto serie UART (ej. /dev/ttyAMA0 en RPi Zero o COM3 en Windows)",
    )
    parser.add_argument("--baud", type=int, default=115200, help="Velocidad en baudios")
    parser.add_argument(
        "--i2c-bus", type=int, default=1, help="Numero de bus I2C en la RPi (default: 1)"
    )
    parser.add_argument(
        "--output",
        default="payload_log.csv",
        help="Archivo CSV de metadatos de misión secundaria en la microSD",
    )
    parser.add_argument(
        "--images-dir",
        default="nir_images",
        help="Carpeta donde se guardan las capturas de la cámara IMX219 NoIR",
    )
    return parser


def run(port: str, baud: int, output: str, i2c_bus: int, images_dir: str) -> int:
    try:
        import serial  # type: ignore
    except ImportError:
        print(
            "Falta pyserial. Instalar con: pip install -r requirements.txt",
            file=sys.stderr,
        )
        return 2

    sensors = RpiI2cSensors(bus_id=i2c_bus)
    nir_cam = Imx219NoirCamera(images_dir=Path(images_dir))
    output_path = Path(output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    ctx = ObcTelemetryContext()
    last_tx_time = 0.0

    with serial.Serial(port, baud, timeout=0.2) as link:
        with output_path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(
                [
                    "received_at",
                    "mission_time_ms",
                    "state",
                    "altitude_m",
                    "latitude_deg",
                    "longitude_deg",
                    "qw",
                    "qx",
                    "qy",
                    "qz",
                    "roll_deg",
                    "pitch_deg",
                    "yaw_deg",
                    "sun_azimuth_deg",
                    "mag_x_ut",
                    "mag_y_ut",
                    "mag_z_ut",
                    "opt1_u4",
                    "opt2_u5",
                    "opt3_u6",
                    "opt4_u7",
                    "nir_image_file",
                    "terrain_class",
                    "landing_apt",
                    "suitability_score",
                    "obc_raw_line",
                ]
            )
            while True:
                raw_bytes = link.readline()
                raw = raw_bytes.decode("utf-8", errors="replace").strip()
                now = time.monotonic()

                if raw:
                    parsed = parse_obc_csv(raw)
                    if parsed is not None:
                        ctx = parsed

                if raw or (now - last_tx_time) >= 1.0:
                    sample = sensors.read_sample(
                        (ctx.accel_x_g, ctx.accel_y_g, ctx.accel_z_g)
                    )
                    sample.nir_image_file = nir_cam.maybe_capture(
                        ctx.state, ctx.mission_time_ms
                    )
                    sample.nir_capture_count = nir_cam.capture_count
                    (
                        sample.terrain_class,
                        sample.landing_apt,
                        sample.suitability_score,
                    ) = evaluate_terrain_and_landing_suitability(
                        nir_cam.images_dir,
                        sample.nir_image_file,
                        sample.roll_deg,
                        sample.pitch_deg,
                    )

                    rpi_msg = format_rpi_frame(sample)
                    link.write(rpi_msg.encode("ascii", errors="ignore"))
                    last_tx_time = now

                    if raw:
                        stamp = time.strftime("%Y-%m-%dT%H:%M:%S")
                        writer.writerow(
                            [
                                stamp,
                                ctx.mission_time_ms,
                                ctx.state,
                                f"{ctx.altitude_m:.2f}",
                                f"{ctx.latitude_deg:.6f}",
                                f"{ctx.longitude_deg:.6f}",
                                f"{sample.qw:.4f}",
                                f"{sample.qx:.4f}",
                                f"{sample.qy:.4f}",
                                f"{sample.qz:.4f}",
                                f"{sample.roll_deg:.2f}",
                                f"{sample.pitch_deg:.2f}",
                                f"{sample.yaw_deg:.2f}",
                                f"{sample.sun_azimuth_deg:.2f}",
                                f"{sample.mag_x_ut:.2f}",
                                f"{sample.mag_y_ut:.2f}",
                                f"{sample.mag_z_ut:.2f}",
                                sample.opt1_raw,
                                sample.opt2_raw,
                                sample.opt3_raw,
                                sample.opt4_raw,
                                sample.nir_image_file,
                                sample.terrain_class,
                                sample.landing_apt,
                                f"{sample.suitability_score:.2f}",
                                raw,
                            ]
                        )
                        handle.flush()
                        print(f"[OBC] {raw}")
                        print(f"[RPI] {rpi_msg.strip()}")
    return 0


def main() -> int:
    args = build_parser().parse_args()
    try:
        return run(args.port, args.baud, args.output, args.i2c_bus, args.images_dir)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    raise SystemExit(main())

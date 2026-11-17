"""Pipeline Post-Vuelo de Procesamiento de Imágenes Multiespectrales NIR y VIS (ISI 2).

Corrige la limitación del estimador en vuelo de `firmware/payload-rpi/src/main.py`
(que leía bytes comprimidos del contenedor PNG) decodificando la matriz real de píxeles
2D mediante OpenCV / Pillow (con decodificador/codificador PNG nativo en NumPy + zlib
como respaldo sin dependencias externas).

Funciones principales:
1. Lee la carpeta de capturas `NIR_<id>_<mission_time_ms>ms.png` y `payload_log.csv`.
2. Decodifica los píxeles reales y calcula estadísticas radiométricas y espaciales:
   - Reflectancia media NIR (`mean_nir`), desviación estándar (`std_roughness`),
     gradiente espacial local (`local_contrast`), porcentaje de saturación (`saturation_pct`).
   - Fracción de píxeles por clase de cobertura: agua/sombra, suelo firme, pastizal abierto
     y vegetación densa/obstáculos.
3. Combina las métricas radiométricas reales con la actitud 3D (`roll_deg`, `pitch_deg`)
   para clasificar el terreno y determinar `landing_apt` (`APTA` / `NO_APTA`).
4. Genera mapas segmentados anotados (`*_segmented.png`), reporte `post_flight_report.csv`
   y gráfico resumen `post_flight_summary.png`.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import numpy as np

try:
    import cv2  # type: ignore

    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False

try:
    from PIL import Image  # type: ignore

    HAS_PIL = True
except ImportError:
    HAS_PIL = False


_NIR_FILENAME_RE = re.compile(
    r"(?:NIR|VIS)_(\d+)_(?:(\d+)ms|m(\d+))\.(?:png|jpg|jpeg)$", re.IGNORECASE
)


@dataclass
class PayloadMetadataRow:
    """Metadatos sincronizados registrados por la Raspberry Pi en payload_log.csv."""

    mission_time_ms: int = 0
    state: str = "Descent"
    altitude_m: float = 0.0
    latitude_deg: float = 0.0
    longitude_deg: float = 0.0
    roll_deg: float = 0.0
    pitch_deg: float = 0.0
    yaw_deg: float = 0.0
    sun_azimuth_deg: float = 0.0
    nir_image_file: str = "NONE"
    in_flight_terrain: str = "SIN_DATOS"
    in_flight_apt: str = "PENDIENTE"
    in_flight_score: float = 0.0


@dataclass
class NirImageAnalysisResult:
    """Resultado completo del análisis post-vuelo sobre píxeles reales de una captura NIR."""

    filename: str
    capture_id: int
    mission_time_ms: int
    width: int
    height: int
    altitude_m: float
    latitude_deg: float
    longitude_deg: float
    roll_deg: float
    pitch_deg: float
    yaw_deg: float
    tilt_deg: float
    mean_nir: float
    std_roughness: float
    local_contrast: float
    saturation_pct: float
    water_shadow_pct: float
    bare_soil_pct: float
    open_pasture_pct: float
    dense_veg_pct: float
    terrain_class: str
    landing_apt: str
    suitability_score: float
    annotated_image_path: str


# ============================================================================
# Codificador / Decodificador PNG Nativo (Respaldo exacto sin OpenCV/Pillow)
# ============================================================================
def _paeth_predictor(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _decode_png_native(filepath: Path) -> np.ndarray:
    """Decodifica un archivo PNG de 8 bits (Escala de grises, RGB o RGBA) a matriz NumPy."""
    raw = filepath.read_bytes()
    if raw[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"El archivo {filepath} no tiene cabecera PNG válida")

    pos = 8
    width = height = bit_depth = color_type = 0
    idat_chunks: List[bytes] = []

    while pos < len(raw):
        length = struct.unpack(">I", raw[pos : pos + 4])[0]
        chunk_type = raw[pos + 4 : pos + 8]
        chunk_data = raw[pos + 8 : pos + 8 + length]
        pos += 12 + length

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, _ = struct.unpack(
                ">IIBBBBB", chunk_data
            )
        elif chunk_type == b"IDAT":
            idat_chunks.append(chunk_data)
        elif chunk_type == b"IEND":
            break

    if bit_depth != 8 or color_type not in (0, 2, 6):
        raise ValueError(
            f"Formato PNG no soportado en modo nativo (bit_depth={bit_depth}, color_type={color_type})"
        )

    channels = {0: 1, 2: 3, 6: 4}[color_type]
    decompressed = zlib.decompress(b"".join(idat_chunks))
    stride = width * channels
    recon = np.zeros((height, stride), dtype=np.uint8)

    offset = 0
    for r in range(height):
        filter_type = decompressed[offset]
        offset += 1
        row_bytes = np.frombuffer(
            decompressed, dtype=np.uint8, count=stride, offset=offset
        ).copy()
        offset += stride

        if filter_type == 0:
            recon[r] = row_bytes
        elif filter_type == 1:
            for c in range(stride):
                left = int(recon[r, c - channels]) if c >= channels else 0
                recon[r, c] = (int(row_bytes[c]) + left) & 0xFF
        elif filter_type == 2:
            up = recon[r - 1] if r > 0 else np.zeros(stride, dtype=np.uint8)
            recon[r] = (row_bytes.astype(np.uint16) + up.astype(np.uint16)) & 0xFF
        elif filter_type == 3:
            for c in range(stride):
                left = int(recon[r, c - channels]) if c >= channels else 0
                up = int(recon[r - 1, c]) if r > 0 else 0
                recon[r, c] = (int(row_bytes[c]) + ((left + up) >> 1)) & 0xFF
        elif filter_type == 4:
            for c in range(stride):
                left = int(recon[r, c - channels]) if c >= channels else 0
                up = int(recon[r - 1, c]) if r > 0 else 0
                up_left = (
                    int(recon[r - 1, c - channels])
                    if (r > 0 and c >= channels)
                    else 0
                )
                recon[r, c] = (
                    int(row_bytes[c]) + _paeth_predictor(left, up, up_left)
                ) & 0xFF

    if channels == 1:
        return recon.reshape((height, width))
    return recon.reshape((height, width, channels))


def save_rgb_or_gray_png(filepath: Path, array: np.ndarray) -> None:
    """Guarda un arreglo NumPy 2D (gris) o 3D (RGB uint8) como archivo PNG estándar."""
    filepath.parent.mkdir(parents=True, exist_ok=True)
    arr = np.asarray(array, dtype=np.uint8)

    if HAS_CV2:
        if arr.ndim == 3 and arr.shape[2] == 3:
            bgr = arr[:, :, ::-1]
            cv2.imwrite(str(filepath), bgr)
        else:
            cv2.imwrite(str(filepath), arr)
        return

    if HAS_PIL:
        Image.fromarray(arr).save(str(filepath), format="PNG")
        return

    # Codificador PNG estándar con zlib + CRC32
    if arr.ndim == 2:
        height, width = arr.shape
        color_type = 0
        raw_rows = b"".join(b"\x00" + arr[r].tobytes() for r in range(height))
    elif arr.ndim == 3 and arr.shape[2] == 3:
        height, width, _ = arr.shape
        color_type = 2
        raw_rows = b"".join(b"\x00" + arr[r].tobytes() for r in range(height))
    else:
        raise ValueError("El arreglo debe ser 2D (gris) o 3D (H, W, 3) uint8")

    def _chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    compressed = zlib.compress(raw_rows, level=6)
    png_bytes = (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", compressed)
        + _chunk(b"IEND", b"")
    )
    filepath.write_bytes(png_bytes)


def load_nir_grayscale_pixels(filepath: Path) -> np.ndarray:
    """Carga una imagen NIR y devuelve su matriz 2D de reflectancia en escala de grises (uint8)."""
    if HAS_CV2:
        img = cv2.imread(str(filepath), cv2.IMREAD_UNCHANGED)
        if img is not None:
            if img.ndim == 2:
                return img.astype(np.uint8)
            if img.ndim == 3:
                # En cámara IMX219 NoIR el canal rojo/luminancia concentra la respuesta NIR
                return cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.uint8)

    if HAS_PIL:
        with Image.open(filepath) as im:
            return np.asarray(im.convert("L"), dtype=np.uint8)

    arr = _decode_png_native(filepath)
    if arr.ndim == 2:
        return arr
    # Promedio ponderado dando prioridad al canal R (índice 0) en sensor NoIR
    return (
        0.55 * arr[:, :, 0].astype(np.float32)
        + 0.30 * arr[:, :, 1].astype(np.float32)
        + 0.15 * arr[:, :, 2].astype(np.float32)
    ).astype(np.uint8)


# ============================================================================
# Análisis Radiométrico, Segmentación y Clasificación Post-Vuelo
# ============================================================================
def load_payload_metadata(csv_path: Optional[Path]) -> Dict[str, PayloadMetadataRow]:
    """Lee payload_log.csv de la Raspberry Pi e indexa por nombre de imagen NIR.

    Acepta tanto los nombres de columna largos ('altitude_m', 'latitude_deg', 'longitude_deg')
    como los cortos ('alt_m', 'lat', 'lon').
    """
    by_image: Dict[str, PayloadMetadataRow] = {}
    if csv_path is None or not csv_path.exists():
        return by_image

    with csv_path.open("r", encoding="utf-8", errors="replace") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            img_name = (
                row.get("nir_image_file")
                or row.get("image_file")
                or row.get("filename")
                or "NONE"
            ).strip()
            if not img_name or img_name == "NONE":
                continue

            alt_raw = row.get("altitude_m")
            if alt_raw in (None, ""):
                alt_raw = row.get("alt_m", 0.0)

            lat_raw = row.get("latitude_deg")
            if lat_raw in (None, ""):
                lat_raw = row.get("lat", 0.0)

            lon_raw = row.get("longitude_deg")
            if lon_raw in (None, ""):
                lon_raw = row.get("lon", 0.0)

            try:
                meta = PayloadMetadataRow(
                    mission_time_ms=int(float(row.get("mission_time_ms", 0) or 0)),
                    state=(row.get("state") or "Descent").strip(),
                    altitude_m=float(alt_raw or 0.0),
                    latitude_deg=float(lat_raw or 0.0),
                    longitude_deg=float(lon_raw or 0.0),
                    roll_deg=float(row.get("roll_deg", 0.0) or 0.0),
                    pitch_deg=float(row.get("pitch_deg", 0.0) or 0.0),
                    yaw_deg=float(row.get("yaw_deg", 0.0) or 0.0),
                    sun_azimuth_deg=float(row.get("sun_azimuth_deg", 0.0) or 0.0),
                    nir_image_file=img_name,
                    in_flight_terrain=(row.get("terrain_class") or "SIN_DATOS").strip(),
                    in_flight_apt=(row.get("landing_apt") or "PENDIENTE").strip(),
                    in_flight_score=float(row.get("suitability_score", 0.0) or 0.0),
                )
                by_image[img_name] = meta
            except ValueError:
                continue
    return by_image


def analyze_nir_matrix(
    gray: np.ndarray,
    roll_deg: float = 0.0,
    pitch_deg: float = 0.0,
) -> Tuple[
    float,
    float,
    float,
    float,
    float,
    float,
    float,
    float,
    str,
    str,
    float,
    np.ndarray,
]:
    """Analiza una matriz 2D real de píxeles NIR y genera un mapa RGB segmentado."""
    if gray.ndim != 2 or gray.size == 0:
        raise ValueError("Se requiere una matriz 2D no vacía de píxeles NIR")

    fgray = gray.astype(np.float32)
    mean_nir = float(np.mean(fgray))
    std_roughness = float(np.std(fgray))

    # Gradiente espacial local (magnitud de diferencias finitas en X e Y)
    if gray.shape[0] >= 2 and gray.shape[1] >= 2:
        gx = np.abs(np.diff(fgray, axis=1))
        gy = np.abs(np.diff(fgray, axis=0))
        grad_map = np.zeros_like(fgray)
        grad_map[:, :-1] += gx * 0.5
        grad_map[:-1, :] += gy * 0.5
        local_contrast = float(np.mean(grad_map))
    else:
        grad_map = np.zeros_like(fgray)
        local_contrast = 0.0

    saturated_mask = (gray <= 5) | (gray >= 250)
    saturation_pct = float(np.mean(saturated_mask) * 100.0)

    # Segmentación píxel por píxel:
    # 1. Agua o sombra profunda: baja reflectancia NIR (< 65)
    water_mask = gray < 65
    # 2. Vegetación densa / arboleda con obstáculos: alta reflectancia (> 165) o alta rugosidad local (> 28)
    dense_veg_mask = (~water_mask) & ((gray > 165) | (grad_map > 28.0))
    # 3. Pastizal abierto uniforme: reflectancia media-alta (115..165) y baja rugosidad
    pasture_mask = (~water_mask) & (~dense_veg_mask) & (gray >= 115)
    # 4. Suelo firme / tierra desnuda plana: reflectancia intermedia (65..114) y baja rugosidad
    soil_mask = (~water_mask) & (~dense_veg_mask) & (~pasture_mask)

    water_pct = float(np.mean(water_mask) * 100.0)
    dense_pct = float(np.mean(dense_veg_mask) * 100.0)
    pasture_pct = float(np.mean(pasture_mask) * 100.0)
    soil_pct = float(np.mean(soil_mask) * 100.0)

    # Clasificación global de la escena sobre píxeles reales
    if mean_nir < 65.0 or water_pct >= 45.0:
        terrain_class = "AGUA_O_SOMBRA"
        base_score = 0.15
    elif (mean_nir > 165.0 and std_roughness > 35.0) or dense_pct >= 45.0:
        terrain_class = "VEG_DENSA_OBSTACULOS"
        base_score = 0.28
    elif mean_nir >= 115.0 or pasture_pct >= soil_pct:
        terrain_class = "PASTIZAL_ABIERTO"
        base_score = 0.92
    else:
        terrain_class = "SUELO_FIRME"
        base_score = 0.86

    tilt_deg = math.hypot(roll_deg, pitch_deg)
    attitude_factor = max(0.0, min(1.0, 1.0 - (tilt_deg / 60.0)))
    roughness_penalty = min(0.40, (std_roughness / 90.0) * 0.25 + (local_contrast / 60.0) * 0.15)
    safe_area_ratio = (pasture_pct + soil_pct) / 100.0

    raw_score = (
        (0.55 * base_score + 0.45 * safe_area_ratio - roughness_penalty)
        * (0.65 + 0.35 * attitude_factor)
    )
    suitability_score = round(max(0.0, min(1.0, raw_score)), 2)
    landing_apt = (
        "APTA" if (suitability_score >= 0.60 and tilt_deg <= 35.0) else "NO_APTA"
    )

    # Construir imagen RGB anotada con superposición de zonas aptas vs obstáculos
    h, w = gray.shape
    rgb = np.stack([gray, gray, gray], axis=-1).astype(np.float32)

    overlay = np.zeros((h, w, 3), dtype=np.float32)
    overlay[water_mask] = [30.0, 100.0, 220.0]       # Azul: agua o sombra
    overlay[dense_veg_mask] = [220.0, 60.0, 45.0]    # Rojo/Naranja: obstáculos / arboleda
    overlay[pasture_mask] = [34.0, 197.0, 94.0]      # Verde: pastizal abierto apto
    overlay[soil_mask] = [163.0, 230.0, 53.0]        # Verde lima: suelo firme apto

    blended = np.clip(0.58 * rgb + 0.42 * overlay, 0, 255).astype(np.uint8)

    # Dibujar retícula nadir en el centro y marco según aptitud (verde = APTA, rojo = NO_APTA)
    cy, cx = h // 2, w // 2
    border_color = np.array(
        [21, 128, 61] if landing_apt == "APTA" else [185, 28, 28], dtype=np.uint8
    )
    bw = max(2, min(h, w) // 40)
    blended[:bw, :] = border_color
    blended[-bw:, :] = border_color
    blended[:, :bw] = border_color
    blended[:, -bw:] = border_color

    if h >= 12 and w >= 12:
        arm = max(4, min(h, w) // 12)
        blended[max(0, cy - arm) : min(h, cy + arm + 1), cx] = [255, 255, 0]
        blended[cy, max(0, cx - arm) : min(w, cx + arm + 1)] = [255, 255, 0]

    return (
        round(mean_nir, 2),
        round(std_roughness, 2),
        round(local_contrast, 2),
        round(saturation_pct, 2),
        round(water_pct, 2),
        round(soil_pct, 2),
        round(pasture_pct, 2),
        round(dense_pct, 2),
        terrain_class,
        landing_apt,
        suitability_score,
        blended,
    )


def _generate_summary_chart_png(
    results: List[NirImageAnalysisResult], output_png: Path
) -> None:
    """Genera post_flight_summary.png con matplotlib (Agg) o respaldo Pillow/OpenCV."""
    output_png.parent.mkdir(parents=True, exist_ok=True)
    title_str = "Clasificación Post-Vuelo de Terreno NIR — YVYSAT (UTN FRRe)"
    y_label_str = "Índice de Aptitud de Aterrizaje (0.0 – 1.0)"
    threshold_val = 0.60
    threshold_label = "Umbral Zona Apta (0.60)"

    # 1. Intentar con matplotlib (backend 'Agg' sin ventana)
    try:
        import matplotlib  # type: ignore

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt  # type: ignore
        from matplotlib.patches import Patch  # type: ignore

        fig, ax = plt.subplots(figsize=(11, 6.2), dpi=150)
        fig.patch.set_facecolor("#F8FAFC")
        ax.set_facecolor("#FFFFFF")

        ax.set_title(title_str, fontsize=14, fontweight="bold", color="#0F172A", pad=14)
        ax.set_ylabel(y_label_str, fontsize=12, fontweight="bold", color="#0F172A")
        ax.set_xlabel("Captura NIR (ID / Archivo)", fontsize=12, fontweight="bold", color="#0F172A")
        ax.set_ylim(0.0, 1.05)
        ax.grid(axis="y", linestyle=":", alpha=0.6, color="#94A3B8")

        line_thresh = ax.axhline(
            threshold_val,
            color="#0F172A",
            linestyle="--",
            linewidth=2.0,
            label=threshold_label,
        )

        if results:
            x_positions = list(range(len(results)))
            x_labels = [f"#{r.capture_id}\n{r.filename}" for r in results]
            # Altura visual mínima de 0.02 para que barras con score 0.00 muestren su color rojo
            bar_heights = [max(0.02, r.suitability_score) for r in results]
            colors = [
                "#2e7d32" if r.landing_apt == "APTA" else "#c62828"
                for r in results
            ]

            bars = ax.bar(
                x_positions,
                bar_heights,
                color=colors,
                edgecolor="#0F172A",
                linewidth=1.3,
                width=0.52,
            )
            ax.set_xticks(x_positions)
            ax.set_xticklabels(x_labels, fontsize=10, fontweight="bold", color="#0F172A")

            for bar, item in zip(bars, results):
                y_val = bar.get_height()
                label_txt = f"{item.suitability_score:.2f}\n({item.terrain_class})"
                ax.annotate(
                    label_txt,
                    xy=(bar.get_x() + bar.get_width() / 2.0, y_val),
                    xytext=(0, 6),
                    textcoords="offset points",
                    ha="center",
                    va="bottom",
                    fontsize=9.5,
                    fontweight="bold",
                    color="#0F172A",
                )

        legend_handles = [
            line_thresh,
            Patch(facecolor="#2e7d32", edgecolor="#0F172A", label="APTA (>= 0.60)"),
            Patch(facecolor="#c62828", edgecolor="#0F172A", label="NO_APTA (< 0.60)"),
        ]
        ax.legend(handles=legend_handles, loc="upper right", frameon=True, fontsize=10)
        fig.tight_layout()
        fig.savefig(str(output_png), dpi=150)
        plt.close(fig)
        return
    except ImportError:
        pass

    # 2. Respaldo de alta resolución con Pillow (ImageDraw + fuentes TrueType Unicode)
    if HAS_PIL:
        from PIL import ImageDraw, ImageFont  # type: ignore

        width, height = 1200, 680
        pil_img = Image.new("RGB", (width, height), (248, 250, 252))
        draw = ImageDraw.Draw(pil_img)

        def _load_font(size: int, bold: bool = False):
            candidates = (
                [
                    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
                    "/System/Library/Fonts/Helvetica.ttc",
                    "C:/Windows/Fonts/arialbd.ttf",
                    "C:/Windows/Fonts/segoeui.ttf",
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                ]
                if bold
                else [
                    "/System/Library/Fonts/Supplemental/Arial.ttf",
                    "/System/Library/Fonts/Helvetica.ttc",
                    "C:/Windows/Fonts/arial.ttf",
                    "C:/Windows/Fonts/segoeui.ttf",
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                ]
            )
            for path in candidates:
                if Path(path).exists():
                    try:
                        return ImageFont.truetype(path, size=size)
                    except Exception:
                        continue
            try:
                return ImageFont.load_default(size=size)
            except TypeError:
                return ImageFont.load_default()

        font_title = _load_font(22, bold=True)
        font_axis = _load_font(16, bold=True)
        font_label = _load_font(14, bold=True)
        font_tick = _load_font(13, bold=False)

        left, right, top, bottom = 105, width - 55, 85, height - 105
        plot_h = bottom - top
        plot_w = right - left

        # Área blanca del gráfico y borde
        draw.rectangle([left, top, right, bottom], fill=(255, 255, 255), outline=(148, 163, 184), width=1)

        def _val_to_y(v: float) -> int:
            clamped = max(0.0, min(1.05, v))
            return int(bottom - (clamped / 1.05) * plot_h)

        # Grilla horizontal y ticks del eje Y
        for tick in (0.0, 0.2, 0.4, 0.6, 0.8, 1.0):
            yt = _val_to_y(tick)
            for gx in range(left, right, 8):
                draw.line([(gx, yt), (min(right, gx + 3), yt)], fill=(203, 213, 225), width=1)
            draw.text((left - 48, yt - 8), f"{tick:.2f}", fill=(15, 23, 42), font=font_tick)

        # Título y etiquetas de ejes
        draw.text((left + 80, 24), title_str, fill=(15, 23, 42), font=font_title)
        draw.text((left, top - 28), y_label_str, fill=(15, 23, 42), font=font_axis)
        draw.text(
            ((left + right) // 2 - 110, height - 38),
            "Captura NIR (ID / Archivo)",
            fill=(15, 23, 42),
            font=font_axis,
        )

        # Barras de cada imagen
        if results:
            n = len(results)
            bar_w = max(36, min(140, plot_w // max(1, n * 3)))
            for idx, item in enumerate(results):
                x_center = left + int((idx + 0.5) * (plot_w / n))
                x0 = max(left + 10, x_center - bar_w // 2)
                x1 = min(right - 10, x_center + bar_w // 2)
                visual_score = max(0.025, item.suitability_score)
                y0 = _val_to_y(visual_score)
                bar_color = (
                    (46, 125, 50) if item.landing_apt == "APTA" else (198, 40, 40)
                )
                draw.rectangle(
                    [x0, y0, x1, bottom],
                    fill=bar_color,
                    outline=(15, 23, 42),
                    width=2,
                )

                # Anotación encima de cada barra: valor numérico exacto + clase
                score_txt = f"{item.suitability_score:.2f}"
                class_txt = f"({item.terrain_class})"
                draw.text(
                    (x_center - 20, max(top + 6, y0 - 40)),
                    score_txt,
                    fill=(15, 23, 42),
                    font=font_axis,
                )
                draw.text(
                    (x_center - len(class_txt) * 4, max(top + 24, y0 - 20)),
                    class_txt,
                    fill=(51, 65, 85),
                    font=font_label,
                )

                # Etiqueta en el eje X
                x_lbl = f"#{item.capture_id}: {item.filename}"
                draw.text(
                    (x_center - len(x_lbl) * 4, bottom + 14),
                    x_lbl,
                    fill=(15, 23, 42),
                    font=font_label,
                )

        # Línea horizontal punteada en 0.60 ("Umbral Zona Apta (0.60)")
        y_thresh = _val_to_y(threshold_val)
        for x in range(left, right, 16):
            draw.line(
                [(x, y_thresh), (min(right, x + 9), y_thresh)],
                fill=(15, 23, 42),
                width=3,
            )

        # Ejes principales X e Y en negrita
        draw.line([(left, top), (left, bottom)], fill=(15, 23, 42), width=3)
        draw.line([(left, bottom), (right, bottom)], fill=(15, 23, 42), width=3)

        # Caja de Leyenda arriba a la derecha
        leg_x0, leg_y0, leg_x1, leg_y1 = right - 295, top + 12, right - 14, top + 108
        draw.rectangle(
            [leg_x0, leg_y0, leg_x1, leg_y1],
            fill=(248, 250, 252),
            outline=(15, 23, 42),
            width=2,
        )
        # Item 1: Línea punteada Umbral 0.60
        draw.line([(leg_x0 + 12, leg_y0 + 22), (leg_x0 + 24, leg_y0 + 22)], fill=(15, 23, 42), width=3)
        draw.line([(leg_x0 + 30, leg_y0 + 22), (leg_x0 + 42, leg_y0 + 22)], fill=(15, 23, 42), width=3)
        draw.text((leg_x0 + 52, leg_y0 + 13), threshold_label, fill=(15, 23, 42), font=font_label)
        # Item 2: Barra verde APTA
        draw.rectangle([leg_x0 + 14, leg_y0 + 44, leg_x0 + 40, leg_y0 + 62], fill=(46, 125, 50), outline=(15, 23, 42))
        draw.text((leg_x0 + 52, leg_y0 + 44), "APTA (>= 0.60)", fill=(15, 23, 42), font=font_label)
        # Item 3: Barra roja NO_APTA
        draw.rectangle([leg_x0 + 14, leg_y0 + 74, leg_x0 + 40, leg_y0 + 92], fill=(198, 40, 40), outline=(15, 23, 42))
        draw.text((leg_x0 + 52, leg_y0 + 74), "NO_APTA (< 0.60)", fill=(15, 23, 42), font=font_label)

        pil_img.save(str(output_png), format="PNG")
        return

    save_rgb_or_gray_png(output_png, canvas)


def process_post_flight_directory(
    images_dir: Path | str,
    payload_csv: Optional[Path | str] = None,
    output_dir: Path | str = "post_flight_output",
) -> Tuple[List[NirImageAnalysisResult], Path, Path]:
    """Ejecuta el pipeline post-vuelo sobre todas las capturas NIR y exporta CSV + PNGs."""
    img_dir = Path(images_dir).resolve()
    out_dir = Path(output_dir).resolve()
    annotated_dir = out_dir / "annotated"
    annotated_dir.mkdir(parents=True, exist_ok=True)

    meta_map = load_payload_metadata(Path(payload_csv) if payload_csv else None)

    image_files = sorted(
        p
        for p in img_dir.iterdir()
        if p.is_file()
        and p.suffix.lower() in (".png", ".jpg", ".jpeg")
        and not p.name.endswith("_segmented.png")
    )
    results: List[NirImageAnalysisResult] = []

    for idx, img_path in enumerate(image_files, start=1):
        match = _NIR_FILENAME_RE.search(img_path.name)
        if match:
            cap_id = int(match.group(1))
            mission_ms = int(match.group(2)) if match.group(2) else (idx * 2000)
        else:
            cap_id = idx
            mission_ms = idx * 2000

        meta = meta_map.get(img_path.name, PayloadMetadataRow(mission_time_ms=mission_ms))
        if meta.mission_time_ms > 0:
            mission_ms = meta.mission_time_ms

        gray = load_nir_grayscale_pixels(img_path)
        h, w = gray.shape

        (
            mean_nir,
            std_rough,
            contrast,
            sat_pct,
            water_pct,
            soil_pct,
            pasture_pct,
            dense_pct,
            terrain,
            apt,
            score,
            annotated_rgb,
        ) = analyze_nir_matrix(gray, roll_deg=meta.roll_deg, pitch_deg=meta.pitch_deg)

        annotated_path = annotated_dir / f"{img_path.stem}_segmented.png"
        save_rgb_or_gray_png(annotated_path, annotated_rgb)

        results.append(
            NirImageAnalysisResult(
                filename=img_path.name,
                capture_id=cap_id,
                mission_time_ms=mission_ms,
                width=w,
                height=h,
                altitude_m=meta.altitude_m,
                latitude_deg=meta.latitude_deg,
                longitude_deg=meta.longitude_deg,
                roll_deg=meta.roll_deg,
                pitch_deg=meta.pitch_deg,
                yaw_deg=meta.yaw_deg,
                tilt_deg=round(math.hypot(meta.roll_deg, meta.pitch_deg), 2),
                mean_nir=mean_nir,
                std_roughness=std_rough,
                local_contrast=contrast,
                saturation_pct=sat_pct,
                water_shadow_pct=water_pct,
                bare_soil_pct=soil_pct,
                open_pasture_pct=pasture_pct,
                dense_veg_pct=dense_pct,
                terrain_class=terrain,
                landing_apt=apt,
                suitability_score=score,
                annotated_image_path=str(annotated_path),
            )
        )

    report_csv_path = out_dir / "post_flight_report.csv"
    with report_csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "filename",
                "capture_id",
                "mission_time_ms",
                "width",
                "height",
                "altitude_m",
                "latitude_deg",
                "longitude_deg",
                "roll_deg",
                "pitch_deg",
                "yaw_deg",
                "tilt_deg",
                "mean_nir",
                "std_roughness",
                "local_contrast",
                "saturation_pct",
                "water_shadow_pct",
                "bare_soil_pct",
                "open_pasture_pct",
                "dense_veg_pct",
                "terrain_class",
                "landing_apt",
                "suitability_score",
                "annotated_image_path",
            ]
        )
        for r in results:
            writer.writerow(
                [
                    r.filename,
                    r.capture_id,
                    r.mission_time_ms,
                    r.width,
                    r.height,
                    f"{r.altitude_m:.2f}",
                    f"{r.latitude_deg:.6f}",
                    f"{r.longitude_deg:.6f}",
                    f"{r.roll_deg:.2f}",
                    f"{r.pitch_deg:.2f}",
                    f"{r.yaw_deg:.2f}",
                    f"{r.tilt_deg:.2f}",
                    f"{r.mean_nir:.2f}",
                    f"{r.std_roughness:.2f}",
                    f"{r.local_contrast:.2f}",
                    f"{r.saturation_pct:.2f}",
                    f"{r.water_shadow_pct:.2f}",
                    f"{r.bare_soil_pct:.2f}",
                    f"{r.open_pasture_pct:.2f}",
                    f"{r.dense_veg_pct:.2f}",
                    r.terrain_class,
                    r.landing_apt,
                    f"{r.suitability_score:.2f}",
                    r.annotated_image_path,
                ]
            )

    summary_chart_path = out_dir / "post_flight_summary.png"
    _generate_summary_chart_png(results, summary_chart_path)
    return results, report_csv_path, summary_chart_path


def build_cli_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Pipeline Post-Vuelo de Imágenes NIR/VIS — CanSat UTN FRRe (ISI 2)"
    )
    parser.add_argument(
        "--images-dir",
        required=True,
        help="Carpeta con las capturas NIR_*.png generadas por la Raspberry Pi",
    )
    parser.add_argument(
        "--payload-csv",
        default=None,
        help="Ruta opcional a payload_log.csv para sincronizar actitud y GPS",
    )
    parser.add_argument(
        "--output-dir",
        default="post_flight_output",
        help="Carpeta donde se exportan post_flight_report.csv y las imágenes segmentadas",
    )
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    args = build_cli_parser().parse_args(argv)
    results, csv_path, chart_path = process_post_flight_directory(
        images_dir=args.images_dir,
        payload_csv=args.payload_csv,
        output_dir=args.output_dir,
    )
    print(f"[OK] Imágenes procesadas: {len(results)}")
    print(f"[OK] Reporte CSV exportado: {csv_path}")
    print(f"[OK] Gráfico resumen PNG:   {chart_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

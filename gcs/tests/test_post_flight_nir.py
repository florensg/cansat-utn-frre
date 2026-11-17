"""Tests unitarios para el pipeline post-vuelo de procesamiento de píxeles NIR reales."""

from __future__ import annotations

import csv
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import numpy as np

from src.post_flight_nir import (
    load_nir_grayscale_pixels,
    process_post_flight_directory,
    save_rgb_or_gray_png,
)


class TestPostFlightNirPipeline(unittest.TestCase):
    """Verifica la decodificación real de píxeles PNG y la clasificación de aptitud."""

    def test_png_roundtrip_and_terrain_classification(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            images_dir = tmp_path / "nir_images"
            output_dir = tmp_path / "output"
            images_dir.mkdir(parents=True, exist_ok=True)

            # 1. Imagen sintética de pastizal abierto uniforme (reflectancia ~140, baja rugosidad) -> APTA
            pasture_img = np.full((64, 64), 140, dtype=np.uint8)
            pasture_img[::2, ::2] = 144
            img1_path = images_dir / "NIR_00001_12000ms.png"
            save_rgb_or_gray_png(img1_path, pasture_img)

            # Verificar que la decodificación recupera los píxeles reales y no el flujo DEFLATE
            decoded = load_nir_grayscale_pixels(img1_path)
            self.assertEqual(decoded.shape, (64, 64))
            self.assertAlmostEqual(float(np.mean(decoded)), 141.0, delta=1.5)

            # 2. Imagen sintética de agua / sombra profunda (reflectancia ~30) -> NO_APTA
            water_img = np.full((64, 64), 30, dtype=np.uint8)
            img2_path = images_dir / "NIR_00002_14000ms.png"
            save_rgb_or_gray_png(img2_path, water_img)

            # 3. Imagen sintética de vegetación densa con alta textura/obstáculos -> NO_APTA
            rng = np.random.default_rng(42)
            rough_veg = rng.integers(90, 250, size=(64, 64), dtype=np.uint8)
            img3_path = images_dir / "NIR_00003_16000ms.png"
            save_rgb_or_gray_png(img3_path, rough_veg)

            # Crear payload_log.csv sincronizado
            payload_csv = tmp_path / "payload_log.csv"
            with payload_csv.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(
                    [
                        "received_at",
                        "mission_time_ms",
                        "state",
                        "altitude_m",
                        "latitude_deg",
                        "longitude_deg",
                        "roll_deg",
                        "pitch_deg",
                        "yaw_deg",
                        "sun_azimuth_deg",
                        "nir_image_file",
                        "terrain_class",
                        "landing_apt",
                        "suitability_score",
                    ]
                )
                writer.writerow(
                    [
                        "2026-11-16T12:00:12",
                        12000,
                        "Descent",
                        250.0,
                        -27.4511,
                        -58.9865,
                        4.0,
                        -3.0,
                        180.0,
                        135.0,
                        "NIR_00001_12000ms.png",
                        "PASTIZAL_ABIERTO",
                        "APTA",
                        0.85,
                    ]
                )
                writer.writerow(
                    [
                        "2026-11-16T12:00:14",
                        14000,
                        "Descent",
                        210.0,
                        -27.4512,
                        -58.9866,
                        5.0,
                        2.0,
                        190.0,
                        136.0,
                        "NIR_00002_14000ms.png",
                        "AGUA_O_SOMBRA",
                        "NO_APTA",
                        0.15,
                    ]
                )

            results, report_csv, summary_png = process_post_flight_directory(
                images_dir=images_dir,
                payload_csv=payload_csv,
                output_dir=output_dir,
            )

            self.assertEqual(len(results), 3)
            self.assertTrue(report_csv.exists())
            self.assertTrue(summary_png.exists())

            # Imagen 1: Pastizal abierto -> APTA
            self.assertEqual(results[0].terrain_class, "PASTIZAL_ABIERTO")
            self.assertEqual(results[0].landing_apt, "APTA")
            self.assertGreaterEqual(results[0].suitability_score, 0.55)
            self.assertTrue(Path(results[0].annotated_image_path).exists())

            # Imagen 2: Agua o sombra -> NO_APTA
            self.assertEqual(results[1].terrain_class, "AGUA_O_SOMBRA")
            self.assertEqual(results[1].landing_apt, "NO_APTA")
            self.assertLess(results[1].suitability_score, 0.55)

            # Imagen 3: Rugosidad/obstáculos altos -> NO_APTA
            self.assertEqual(results[2].landing_apt, "NO_APTA")

            # 4. Verificar que load_payload_metadata soporta columnas cortas (alt_m, lat, lon)
            short_csv = tmp_path / "payload_short.csv"
            with short_csv.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(
                    ["mission_time_ms", "state", "alt_m", "lat", "lon", "roll_deg", "pitch_deg", "nir_image_file"]
                )
                writer.writerow(
                    [12000, "Descent", 248.5, -27.4501, -58.9802, 2.0, -1.5, "NIR_00001_12000ms.png"]
                )

            results_short, _, _ = process_post_flight_directory(
                images_dir=images_dir,
                payload_csv=short_csv,
                output_dir=output_dir,
            )
            self.assertAlmostEqual(results_short[0].altitude_m, 248.5)
            self.assertAlmostEqual(results_short[0].latitude_deg, -27.4501)
            self.assertAlmostEqual(results_short[0].longitude_deg, -58.9802)


if __name__ == "__main__":
    unittest.main()

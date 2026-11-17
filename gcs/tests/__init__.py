"""Inicialización del paquete de tests con resolución automática de ruta a src/."""

import sys
from pathlib import Path

_GCS_ROOT = str(Path(__file__).resolve().parent.parent)
if _GCS_ROOT not in sys.path:
    sys.path.insert(0, _GCS_ROOT)

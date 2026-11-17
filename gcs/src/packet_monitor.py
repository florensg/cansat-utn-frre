"""Monitor de paquetes recibidos y perdidos para la Estación Terrena (Requisito G14 y G8).

Distingue explícitamente:
- PACKET_COUNT: Contador de paquetes transmitidos por el OBC.
- RX_COUNT: Cantidad real de paquetes válidos recibidos por la estación terrena (G14).
- LOST_COUNT: Paquetes perdidos detectados por saltos hacia adelante en PACKET_COUNT.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class PacketStats:
    """Instantánea de las métricas de recepción de paquetes."""

    rx_count: int = 0
    last_tx_packet_count: Optional[int] = None
    lost_count: int = 0
    duplicate_or_reordered_count: int = 0
    reboot_count: int = 0
    malformed_count: int = 0

    @property
    def packet_loss_percent(self) -> float:
        """Porcentaje de pérdida de paquetes respecto al total esperado."""
        total_expected = self.rx_count + self.lost_count
        if total_expected <= 0:
            return 0.0
        return (self.lost_count / total_expected) * 100.0


class PacketMonitor:
    """Rastrea la secuencia de PACKET_COUNT del OBC y contabiliza RX_COUNT y pérdidas."""

    def __init__(self) -> None:
        self._rx_count: int = 0
        self._last_packet_count: Optional[int] = None
        self._lost_count: int = 0
        self._duplicate_or_reordered_count: int = 0
        self._reboot_count: int = 0
        self._malformed_count: int = 0

    def register_valid_packet(self, packet_count: int) -> PacketStats:
        """Registra la recepción de un paquete válido con su PACKET_COUNT del OBC."""
        if packet_count < 0:
            raise ValueError("packet_count no puede ser negativo")

        self._rx_count += 1

        if self._last_packet_count is None:
            self._last_packet_count = packet_count
            return self.stats

        # Si el OBC reinicia su secuencia a PACKET_COUNT == 1 luego de un vuelo previo,
        # reiniciar limpiamente los contadores de la sesión actual y registrar el evento de reboot
        if packet_count == 1 and self._last_packet_count > 1:
            self._reboot_count += 1
            self._rx_count = 1
            self._lost_count = 0
            self._duplicate_or_reordered_count = 0
            self._malformed_count = 0
            self._last_packet_count = 1
            return self.stats

        delta = packet_count - self._last_packet_count
        if delta == 1:
            # Secuencia perfecta sin pérdida
            self._last_packet_count = packet_count
        elif delta > 1:
            # Salto hacia adelante: se perdieron (delta - 1) paquetes en el enlace
            self._lost_count += delta - 1
            self._last_packet_count = packet_count
        elif delta == 0:
            # Paquete duplicado
            self._duplicate_or_reordered_count += 1
        else:
            # packet_count retrocedió: si cayó a un valor bajo (<= 5) indica reinicio sin FRAM
            if packet_count <= 5 and self._last_packet_count > 5:
                self._reboot_count += 1
                self._last_packet_count = packet_count
            else:
                self._duplicate_or_reordered_count += 1

        return self.stats

    def register_malformed_packet(self) -> PacketStats:
        """Incrementa el contador de tramas corruptas o rechazadas por el decoder."""
        self._malformed_count += 1
        return self.stats

    def reset(self) -> None:
        """Reinicia todos los contadores para una nueva sesión de vuelo."""
        self._rx_count = 0
        self._last_packet_count = None
        self._lost_count = 0
        self._duplicate_or_reordered_count = 0
        self._reboot_count = 0
        self._malformed_count = 0

    @property
    def stats(self) -> PacketStats:
        """Retorna una copia inmutable de las estadísticas actuales."""
        return PacketStats(
            rx_count=self._rx_count,
            last_tx_packet_count=self._last_packet_count,
            lost_count=self._lost_count,
            duplicate_or_reordered_count=self._duplicate_or_reordered_count,
            reboot_count=self._reboot_count,
            malformed_count=self._malformed_count,
        )

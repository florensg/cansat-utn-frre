#pragma once

#include <cstdint>

namespace cansat {

// Identificador asignado por CONAE. Se reemplaza por el numero definitivo del equipo.
constexpr uint16_t kTeamId = 1000;

// Periodo de telemetria y comunicaciones.
constexpr uint32_t kTelemetryPeriodMs = 1000;
constexpr uint32_t kSerialBaud = 115200;

// Parametros de deteccion de apogeo y segundo paracaidas.
constexpr float kMinApogeeAltitudeM = 100.0f;
constexpr float kApogeeDropM = 5.0f;
constexpr float kSecondParachuteRatio = 0.80f;
constexpr uint32_t kMinFlightTimeMs = 10000;

// Robustez.
constexpr uint32_t kWatchdogTimeoutMs = 4000;

// Direcciones I2C de los dispositivos persistentes.
constexpr uint8_t kFramI2cAddress = 0x50;
constexpr uint8_t kRtcI2cAddress = 0x52;

}  // namespace cansat

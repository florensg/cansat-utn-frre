#pragma once

#include <cstdint>

namespace cansat {

// Placa seleccionada: ESP32-S3 DevKitC-1 de 44 pines.
// Fuente: Elementos del CanSat v2, componente C-002 (Seleccionado).
// El STM32 C-001 figura como Descartado en la misma planilla.
constexpr const char* kBoardName = "ESP32-S3 DevKitC-1";

// Identificador asignado por CONAE. Se reemplaza por el numero definitivo del equipo.
constexpr uint16_t kTeamId = 1000;

// Periodo de telemetria y comunicaciones.
constexpr uint32_t kTelemetryPeriodMs = 1000;
constexpr uint32_t kSerialBaud = 115200;

// Bus I2C para FRAM MB85RC256V, RTC RV-3028-C7 e INA219.
// GPIO8/GPIO9 son los pines I2C por defecto del core Arduino-ESP32 para ESP32-S3.
// Confirmar el pinout definitivo con IEM 3 antes de cablear.
constexpr uint8_t kI2cSdaPin = 8;
constexpr uint8_t kI2cSclPin = 9;
constexpr uint32_t kI2cClockHz = 400000;

// UART de radio y placa secundaria: se definen cuando IEM 3 cierre el pinout.
// El monitor USB usa Serial (USB CDC) y no consume pines del ESP32-S3.

// Parametros de deteccion de apogeo y segundo paracaidas.
constexpr float kMinApogeeAltitudeM = 100.0f;
constexpr float kApogeeDropM = 5.0f;
constexpr float kSecondParachuteRatio = 0.80f;
constexpr uint32_t kMinFlightTimeMs = 10000;

// Robustez. El ESP32-S3 usa el Task Watchdog Timer (TWDT); el RTC WDT queda como
// respaldo de ultimo recurso.
constexpr uint32_t kWatchdogTimeoutMs = 4000;

// Direcciones I2C de los dispositivos persistentes.
constexpr uint8_t kFramI2cAddress = 0x50;
constexpr uint8_t kRtcI2cAddress = 0x52;

}  // namespace cansat

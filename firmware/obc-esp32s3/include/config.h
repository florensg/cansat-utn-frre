#pragma once

#include <cstdint>

namespace cansat {

// Placa seleccionada: ESP32-S3 DevKitC-1 de 44 pines (U1 en PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf).
// Fuente: Elementos del CanSat v2, componente C-002 (Seleccionado).
constexpr const char* kBoardName = "ESP32-S3 DevKitC-1";

// Identificador asignado por CONAE. Se reemplaza por el numero definitivo del equipo.
constexpr uint16_t kTeamId = 1000;

// Periodo de telemetria y comunicaciones.
constexpr uint32_t kTelemetryPeriodMs = 1000;
constexpr uint32_t kSerialBaud = 115200;

// ---------------------------------------------------------------------------
// Bus I2C del OBC (MPU1: MPU-6050, INA1: INA219, VRAM1: FRAM MB85RC256V,
// y soporte para BMP280 I2C y XIAO U8/U9).
// En el esquema v3 se traza hacia GPIO 13/14 (o GPIO 8/9 unificado).
// ---------------------------------------------------------------------------
constexpr uint8_t kI2cSdaPin = 8;
constexpr uint8_t kI2cSclPin = 9;
constexpr uint8_t kI2cAltSdaPin = 13;
constexpr uint8_t kI2cAltSclPin = 14;
constexpr uint32_t kI2cClockHz = 400000;

// ---------------------------------------------------------------------------
// Bus SPI para U2 (BMP280) segun PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf:
//   SDI (MOSI) -> GPIO 8
//   SCK        -> GPIO 9
//   CSB        -> GPIO 4
//   SDO (MISO) -> GPIO 5
// ---------------------------------------------------------------------------
constexpr uint8_t kBmp280SpiMosiPin = 8;
constexpr uint8_t kBmp280SpiSckPin = 9;
constexpr uint8_t kBmp280SpiCsPin = 4;
constexpr uint8_t kBmp280SpiMisoPin = 5;

// ---------------------------------------------------------------------------
// Señales discretas en PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf:
//   M1 (Servo SG90) PWM -> GPIO 7
//   MPU-6050 INT        -> GPIO 14
// ---------------------------------------------------------------------------
constexpr uint8_t kPinServoV3 = 7;
constexpr uint8_t kPinMpu6050Int = 14;

// ---------------------------------------------------------------------------
// UARTs de vuelo segun PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf:
//   1. XBee S1 Module (USART1) y enlace con Raspberry Pi Zero W (GPIO14/15).
//   2. GPS NEO-M8N (U3) a 9600 baudios.
// ---------------------------------------------------------------------------
constexpr uint8_t kGpsRxPin = 18;
constexpr uint8_t kGpsTxPin = 17;
constexpr uint32_t kGpsBaud = 9600;

constexpr uint8_t kRadioAndRpiRxPin = 1;
constexpr uint8_t kRadioAndRpiTxPin = 2;
constexpr uint32_t kRadioAndRpiBaud = 115200;

// Parametros de deteccion de apogeo y segundo paracaidas.
constexpr float kMinApogeeAltitudeM = 100.0f;
constexpr float kApogeeDropM = 5.0f;
constexpr float kSecondParachuteRatio = 0.80f;
constexpr uint32_t kMinFlightTimeMs = 10000;

// Robustez: Task Watchdog Timer (TWDT).
constexpr uint32_t kWatchdogTimeoutMs = 4000;

// Direcciones I2C de los dispositivos en el circuito v3:
// - OBC: MPU-6050 (0x68), INA219 (0x40), FRAM MB85RC256V (0x50), BMP280 (0x76/0x77)
// - XIAO camaras (esclavos I2C): U8 VIS (0x30), U9 NIR (0x31)
// - RPi Zero (en su propio bus GPIO2/GPIO3): LIS3MDL (0x1C/0x1E), ADS1115 (0x48)
constexpr uint8_t kFramI2cAddress = 0x50;
constexpr uint8_t kMpu6050I2cAddress = 0x68;
constexpr uint8_t kBmp280I2cAddress = 0x76;
constexpr uint8_t kBmp280AltI2cAddress = 0x77;
constexpr uint8_t kLis3mdlI2cAddress = 0x1C;
constexpr uint8_t kIna219I2cAddress = 0x40;
constexpr uint8_t kAds1115I2cAddress = 0x48;
constexpr uint8_t kXiaoVisI2cAddress = 0x30;
constexpr uint8_t kXiaoNirI2cAddress = 0x31;

// INA219: shunt de 0.1 ohm en la placa del modulo.
constexpr float kIna219ShuntOhms = 0.1f;

// ADS1115: cuatro canales single-ended para los TSL251RD (U4..U7).
constexpr uint8_t kAds1115ChannelCount = 4;

}  // namespace cansat

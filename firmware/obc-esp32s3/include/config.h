#pragma once

#include <cstdint>

namespace cansat {

// Placa seleccionada: ESP32-S3 DevKitC-1 de 44 pines (U1 en el pinmap v.2).
// Fuente: Elementos del CanSat v2, componente C-002 (Seleccionado).
constexpr const char* kBoardName = "ESP32-S3 DevKitC-1";

// Identificador asignado por CONAE. Se reemplaza por el numero definitivo del equipo.
constexpr uint16_t kTeamId = 1000;

// Periodo de telemetria y comunicaciones.
constexpr uint32_t kTelemetryPeriodMs = 1000;
constexpr uint32_t kSerialBaud = 115200;

// ---------------------------------------------------------------------------
// Bus I2C del OBC (MPU1: MPU-6050, INA1: INA219, VRAM1: FRAM MB85RC256V,
// BMP280 y RTC RV-3028-C7). Bus unico en GPIO 8/9 segun el pinmap v.2.
// ---------------------------------------------------------------------------
constexpr uint8_t kI2cSdaPin = 8;
constexpr uint8_t kI2cSclPin = 9;
// Obsoletos: con el pinmap v.2 el bus I2C es unico en GPIO 8/9. GPIO13/14 son
// la UART de la Raspberry Pi. No reactivar un bus alternativo en estos pines.
constexpr uint8_t kI2cAltSdaPin = 13;
constexpr uint8_t kI2cAltSclPin = 14;
constexpr uint32_t kI2cClockHz = 400000;

// ---------------------------------------------------------------------------
// BMP280: en el pinmap v.2 va por I2C en 0x76 con CSB atado a 3V3. Estas
// constantes SPI quedan solo para el codigo legacy y NO deben usarse: GPIO4 es
// la interrupcion del MPU-6050 y GPIO5 el PPS del GPS.
// ---------------------------------------------------------------------------
constexpr uint8_t kBmp280SpiMosiPin = 8;
constexpr uint8_t kBmp280SpiSckPin = 9;
constexpr uint8_t kBmp280SpiCsPin = 4;
constexpr uint8_t kBmp280SpiMisoPin = 5;

// ---------------------------------------------------------------------------
// Señales discretas segun el pinmap v.2:
//   M1 (Servo SG90) PWM -> GPIO 7 (mecanismo de liberacion via driver)
//   MPU-6050 INT        -> GPIO 4
//   RV-3028-C7 INT      -> GPIO10 (opcional)
//   NEO-M8N PPS         -> GPIO5 (opcional, no conectado)
//   Panel solar (ADC)   -> GPIO1 (requiere acondicionamiento)
//   Baliza (driver)     -> GPIO6 (reservado; el OBC NO controla la baliza:
//                          C8/E8/E9 exigen bateria e interruptor independientes)
// ---------------------------------------------------------------------------
constexpr uint8_t kPinServoV3 = 7;
constexpr uint8_t kPinMpu6050Int = 4;
constexpr uint8_t kPinRtcInt = 10;
constexpr uint8_t kPinGpsPps = 5;
constexpr uint8_t kPinPanelSolarAdc = 1;
// Reservado para una baliza opcional controlada por el OBC. Hoy no se usa:
// la baliza obligatoria es independiente (ver indicadores-v2.md).
constexpr uint8_t kPinBuzzer = 6;

// ---------------------------------------------------------------------------
// UARTs de vuelo segun el pinmap v.2 (GPIO14/15/16 quedan para las UART; las
// llaves EN_NADIR/EN_SERVO/EN_SEC se movieron a 38/39/40):
//   GPS NEO-M8N:  ESP RX = GPIO17, ESP TX = GPIO18 (9600 8N1, UART1)
//   XBee-PRO:     ESP RX = GPIO15, ESP TX = GPIO16 (baud por confirmar, UART2)
//   Raspberry Pi: ESP RX = GPIO14, ESP TX = GPIO13 (115200 8N1, UART0)
// ---------------------------------------------------------------------------
constexpr uint8_t kGpsRxPin = 17;
constexpr uint8_t kGpsTxPin = 18;
constexpr uint32_t kGpsBaud = 9600;

constexpr uint8_t kXbeeRxPin = 15;
constexpr uint8_t kXbeeTxPin = 16;
constexpr uint32_t kXbeeBaud = 115200;

constexpr uint8_t kRpiRxPin = 14;
constexpr uint8_t kRpiTxPin = 13;
constexpr uint32_t kRpiBaud = 115200;

// ---------------------------------------------------------------------------
// Power gating (CIRCUITO ELECTRONICO Y ALIMENTACION v.2).
//
// Cada riel conmuta con una llave electronica (TPS2553 / TPS22946) o un
// AO3401A excitado por un 2N7002. La compuerta del 2N7002 es activa en HIGH:
//   GPIO HIGH -> 2N7002 conduce -> gate del AO3401A a GND -> riel ENCENDIDO.
// Un pull-up de 100k mantiene el riel APAGADO si el GPIO queda flotante.
//
// BLOQUEANTE RESUELTO (2026-11-16): las UART de RPi/XBee quedan en GPIO13-16
// y las llaves EN_NADIR/EN_SERVO/EN_SEC se movieron a GPIO38/39/40.
// Catriel debe confirmar y cablear esos tres pines antes de volar.
// ---------------------------------------------------------------------------
constexpr bool kPowerGatingEnabled = true;
constexpr uint8_t kPowerPinUnassigned = 0xFF;

// Rieles base: siempre alimentados, sin GPIO de habilitacion en las listas.
constexpr uint8_t kPowerPinRtc = kPowerPinUnassigned;
constexpr uint8_t kPowerPinFram = kPowerPinUnassigned;
constexpr uint8_t kPowerPinSensors = kPowerPinUnassigned;
constexpr uint8_t kPowerPinGps = kPowerPinUnassigned;
constexpr uint8_t kPowerPinRadio = kPowerPinUnassigned;

// Llaves de la lista de transistores (reubicadas fuera de las UART).
constexpr uint8_t kPowerPinRpi = 11;        // EN_RPI
constexpr uint8_t kPowerPinXiaoCenit = 12;  // EN_CENIT
constexpr uint8_t kPowerPinXiaoNadir = 38;  // EN_NADIR reubicado
constexpr uint8_t kPowerPinServo = 39;      // EN_SERVO reubicado
constexpr uint8_t kPowerPinSec = 40;        // EN_SEC reubicado

// Tiempo de establecimiento tras conmutar un riel antes de volver a hablar con
// el dispositivo. La RPi Zero tarda decenas de segundos en bootear, por eso se
// reprograma la re-sondeo de las camaras XIAO con un margen mayor.
constexpr uint32_t kPowerRailSettleMs = 50;
constexpr uint32_t kXiaoBootAfterRailMs = 4000;

// Indicador de encendido (Anexo A v2, E6). La logica de patrones ya esta en
// flight_core/status.cpp y la actuacion en src/status_output.cpp; los pines se
// asignan cuando IEM 4 cablee los LED (Fase 5). Mientras valgan
// kPowerPinUnassigned, status_output no toca ningun GPIO.
constexpr uint8_t kPinStatusLedOk = kPowerPinUnassigned;
constexpr uint8_t kPinStatusLedFault = kPowerPinUnassigned;

// Parametros de deteccion de apogeo y segundo paracaidas.
constexpr float kMinApogeeAltitudeM = 100.0f;
constexpr float kApogeeDropM = 5.0f;
constexpr float kSecondParachuteRatio = 0.80f;
constexpr uint32_t kMinFlightTimeMs = 10000;

// Robustez: Task Watchdog Timer (TWDT).
constexpr uint32_t kWatchdogTimeoutMs = 4000;

// Direcciones I2C de los dispositivos en el circuito v.2:
// - OBC: MPU-6050 (0x68), INA219 (0x40), FRAM MB85RC256V (0x50), BMP280 (0x76/0x77),
//   RTC RV-3028-C7 (0x52)
// - XIAO camaras (esclavos I2C): U8 VIS (0x30), U9 NIR (0x31)
// - RPi Zero (en su propio bus GPIO2/GPIO3): LIS3MDL (0x1C/0x1E), ADS1115 (0x48)
constexpr uint8_t kFramI2cAddress = 0x50;
constexpr uint8_t kRtcI2cAddress = 0x52;
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

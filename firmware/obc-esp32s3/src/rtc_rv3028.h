#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "flight_core/mission_time.h"

namespace cansat {

// ---------------------------------------------------------------------------
// Driver minimo del RV-3028-C7 (I2C 0x52) para el OBC del CanSat v.2.
//
// Solo se implementa lo que la mision necesita: leer y escribir fecha/hora en
// UTC y saber si el valor es confiable despues de un corte de energia.
//
// Mapa de registros (datasheet Micro Crystal, seccion 3):
//   0x00 Seconds (BCD)   0x01 Minutes   0x02 Hours   0x03 Weekday
//   0x04 Date            0x05 Months    0x06 Years
//   0x0E Status (bit0 = PORF: perdida de energia, hora no confiable)
//   0x10 Control2 (bit1 = 12/24 h, bit0 = reset por software)
//
// Ojo con el orden: el weekday esta en 0x03 y la fecha del mes en 0x04.
// ---------------------------------------------------------------------------

struct RtcHealth {
  bool present = false;
  bool time_valid = false;
  bool power_loss = false;  // PORF activo: la hora se perdio
  uint8_t status = 0;
};

// Sondea el RTC, lo fuerza a modo 24 h y devuelve su estado de salud.
RtcHealth rtc_init(TwoWire& bus, uint8_t address);

// Lee fecha/hora. Devuelve false si el bus fallo o el BCD no es coherente.
bool rtc_read_datetime(TwoWire& bus, uint8_t address, DateTime& out);

// Escribe fecha/hora y limpia los flags de estado (incluido PORF).
bool rtc_write_datetime(TwoWire& bus, uint8_t address, const DateTime& dt);

// Limpia los flags de interrupcion y PORF.
bool rtc_clear_flags(TwoWire& bus, uint8_t address);

}  // namespace cansat

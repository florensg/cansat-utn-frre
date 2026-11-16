#pragma once

#include "flight_core/status.h"

namespace cansat {

// ---------------------------------------------------------------------------
// Actuacion fisica del LED de estado (requisito E6).
//
// Los pines viven en `config.h` como `kPinStatusLedOk` / `kPinStatusLedFault`.
// Hoy estan sin asignar (`kPowerPinUnassigned`): el hardware de los LED se
// cablea en la Fase 5, asi que estas funciones no tocan ningun pin hasta que
// se definan. La logica de patrones (probada en `native`) ya queda lista.
// ---------------------------------------------------------------------------
void status_output_init();
void status_output_apply(const StatusLeds& leds);

}  // namespace cansat

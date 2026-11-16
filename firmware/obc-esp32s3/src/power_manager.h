#pragma once

#include <cstdint>

#include "flight_core/power.h"

namespace cansat {

// Capa de actuacion de los rieles conmutables del circuito v.2.
//
// La politica (que riel va encendido en cada estado) vive en flight_core/power.h
// y es pura/testeable. Este modulo solo la traduce a GPIO y mantiene el estado
// actual. Con kPowerGatingEnabled = false no se maneja ningun pin y todos los
// rieles se reportan encendidos (comportamiento del circuito anterior).

// Configura los GPIO de habilitacion y aplica el estado inicial (LaunchPad).
void power_manager_init();

// Aplica la politica para el estado de vuelo y el override de la GCS.
// Devuelve la mascara de rieles que cambiaron de estado (0 si no hubo cambios).
PowerMask power_manager_apply(State state, bool payload_prepared);

// Mascara de rieles actualmente encendidos.
PowerMask power_manager_mask();

// Consulta si un riel esta encendido segun el ultimo apply.
bool power_manager_rail_on(PowerRail rail);

// Override manual de la carga util (comando RPI / RPIOFF desde la GCS).
void power_manager_set_payload_prepared(bool on);
bool power_manager_payload_prepared();

}  // namespace cansat

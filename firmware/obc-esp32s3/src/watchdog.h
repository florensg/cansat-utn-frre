#pragma once

namespace cansat {

// ---------------------------------------------------------------------------
// Task Watchdog Timer (TWDT) del ESP32-S3.
//
// Se inicializa al final de `setup()` (no antes: el sondeo de sensores y el
// arranque de las XIAO pueden tardar) y se refresca una vez por vuelta de
// `loop()`. Si el lazo se cuelga mas de `kWatchdogTimeoutMs`, el TWDT reinicia
// el OBC. El tiempo de mision sobrevive porque el RTC y la FRAM son
// persistentes (F2).
//
// Devuelve false si el TWDT no pudo inicializarse; en ese caso el firmware
// sigue funcionando sin watchdog en lugar de abortar.
// ---------------------------------------------------------------------------
bool watchdog_init();
void watchdog_feed();

// Causa del reinicio actual, para diagnostico post-vuelo.
//   'P' power-on      'S' software      'W' task watchdog
//   'I' interrupt WDT 'X' panic         'B' brownout
//   'E' externo       'D' deep sleep    'O' SDIO
//   '?' desconocida
char reset_reason_tag();
const char* reset_reason_name();

}  // namespace cansat

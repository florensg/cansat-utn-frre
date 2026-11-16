#include "watchdog.h"

#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "config.h"

namespace cansat {
namespace {

bool g_enabled = false;

}  // namespace

bool watchdog_init() {
  esp_err_t err = ESP_OK;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // Arduino-ESP32 3.x / IDF 5.x: la configuracion va por estructura.
  esp_task_wdt_config_t config = {};
  config.timeout_ms = kWatchdogTimeoutMs;
  config.idle_core_mask = 0;
  config.trigger_panic = true;
  err = esp_task_wdt_init(&config);
#else
  // Arduino-ESP32 2.x: timeout en segundos y flag de panic.
  constexpr uint32_t kTimeoutSeconds = (kWatchdogTimeoutMs + 999u) / 1000u;
  err = esp_task_wdt_init(kTimeoutSeconds, true);
#endif
  // ESP_ERR_INVALID_STATE: el TWDT ya estaba inicializado (por el core o por
  // una llamada previa). No es un error: se reutiliza la configuracion vigente.
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return false;
  }

  // Suscribe la tarea de `loop()` (nullptr = tarea actual).
  const esp_err_t added = esp_task_wdt_add(nullptr);
  g_enabled = (added == ESP_OK || added == ESP_ERR_INVALID_STATE);
  return g_enabled;
}

void watchdog_feed() {
  // Si la inicializacion fallo no se toca el TWDT: es preferible volar sin
  // watchdog a provocar un error por llamar a una API no inicializada.
  if (g_enabled) {
    esp_task_wdt_reset();
  }
}

char reset_reason_tag() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return 'P';
    case ESP_RST_EXT:
      return 'E';
    case ESP_RST_SW:
      return 'S';
    case ESP_RST_PANIC:
      return 'X';
    case ESP_RST_INT_WDT:
      return 'I';
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return 'W';
    case ESP_RST_DEEPSLEEP:
      return 'D';
    case ESP_RST_BROWNOUT:
      return 'B';
    case ESP_RST_SDIO:
      return 'O';
    default:
      break;
  }
  return '?';
}

const char* reset_reason_name() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "power-on";
    case ESP_RST_EXT:
      return "externo";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt-wdt";
    case ESP_RST_TASK_WDT:
      return "task-wdt";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deep-sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      break;
  }
  return "desconocido";
}

}  // namespace cansat

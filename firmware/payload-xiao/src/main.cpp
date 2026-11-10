#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "esp_camera.h"

#include <cstdint>
#include <cstdio>

// ============================================================================
// Firmware para U8 y U9 (Seeed Studio XIAO ESP32-S3 Sense - OV2640 Cenit / Nadir)
// Alineado a PROPUESTA_DE_CIRCUITO_CANSAT_v3.pdf y Arquitectura General Hardware.drawio:
//   - Pines D4 (SDA = GPIO 5) y D5 (SCL = GPIO 6) conectados al bus I2C del OBC
//   - U8 (Camara Cenit - paracaidas): direccion esclava I2C 0x30
//   - U9 (Camara Nadir Visible):      direccion esclava I2C 0x31
//   - Al recibir el comando 0x01 / 0x02 / 0x03 en el registro 0x01 desde el OBC
//     (en Launch, Apogeo o 2do Paracaidas), captura fotogramas JPEG en la
//     tarjeta microSD de la placa Sense y expone el contador de capturas por I2C.
// ============================================================================

#ifndef XIAO_I2C_SLAVE_ADDR
#define XIAO_I2C_SLAVE_ADDR 0x30
#endif

#ifndef XIAO_CAMERA_ROLE_NIR
#define XIAO_CAMERA_ROLE_NIR 0
#endif

namespace {

// Pines internos de la camara OV2640 en XIAO ESP32-S3 Sense
constexpr int kPwdnGpioNum = -1;
constexpr int kResetGpioNum = -1;
constexpr int kXclkGpioNum = 10;
constexpr int kSiodGpioNum = 40;
constexpr int kSiocGpioNum = 39;

constexpr int kY9GpioNum = 48;
constexpr int kY8GpioNum = 11;
constexpr int kY7GpioNum = 12;
constexpr int kY6GpioNum = 14;
constexpr int kY5GpioNum = 16;
constexpr int kY4GpioNum = 18;
constexpr int kY3GpioNum = 17;
constexpr int kY2GpioNum = 15;
constexpr int kVsyncGpioNum = 38;
constexpr int kHrefGpioNum = 47;
constexpr int kPclkGpioNum = 13;

// Pin CS de la microSD en la placa de expansion XIAO ESP32-S3 Sense
constexpr int kSdCsPin = 21;

// Pines D4 (SDA) y D5 (SCL) del header del XIAO ESP32-S3 conectados en el PDF v3
constexpr int kXiaoI2cSdaPin = 5;  // D4
constexpr int kXiaoI2cSclPin = 6;  // D5

volatile uint8_t g_acquisition_mode = 0;  // 0 = idle, 1=ascent, 2=apogee, 3=para2
volatile bool g_capture_requested = false;
uint16_t g_capture_count = 0;
uint32_t g_last_periodic_capture_ms = 0;
bool g_camera_ok = false;
bool g_sd_ok = false;

void on_i2c_receive(int num_bytes) {
  if (num_bytes < 1) {
    return;
  }
  const uint8_t reg = static_cast<uint8_t>(Wire.read());
  if (reg == 0x01 && Wire.available() > 0) {
    const uint8_t cmd = static_cast<uint8_t>(Wire.read());
    g_acquisition_mode = cmd;
    if (cmd > 0) {
      g_capture_requested = true;
    }
  }
  while (Wire.available() > 0) {
    Wire.read();
  }
}

void on_i2c_request() {
  uint8_t resp[3];
  resp[0] = (g_camera_ok ? 0x01 : 0x00) | (g_sd_ok ? 0x02 : 0x00) |
            ((g_acquisition_mode & 0x0F) << 4);
  resp[1] = static_cast<uint8_t>((g_capture_count >> 8) & 0xFF);
  resp[2] = static_cast<uint8_t>(g_capture_count & 0xFF);
  Wire.write(resp, sizeof(resp));
}

bool init_camera() {
  camera_config_t config{};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = kY2GpioNum;
  config.pin_d1 = kY3GpioNum;
  config.pin_d2 = kY4GpioNum;
  config.pin_d3 = kY5GpioNum;
  config.pin_d4 = kY6GpioNum;
  config.pin_d5 = kY7GpioNum;
  config.pin_d6 = kY8GpioNum;
  config.pin_d7 = kY9GpioNum;
  config.pin_xclk = kXclkGpioNum;
  config.pin_pclk = kPclkGpioNum;
  config.pin_vsync = kVsyncGpioNum;
  config.pin_href = kHrefGpioNum;
  config.pin_sccb_sda = kSiodGpioNum;
  config.pin_sccb_scl = kSiocGpioNum;
  config.pin_pwdn = kPwdnGpioNum;
  config.pin_reset = kResetGpioNum;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_SVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  return esp_camera_init(&config) == ESP_OK;
}

bool capture_and_save_frame() {
  if (!g_camera_ok) {
    return false;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr) {
    return false;
  }

  ++g_capture_count;
  if (g_sd_ok) {
    char path[64];
    std::snprintf(path, sizeof(path), "/%s_%05u_m%u.jpg",
                  XIAO_CAMERA_ROLE_NIR ? "NIR" : "VIS",
                  static_cast<unsigned>(g_capture_count),
                  static_cast<unsigned>(g_acquisition_mode));
    File file = SD.open(path, FILE_WRITE);
    if (file) {
      file.write(fb->buf, fb->len);
      file.close();
    }
  }

  esp_camera_fb_return(fb);
  return true;
}

}  // namespace

void setup() {
  Serial.begin(115200);

  g_camera_ok = init_camera();
  g_sd_ok = SD.begin(kSdCsPin);

  // Inicia el bus I2C en modo esclavo sobre D4 (SDA) y D5 (SCL) como en v3
  Wire.onReceive(on_i2c_receive);
  Wire.onRequest(on_i2c_request);
  Wire.begin(static_cast<uint8_t>(XIAO_I2C_SLAVE_ADDR), kXiaoI2cSdaPin,
             kXiaoI2cSclPin, 400000);

  Serial.printf("XIAO ESP32-S3 Sense [%s] I2C=0x%02X CAM=%s SD=%s\n",
                XIAO_CAMERA_ROLE_NIR ? "U9-NIR" : "U8-VIS",
                static_cast<unsigned>(XIAO_I2C_SLAVE_ADDR),
                g_camera_ok ? "OK" : "FAIL", g_sd_ok ? "OK" : "FAIL");
}

void loop() {
  const uint32_t now = millis();

  if (g_capture_requested) {
    g_capture_requested = false;
    capture_and_save_frame();
    g_last_periodic_capture_ms = now;
  }

  // Durante el descenso (modos 2 y 3), captura un cuadro cada 2 segundos
  if ((g_acquisition_mode == 2 || g_acquisition_mode == 3) &&
      (now - g_last_periodic_capture_ms) >= 2000u) {
    g_last_periodic_capture_ms = now;
    capture_and_save_frame();
  }
}

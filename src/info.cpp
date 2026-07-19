#include "config.h"

namespace {
  unsigned long infoStart = 0;
}

void infoSetup() {
  infoStart = millis();
}

void infoLoop() {
  if (buttonPressed(BTN_BACK)) {
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  
  u8g2.clearBuffer();
  drawFunctionHeader("Info/Sobre");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 22);
  u8g2.print(FIRMWARE_VERSION);
  u8g2.setCursor(0, 34);
  u8g2.print("ESP32: ");
  #if defined(CONFIG_IDF_TARGET_ESP32S2)
    u8g2.print("ESP32-S2");
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    u8g2.print("ESP32-S3");
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    u8g2.print("ESP32-C3");
  #else
    u8g2.print("ESP32");
  #endif
  u8g2.setCursor(0, 46);
  u8g2.print("Free RAM: ");
  u8g2.print(ESP.getFreeHeap());
  u8g2.setCursor(0, 58);
  u8g2.print("CPU: ");
  u8g2.print(ESP.getCpuFreqMHz());
  u8g2.print(" MHz");
  u8g2.sendBuffer();
}

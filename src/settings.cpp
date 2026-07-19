#include "config.h"

namespace {
  int settingItem = 0;
  const char* settingsNames[] = {"Brilho OLED", "Som", "Reset"};
  const int NUM_SETTINGS = 3;
  int brightness = 128;
  bool soundEnabled = true;
}

void settingsSetup() {
  settingItem = 0;
  brightness = prefs.getUChar("brightness", 128);
  soundEnabled = prefs.getBool("sound", true);
}

void settingsLoop() {
  if (buttonPressed(BTN_BACK)) {
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  
  if (buttonPressed(BTN_UP) && settingItem > 0) {
    settingItem--;
    delay(80);
  }
  if (buttonPressed(BTN_DOWN) && settingItem < NUM_SETTINGS - 1) {
    settingItem++;
    delay(80);
  }
  
  if (buttonPressed(BTN_SELECT)) {
    switch (settingItem) {
      case 0:
        brightness += 32;
        if (brightness > 255) brightness = 64;
        u8g2.setContrast(brightness);
        prefs.putUChar("brightness", brightness);
        Serial.printf("[SET] Brilho: %d\n", brightness);
        break;
      case 1:
        soundEnabled = !soundEnabled;
        prefs.putBool("sound", soundEnabled);
        Serial.printf("[SET] Som: %s\n", soundEnabled ? "ON" : "OFF");
        break;
      case 2:
        prefs.clear();
        u8g2.clearBuffer();
        drawFunctionHeader("Configuracoes");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 28);
        u8g2.print("Resetando...");
        u8g2.sendBuffer();
        delay(1000);
        ESP.restart();
        break;
    }
    delay(200);
  }
  
  u8g2.clearBuffer();
  drawFunctionHeader("Configuracoes");
  u8g2.setFont(u8g2_font_6x10_tr);
  
  for (int i = 0; i < NUM_SETTINGS; i++) {
    int y = 22 + (i * 12);
    u8g2.setCursor(0, y);
    if (i == settingItem) u8g2.print("> ");
    else u8g2.print("  ");
    u8g2.print(settingsNames[i]);
    u8g2.print(": ");
    if (i == 0) u8g2.print(brightness);
    else if (i == 1) u8g2.print(soundEnabled ? "ON" : "OFF");
  }
  u8g2.sendBuffer();
}

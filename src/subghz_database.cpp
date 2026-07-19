#include "config.h"

namespace {
  int dbIndex = 0;
  unsigned long dbLastTX = 0;
}

const SubGHzEntry subGHzDatabase[] = {
  {"Garagem 433", 433.92, 0, 0x123456, 24},
  {"Portao 433",  433.92, 0, 0xABCDEF, 24},
  {"Alarme 315",  315.00, 1, 0x555555, 24},
  {"Luz RF 433",  433.92, 2, 0x000001, 24},
  {"Carro 315",   315.00, 0, 0x111111, 24},
  {"Carro 433",   433.92, 0, 0x222222, 24},
  {"Portao 868",  868.35, 0, 0x333333, 24},
  {"Alarme 868",  868.35, 1, 0x444444, 24},
  {"Garagem 315", 315.00, 0, 0x666666, 24},
  {"Controle TV", 433.92, 2, 0x777777, 24},
};
const int SUBGHZ_DB_COUNT = sizeof(subGHzDatabase) / sizeof(subGHzDatabase[0]);

void subGHzDBSetup() {
  dbIndex = 0;
  dbLastTX = 0;
}

void subGHzDBLoop() {
  if (buttonPressed(BTN_BACK)) {
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  
  if (buttonPressed(BTN_UP) && dbIndex > 0) {
    dbIndex--;
    delay(80);
  }
  if (buttonPressed(BTN_DOWN) && dbIndex < SUBGHZ_DB_COUNT - 1) {
    dbIndex++;
    delay(80);
  }
  
  if (buttonPressed(BTN_SELECT)) {
    const SubGHzEntry& entry = subGHzDatabase[dbIndex];
    // Define frequência temporária para transmissão
    currentSignal.frequency = (uint32_t)(entry.frequency * 1000);
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(entry.frequency);
    ELECHOUSE_cc1101.setModulation(0);
    transmitCode(entry.code, entry.bits, protocols[entry.protocolIndex]);
    Serial.printf("[DB] TX: %s | %.2f MHz | 0x%06X\n", entry.name, entry.frequency, entry.code);
    dbLastTX = millis();
    delay(200);
  }
  
  u8g2.clearBuffer();
  drawFunctionHeader("Sub-GHz DB");
  u8g2.setFont(u8g2_font_6x10_tr);
  
  for (int i = 0; i < 4 && (dbIndex + i) < SUBGHZ_DB_COUNT; i++) {
    int idx = dbIndex + i;
    int y = 22 + (i * 10);
    u8g2.setCursor(0, y);
    if (idx == dbIndex) u8g2.print("> ");
    else u8g2.print("  ");
    u8g2.print(subGHzDatabase[idx].name);
  }
  
  u8g2.setCursor(0, 62);
  u8g2.print("Freq: ");
  u8g2.print(subGHzDatabase[dbIndex].frequency);
  u8g2.sendBuffer();
}

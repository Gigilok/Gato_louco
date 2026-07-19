#include "config.h"

namespace {
  int smSlot = 0;
  int smAction = 0;
}

void signalManagerSetup() {
  smSlot = 0;
  smAction = 0;
  loadAllSignals();
}

void signalManagerLoop() {
  // Verifica BACK primeiro para sair
  if (buttonPressed(BTN_BACK)) {
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  
  if (buttonPressed(BTN_UP) && smSlot > 0) {
    smSlot--;
    delay(80);
  }
  if (buttonPressed(BTN_DOWN) && smSlot < MAX_SAVED_SIGNALS - 1) {
    smSlot++;
    delay(80);
  }
  
  if (buttonPressed(BTN_SELECT)) {
    if (savedSignals[smSlot].active) {
      currentSignal = savedSignals[smSlot].signal;
      selectedSignalSlot = smSlot;
      hasSavedSignal = true;
      Serial.printf("[SIG] Slot %d carregado: %.3f MHz\n", smSlot, currentSignal.frequency / 1000.0);
    } else {
      Serial.printf("[SIG] Slot %d vazio\n", smSlot);
    }
    delay(200);
  }
  
  // Long press nao implementado aqui para evitar conflito com BACK
  // Use SELECT para carregar, BACK para sair
  
  u8g2.clearBuffer();
  drawFunctionHeader("Gerenciar Sinais");
  u8g2.setFont(u8g2_font_6x10_tr);
  
  for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
    int y = 22 + (i * 10);
    u8g2.setCursor(0, y);
    if (i == smSlot) u8g2.print("> ");
    else u8g2.print("  ");
    u8g2.print("Slot ");
    u8g2.print(i + 1);
    u8g2.print(": ");
    if (savedSignals[i].active) {
      u8g2.print(savedSignals[i].name);
      u8g2.print(" ");
      u8g2.print(savedSignals[i].signal.frequency / 1000.0);
    } else {
      u8g2.print("Vazio");
    }
  }
  u8g2.sendBuffer();
}

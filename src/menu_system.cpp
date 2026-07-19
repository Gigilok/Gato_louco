#include "config.h"
#include "icons.h"

#define CAT_RF_SCANNER    0
#define CAT_WIRELESS      1
#define CAT_ATTACKS       2
#define CAT_RECORDINGS    3
#define CAT_CONFIG        4
#define CAT_PAIRING       5
#define NUM_CATEGORIES    6

const char* category_names[NUM_CATEGORIES] = {
  "RF Scanner", "Wireless", "Ataques", "Gravacoes", "Config", "Pareamento"
};

struct MenuItem {
  const char* name;
  int iconIndex;
  void (*setupFunc)();
  void (*loopFunc)();
};

const MenuItem rfItems[] = {
  {"nRF24 Scanner",     0, nrfScannerSetup,      nrfScannerLoop},
  {"nRF24 Jammer",      1, nrfJammerSetup,       nrfJammerLoop},
  {"CC1101 Scanner",    2, cc1101ScannerSetup,   cc1101ScannerLoop},
  {"CC1101 FreqSweep",  5, cc1101FreqSweepSetup, cc1101FreqSweepLoop},
};
const int RF_ITEM_COUNT = 4;

const MenuItem wirelessItems[] = {
  {"BLE Scan",       6, bleScanSetup,          bleScanLoop},
  {"WiFi Scan",      7, wifiDeauthSetup,       wifiDeauthLoop},
  {"WiFi Connect",   7, wifiConnectSetup,      wifiConnectLoop},
  {"WiFi Deauth",    7, wifiDeauthAttackSetup, wifiDeauthAttackLoop},
  {"Sour Apple",     8, sourAppleSetup,        sourAppleLoop},
};
const int WIRELESS_ITEM_COUNT = 5;

const MenuItem attackItems[] = {
  {"CC1101 BruteForce", 4, cc1101BruteForceSetup, cc1101BruteForceLoop},
  {"Sub-GHz DB",        4, subGHzDBSetup,          subGHzDBLoop},
  {"Evil Twin",         7, [](){ iniciarAtaqueEvilTwin(); }, [](){ verificarEstadoAtaques(); }},
  {"Handshake",         9, [](){ iniciarCapturaHandshakeAlvo(); }, [](){ verificarEstadoAtaques(); }},
  {"WPS Brute",         4, [](){ iniciarAtaqueWPS(); }, [](){ verificarEstadoAtaques(); }},
  {"Deauth Flood",      1, [](){ 
    deauthFloodAtivo = true; 
    deauthAlvoIndex = redeSelecionada >= 0 ? redeSelecionada : 0;
    ataqueEmAndamento = true;
    deauthPacketCount = 0;
    deauthLastSend = millis();
  }, [](){ verificarEstadoAtaques(); }},
  {"MITM (Offload)",   10, [](){ iniciarAtaqueMITM(); }, [](){ verificarEstadoAtaques(); }},
};
const int ATTACK_ITEM_COUNT = 7;

const MenuItem recordingItems[] = {
  {"CC1101 Capture",  3, cc1101TransceiverSetup, cc1101TransceiverLoop},
  {"Gerenciar Sinais", 9, signalManagerSetup,   signalManagerLoop},
};
const int RECORDING_ITEM_COUNT = 2;

const MenuItem configItems[] = {
  {"Configuracoes", 10, settingsSetup, settingsLoop},
  {"Info/Sobre",    11, infoSetup,     infoLoop},
};
const int CONFIG_ITEM_COUNT = 2;

const MenuItem pairingItems[] = {
  {"Parear Device",  10, pairingSetup, pairingLoop},
  {"Gerenciar Pares", 9, [](){ 
    u8g2.clearBuffer();
    drawFunctionHeader("Pareamentos");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("Dispositivos:");
    u8g2.setCursor(0, 40);
    u8g2.print(numDispositivosPareados);
    u8g2.print("/");
    u8g2.print(PAIR_MAX_DEVICES);
    u8g2.sendBuffer();
    delay(2000);
    current_screen = 0;
    drawMenu();
  }, [](){}},
};
const int PAIRING_ITEM_COUNT = 2;

namespace {
  int menuLevel = 0;
  int selectedCategory = 0;
  int selectedItem = 0;
  int currentFuncIndex = -1;

  struct FuncMapping {
    int cat;
    int item;
    void (*setupFunc)();
    void (*loopFunc)();
  };
  FuncMapping allFuncs[64];
  int totalFuncs = 0;
}

void initMenuSystem() {
  totalFuncs = 0;
  for (int i = 0; i < RF_ITEM_COUNT; i++) {
    allFuncs[totalFuncs++] = {CAT_RF_SCANNER, i, rfItems[i].setupFunc, rfItems[i].loopFunc};
  }
  for (int i = 0; i < WIRELESS_ITEM_COUNT; i++) {
    allFuncs[totalFuncs++] = {CAT_WIRELESS, i, wirelessItems[i].setupFunc, wirelessItems[i].loopFunc};
  }
  for (int i = 0; i < ATTACK_ITEM_COUNT; i++) {
    allFuncs[totalFuncs++] = {CAT_ATTACKS, i, attackItems[i].setupFunc, attackItems[i].loopFunc};
  }
  for (int i = 0; i < RECORDING_ITEM_COUNT; i++) {
    allFuncs[totalFuncs++] = {CAT_RECORDINGS, i, recordingItems[i].setupFunc, recordingItems[i].loopFunc};
  }
  for (int i = 0; i < CONFIG_ITEM_COUNT; i++) {
    allFuncs[totalFuncs++] = {CAT_CONFIG, i, configItems[i].setupFunc, configItems[i].loopFunc};
  }
  for (int i = 0; i < PAIRING_ITEM_COUNT; i++) {
    allFuncs[totalFuncs++] = {CAT_PAIRING, i, pairingItems[i].setupFunc, pairingItems[i].loopFunc};
  }
  menuLevel = 0;
  selectedCategory = 0;
  selectedItem = 0;
  currentFuncIndex = -1;
}

void drawHeader() {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 7);
  u8g2.print("Crazy Cat");
  u8g2.setCursor(95, 7);
  u8g2.print(FIRMWARE_SHORT);
  u8g2.drawHLine(0, 9, 128);
}

void drawFunctionHeader(const char* title) {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 7);
  u8g2.print("Crazy Cat");
  u8g2.setCursor(95, 7);
  u8g2.print(FIRMWARE_SHORT);
  u8g2.drawHLine(0, 9, 128);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 20);
  u8g2.print(title);
  u8g2.drawHLine(0, 22, 128);
}

void drawCategoryMenu() {
  u8g2.clearBuffer();
  drawHeader();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 20);
  u8g2.print("Categorias:");
  int start = selectedCategory;
  for (int i = 0; i < 4 && (start + i) < NUM_CATEGORIES; i++) {
    int idx = start + i;
    int y = 34 + (i * 10);
    if (idx == selectedCategory) {
      u8g2.drawBox(0, y - 8, 128, 10);
      u8g2.setDrawColor(0);
    }
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(4, y);
    u8g2.print("> ");
    u8g2.print(category_names[idx]);
    if (idx == selectedCategory) {
      u8g2.setDrawColor(1);
    }
  }
  u8g2.sendBuffer();
}

void drawItemMenu() {
  u8g2.clearBuffer();
  drawHeader();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 20);
  u8g2.print(category_names[selectedCategory]);

  const MenuItem* items = nullptr;
  int itemCount = 0;
  switch (selectedCategory) {
    case CAT_RF_SCANNER:   items = rfItems;       itemCount = RF_ITEM_COUNT; break;
    case CAT_WIRELESS:     items = wirelessItems; itemCount = WIRELESS_ITEM_COUNT; break;
    case CAT_ATTACKS:      items = attackItems;   itemCount = ATTACK_ITEM_COUNT; break;
    case CAT_RECORDINGS:   items = recordingItems; itemCount = RECORDING_ITEM_COUNT; break;
    case CAT_CONFIG:       items = configItems;   itemCount = CONFIG_ITEM_COUNT; break;
    case CAT_PAIRING:      items = pairingItems;  itemCount = PAIRING_ITEM_COUNT; break;
  }
  if (!items) return;

  int visible = 4;
  int start = (selectedItem / visible) * visible;

  for (int i = 0; i < visible && (start + i) < itemCount; i++) {
    int idx = start + i;
    int y = 34 + (i * 10);
    if (idx == selectedItem) {
      u8g2.drawBox(0, y - 8, 128, 10);
      u8g2.setDrawColor(0);
    }
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(4, y);
    u8g2.print("  ");
    u8g2.print(items[idx].name);
    if (idx == selectedItem) {
      u8g2.setDrawColor(1);
    }
  }

  int totalPages = (itemCount + visible - 1) / visible;
  int currentPage = selectedItem / visible;
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(100, 63);
  u8g2.print(currentPage + 1);
  u8g2.print("/");
  u8g2.print(totalPages);
  u8g2.sendBuffer();
}

void drawMenu() {
  if (menuLevel == 0) drawCategoryMenu();
  else drawItemMenu();
}

void handleMenu() {
  if (menuLevel == 0) {
    if (buttonPressed(BTN_UP)) {
      if (selectedCategory > 0) { selectedCategory--; drawMenu(); delay(80); }
    }
    if (buttonPressed(BTN_DOWN)) {
      if (selectedCategory < NUM_CATEGORIES - 1) { selectedCategory++; drawMenu(); delay(80); }
    }
    if (buttonPressed(BTN_SELECT)) {
      menuLevel = 1; selectedItem = 0; drawMenu(); delay(150);
    }
  } else {
    int itemCount = 0;
    switch (selectedCategory) {
      case CAT_RF_SCANNER:   itemCount = RF_ITEM_COUNT; break;
      case CAT_WIRELESS:     itemCount = WIRELESS_ITEM_COUNT; break;
      case CAT_ATTACKS:      itemCount = ATTACK_ITEM_COUNT; break;
      case CAT_RECORDINGS:   itemCount = RECORDING_ITEM_COUNT; break;
      case CAT_CONFIG:       itemCount = CONFIG_ITEM_COUNT; break;
      case CAT_PAIRING:      itemCount = PAIRING_ITEM_COUNT; break;
    }
    if (buttonPressed(BTN_UP)) {
      if (selectedItem > 0) { selectedItem--; drawMenu(); delay(80); }
    }
    if (buttonPressed(BTN_DOWN)) {
      if (selectedItem < itemCount - 1) { selectedItem++; drawMenu(); delay(80); }
    }
    if (buttonPressed(BTN_SELECT)) {
      currentFuncIndex = -1;
      for (int i = 0; i < totalFuncs; i++) {
        if (allFuncs[i].cat == selectedCategory && allFuncs[i].item == selectedItem) {
          currentFuncIndex = i; break;
        }
      }
      if (currentFuncIndex >= 0 && allFuncs[currentFuncIndex].setupFunc) {
        current_screen = 1; back_pressed = false;
        u8g2.clearBuffer(); drawHeader();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(10, 35); u8g2.print("Carregando...");
        u8g2.sendBuffer();

        if (selectedCategory == CAT_ATTACKS) {
          wifi_mode_t currentMode = WiFi.getMode();
          bool pairingAPActive = (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA);

          if (!pairingAPActive && pairingState != PAIR_SHOWING_CODE) {
            wifiPentestInit();
          } else {
            Serial.println("[MENU] Skipping wifiPentestInit - pairing AP active");
          }
        }

        allFuncs[currentFuncIndex].setupFunc();
      }
    }
    if (buttonPressed(BTN_BACK)) {
      menuLevel = 0; selectedItem = 0; drawMenu(); delay(150);
    }
  }
}

void runCurrentFunction() {
  if (current_screen == 0) return;
  if (currentFuncIndex >= 0 && currentFuncIndex < totalFuncs && allFuncs[currentFuncIndex].loopFunc) {
    allFuncs[currentFuncIndex].loopFunc();
  }
}

// ==========================================
// SIMULACAO DE BOTOES PARA SERIAL/BT/MQTT
// ==========================================
void simulateButtonUp() {
  if (current_screen == 0) {
    if (menuLevel == 0) {
      if (selectedCategory > 0) { selectedCategory--; drawMenu(); }
    } else {
      if (selectedItem > 0) { selectedItem--; drawMenu(); }
    }
  }
}

void simulateButtonDown() {
  if (current_screen == 0) {
    if (menuLevel == 0) {
      if (selectedCategory < NUM_CATEGORIES - 1) { selectedCategory++; drawMenu(); }
    } else {
      int itemCount = 0;
      switch (selectedCategory) {
        case CAT_RF_SCANNER: itemCount = RF_ITEM_COUNT; break;
        case CAT_WIRELESS: itemCount = WIRELESS_ITEM_COUNT; break;
        case CAT_ATTACKS: itemCount = ATTACK_ITEM_COUNT; break;
        case CAT_RECORDINGS: itemCount = RECORDING_ITEM_COUNT; break;
        case CAT_CONFIG: itemCount = CONFIG_ITEM_COUNT; break;
        case CAT_PAIRING: itemCount = PAIRING_ITEM_COUNT; break;
      }
      if (selectedItem < itemCount - 1) { selectedItem++; drawMenu(); }
    }
  }
}

void simulateButtonSelect() {
  if (current_screen == 0) {
    if (menuLevel == 0) {
      menuLevel = 1; selectedItem = 0; drawMenu();
    } else {
      currentFuncIndex = -1;
      for (int i = 0; i < totalFuncs; i++) {
        if (allFuncs[i].cat == selectedCategory && allFuncs[i].item == selectedItem) {
          currentFuncIndex = i; break;
        }
      }
      if (currentFuncIndex >= 0 && allFuncs[currentFuncIndex].setupFunc) {
        current_screen = 1; back_pressed = false;

        if (selectedCategory == CAT_ATTACKS) {
          wifi_mode_t currentMode = WiFi.getMode();
          bool pairingAPActive = (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA);

          if (!pairingAPActive && pairingState != PAIR_SHOWING_CODE) {
            wifiPentestInit();
          } else {
            Serial.println("[MENU] Skipping wifiPentestInit - pairing AP active");
          }
        }

        allFuncs[currentFuncIndex].setupFunc();
      }
    }
  }
}

void simulateButtonBack() {
  if (current_screen == 1) {
    // CORRECAO CRITICA: Para ataques WiFi antes de simular BACK
    pararAtaqueAtual();
    back_pressed = true;
  } else if (menuLevel == 1) {
    menuLevel = 0; selectedItem = 0; drawMenu();
  }
}

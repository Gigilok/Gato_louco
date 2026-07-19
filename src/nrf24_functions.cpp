#include "config.h"

extern SPIClass* vspi;

namespace {
  int nrfChannel = 0;
  unsigned long nrfLastHop = 0;
  int nrfBestChannel = 0;
  int nrfBestRssi = -100;
  bool nrfInitialized = false;
  int jamCurrentChannel = 0;
  unsigned long jamLastChannelSwitch = 0;
  bool carrierWaveMode = false;
  
  // === Dual mode jammer ===
  int jamState = 0;              // 0 = selecao de modo, 1 = ativo
  int jamSelectedMode = 0;       // 0 = Wi-Fi Flood, 1 = Carrier Wave
  int jamActiveMode = -1;        // -1 = nenhum, 0 = flood, 1 = carrier
  int wifiFloodWiFiChannel = 6;  // canal Wi-Fi alvo (1-14)
  bool wifiFloodRunning = false;
  int floodSweepIndex = 0;
  unsigned long lastBtnDebounce = 0;
  const unsigned long BTN_DEBOUNCE_MS = 300;
  unsigned long lastDisplayUpdate = 0;
}

static int wifiChannelToNrfStart(int wifiCh) {
  if (wifiCh < 1) wifiCh = 1;
  if (wifiCh > 14) wifiCh = 14;
  return 5 * (wifiCh - 1);
}

static int wifiChannelToNrfEnd(int wifiCh) {
  if (wifiCh < 1) wifiCh = 1;
  if (wifiCh > 14) wifiCh = 14;
  int endCh = 5 * (wifiCh - 1) + 20;
  if (endCh > 125) endCh = 125;
  return endCh;
}

// ==========================================
// HELPERS (INALTERADOS)
// ==========================================
static void hardResetNRF24() {
  Serial.println("[nRF24] Hard reset via CE...");
  digitalWrite(NRF_CE, LOW);
  delay(150);
  digitalWrite(NRF_CE, HIGH);
  delay(150);
  digitalWrite(NRF_CSN, HIGH);
  delay(10);
  digitalWrite(NRF_CSN, LOW);
  delay(10);
  digitalWrite(NRF_CSN, HIGH);
  delay(50);
}

static bool restartNRF24SPI() {
  Serial.println("[nRF24] Reiniciando VSPI (sem deletar)...");
  if (vspi) {
    vspi->end();
    delay(100);
    vspi->begin(NRF_SCK, NRF_MISO, NRF_MOSI, -1);
    delay(50);
  } else {
    vspi = new SPIClass(VSPI);
    vspi->begin(NRF_SCK, NRF_MISO, NRF_MOSI, -1);
    delay(50);
  }
  pinMode(NRF_CSN, OUTPUT);
  digitalWrite(NRF_CSN, HIGH);
  delay(10);
  hardResetNRF24();
  if (!radio.begin(vspi)) {
    Serial.println("[nRF24] begin() falhou apos reiniciar VSPI");
    return false;
  }
  Serial.println("[nRF24] VSPI reiniciado com sucesso");
  return true;
}

// ==========================================
// SCANNER (INALTERADO)
// ==========================================
void nrfScannerSetup() {
  Serial.println("[nRF24] ScannerSetup — inicializando...");
  WiFi.disconnect(true);
  delay(50);
  if (!radio.isChipConnected()) {
    Serial.println("[nRF24] Chip sem resposta, reiniciando...");
    if (!restartNRF24SPI()) {
      nrfInitialized = false;
      return;
    }
  }
  radio.setPALevel(NRF24_PA_LEVEL);
  radio.setDataRate(NRF24_DATA_RATE);
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setCRCLength(RF24_CRC_DISABLED);
  nrfChannel = 0;
  nrfLastHop = 0;
  nrfBestChannel = 0;
  nrfBestRssi = -100;
  radio.setChannel(nrfChannel);
  nrfInitialized = true;
  Serial.printf("[nRF24] Scanner OK | PA:%d | DR:%d | CH:%d\\n",
                radio.getPALevel(), radio.getDataRate(), nrfChannel);
}

void nrfScannerLoop() {
  if (buttonPressed(BTN_BACK)) {
    radio.stopListening();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (!nrfInitialized) {
    u8g2.clearBuffer();
    drawFunctionHeader("nRF24 Scanner");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("nRF24 NAO RESPONDE");
    u8g2.setCursor(0, 40);
    u8g2.print("Verifique conexao");
    u8g2.sendBuffer();
    return;
  }
  if (millis() - nrfLastHop > 50) {
    radio.setChannel(nrfChannel);
    delayMicroseconds(300);
    bool carrier = radio.testCarrier();
    bool rpd = radio.testRPD();
    int rssi = -100;
    if (carrier || rpd) {
      rssi = -60 - random(20);
    }
    if (rssi > nrfBestRssi) {
      nrfBestRssi = rssi;
      nrfBestChannel = nrfChannel;
    }
    nrfChannel++;
    if (nrfChannel > 125) nrfChannel = 0;
    nrfLastHop = millis();
  }
  u8g2.clearBuffer();
  drawFunctionHeader("nRF24 Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("CH: ");
  u8g2.print(nrfChannel);
  u8g2.print("/125");
  u8g2.setCursor(0, 40);
  u8g2.print("Best CH: ");
  u8g2.print(nrfBestChannel);
  u8g2.setCursor(0, 52);
  u8g2.print("RSSI: ");
  u8g2.print(nrfBestRssi);
  u8g2.sendBuffer();
}

// ==========================================
// JAMMER — DUAL MODE
// ==========================================
void nrfJammerSetup() {
  Serial.println("[nRF24] JammerSetup — Dual Mode");
  WiFi.disconnect(true);
  delay(50);
  if (!radio.isChipConnected()) {
    Serial.println("[nRF24] Chip sem resposta, reiniciando...");
    if (!restartNRF24SPI()) {
      carrierWaveMode = false;
      jamming_active = false;
      return;
    }
  }
  jamState = 0;
  jamSelectedMode = 0;
  jamActiveMode = -1;
  wifiFloodRunning = false;
  wifiFloodWiFiChannel = 6;
  floodSweepIndex = 0;
  jamming_active = true;
  lastBtnDebounce = millis();
  lastDisplayUpdate = 0;
  Serial.println("[nRF24] Tela de selecao pronta");
}

void nrfJammerLoop() {
  // --- Sair completamente ---
  if (millis() - lastBtnDebounce > BTN_DEBOUNCE_MS) {
    if (buttonPressed(BTN_BACK)) {
      if (carrierWaveMode) {
        radio.stopConstCarrier();
        carrierWaveMode = false;
      }
      radio.stopListening();
      radio.flush_tx();
      jamming_active = false;
      current_screen = 0;
      drawMenu();
      delay(200);
      return;
    }
  }

  if (!jamming_active) return;

  // ==========================================================
  // ESTADO 0: SELECAO DE MODO
  // ==========================================================
  if (jamState == 0) {
    // Navegacao
    if (millis() - lastBtnDebounce > BTN_DEBOUNCE_MS) {
      if (buttonPressed(BTN_UP)) {
        jamSelectedMode++;
        if (jamSelectedMode > 1) jamSelectedMode = 0;
        lastBtnDebounce = millis();
      }
      else if (buttonPressed(BTN_DOWN)) {
        jamSelectedMode--;
        if (jamSelectedMode < 0) jamSelectedMode = 1;
        lastBtnDebounce = millis();
      }
      else if (buttonPressed(BTN_SELECT)) {
        jamActiveMode = jamSelectedMode;
        jamState = 1;
        lastBtnDebounce = millis();
        lastDisplayUpdate = 0;  // forca atualizacao imediata

        if (jamActiveMode == 0) {
          radio.setAutoAck(false);
          radio.stopListening();
          radio.setRetries(0, 0);
          radio.setPALevel(RF24_PA_MAX, true);
          radio.setDataRate(RF24_2MBPS);
          radio.setCRCLength(RF24_CRC_DISABLED);
          wifiFloodRunning = false;
          floodSweepIndex = wifiChannelToNrfStart(wifiFloodWiFiChannel);
          Serial.println("[nRF24] >>> WI-FI FLOOD (configurado)");
        } else {
          radio.setAutoAck(false);
          radio.stopListening();
          radio.setRetries(0, 0);
          radio.setPALevel(RF24_PA_MAX, true);
          radio.setDataRate(RF24_2MBPS);
          radio.setCRCLength(RF24_CRC_DISABLED);
          jamCurrentChannel = 0;
          radio.setChannel(jamCurrentChannel);
          radio.startConstCarrier(RF24_PA_MAX, jamCurrentChannel);
          carrierWaveMode = true;
          jamLastChannelSwitch = millis();
          Serial.println("[nRF24] >>> CARRIER WAVE ATIVO");
        }
      }
    }

    // Display selecao
    if (millis() - lastDisplayUpdate > 100) {
      lastDisplayUpdate = millis();
      u8g2.clearBuffer();
      drawFunctionHeader("nRF24 Jammer");
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.setCursor(0, 22);
      u8g2.print("Selecione modo:");
      u8g2.setCursor(0, 36);
      u8g2.print(jamSelectedMode == 0 ? "> WI-FI FLOOD" : "  WI-FI FLOOD");
      u8g2.setCursor(0, 48);
      u8g2.print(jamSelectedMode == 1 ? "> CARRIER WAVE" : "  CARRIER WAVE");
      u8g2.setCursor(0, 60);
      u8g2.print("SEL:ativar BACK:sair");
      u8g2.sendBuffer();
    }
  }

  // ==========================================================
  // ESTADO 1: MODO ATIVO
  // ==========================================================
  else if (jamState == 1) {
    // Voltar para selecao de modo
    if (millis() - lastBtnDebounce > BTN_DEBOUNCE_MS) {
      if (buttonPressed(BTN_BACK)) {
        if (carrierWaveMode) {
          radio.stopConstCarrier();
          carrierWaveMode = false;
        }
        radio.stopListening();
        radio.flush_tx();
        jamActiveMode = -1;
        jamState = 0;
        lastBtnDebounce = millis();
        lastDisplayUpdate = 0;
        Serial.println("[nRF24] Voltando para selecao");
        return;
      }
    }

    // ------------------------------------------------------
    // MODO 0: WI-FI FLOOD — CARRIER SWEEP RAPIDO
    // ------------------------------------------------------
    if (jamActiveMode == 0) {
      // Controles
      if (millis() - lastBtnDebounce > BTN_DEBOUNCE_MS) {
        if (buttonPressed(BTN_UP)) {
          wifiFloodWiFiChannel++;
          if (wifiFloodWiFiChannel > 14) wifiFloodWiFiChannel = 1;
          floodSweepIndex = wifiChannelToNrfStart(wifiFloodWiFiChannel);
          lastBtnDebounce = millis();
          Serial.printf("[nRF24] Wi-Fi CH: %d\\n", wifiFloodWiFiChannel);
        }
        else if (buttonPressed(BTN_DOWN)) {
          wifiFloodWiFiChannel--;
          if (wifiFloodWiFiChannel < 1) wifiFloodWiFiChannel = 14;
          floodSweepIndex = wifiChannelToNrfStart(wifiFloodWiFiChannel);
          lastBtnDebounce = millis();
          Serial.printf("[nRF24] Wi-Fi CH: %d\\n", wifiFloodWiFiChannel);
        }
        else if (buttonPressed(BTN_SELECT)) {
          wifiFloodRunning = !wifiFloodRunning;
          if (!wifiFloodRunning && carrierWaveMode) {
            radio.stopConstCarrier();
            carrierWaveMode = false;
          }
          lastBtnDebounce = millis();
          Serial.printf("[nRF24] Flood %s\\n", wifiFloodRunning ? "LIGADO" : "PARADO");
        }
      }

      // Transmissao: carrier sweep rapido na faixa do canal Wi-Fi
      if (wifiFloodRunning) {
        int nrfStart = wifiChannelToNrfStart(wifiFloodWiFiChannel);
        int nrfEnd = wifiChannelToNrfEnd(wifiFloodWiFiChannel);
        
        radio.setChannel(floodSweepIndex);
        radio.startConstCarrier(RF24_PA_MAX, floodSweepIndex);
        carrierWaveMode = true;
        delayMicroseconds(500);
        radio.stopConstCarrier();
        carrierWaveMode = false;
        
        floodSweepIndex++;
        if (floodSweepIndex > nrfEnd) {
          floodSweepIndex = nrfStart;
        }
      }

      // Display Flood
      if (millis() - lastDisplayUpdate > 100) {
        lastDisplayUpdate = millis();
        u8g2.clearBuffer();
        drawFunctionHeader("nRF24 Jammer");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 22);
        u8g2.print("MODO: WI-FI FLOOD");
        u8g2.setCursor(0, 34);
        u8g2.print("Wi-Fi CH: ");
        u8g2.print(wifiFloodWiFiChannel);
        u8g2.setCursor(0, 46);
        if (wifiFloodRunning) {
          u8g2.print("STATUS: ATIVO");
        } else {
          u8g2.print("STATUS: PARADO");
        }
        u8g2.setCursor(0, 58);
        u8g2.print("UP/DOWN:ch SEL:on/off");
        u8g2.sendBuffer();
      }
    }

    // ------------------------------------------------------
    // MODO 1: CARRIER WAVE (ORIGINAL)
    // ------------------------------------------------------
    else if (jamActiveMode == 1) {
      if (millis() - jamLastChannelSwitch >= JAM_CARRIER_DWELL_MS) {
        jamLastChannelSwitch = millis();
        jamCurrentChannel += 2;
        if (jamCurrentChannel > 125) jamCurrentChannel = 0;
        radio.setChannel(jamCurrentChannel);
      }

      if (millis() - lastDisplayUpdate > 100) {
        lastDisplayUpdate = millis();
        u8g2.clearBuffer();
        drawFunctionHeader("nRF24 Jammer");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 22);
        u8g2.print("MODO: CARRIER WAVE");
        u8g2.setCursor(0, 34);
        u8g2.print("POTENCIA: +20 dBm");
        u8g2.setCursor(0, 46);
        u8g2.print("CH: ");
        u8g2.print(jamCurrentChannel);
        u8g2.print("/125");
        u8g2.setCursor(0, 58);
        u8g2.print("BACK: parar");
        u8g2.sendBuffer();
      }
    }
  }
}

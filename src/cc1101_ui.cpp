#include "config.h"

namespace {
  int scannerFreqIndex = 0;
  unsigned long scannerLastHop = 0;
  int scannerBestRssi = -120;
  uint32_t scannerBestFreq = 0;
  int sweepIndex = 0;
  unsigned long sweepLast = 0;
}

void cc1101ScannerSetup() {
  scannerFreqIndex = 0;
  scannerLastHop = 0;
  scannerBestRssi = -120;
  scannerBestFreq = 0;
  ELECHOUSE_cc1101.setSidle();
  Serial.println("[CC1101] Scanner iniciado");
}

void cc1101ScannerLoop() {
  if (buttonPressed(BTN_BACK)) {
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }

  if (millis() - scannerLastHop > 100) {
    ELECHOUSE_cc1101.setMHZ(SCAN_FREQUENCIES[scannerFreqIndex] / 1000.0);
    ELECHOUSE_cc1101.SetRx();
    delay(10);
    int rssi = ELECHOUSE_cc1101.getRssi();

    if (rssi > scannerBestRssi) {
      scannerBestRssi = rssi;
      scannerBestFreq = SCAN_FREQUENCIES[scannerFreqIndex];
    }

    scannerFreqIndex++;
    if (scannerFreqIndex >= NUM_FREQS) scannerFreqIndex = 0;
    scannerLastHop = millis();
  }

  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("CH: ");
  u8g2.print(scannerFreqIndex);
  u8g2.print("/");
  u8g2.print(NUM_FREQS);
  u8g2.setCursor(0, 40);
  u8g2.print("Best Freq: ");
  u8g2.print(scannerBestFreq / 1000.0);
  u8g2.setCursor(0, 52);
  u8g2.print("RSSI: ");
  u8g2.print(scannerBestRssi);
  u8g2.sendBuffer();
}

void cc1101TransceiverSetup() {
  resetCC1101State();
  if (!hasSavedSignal) {
    Serial.println("[CC1101] Nenhum sinal salvo. SEL para capturar.");
  } else {
    Serial.printf("[CC1101] Sinal pronto: %d pulsos em %.3f MHz\n",
                  currentSignal.dataLength, currentSignal.frequency / 1000.0);
  }
  Serial.println("[CC1101] Transceiver pronto");
}

void cc1101TransceiverLoop() {
  if (buttonPressed(BTN_BACK)) {
    if (getCCState() == CC_CAPTURING) stopCapture();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }

  // Se esta capturando, processa bordas e atualiza display
  if (getCCState() == CC_CAPTURING) {
    captureStep();

    u8g2.clearBuffer();
    drawFunctionHeader("CC1101 Capture");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("CAPTURANDO...");
    u8g2.setCursor(0, 40);
    u8g2.print("Pulsos: ");
    u8g2.print(getCaptureIndex());
    u8g2.setCursor(0, 52);
    u8g2.print("Freq: ");
    u8g2.print(getDetectedFrequency() / 1000.0);
    u8g2.print(" MHz");
    u8g2.sendBuffer();
    return;
  }

  if (buttonPressed(BTN_SELECT)) {
    if (!hasSavedSignal) {
      uint32_t freq = autoDetectFrequency();
      startCapture(freq);
      Serial.printf("[CC1101] Captura em %.3f MHz\n", freq / 1000.0);
    } else {
      transmitRawSignal();
      Serial.println("[CC1101] Sinal transmitido");
    }
    delay(200);
    return;
  }

  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 Capture");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);

  if (hasSavedSignal) {
    u8g2.print("SEL: Transmitir");
    u8g2.setCursor(0, 40);
    u8g2.print("Pulsos: ");
    u8g2.print(currentSignal.dataLength);
    u8g2.setCursor(0, 52);
    u8g2.print("Freq: ");
    u8g2.print(currentSignal.frequency / 1000.0);
    u8g2.print(" MHz");
  } else {
    u8g2.print("SEL: Capturar sinal");
    u8g2.setCursor(0, 40);
    u8g2.print("Nenhum sinal salvo");
  }
  u8g2.sendBuffer();
}

void cc1101FreqSweepSetup() {
  sweepIndex = 0;
  sweepLast = 0;
  ELECHOUSE_cc1101.setSidle();
  Serial.println("[CC1101] Freq Sweep iniciado");
}

void cc1101FreqSweepLoop() {
  if (buttonPressed(BTN_BACK)) {
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }

  if (millis() - sweepLast > 50) {
    ELECHOUSE_cc1101.setMHZ(SCAN_FREQUENCIES[sweepIndex] / 1000.0);
    ELECHOUSE_cc1101.SetRx();
    delay(5);
    int rssi = ELECHOUSE_cc1101.getRssi();

    if (rssi > RSSI_THRESHOLD) {
      Serial.printf("[SWEEP] Sinal em %.3f MHz | RSSI: %d\n",
                    SCAN_FREQUENCIES[sweepIndex] / 1000.0, rssi);
    }

    sweepIndex++;
    if (sweepIndex >= NUM_FREQS) sweepIndex = 0;
    sweepLast = millis();
  }

  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 FreqSweep");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("Varrendo...");
  u8g2.setCursor(0, 40);
  u8g2.print("Freq: ");
  u8g2.print(SCAN_FREQUENCIES[sweepIndex] / 1000.0);
  u8g2.print(" MHz");
  u8g2.setCursor(0, 52);
  u8g2.print("RSSI: ");
  u8g2.print(ELECHOUSE_cc1101.getRssi());
  u8g2.sendBuffer();
}

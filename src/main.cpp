#include "config.h"

void bootAnimation() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "Crazy Cat");
  u8g2.sendBuffer();
  delay(300);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_open);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "Crazy Cat");
  u8g2.sendBuffer();
  delay(600);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_blink);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "Crazy Cat");
  u8g2.sendBuffer();
  delay(150);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_open);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "Crazy Cat");
  u8g2.sendBuffer();
  delay(400);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_blink);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "Crazy Cat");
  u8g2.sendBuffer();
  delay(150);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_open);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "Crazy Cat");
  u8g2.drawFrame(20, 52, 88, 6);
  u8g2.sendBuffer();
  delay(200);

  for (int i = 0; i <= 84; i += 4) {
    u8g2.drawBox(22, 54, i, 2);
    u8g2.sendBuffer();
    delay(25);
  }

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(95, 62);
  u8g2.print(FIRMWARE_SHORT);
  u8g2.sendBuffer();
  delay(400);
}

void drawConnectionStatus() {
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate < 2000) return;
  lastStatusUpdate = millis();

  if (current_screen == 0) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setCursor(110, 7);
    if (WiFi.status() == WL_CONNECTED) {
      u8g2.print("W");
    } else if (deviceIsPaired) {
      u8g2.print("P");
    } else {
      u8g2.print(".");
    }
  }
}

void setup() {
  // DIAGNÓSTICO: LED onboard GPIO 2 — pisca = código rodando
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  delay(200);
  digitalWrite(2, LOW);
  delay(200);
  digitalWrite(2, HIGH);

  // Delay inicial para estabilizar regulador e flash
  delay(500);

  setupHardware();

  // Se chegou aqui, setupHardware() não travou
  digitalWrite(2, LOW);
  delay(200);
  digitalWrite(2, HIGH);
  delay(200);
  digitalWrite(2, LOW);
  esp_task_wdt_reset();

  pairingInit();
  esp_task_wdt_reset();

  initCryptoManager();
  esp_task_wdt_reset();

  initUnifiedComm();
  esp_task_wdt_reset();

  // ==========================================
  // NOVO: Inicializar RMT para captura RAW
  // ==========================================
  initRMTCapture();
  esp_task_wdt_reset();

  initMenuSystem();
  initSerialControl();
  initBluetoothControl();
  esp_task_wdt_reset();
  bootAnimation();
  esp_task_wdt_reset();
  drawMenu();

  Serial.println("========================================");
  Serial.println("  Crazy Cat v3.3-POWER - ULTRA EDITION");
  Serial.println("========================================");
  Serial.println("NOVO v3.3-POWER:");
  Serial.println("  + RMT driver para captura RAW (1us precision)");
  Serial.println("  + De Bruijn brute force sequences");
  Serial.println("  + Formato .sub compativel com Flipper Zero");
  Serial.println("  + Captive portal detection (Android/iOS/Windows)");
  Serial.println("  + PMKID extraction para hashcat");
  Serial.println("  + WiFi 54 Mbps data rate optimization");
  Serial.println("  + nRF24 RF_SETUP corrigido (0x92)");
  Serial.println("----------------------------------------");
  Serial.println("Communication methods:");
  Serial.println("  - Serial: 115200 baud");
  Serial.println("  - Bluetooth: 'CrazyCat' SPP");
  Serial.println("  - WiFi REST API: /api/* (port 8080)");
  Serial.println("  - MQTT-TLS: broker.hivemq.com:8883");
  Serial.println("  - Pairing: 8-digit code");
  Serial.println("----------------------------------------");
  Serial.printf("Device ID: %s\n", deviceFingerprint);
  Serial.printf("Paired: %s\n", deviceIsPaired ? "YES" : "NO");
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("========================================");
}

namespace {
  unsigned long lastBTEventTime = 0;
  bool btConnectionActive = false;
}

void loop() {
  // Diagnóstico: pisca LED a cada 2 segundos = loop rodando
  static unsigned long lastLedToggle = 0;
  static bool ledState = false;
  if (millis() - lastLedToggle > 2000) {
    lastLedToggle = millis();
    ledState = !ledState;
    digitalWrite(2, ledState ? HIGH : LOW);
  }

  esp_task_wdt_reset();
  checkBackInterruptFlag();

  processSerialCommandsUnified();
  processBluetoothCommandsUnified();

  if (!evilTwinAtivo && !mitmAtivo) {
    processHttpServer();
  }

  if (evilTwinAtivo || mitmAtivo) {
    processarServidorWeb();
  }

  if (dnsAtivo) {
    dnsServer.processNextRequest();
  }

  mqttSecureLoop();

  if (WiFi.status() == WL_CONNECTED) {
    mqttLoop();
  }

  drawConnectionStatus();

  if (current_screen == 0) {
    handleMenu();
  } else {
    runCurrentFunction();
  }

  if (ataqueEmAndamento) {
    static unsigned long lastAttackCheck = 0;
    if (millis() - lastAttackCheck > 100) {
      verificarEstadoAtaques();
      lastAttackCheck = millis();
    }
  }

  delay(10);

  static unsigned long lastWdtReset = 0;
  if (millis() - lastWdtReset > 1000) {
    esp_task_wdt_reset();
    lastWdtReset = millis();
  }
}

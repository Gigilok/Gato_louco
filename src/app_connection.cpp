#include "config.h"

// Estado local da conexão com aplicativo (Serial ou Bluetooth)
static struct {
  uint8_t connectionType; // 0=Serial, 1=BT
  bool isConnected;
  bool isAuthenticated;
  char deviceId[33];
  unsigned long lastActivity;
} appConn;

void initAppConnection() {
  appConn.connectionType = 0;
  appConn.isConnected = false;
  appConn.isAuthenticated = false;
  appConn.deviceId[0] = '\0';
  appConn.lastActivity = 0;
}

// Verifica código de pareamento e autoriza dispositivo remoto
bool verifyPairingCode(const char* code, const char* deviceId) {
  if (!code || !deviceId) return false;
  
  // Se estamos no modo de exibição de código e o código bate
  if (pairingState == PAIR_SHOWING_CODE && validatePairCode(code)) {
    // Gera token de sessão seguro (32 hex chars = 16 bytes)
    char token[33];
    uint8_t tok[16];
    esp_fill_random(tok, 16);
    for (int i = 0; i < 16; i++) {
      snprintf(token + (i * 2), 3, "%02X", tok[i]);
    }
    token[32] = '\0';
    
    if (pairDevice(deviceId, token)) {
      appConn.isAuthenticated = true;
      appConn.isConnected = true;
      strncpy(appConn.deviceId, deviceId, 32);
      appConn.deviceId[32] = '\0';
      appConn.lastActivity = millis();
      Serial.printf("[APP] Device %s pareado e autenticado\n", deviceId);
      return true;
    }
  }
  
  // Verifica se deviceId já possui token válido
  if (isDeviceAuthorized(deviceId)) {
    appConn.isAuthenticated = true;
    appConn.isConnected = true;
    strncpy(appConn.deviceId, deviceId, 32);
    appConn.deviceId[32] = '\0';
    appConn.lastActivity = millis();
    return true;
  }
  return false;
}

void processAppConnection() {
  // Pisca código de pareamento na tela principal se ativo
  if (pairingState == PAIR_SHOWING_CODE) {
    static unsigned long lastBlink = 0;
    static bool showCode = true;
    if (millis() - lastBlink > 1000) {
      showCode = !showCode;
      lastBlink = millis();
    }
    if (current_screen == 0 && showCode) {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.setCursor(0, 63);
      u8g2.print("Pair: ");
      u8g2.print(currentPairCode);
    }
  }
  
  // Timeout de conexão autenticada (5 minutos sem atividade)
  if (appConn.isConnected && (millis() - appConn.lastActivity > 300000)) {
    appConn.isConnected = false;
    appConn.isAuthenticated = false;
    appConn.deviceId[0] = '\0';
    Serial.println("[APP] Conexao expirada por inatividade");
  }
}

void sendToApp(const char* jsonMsg) {
  if (!jsonMsg) return;
  if (appConn.connectionType == 0) {
    Serial.println(jsonMsg);
  } else if (appConn.connectionType == 1) {
    SerialBT.println(jsonMsg);
  }
}

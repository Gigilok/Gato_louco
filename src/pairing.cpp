#include "config.h"

namespace {
  bool wifiAPActive = false;
  unsigned long lastMqttReconnect = 0;
  unsigned long lastMqttPublish = 0;
  char mqttTopicDevice[64];
  char mqttTopicControl[64];
  char mqttTopicStatus[64];
  bool mqttConnected = false;
  unsigned long lastPairCodeRefresh = 0;
  unsigned long lastPairAttempt = 0;
}

// ==========================================
// MQTT CALLBACK (LEGADO)
// ==========================================
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (length > 512) return;

  char msg[513];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.printf("[MQTT] RX: %s -> %s\n", topic, msg);

  if (strncmp(msg, "ENC:", 4) == 0) {
    mqttHandleCommand(topic, (uint8_t*)msg, length);
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) return;

  const char* cmd = doc["cmd"] | "";

  if (strcmp(cmd, "pair_request") == 0 || strcmp(cmd, "pair") == 0) {
    const char* code = doc["code"] | "";
    const char* deviceId = doc["device_id"] | "";

    lastPairAttempt = millis();

    if (validatePairCode(code)) {
      char token[65];
      generateSecureToken(token, sizeof(token));

      if (pairDevice(deviceId, token)) {
        uint8_t salt[16];
        esp_fill_random(salt, 16);
        deriveKeyFromPairCode(code, salt, pairAesKey);

        JsonDocument resp;
        resp["status"] = "paired";
        resp["token"] = token;
        resp["device_fp"] = deviceFingerprint;
        resp["firmware"] = FIRMWARE_VERSION;
        resp["salt"] = base64Encode(salt, 16);

        String respStr;
        serializeJson(resp, respStr);
        mqttClient.publish(mqttTopicDevice, respStr.c_str());

        pairingState = PAIR_CONFIRMED;
        Serial.printf("[PAIR] Sucesso: %s\n", deviceId);
      }
    } else {
      JsonDocument resp;
      resp["status"] = "invalid_code";
      String respStr;
      serializeJson(resp, respStr);
      mqttClient.publish(mqttTopicDevice, respStr.c_str());
    }
  }
  else if (strcmp(cmd, "control") == 0) {
    const char* token = doc["token"] | "";
    if (!isDeviceAuthorized(token)) {
      JsonDocument resp;
      resp["status"] = "unauthorized";
      String respStr;
      serializeJson(resp, respStr);
      mqttClient.publish(mqttTopicDevice, respStr.c_str());
      return;
    }

    const char* action = doc["action"] | "";
    String result = processApiCommand(action, &doc);

    JsonDocument resp;
    resp["status"] = "ok";
    resp["action"] = action;
    resp["result"] = result;

    String respStr;
    serializeJson(resp, respStr);
    mqttClient.publish(mqttTopicDevice, respStr.c_str());
  }
  else if (strcmp(cmd, "unpair") == 0) {
    const char* deviceId = doc["device_id"] | "";
    if (unpairDevice(deviceId)) {
      JsonDocument resp;
      resp["status"] = "unpaired";
      String respStr;
      serializeJson(resp, respStr);
      mqttClient.publish(mqttTopicDevice, respStr.c_str());
    }
  }
}

// ==========================================
// MQTT INIT (LEGADO)
// ==========================================
void mqttInit() {
  // Proteção: se deviceFingerprint vazio, gerar antes de usar
  if (strlen(deviceFingerprint) == 0) {
    generateDeviceFingerprint(deviceFingerprint, sizeof(deviceFingerprint));
  }
  snprintf(mqttTopicDevice, sizeof(mqttTopicDevice), "%s%s/device", MQTT_TOPIC_PREFIX, deviceFingerprint);
  snprintf(mqttTopicControl, sizeof(mqttTopicControl), "%s%s/control", MQTT_TOPIC_PREFIX, deviceFingerprint);
  snprintf(mqttTopicStatus, sizeof(mqttTopicStatus), "%s%s/status", MQTT_TOPIC_PREFIX, deviceFingerprint);

  mqttClient.setServer(MQTT_BROKER_PRIMARY, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(2048);

  Serial.printf("[MQTT] Topics: device=%s control=%s\n", mqttTopicDevice, mqttTopicControl);
}

void mqttReconnect() {
  if (mqttConnected) return;
  if (millis() - lastMqttReconnect < 5000) return;
  lastMqttReconnect = millis();

  if (WiFi.status() != WL_CONNECTED) return;

  String clientId = "CrazyCat-" + String(deviceFingerprint);

  if (mqttClient.connect(clientId.c_str())) {
    mqttClient.subscribe(mqttTopicControl);
    mqttConnected = true;
    Serial.println("[MQTT] Conectado!");

    JsonDocument doc;
    doc["status"] = "online";
    doc["firmware"] = FIRMWARE_VERSION;
    doc["paired"] = deviceIsPaired;
    doc["ip"] = WiFi.localIP().toString();
    String msg;
    serializeJson(doc, msg);
    mqttClient.publish(mqttTopicStatus, msg.c_str(), true);
  } else {
    mqttClient.setServer(MQTT_BROKER_SECONDARY, MQTT_PORT);
    if (mqttClient.connect(clientId.c_str())) {
      mqttClient.subscribe(mqttTopicControl);
      mqttConnected = true;
      Serial.println("[MQTT] Conectado (secundario)!");
    }
  }
}

void mqttLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    mqttConnected = false;
    return;
  }

  if (!mqttClient.connected()) {
    mqttConnected = false;
    mqttReconnect();
  }

  if (mqttClient.connected()) {
    mqttClient.loop();

    if (millis() - lastMqttPublish > 30000) {
      mqttPublishStatus();
      lastMqttPublish = millis();
    }
  }
}

void mqttPublishStatus() {
  if (!mqttClient.connected()) return;

  JsonDocument doc;
  doc["status"] = "online";
  doc["uptime"] = millis() / 1000;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["paired"] = deviceIsPaired;
  doc["paired_count"] = numDispositivosPareados;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();

  String msg;
  serializeJson(doc, msg);
  mqttClient.publish(mqttTopicStatus, msg.c_str(), true);
}

void mqttHandleCommand(const char* topic, uint8_t* payload, unsigned int length) {
  // Placeholder para comandos criptografados futuros
}

// ==========================================
// WiFi AP PARA PAREAMENTO
// ==========================================
void setupWiFiAPForPairing() {
  if (wifiAPActive) return;

  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_MODE_NULL);
  delay(50);
  WiFi.mode(WIFI_AP);
  delay(100);

  String apName = "CrazyCat-" + String(deviceFingerprint).substring(0, 6);
  WiFi.softAP(apName.c_str(), SENHA_AP_PADRAO);
  delay(500);

  wifiAPActive = true;
  Serial.printf("[PAIR] AP ativo: %s\n", apName.c_str());
  Serial.printf("[PAIR] HTTP API: http://192.168.4.1:8080/api/status\n");
  Serial.printf("[PAIR] HTTP API: http://192.168.4.1:8080/api/pair\n");
}

void stopWiFiAPForPairing() {
  if (!wifiAPActive) return;

  esp_wifi_set_promiscuous(false);
  delay(50);

  WiFi.softAPdisconnect(true);
  delay(200);

  WiFi.disconnect(true, true);
  delay(100);

  WiFi.mode(WIFI_MODE_NULL);
  delay(100);

  WiFi.setAutoReconnect(false);

  wifiAPActive = false;
  Serial.println("[PAIR] AP desativado");
}

// ==========================================
// PAREAMENTO SETUP/LOOP
// ==========================================
void pairingInit() {
  pairingState = PAIR_IDLE;
  memset(currentPairCode, 0, sizeof(currentPairCode));
  mqttInit();
}

void pairingSetup() {
  generatePairCode();
  setupWiFiAPForPairing();

  u8g2.clearBuffer();
  drawFunctionHeader("Pareamento");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 24);
  u8g2.print("Codigo:");
  u8g2.setFont(u8g2_font_10x20_tr);
  u8g2.setCursor(20, 48);
  u8g2.print(currentPairCode);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 60);
  u8g2.print("App: CrazyCat v3.1");
  u8g2.sendBuffer();

  Serial.printf("[PAIR] Código: %s | Timeout: %ds\n", currentPairCode, PAIR_TIMEOUT_MS / 1000);
  Serial.printf("[PAIR] WiFi AP: CrazyCat-%s\n", String(deviceFingerprint).substring(0, 6).c_str());
  Serial.printf("[PAIR] Senha AP: %s\n", SENHA_AP_PADRAO);
}

void pairingLoop() {
  if (buttonPressed(BTN_BACK)) {
    stopWiFiAPForPairing();
    pairingState = PAIR_IDLE;
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }

  bool shouldRefresh = (millis() - lastPairCodeRefresh > PAIR_CODE_REFRESH_MS);
  bool recentAttempt = (millis() - lastPairAttempt < 10000);

  if (shouldRefresh && !recentAttempt) {
    generatePairCode();
    lastPairCodeRefresh = millis();

    u8g2.clearBuffer();
    drawFunctionHeader("Pareamento");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 24);
    u8g2.print("Novo Codigo:");
    u8g2.setFont(u8g2_font_10x20_tr);
    u8g2.setCursor(20, 48);
    u8g2.print(currentPairCode);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 60);
    u8g2.print("Aguardando app...");
    u8g2.sendBuffer();
  }

  if (millis() > pairTimeoutAt && pairingState != PAIR_CONFIRMED) {
    stopWiFiAPForPairing();
    pairingState = PAIR_FAILED;

    u8g2.clearBuffer();
    drawFunctionHeader("Pareamento");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 30);
    u8g2.print("Timeout!");
    u8g2.setCursor(0, 42);
    u8g2.print("Tente novamente");
    u8g2.sendBuffer();
    delay(2000);

    current_screen = 0;
    drawMenu();
    return;
  }

  if (pairingState == PAIR_CONFIRMED) {
    u8g2.clearBuffer();
    drawFunctionHeader("Pareamento");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("SUCESSO!");
    u8g2.setCursor(0, 42);
    u8g2.print("Dispositivo pareado");
    u8g2.setCursor(0, 54);
    u8g2.print("Total: ");
    u8g2.print(numDispositivosPareados);
    u8g2.sendBuffer();
    delay(3000);

    stopWiFiAPForPairing();
    current_screen = 0;
    drawMenu();
    return;
  }

  delay(100);
}

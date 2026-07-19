#include "config.h"
#include <ArduinoJson.h>

// ==========================================
// SERVIDOR HTTP REST API - COMPATÍVEL COM APK
// ==========================================

namespace {
  WebServer httpServer(8080);
  bool httpServerRunning = false;

  uint8_t* otaBuffer = nullptr;
  size_t otaBufferSize = 0;
  size_t otaBytesReceived = 0;
  bool otaInProgress = false;
}

// ==========================================
// HELPERS JSON
// ==========================================
static void sendJson(int code, const JsonDocument& doc) {
  String response;
  serializeJson(doc, response);
  httpServer.send(code, "application/json", response);
}

static void sendError(int code, const char* message) {
  JsonDocument doc;
  doc["status"] = "error";
  doc["message"] = message;
  sendJson(code, doc);
}

static bool checkAuth() {
  String token;
  if (httpServer.hasHeader("Authorization")) {
    String auth = httpServer.header("Authorization");
    if (auth.startsWith("Bearer ")) {
      token = auth.substring(7);
    }
  }
  if (token.isEmpty() && httpServer.hasArg("token")) {
    token = httpServer.arg("token");
  }
  if (token.isEmpty()) {
    sendError(401, "Token required");
    return false;
  }
  if (!isDeviceAuthorized(token.c_str())) {
    sendError(403, "Invalid token");
    return false;
  }
  return true;
}

// ==========================================
// ENDPOINT: GET /api/status
// ==========================================
static void handleApiStatus() {
  JsonDocument doc;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["deviceId"] = deviceFingerprint;
  doc["paired"] = deviceIsPaired;
  doc["pairedCount"] = numDispositivosPareados;
  doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["bleActive"] = false;
  doc["cc1101Status"] = ELECHOUSE_cc1101.getCC1101() ? "OK" : "ERROR";
  doc["nrf24Status"] = radio.isChipConnected() ? "OK" : "ERROR";
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["cpuFreq"] = ESP.getCpuFreqMHz();
  doc["api_port"] = 8080;
  sendJson(200, doc);
}

// ==========================================
// ENDPOINT: POST /api/pair
// ==========================================
static void handleApiPair() {
  if (!httpServer.hasArg("plain")) {
    sendError(400, "JSON body required");
    return;
  }

  String body = httpServer.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    sendError(400, "Invalid JSON");
    return;
  }

  const char* code = doc["code"] | "";
  const char* deviceId = doc["device_id"] | "";

  if (strlen(code) != PAIR_CODE_LENGTH) {
    sendError(400, "Invalid code length");
    return;
  }

  if (!validatePairCode(code)) {
    sendError(403, "Invalid or expired pair code");
    return;
  }

  char token[65];
  uint8_t tokenBytes[32];
  esp_fill_random(tokenBytes, 32);
  for (int i = 0; i < 32; i++) {
    snprintf(token + (i * 2), 3, "%02X", tokenBytes[i]);
  }
  token[64] = '\0';

  if (pairDevice(deviceId, token)) {
    // CORREÇÃO: Usar funções mbedtls com sufixo _ret (compatível ESP-IDF 5.x)
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    uint8_t salt[16];
    esp_fill_random(salt, 16);
    uint8_t keyMaterial[PAIR_CODE_LENGTH + 16];
    memcpy(keyMaterial, code, PAIR_CODE_LENGTH);
    memcpy(keyMaterial + PAIR_CODE_LENGTH, salt, 16);

    mbedtls_sha256_update(&ctx, keyMaterial, sizeof(keyMaterial));
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    for (int i = 1; i < 10000; i++) {
      mbedtls_sha256_init(&ctx);
      mbedtls_sha256_starts(&ctx, 0);
      mbedtls_sha256_update(&ctx, hash, 32);
      mbedtls_sha256_update(&ctx, keyMaterial, sizeof(keyMaterial));
      mbedtls_sha256_finish(&ctx, hash);
      mbedtls_sha256_free(&ctx);
    }

    memcpy(pairAesKey, hash, PAIR_AES_KEY_SIZE);

    JsonDocument resp;
    resp["status"] = "paired";
    resp["token"] = token;
    resp["mqtt_topic"] = String("crazycat/") + deviceFingerprint + "/control";
    resp["device_fp"] = deviceFingerprint;
    resp["salt"] = base64Encode(salt, 16);
    sendJson(200, resp);

    pairingState = PAIR_CONFIRMED;
    Serial.printf("[HTTP] Device paired: %s\n", deviceId);
  } else {
    sendError(500, "Failed to pair device");
  }
}

// ==========================================
// ENDPOINT: POST /api/command
// ==========================================
static void handleApiCommand() {
  if (!checkAuth()) return;

  if (!httpServer.hasArg("plain")) {
    sendError(400, "JSON body required");
    return;
  }

  String body = httpServer.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    sendError(400, "Invalid JSON");
    return;
  }

  const char* cmd = doc["cmd"] | "";
  String result = processApiCommand(cmd, &doc);

  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = result;
  resp["command"] = cmd;
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: POST /api/attack
// ==========================================
static void handleApiAttack() {
  if (!checkAuth()) return;

  String cmd = httpServer.arg("cmd");
  String target = httpServer.arg("target");

  if (cmd.isEmpty()) {
    sendError(400, "cmd parameter required");
    return;
  }

  String result = processApiCommand(cmd.c_str(), nullptr);

  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = result;
  resp["attack"] = cmd;
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: GET /api/wifi/scan
// ==========================================
static void handleApiWifiScan() {
  if (!checkAuth()) return;

  escanearRedesWiFi();

  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();

  for (int i = 0; i < numRedesDescobertas; i++) {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = redesDescobertas[i].ssid;
    net["rssi"] = redesDescobertas[i].rssi;
    char bssidStr[18];
    snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             redesDescobertas[i].bssid[0], redesDescobertas[i].bssid[1],
             redesDescobertas[i].bssid[2], redesDescobertas[i].bssid[3],
             redesDescobertas[i].bssid[4], redesDescobertas[i].bssid[5]);
    net["bssid"] = bssidStr;
    net["channel"] = redesDescobertas[i].canal;
    net["encrypted"] = redesDescobertas[i].criptografada;
    net["security"] = redesDescobertas[i].tipoSeguranca;
    net["wps"] = redesDescobertas[i].wpsAtivo;
    net["clients"] = 0;
  }

  sendJson(200, doc);
}

// ==========================================
// ENDPOINT: POST /api/nrf24/scan/start
// ==========================================
static void handleApiNrf24ScanStart() {
  if (!checkAuth()) return;
  nrfScannerSetup();
  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = "nRF24 scan started";
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: POST /api/nrf24/scan/stop
// CORREÇÃO CRÍTICA: NÃO chamar radio.powerDown() — mata o rádio!
// ==========================================
static void handleApiNrf24ScanStop() {
  if (!checkAuth()) return;
  radio.stopListening();
  // CORREÇÃO: Removido radio.powerDown() que corrompia o estado SPI
  // Apenas stopListening() é suficiente para parar o scanner
  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = "nRF24 scan stopped";
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: POST /api/nrf24/jam/start
// ==========================================
static void handleApiNrf24JamStart() {
  if (!checkAuth()) return;
  nrfJammerSetup();
  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = "nRF24 jamming started";
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: POST /api/nrf24/jam/stop
// CORREÇÃO CRÍTICA: NÃO chamar radio.powerDown() — mata o rádio!
// Usar flush_tx() + stopListening() para parada segura
// ==========================================
static void handleApiNrf24JamStop() {
  if (!checkAuth()) return;
  jamming_active = false;
  // CORREÇÃO: flush_tx() limpa FIFO TX, stopListening() para TX
  // NUNCA powerDown() — requer powerUp() + reconfiguração completa
  radio.flush_tx();
  radio.stopListening();
  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = "nRF24 jamming stopped";
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: POST /api/cc1101/capture
// ==========================================
static void handleApiCc1101Capture() {
  if (!checkAuth()) return;

  float freq = httpServer.arg("frequency").toFloat();
  if (freq <= 0) freq = 433.92;

  uint32_t freqKHz = (uint32_t)(freq * 1000);
  startCapture(freqKHz);

  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = "CC1101 capture started";
  resp["frequency"] = freq;
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: GET /api/rf/scan
// ==========================================
static void handleApiRfScan() {
  if (!checkAuth()) return;

  JsonDocument doc;
  JsonArray signals = doc.to<JsonArray>();

  for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
    if (savedSignals[i].active) {
      JsonObject sig = signals.add<JsonObject>();
      sig["name"] = savedSignals[i].name;
      sig["frequency"] = savedSignals[i].signal.frequency / 1000.0;
      sig["pulses"] = savedSignals[i].signal.dataLength;
      sig["active"] = true;
      sig["rawData"] = "";
      sig["timestamp"] = savedSignals[i].signal.timestamp;
      sig["protocol"] = "CC1101";
    }
  }

  for (int i = 0; i < SUBGHZ_DB_COUNT; i++) {
    JsonObject sig = signals.add<JsonObject>();
    sig["name"] = subGHzDatabase[i].name;
    sig["frequency"] = subGHzDatabase[i].frequency;
    sig["pulses"] = subGHzDatabase[i].bits;
    sig["active"] = true;
    sig["rawData"] = String("0x") + String(subGHzDatabase[i].code, HEX);
    sig["timestamp"] = 0;
    sig["protocol"] = protocols[subGHzDatabase[i].protocolIndex].name;
  }

  sendJson(200, doc);
}

// ==========================================
// ENDPOINT: POST /api/cc1101/transmit
// ==========================================
static void handleApiCc1101Transmit() {
  if (!checkAuth()) return;

  if (!httpServer.hasArg("plain")) {
    sendError(400, "JSON body required");
    return;
  }

  String body = httpServer.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    sendError(400, "Invalid JSON");
    return;
  }

  float freq = doc["frequency"] | 433.92;
  const char* codeStr = doc["code"] | "0x123456";
  int bits = doc["bits"] | 24;
  const char* protocolName = doc["protocol"] | "PT2262";

  uint32_t code = strtoul(codeStr, nullptr, 0);

  int protoIdx = 0;
  for (int i = 0; i < NUM_PROTOCOLS; i++) {
    if (strcmp(protocols[i].name, protocolName) == 0) {
      protoIdx = i;
      break;
    }
  }

  currentSignal.frequency = (uint32_t)(freq * 1000);
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(freq);
  transmitCode(code, bits, protocols[protoIdx]);

  JsonDocument resp;
  resp["status"] = "ok";
  resp["message"] = "Signal transmitted";
  sendJson(200, resp);
}

// ==========================================
// ENDPOINT: POST /api/firmware (OTA)
// ==========================================
static void handleApiFirmware() {
  if (!checkAuth()) return;

  HTTPUpload& upload = httpServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      sendError(500, "OTA begin failed");
      return;
    }
    otaInProgress = true;
    otaBytesReceived = 0;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.end(false);
      otaInProgress = false;
      sendError(500, "OTA write failed");
      return;
    }
    otaBytesReceived += upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      otaInProgress = false;
      JsonDocument resp;
      resp["status"] = "ok";
      resp["message"] = "Firmware updated, restarting...";
      resp["bytes"] = otaBytesReceived;
      sendJson(200, resp);
      delay(1000);
      ESP.restart();
    } else {
      otaInProgress = false;
      sendError(500, "OTA end failed");
    }
  }
}

// ==========================================
// PROCESSADOR DE COMANDOS API
// ==========================================
String processApiCommand(const char* cmd, JsonDocument* params) {
  String cmdStr = String(cmd);
  cmdStr.toUpperCase();

  if (cmdStr == "UP" || cmdStr == "BUTTON_UP") {
    simulateButtonUp();
    return "Button UP simulated";
  }
  if (cmdStr == "DOWN" || cmdStr == "BUTTON_DOWN") {
    simulateButtonDown();
    return "Button DOWN simulated";
  }
  if (cmdStr == "OK" || cmdStr == "SELECT" || cmdStr == "BUTTON_SELECT") {
    simulateButtonSelect();
    return "Button SELECT simulated";
  }
  if (cmdStr == "BACK" || cmdStr == "BUTTON_BACK") {
    simulateButtonBack();
    return "Button BACK simulated";
  }

  if (cmdStr == "SCAN_WIFI" || cmdStr == "WIFI_SCAN") {
    escanearRedesWiFi();
    return String("WiFi scan complete: ") + numRedesDescobertas + " networks";
  }
  if (cmdStr == "SCAN_BLE" || cmdStr == "BLE_SCAN") {
    bleScanSetup();
    return "BLE scan started";
  }

  if (cmdStr == "DEAUTH" || cmdStr == "DEAUTH_FLOOD") {
    if (numRedesDescobertas > 0 && redeSelecionada >= 0) {
      uint8_t bssid[6];
      memcpy(bssid, redesDescobertas[redeSelecionada].bssid, 6);
      ataqueDeauth(bssid, 50);
      return "Deauth attack started";
    }
    return "No network selected";
  }
  if (cmdStr == "EVIL" || cmdStr == "EVIL_TWIN") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarAtaqueEvilTwin();
      return "Evil Twin attack started";
    }
    return "No network found";
  }
  if (cmdStr == "HANDSHAKE" || cmdStr == "HANDSHAKE_CAPTURE") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarCapturaHandshakeAlvo();
      return "Handshake capture started";
    }
    return "No network found";
  }
  if (cmdStr == "WPS" || cmdStr == "WPS_BRUTE") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarAtaqueWPS();
      return "WPS brute force started";
    }
    return "No network found";
  }
  if (cmdStr == "MITM" || cmdStr == "MITM_OFFLOAD") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarAtaqueMITM();
      return "MITM offload started";
    }
    return "No network found";
  }
  if (cmdStr == "STOP") {
    pararAtaqueAtual();
    return "All attacks stopped";
  }

  if (cmdStr == "NRF24_SCAN_START") {
    nrfScannerSetup();
    return "nRF24 scan started";
  }
  // CORREÇÃO: NÃO chamar powerDown() — mata o rádio
  if (cmdStr == "NRF24_SCAN_STOP") {
    radio.stopListening();
    return "nRF24 scan stopped";
  }
  if (cmdStr == "NRF24_JAM_START") {
    nrfJammerSetup();
    return "nRF24 jamming started";
  }
  // CORREÇÃO: flush_tx() + stopListening() em vez de powerDown()
  if (cmdStr == "NRF24_JAM_STOP") {
    jamming_active = false;
    radio.flush_tx();
    radio.stopListening();
    return "nRF24 jamming stopped";
  }
  if (cmdStr == "CC1101_CAPTURE_START") {
    uint32_t freq = autoDetectFrequency();
    startCapture(freq);
    return "CC1101 capture started";
  }
  if (cmdStr == "CC1101_CAPTURE_STOP") {
    stopCapture();
    return "CC1101 capture stopped";
  }
  if (cmdStr == "CC1101_SWEEP") {
    cc1101FreqSweepSetup();
    return "CC1101 frequency sweep started";
  }
  if (cmdStr.startsWith("CC1101_TX_")) {
    if (hasSavedSignal) {
      transmitRawSignal();
      return "Signal transmitted";
    }
    return "No signal saved";
  }
  if (cmdStr.startsWith("SUBGHZ_TX_")) {
    return "SubGHz transmission (not implemented in command)";
  }
  if (cmdStr == "SUBGHZ_LIST") {
    return "SubGHz database listed";
  }
  if (cmdStr.startsWith("SIGNAL_LIST")) {
    int count = 0;
    for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
      if (savedSignals[i].active) count++;
    }
    return String("Signals: ") + count + "/" + MAX_SAVED_SIGNALS;
  }
  if (cmdStr.startsWith("TX_")) {
    return "Transmit signal by name";
  }
  if (cmdStr.startsWith("DEL_")) {
    return "Delete signal by name";
  }


  if (cmdStr == "FIRMWARE_START") {
    otaInProgress = true;
    otaBytesReceived = 0;
    return "OTA ready";
  }
  if (cmdStr.startsWith("FW:")) {
    return "OTA chunk received";
  }
  if (cmdStr == "FIRMWARE_END") {
    return "OTA complete";
  }

  if (cmdStr == "REBOOT") {
    ESP.restart();
    return "Rebooting...";
  }
  if (cmdStr == "STATUS") {
    return String("FW: ") + FIRMWARE_VERSION + " | Heap: " + ESP.getFreeHeap();
  }

  if (cmdStr == "SOUR_APPLE_START") {
    sourAppleSetup();
    return "Sour Apple spam started";
  }
  if (cmdStr == "SOUR_APPLE_STOP") {
    return "Sour Apple spam stopped";
  }

  return "Unknown command: " + cmdStr;
}

// ==========================================
// BASE64 ENCODE (helper)
// ==========================================
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64Encode(const uint8_t* data, size_t len) {
  String out;
  out.reserve(((len + 2) / 3) * 4);
  for (size_t i = 0; i < len; i += 3) {
    uint32_t val = data[i] << 16;
    if (i + 1 < len) val |= data[i + 1] << 8;
    if (i + 2 < len) val |= data[i + 2];
    out += b64_table[(val >> 18) & 0x3F];
    out += b64_table[(val >> 12) & 0x3F];
    out += (i + 1 < len) ? b64_table[(val >> 6) & 0x3F] : '=';
    out += (i + 2 < len) ? b64_table[val & 0x3F] : '=';
  }
  return out;
}

// ==========================================
// INICIALIZAÇÃO DO SERVIDOR HTTP
// ==========================================
void initHttpServer() {
  if (httpServerRunning) return;

  if (evilTwinAtivo || mitmAtivo) {
    Serial.println("[HTTP] REST API deferred - pentest server active on port 80");
    return;
  }

  httpServer.on("/api/status", HTTP_GET, handleApiStatus);
  httpServer.on("/api/pair", HTTP_POST, handleApiPair);
  httpServer.on("/api/command", HTTP_POST, handleApiCommand);
  httpServer.on("/api/attack", HTTP_POST, handleApiAttack);
  httpServer.on("/api/wifi/scan", HTTP_GET, handleApiWifiScan);
  httpServer.on("/api/nrf24/scan/start", HTTP_POST, handleApiNrf24ScanStart);
  httpServer.on("/api/nrf24/scan/stop", HTTP_POST, handleApiNrf24ScanStop);
  httpServer.on("/api/nrf24/jam/start", HTTP_POST, handleApiNrf24JamStart);
  httpServer.on("/api/nrf24/jam/stop", HTTP_POST, handleApiNrf24JamStop);
  httpServer.on("/api/cc1101/capture", HTTP_POST, handleApiCc1101Capture);
  httpServer.on("/api/rf/scan", HTTP_GET, handleApiRfScan);
  httpServer.on("/api/cc1101/transmit", HTTP_POST, handleApiCc1101Transmit);
  httpServer.on("/api/firmware", HTTP_POST, handleApiFirmware);

  httpServer.onNotFound([]() {
    httpServer.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_MODE_STA || mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
    httpServer.begin();
    httpServerRunning = true;
    Serial.println("[HTTP] REST API server started on port 8080");
  } else {
    Serial.println("[HTTP] REST API deferred - WiFi not active yet");
  }
}

void processHttpServer() {
  if (evilTwinAtivo || mitmAtivo) {
    if (httpServerRunning) {
      httpServer.stop();
      httpServerRunning = false;
      Serial.println("[HTTP] REST API paused - pentest using port 80");
    }
    return;
  }

  if (!httpServerRunning) {
    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
      httpServer.begin();
      httpServerRunning = true;
      Serial.println("[HTTP] REST API server started (deferred) on port 8080");
    }
    return;
  }
  httpServer.handleClient();
}

void stopHttpServer() {
  if (httpServerRunning) {
    httpServer.stop();
    httpServerRunning = false;
  }
}

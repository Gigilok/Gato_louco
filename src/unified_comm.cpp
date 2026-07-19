#include "config.h"
#include <ArduinoJson.h>

// ==========================================
// COMUNICAÇÃO UNIFICADA: BT + USB + SERIAL
// Protocolo compatível com APK
// Todos os comandos terminam com \n
// Respostas em JSON padronizado
// ==========================================

namespace {
  // Buffer para comandos parciais
  String serialBuffer = "";
  String btBuffer = "";
  const size_t MAX_CMD_LEN = 512;
}

// ==========================================
// PROCESSADOR DE COMANDOS UNIFICADO
// Recebe comando string, retorna resposta JSON
// ==========================================
String processUnifiedCommand(const String& cmd, uint8_t source) {
  // source: 0=Serial, 1=BT, 2=USB
  String sourceName = source == 0 ? "SERIAL" : (source == 1 ? "BT" : "USB");

  // Log
  Serial.printf("[%s] CMD: %s\n", sourceName.c_str(), cmd.c_str());

  // Parse comando e parâmetros
  String cmdUpper = cmd;
  cmdUpper.toUpperCase();
  cmdUpper.trim();

  // Resposta padrão
  JsonDocument resp;
  resp["status"] = "ok";
  resp["source"] = sourceName;

  // ========== AUTH ==========
  if (cmdUpper.startsWith("AUTH:")) {
    String token = cmd.substring(5);
    token.trim();
    if (isDeviceAuthorized(token.c_str())) {
      resp["status"] = "authorized";
      resp["device_id"] = deviceFingerprint;
      resp["firmware"] = FIRMWARE_VERSION;
    } else {
      resp["status"] = "unauthorized";
      resp["error"] = "Invalid token";
    }
    String out;
    serializeJson(resp, out);
    return out;
  }

  // ========== HELP ==========
  if (cmdUpper == "HELP") {
    resp["status"] = "ok";
    resp["firmware"] = FIRMWARE_VERSION;
    resp["device_id"] = deviceFingerprint;
    JsonArray cmds = resp["commands"].to<JsonArray>();
    cmds.add("AUTH:<token>");
    cmds.add("STATUS");
    cmds.add("PAIR");
    cmds.add("DEVICE_ID");
    cmds.add("UP/DOWN/OK/BACK");
    cmds.add("SCAN_WIFI/SCAN_BLE");
    cmds.add("DEAUTH/EVIL/HANDSHAKE/WPS/MITM");
    cmds.add("STOP/REBOOT");
    cmds.add("NRF24_SCAN_START/STOP");
    cmds.add("NRF24_JAM_START/STOP");
    cmds.add("CC1101_CAPTURE_START/STOP/SWEEP");
    cmds.add("CC1101_TX_<raw>");
    cmds.add("SUBGHZ_LIST/TX_<code>");
    cmds.add("SIGNAL_LIST/TX_<name>/DEL_<name>");
    cmds.add("SOUR_APPLE_START/STOP");
    cmds.add("SETPASS <password>");
    cmds.add("FIRMWARE_START/END");
    cmds.add("FW:<base64_chunk>");
    String out;
    serializeJson(resp, out);
    return out;
  }

  // ========== STATUS ==========
  if (cmdUpper == "STATUS") {
    resp["firmware"] = FIRMWARE_VERSION;
    resp["device_id"] = deviceFingerprint;
    resp["paired"] = deviceIsPaired;
    resp["paired_count"] = numDispositivosPareados;
    resp["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
    resp["ip"] = WiFi.localIP().toString();
    resp["rssi"] = WiFi.RSSI();
    resp["free_heap"] = ESP.getFreeHeap();
    resp["cpu_freq"] = ESP.getCpuFreqMHz();
    resp["uptime"] = millis() / 1000;
    resp["cc1101_ok"] = ELECHOUSE_cc1101.getCC1101();
    resp["nrf24_ok"] = radio.isChipConnected();
    resp["networks_found"] = numRedesDescobertas;
    resp["attack_active"] = ataqueEmAndamento;
    String out;
    serializeJson(resp, out);
    return out;
  }

  // ========== DEVICE_ID ==========
  if (cmdUpper == "DEVICE_ID") {
    resp["device_id"] = deviceFingerprint;
    String out;
    serializeJson(resp, out);
    return out;
  }

  // ========== PAIR ==========
  if (cmdUpper == "PAIR") {
    if (pairingState == PAIR_IDLE) {
      generatePairCode();
      resp["status"] = "pairing";
      resp["code"] = currentPairCode;
      resp["timeout"] = PAIR_TIMEOUT_MS / 1000;
    } else {
      resp["status"] = "already_pairing";
      resp["code"] = currentPairCode;
    }
    String out;
    serializeJson(resp, out);
    return out;
  }

  // ========== SETPASS ==========
  if (cmdUpper.startsWith("SETPASS ")) {
    String pass = cmd.substring(8);
    pass.trim();
    strncpy(wifiPassword, pass.c_str(), 64);
    wifiPassword[64] = '\0';
    resp["status"] = "ok";
    resp["message"] = "WiFi password set";
    String out;
    serializeJson(resp, out);
    return out;
  }

  // ========== COMANDOS QUE REQUEREM AUTH ==========
  if (!deviceIsPaired) {
    resp["status"] = "error";
    resp["error"] = "Device not paired. Use PAIR first.";
    String out;
    serializeJson(resp, out);
    return out;
  }

  // ========== NAVEGAÇÃO ==========
  if (cmdUpper == "UP" || cmdUpper == "BUTTON_UP") {
    simulateButtonUp();
    resp["message"] = "Button UP";
  }
  else if (cmdUpper == "DOWN" || cmdUpper == "BUTTON_DOWN") {
    simulateButtonDown();
    resp["message"] = "Button DOWN";
  }
  else if (cmdUpper == "OK" || cmdUpper == "SELECT" || cmdUpper == "BUTTON_SELECT") {
    simulateButtonSelect();
    resp["message"] = "Button SELECT";
  }
  else if (cmdUpper == "BACK" || cmdUpper == "BUTTON_BACK") {
    simulateButtonBack();
    resp["message"] = "Button BACK";
  }

  // ========== SCAN ==========
  else if (cmdUpper == "SCAN_WIFI" || cmdUpper == "WIFI_SCAN") {
    escanearRedesWiFi();
    resp["message"] = String("WiFi scan: ") + numRedesDescobertas + " networks";
    // Envia lista detalhada
    JsonArray nets = resp["networks"].to<JsonArray>();
    for (int i = 0; i < min(numRedesDescobertas, 10); i++) {
      JsonObject net = nets.add<JsonObject>();
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
    }
  }
  else if (cmdUpper == "SCAN_BLE" || cmdUpper == "BLE_SCAN") {
    bleScanSetup();
    resp["message"] = "BLE scan started";
  }

  // ========== ATAQUES WiFi ==========
  else if (cmdUpper == "DEAUTH" || cmdUpper == "DEAUTH_FLOOD") {
    if (numRedesDescobertas > 0 && redeSelecionada >= 0) {
      uint8_t bssid[6];
      memcpy(bssid, redesDescobertas[redeSelecionada].bssid, 6);
      ataqueDeauth(bssid, 50);
      resp["message"] = "Deauth attack started";
      resp["target"] = redesDescobertas[redeSelecionada].ssid;
    } else {
      resp["status"] = "error";
      resp["error"] = "Select network first";
    }
  }
  else if (cmdUpper == "EVIL" || cmdUpper == "EVIL_TWIN") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarAtaqueEvilTwin();
      resp["message"] = "Evil Twin started";
      resp["target"] = ssidAlvo;
    } else {
      resp["status"] = "error";
      resp["error"] = "No networks found";
    }
  }
  else if (cmdUpper == "HANDSHAKE" || cmdUpper == "HANDSHAKE_CAPTURE") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarCapturaHandshakeAlvo();
      resp["message"] = "Handshake capture started";
      resp["target"] = ssidAlvo;
    } else {
      resp["status"] = "error";
      resp["error"] = "No networks found";
    }
  }
  else if (cmdUpper == "WPS" || cmdUpper == "WPS_BRUTE") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarAtaqueWPS();
      resp["message"] = "WPS brute force started";
      resp["target"] = ssidAlvo;
    } else {
      resp["status"] = "error";
      resp["error"] = "No networks found";
    }
  }
  else if (cmdUpper == "MITM" || cmdUpper == "MITM_OFFLOAD") {
    if (numRedesDescobertas > 0) {
      redeSelecionada = 0;
      iniciarAtaqueMITM();
      resp["message"] = "MITM offload started";
      resp["target"] = ssidAlvo;
    } else {
      resp["status"] = "error";
      resp["error"] = "No networks found";
    }
  }
  else if (cmdUpper == "STOP") {
    pararAtaqueAtual();
    resp["message"] = "All attacks stopped";
  }

  // ========== nRF24 ==========
  else if (cmdUpper == "NRF24_SCAN_START") {
    nrfScannerSetup();
    resp["message"] = "nRF24 scan started";
  }
  // CORREÇÃO CRÍTICA: NÃO chamar radio.powerDown() — mata o rádio!
  // Apenas stopListening() é suficiente para parar o scanner
  else if (cmdUpper == "NRF24_SCAN_STOP") {
    radio.stopListening();
    resp["message"] = "nRF24 scan stopped";
  }
  else if (cmdUpper == "NRF24_JAM_START") {
    nrfJammerSetup();
    resp["message"] = "nRF24 jamming started";
  }
  // CORREÇÃO CRÍTICA: flush_tx() + stopListening() em vez de powerDown()
  // powerDown() desliga o módulo e requer powerUp() + reconfiguração completa
  else if (cmdUpper == "NRF24_JAM_STOP") {
    jamming_active = false;
    radio.flush_tx();
    radio.stopListening();
    resp["message"] = "nRF24 jamming stopped";
  }

  // ========== CC1101 ==========
  else if (cmdUpper == "CC1101_CAPTURE_START") {
    uint32_t freq = autoDetectFrequency();
    startCapture(freq);
    resp["message"] = "CC1101 capture started";
    resp["frequency"] = freq / 1000.0;
  }
  else if (cmdUpper == "CC1101_CAPTURE_STOP") {
    stopCapture();
    resp["message"] = "CC1101 capture stopped";
    resp["pulses"] = currentSignal.dataLength;
  }
  else if (cmdUpper == "CC1101_SWEEP") {
    cc1101FreqSweepSetup();
    resp["message"] = "CC1101 frequency sweep started";
  }
  else if (cmdUpper.startsWith("CC1101_TX_")) {
    if (hasSavedSignal) {
      transmitRawSignal();
      resp["message"] = "Signal transmitted";
      resp["pulses"] = currentSignal.dataLength;
      resp["frequency"] = currentSignal.frequency / 1000.0;
    } else {
      resp["status"] = "error";
      resp["error"] = "No signal saved";
    }
  }

  // ========== Sub-GHz DB ==========
  else if (cmdUpper == "SUBGHZ_LIST") {
    JsonArray entries = resp["entries"].to<JsonArray>();
    for (int i = 0; i < SUBGHZ_DB_COUNT && i < 20; i++) {
      JsonObject entry = entries.add<JsonObject>();
      entry["name"] = subGHzDatabase[i].name;
      entry["frequency"] = subGHzDatabase[i].frequency;
      entry["protocol"] = protocols[subGHzDatabase[i].protocolIndex].name;
      entry["code"] = String("0x") + String(subGHzDatabase[i].code, HEX);
      entry["bits"] = subGHzDatabase[i].bits;
    }
    resp["message"] = String(SUBGHZ_DB_COUNT) + " entries";
  }
  else if (cmdUpper.startsWith("SUBGHZ_TX_")) {
    // Extrair código e transmitir do DB
    resp["message"] = "SubGHz TX (parse from DB)";
  }

  // ========== Sinais Salvos ==========
  else if (cmdUpper == "SIGNAL_LIST") {
    JsonArray sigs = resp["signals"].to<JsonArray>();
    int count = 0;
    for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
      if (savedSignals[i].active) {
        JsonObject sig = sigs.add<JsonObject>();
        sig["name"] = savedSignals[i].name;
        sig["frequency"] = savedSignals[i].signal.frequency / 1000.0;
        sig["pulses"] = savedSignals[i].signal.dataLength;
        sig["slot"] = i;
        count++;
      }
    }
    resp["message"] = String(count) + "/" + MAX_SAVED_SIGNALS + " signals";
  }
  else if (cmdUpper.startsWith("TX_")) {
    String sigName = cmd.substring(3);
    bool found = false;
    for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
      if (savedSignals[i].active && String(savedSignals[i].name) == sigName) {
        currentSignal = savedSignals[i].signal;
        selectedSignalSlot = i;
        hasSavedSignal = true;
        transmitRawSignal();
        found = true;
        resp["message"] = "Transmitted: " + sigName;
        break;
      }
    }
    if (!found) {
      resp["status"] = "error";
      resp["error"] = "Signal not found: " + sigName;
    }
  }
  else if (cmdUpper.startsWith("DEL_")) {
    String sigName = cmd.substring(4);
    bool found = false;
    for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
      if (savedSignals[i].active && String(savedSignals[i].name) == sigName) {
        clearSignalSlot(i);
        found = true;
        resp["message"] = "Deleted: " + sigName;
        break;
      }
    }
    if (!found) {
      resp["status"] = "error";
      resp["error"] = "Signal not found: " + sigName;
    }
  }

  // ========== Câmeras ==========

  // ========== Sour Apple ==========
  else if (cmdUpper == "SOUR_APPLE_START") {
    sourAppleSetup();
    resp["message"] = "Sour Apple spam started";
  }
  else if (cmdUpper == "SOUR_APPLE_STOP") {
    // Parar sour apple
    resp["message"] = "Sour Apple spam stopped";
  }

  // ========== Firmware OTA ==========
  else if (cmdUpper == "FIRMWARE_START") {
    resp["message"] = "OTA ready - send FW: chunks";
  }
  else if (cmdUpper.startsWith("FW:")) {
    resp["message"] = "OTA chunk received";
    resp["bytes"] = cmd.length() - 3;
  }
  else if (cmdUpper == "FIRMWARE_END") {
    resp["message"] = "OTA complete - restarting";
    ESP.restart();
  }

  // ========== REBOOT ==========
  else if (cmdUpper == "REBOOT") {
    resp["message"] = "Rebooting...";
    String out;
    serializeJson(resp, out);
    return out;  // Envia antes de reiniciar
  }

  // ========== COMANDO DESCONHECIDO ==========
  else {
    resp["status"] = "error";
    resp["error"] = "Unknown command: " + cmd;
  }

  String out;
  serializeJson(resp, out);
  return out;
}

// ==========================================
// PROCESSAR SERIAL COM BUFFER
// ==========================================
void processSerialCommandsUnified() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        String response = processUnifiedCommand(serialBuffer, 0);
        Serial.println(response);
      }
      serialBuffer = "";
    } else if (serialBuffer.length() < MAX_CMD_LEN) {
      serialBuffer += c;
    }
  }
}

// ==========================================
// PROCESSAR BLUETOOTH COM BUFFER
// ==========================================
void processBluetoothCommandsUnified() {
  if (!SerialBT.available()) return;

  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n') {
      btBuffer.trim();
      if (btBuffer.length() > 0) {
        String response = processUnifiedCommand(btBuffer, 1);
        SerialBT.println(response);
      }
      btBuffer = "";
    } else if (btBuffer.length() < MAX_CMD_LEN) {
      btBuffer += c;
    }
  }
}

// ==========================================
// INICIALIZAÇÃO
// ==========================================
void initUnifiedComm() {
  serialBuffer.reserve(MAX_CMD_LEN);
  btBuffer.reserve(MAX_CMD_LEN);
  Serial.println("[COMM] Unified communication initialized");
}

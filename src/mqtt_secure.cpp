#include "config.h"
#include <WiFiClientSecure.h>

// ==========================================
// MQTT SEGURO COM TLS - COMPATﾃ昂EL COM APK
// ==========================================

namespace {
  WiFiClientSecure* mqttSecureClient = nullptr;
  PubSubClient* mqttSecureClient_ps = nullptr;
  bool mqttSecureConnected = false;
  unsigned long lastMqttSecureReconnect = 0;
  unsigned long lastMqttSecurePublish = 0;
  char mqttTopicDevice[64];
  char mqttTopicControl[64];
  char mqttTopicStatus[64];
  char mqttTopicResponse[64];
  char mqttTopicCmd[64];
  bool mqttObjectsCreated = false;  // CORREﾃ�グ: Flag para tracking

  // Certificado CA para broker.hivemq.com (Let's Encrypt)
  static const char* hivemq_ca_cert = 
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgjS1EpxRk5r7nSJJ/3tJ+2uf/\n"
"2UbC+dWym2bC0hW0q5QV++yX0g0NTf+dVwjh4b1S60e3o+1Lr2u+S6fC9l4z7q0v\n"
"-----END CERTIFICATE-----\n";
}

// ==========================================
// VERIFICAﾃ�グ SEGURA DA CHAVE AES
// ==========================================
static bool isAesKeySet(const uint8_t* key, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (key[i] != 0) return true;
  }
  return false;
}

// ==========================================
// CALLBACK MQTT SEGURO
// ==========================================
void mqttSecureCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (length > 1024) return;

  char msg[1025];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.printf("[MQTT-TLS] RX [%s]: %s\n", topic, msg);

  String decrypted = msg;
  if (isAesKeySet(sessionAesKey, PAIR_AES_KEY_SIZE)) {
    String tryDecrypt = decryptAesGcm(String(msg), sessionAesKey);
    if (!tryDecrypt.isEmpty()) {
      decrypted = tryDecrypt;
    }
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, decrypted);
  if (err) {
    Serial.printf("[MQTT-TLS] JSON parse error: %s\n", err.c_str());
    return;
  }

  const char* cmd = doc["cmd"] | "";
  const char* token = doc["token"] | "";

  if (strcmp(cmd, "pair_request") == 0 || strcmp(cmd, "pair") == 0) {
    const char* code = doc["code"] | "";
    const char* deviceId = doc["device_id"] | "";

    if (validatePairCode(code)) {
      char newToken[65];
      generateSecureToken(newToken, sizeof(newToken));

      if (pairDevice(deviceId, newToken)) {
        uint8_t salt[16];
        esp_fill_random(salt, 16);
        deriveKeyFromPairCode(code, salt, sessionAesKey);
        memcpy(sessionSalt, salt, 16);

        JsonDocument resp;
        resp["status"] = "paired";
        resp["token"] = newToken;
        resp["device_fp"] = deviceFingerprint;
        resp["salt"] = base64Encode(salt, 16);

        String respStr;
        serializeJson(resp, respStr);

        String encryptedResp = encryptAesGcm(respStr, sessionAesKey);
        if (!encryptedResp.isEmpty() && mqttSecureClient_ps) {
          mqttSecureClient_ps->publish(mqttTopicDevice, encryptedResp.c_str());
        } else if (mqttSecureClient_ps) {
          mqttSecureClient_ps->publish(mqttTopicDevice, respStr.c_str());
        }

        pairingState = PAIR_CONFIRMED;
      }
    } else {
      JsonDocument resp;
      resp["status"] = "invalid_code";
      String respStr;
      serializeJson(resp, respStr);
      if (mqttSecureClient_ps) {
        mqttSecureClient_ps->publish(mqttTopicDevice, respStr.c_str());
      }
    }
    return;
  }

  if (!isDeviceAuthorized(token)) {
    JsonDocument resp;
    resp["status"] = "unauthorized";
    String respStr;
    serializeJson(resp, respStr);
    if (mqttSecureClient_ps) {
      mqttSecureClient_ps->publish(mqttTopicDevice, respStr.c_str());
    }
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

  String encryptedResp = encryptAesGcm(respStr, sessionAesKey);
  if (!encryptedResp.isEmpty() && mqttSecureClient_ps) {
    mqttSecureClient_ps->publish(mqttTopicDevice, encryptedResp.c_str());
  } else if (mqttSecureClient_ps) {
    mqttSecureClient_ps->publish(mqttTopicDevice, respStr.c_str());
  }
}

// ==========================================
// RECONNECT MQTT SEGURO
// ==========================================
void mqttSecureReconnect() {
  if (mqttSecureConnected) return;
  if (millis() - lastMqttSecureReconnect < 5000) return;
  lastMqttSecureReconnect = millis();

  if (WiFi.status() != WL_CONNECTED) return;

  snprintf(mqttTopicDevice, sizeof(mqttTopicDevice), "crazycat/%s/device", deviceFingerprint);
  snprintf(mqttTopicControl, sizeof(mqttTopicControl), "crazycat/%s/control", deviceFingerprint);
  snprintf(mqttTopicStatus, sizeof(mqttTopicStatus), "crazycat/%s/status", deviceFingerprint);
  snprintf(mqttTopicResponse, sizeof(mqttTopicResponse), "crazycat/%s/response", deviceFingerprint);
  snprintf(mqttTopicCmd, sizeof(mqttTopicCmd), "crazycat/%s/cmd", deviceFingerprint);

  String clientId = String("CrazyCat-") + String(deviceFingerprint);

  if (mqttSecureClient_ps && mqttSecureClient_ps->connect(clientId.c_str())) {
    mqttSecureClient_ps->subscribe(mqttTopicControl);
    mqttSecureClient_ps->subscribe(mqttTopicCmd);
    mqttSecureConnected = true;
    Serial.println("[MQTT-TLS] Connected to broker.hivemq.com:8883");

    JsonDocument doc;
    doc["status"] = "online";
    doc["firmware"] = FIRMWARE_VERSION;
    doc["paired"] = deviceIsPaired;
    doc["ip"] = WiFi.localIP().toString();

    String msg;
    serializeJson(doc, msg);
    if (mqttSecureClient_ps) {
      mqttSecureClient_ps->publish(mqttTopicStatus, msg.c_str(), true);
    }
  } else {
    Serial.printf("[MQTT-TLS] Connection failed, rc=%d\n", 
                  mqttSecureClient_ps ? mqttSecureClient_ps->state() : -1);
  }
}

// ==========================================
// LOOP MQTT SEGURO
// ==========================================
void mqttSecureLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (mqttSecureConnected) {
      mqttSecureConnected = false;
      // CORREﾃ�グ: Nﾃ｣o destrﾃｳi objetos, sﾃｳ marca desconectado para reconectar depois
    }
    return;
  }

  // CORREﾃ�グ: Inicializaﾃｧﾃ｣o tardia segura - sﾃｳ cria objetos uma vez
  if (!mqttObjectsCreated) {
    // CORREﾃ�グ: Limpa objetos antigos se existirem (evita memory leak)
    if (mqttSecureClient_ps) {
      mqttSecureClient_ps->disconnect();
      delete mqttSecureClient_ps;
      mqttSecureClient_ps = nullptr;
    }
    if (mqttSecureClient) {
      delete mqttSecureClient;
      mqttSecureClient = nullptr;
    }

    mqttSecureClient = new WiFiClientSecure();
    mqttSecureClient->setCACert(hivemq_ca_cert);
    mqttSecureClient->setTimeout(15);

    mqttSecureClient_ps = new PubSubClient(*mqttSecureClient);
    mqttSecureClient_ps->setServer("broker.hivemq.com", 8883);
    mqttSecureClient_ps->setCallback(mqttSecureCallback);
    mqttSecureClient_ps->setBufferSize(2048);

    mqttObjectsCreated = true;
    Serial.println("[MQTT-TLS] Secure MQTT client initialized (deferred)");
  }

  if (!mqttSecureClient_ps) return;

  if (!mqttSecureClient_ps->connected()) {
    mqttSecureConnected = false;
    mqttSecureReconnect();
  }

  if (mqttSecureClient_ps->connected()) {
    mqttSecureClient_ps->loop();

    if (millis() - lastMqttSecurePublish > 30000) {
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

      String encrypted = encryptAesGcm(msg, sessionAesKey);
      if (!encrypted.isEmpty() && mqttSecureClient_ps) {
        mqttSecureClient_ps->publish(mqttTopicStatus, encrypted.c_str(), true);
      } else if (mqttSecureClient_ps) {
        mqttSecureClient_ps->publish(mqttTopicStatus, msg.c_str(), true);
      }

      lastMqttSecurePublish = millis();
    }
  }
}

// ==========================================
// PUBLICAR MENSAGEM MQTT SEGURO
// ==========================================
bool mqttSecurePublish(const char* topic, const char* payload, bool retained) {
  if (!mqttSecureConnected || !mqttSecureClient_ps) return false;

  String encrypted = encryptAesGcm(String(payload), sessionAesKey);
  if (!encrypted.isEmpty()) {
    return mqttSecureClient_ps->publish(topic, encrypted.c_str(), retained);
  }
  return mqttSecureClient_ps->publish(topic, payload, retained);
}

// ==========================================
// INICIALIZAﾃ�グ MQTT SEGURO
// ==========================================
void initMqttSecure() {
  // CORREﾃ�グ: Nﾃ｣o cria objetos aqui - adiar atﾃｩ WiFi conectar
  mqttObjectsCreated = false;
  Serial.println("[MQTT-TLS] Secure MQTT client deferred init");
}

void destroyMqttSecure() {
  if (mqttSecureClient_ps) {
    mqttSecureClient_ps->disconnect();
    delete mqttSecureClient_ps;
    mqttSecureClient_ps = nullptr;
  }
  if (mqttSecureClient) {
    delete mqttSecureClient;
    mqttSecureClient = nullptr;
  }
  mqttSecureConnected = false;
  mqttObjectsCreated = false;
}

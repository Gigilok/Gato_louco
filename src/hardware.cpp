#include "config.h"

SPIClass* vspi = nullptr;
SPIClass* hspi = nullptr;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
RF24 radio(NRF_CE, NRF_CSN, 8000000);
Preferences prefs;
BluetoothSerial SerialBT;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool jamming_active = false;
uint8_t current_menu_item = 0;
volatile uint8_t current_screen = 0;
volatile bool back_pressed = false;

CC1101Signal currentSignal;
bool hasSavedSignal = false;
SavedSignal savedSignals[MAX_SAVED_SIGNALS];
int selectedSignalSlot = 0;

DispositivoPareado dispositivosPareados[PAIR_MAX_DEVICES];
int numDispositivosPareados = 0;
PairingState pairingState = PAIR_IDLE;
char currentPairCode[PAIR_CODE_LENGTH + 1] = "";
unsigned long pairCodeGeneratedAt = 0;
unsigned long pairTimeoutAt = 0;
uint8_t pairAesKey[PAIR_AES_KEY_SIZE];
bool deviceIsPaired = false;
char deviceFingerprint[33] = "";

void loadAllSignals() {
  hasSavedSignal = false;
  for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
    char keyName[16];
    snprintf(keyName, sizeof(keyName), "sig%d", i);
    size_t len = prefs.getBytesLength(keyName);
    if (len == sizeof(CC1101Signal)) {
      prefs.getBytes(keyName, &savedSignals[i].signal, sizeof(CC1101Signal));
      if (savedSignals[i].signal.frequency > 0 && savedSignals[i].signal.dataLength > 0) {
        savedSignals[i].active = true;
        snprintf(savedSignals[i].name, 16, "Sinal %d", i + 1);
        hasSavedSignal = true;
      } else {
        savedSignals[i].active = false;
        memset(savedSignals[i].name, 0, 16);
      }
    } else {
      savedSignals[i].active = false;
      memset(savedSignals[i].name, 0, 16);
    }
  }
  if (hasSavedSignal) {
    for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
      if (savedSignals[i].active) {
        currentSignal = savedSignals[i].signal;
        selectedSignalSlot = i;
        break;
      }
    }
  }
}

void saveSignalToSlot(int slot, const CC1101Signal& sig) {
  if (slot < 0 || slot >= MAX_SAVED_SIGNALS) return;
  char keyName[16];
  snprintf(keyName, sizeof(keyName), "sig%d", slot);
  prefs.putBytes(keyName, &sig, sizeof(CC1101Signal));
  savedSignals[slot].signal = sig;
  savedSignals[slot].active = true;
  snprintf(savedSignals[slot].name, 16, "Sinal %d", slot + 1);
  hasSavedSignal = true;
  currentSignal = sig;
  Serial.printf("Sinal salvo no slot %d: %.3f MHz, %d pulsos\n",
                slot, sig.frequency / 1000.0, sig.dataLength);
}

void clearSignalSlot(int slot) {
  if (slot < 0 || slot >= MAX_SAVED_SIGNALS) return;
  char keyName[16];
  snprintf(keyName, sizeof(keyName), "sig%d", slot);
  CC1101Signal emptySignal;
  memset(&emptySignal, 0, sizeof(CC1101Signal));
  prefs.putBytes(keyName, &emptySignal, sizeof(CC1101Signal));
  savedSignals[slot].active = false;
  memset(savedSignals[slot].name, 0, 16);
  hasSavedSignal = false;
  for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
    if (savedSignals[i].active) {
      hasSavedSignal = true;
      currentSignal = savedSignals[i].signal;
      break;
    }
  }
  Serial.printf("Sinal do slot %d apagado\n", slot);
}

void generateDeviceFingerprint(char* out, size_t len) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const uint8_t*)macStr, strlen(macStr));
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  for (int i = 0; i < 16 && i < (int)len - 1; i++) {
    snprintf(out + (i * 2), 3, "%02X", hash[i]);
  }
  out[min((size_t)32, len - 1)] = '\0';
}

void generatePairCode() {
  randomSeed(esp_random());
  for (int i = 0; i < PAIR_CODE_LENGTH; i++) {
    currentPairCode[i] = '0' + random(10);
  }
  currentPairCode[PAIR_CODE_LENGTH] = '\0';
  pairCodeGeneratedAt = millis();
  pairTimeoutAt = millis() + PAIR_TIMEOUT_MS;
  pairingState = PAIR_SHOWING_CODE;

  uint8_t salt[16];
  esp_fill_random(salt, 16);
  deriveKeyFromPairCode(currentPairCode, salt, pairAesKey);

  Serial.printf("[PAIR] Codigo gerado: %s\n", currentPairCode);
}

bool validatePairCode(const char* code) {
  if (strlen(code) != PAIR_CODE_LENGTH) return false;
  if (millis() > pairTimeoutAt) return false;
  return (strcmp(code, currentPairCode) == 0);
}

bool pairDevice(const char* deviceId, const char* token) {
  if (numDispositivosPareados >= PAIR_MAX_DEVICES) {
    for (int i = 0; i < PAIR_MAX_DEVICES - 1; i++) {
      dispositivosPareados[i] = dispositivosPareados[i + 1];
    }
    numDispositivosPareados = PAIR_MAX_DEVICES - 1;
  }

  int idx = numDispositivosPareados;
  strncpy(dispositivosPareados[idx].deviceId, deviceId, 32);
  dispositivosPareados[idx].deviceId[32] = '\0';
  strncpy(dispositivosPareados[idx].token, token, 64);
  dispositivosPareados[idx].token[64] = '\0';
  dispositivosPareados[idx].timestamp = millis();
  dispositivosPareados[idx].ativo = true;
  strncpy(dispositivosPareados[idx].nome, "Device", 20);
  dispositivosPareados[idx].nome[20] = '\0';
  numDispositivosPareados++;

  prefs.putInt("numPaired", numDispositivosPareados);
  char key[32];
  snprintf(key, sizeof(key), "paired_%d", idx);
  prefs.putBytes(key, &dispositivosPareados[idx], sizeof(DispositivoPareado));

  deviceIsPaired = true;
  Serial.printf("[PAIR] Dispositivo pareado: %s\n", deviceId);
  return true;
}

bool unpairDevice(const char* deviceId) {
  for (int i = 0; i < numDispositivosPareados; i++) {
    if (strcmp(dispositivosPareados[i].deviceId, deviceId) == 0) {
      dispositivosPareados[i].ativo = false;
      for (int j = i; j < numDispositivosPareados - 1; j++) {
        dispositivosPareados[j] = dispositivosPareados[j + 1];
      }
      numDispositivosPareados--;
      prefs.putInt("numPaired", numDispositivosPareados);
      if (numDispositivosPareados == 0) deviceIsPaired = false;
      return true;
    }
  }
  return false;
}

bool isDeviceAuthorized(const char* token) {
  if (!deviceIsPaired || numDispositivosPareados == 0) return false;
  for (int i = 0; i < numDispositivosPareados; i++) {
    if (dispositivosPareados[i].ativo && strcmp(dispositivosPareados[i].token, token) == 0) {
      return true;
    }
  }
  return false;
}

void encryptPayload(uint8_t* input, uint8_t* output, size_t len, uint8_t* iv) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, pairAesKey, 256);

  size_t nc_off = 0;
  uint8_t stream_block[16];
  mbedtls_aes_crypt_ctr(&aes, len, &nc_off, iv, stream_block, input, output);
  mbedtls_aes_free(&aes);
}

void decryptPayload(uint8_t* input, uint8_t* output, size_t len, uint8_t* iv) {
  encryptPayload(input, output, len, iv);
}

void loadPairedDevices() {
  numDispositivosPareados = prefs.getInt("numPaired", 0);
  if (numDispositivosPareados > PAIR_MAX_DEVICES) numDispositivosPareados = 0;

  for (int i = 0; i < numDispositivosPareados; i++) {
    char key[32];
    snprintf(key, sizeof(key), "paired_%d", i);
    size_t len = prefs.getBytesLength(key);
    if (len == sizeof(DispositivoPareado)) {
      prefs.getBytes(key, &dispositivosPareados[i], sizeof(DispositivoPareado));
    }
  }

  if (numDispositivosPareados > 0) deviceIsPaired = true;
  generateDeviceFingerprint(deviceFingerprint, sizeof(deviceFingerprint));
  Serial.printf("[PAIR] %d dispositivos carregados\n", numDispositivosPareados);
}

void setupHardware() {
  Serial.begin(115200);
  delay(100);

  // CORRECAO CRITICA: SPIFFS REMOVIDO - huge_app.csv nao tem particao SPIFFS
  // Todas as operacoes de arquivo agora usam NVS (Preferences) ou RAM

  if (!prefs.begin("madcat", false)) {
    Serial.println("NVS: Erro ao abrir namespace 'madcat'");
  }
  esp_task_wdt_reset();
  delay(10);

  selectedSignalSlot = prefs.getInt("selectedSlot", 0);
  if (selectedSignalSlot < 0 || selectedSignalSlot >= MAX_SAVED_SIGNALS) {
    selectedSignalSlot = 0;
  }

  loadPairedDevices();

  esp_task_wdt_reset();
  delay(10);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setContrast(prefs.getUChar("brightness", 128));
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Iniciando...");
  u8g2.sendBuffer();
  delay(100);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  delay(10);

  pinMode(CC_MISO, INPUT_PULLDOWN);
  delay(50);

  Serial.println("========================================");
  Serial.println("  Crazy Cat v3.3-POWER - ULTRA EDITION");
  Serial.println("========================================");

  SPI.begin(CC_SCK, CC_MISO, CC_MOSI, -1);
  esp_task_wdt_reset();
  delay(50);

  ELECHOUSE_cc1101.setSpiPin(CC_SCK, CC_MISO, CC_MOSI, CC_CSN);

  bool cc1101_ok = false;
  for (int tentativa = 0; tentativa < 3; tentativa++) {
    esp_task_wdt_reset();
    if (ELECHOUSE_cc1101.getCC1101()) {
      cc1101_ok = true;
      break;
    }
    delay(50);
  }

  if (cc1101_ok) {
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setGDO0(CC_GDO0);
    ELECHOUSE_cc1101.setGDO(CC_GDO0, CC_GDO2);
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setModulation(0);
    ELECHOUSE_cc1101.setMHZ(433.92);

    cc1101SetMaxPower();

    u8g2.drawStr(0, 25, "CC1101: OK");
    Serial.println("[CC1101] OK (MAX POWER)");
  } else {
    Serial.println("[CC1101] Falha");
    u8g2.drawStr(0, 25, "ERRO: CC1101");
  }
  u8g2.sendBuffer();
  delay(200);

  vspi = new SPIClass(VSPI);
  vspi->begin(NRF_SCK, NRF_MISO, NRF_MOSI, -1);
  delay(50);

  pinMode(NRF_CSN, OUTPUT);
  digitalWrite(NRF_CSN, HIGH);
  delay(10);

  bool nrf24_ok = false;
  for (int tentativa = 0; tentativa < 5 && !nrf24_ok; tentativa++) {
    esp_task_wdt_reset();
    if (tentativa > 0) {
      Serial.printf("[nRF24] Tentativa %d/5...\n", tentativa + 1);
      delay(500);
    }
    digitalWrite(NRF_CSN, HIGH);
    delay(50);
    nrf24_ok = radio.begin(vspi);
    if (nrf24_ok) {
      bool chipConnected = radio.isChipConnected();
      if (chipConnected) {
        radio.setPALevel(NRF24_PA_LEVEL);
        radio.setDataRate(NRF24_DATA_RATE);
        radio.setAutoAck(false);
        radio.stopListening();
      } else {
        nrf24_ok = false;
      }
    }
    if (!nrf24_ok) delay(200);
  }

  if (nrf24_ok) {
    u8g2.drawStr(0, 35, "nRF24: OK");
    u8g2.sendBuffer();
    Serial.println("[nRF24] OK (PA+LNA MAX POWER ready)");
  } else {
    Serial.println("[nRF24] Falha");
    u8g2.drawStr(0, 35, "ERRO: nRF24");
    u8g2.sendBuffer();
    delay(500);
  }
  delay(100);

  if (cc1101_ok) ELECHOUSE_cc1101.setSidle();
  radio.stopListening();

  Serial.println("[SPI] OK");
  delay(50);
  loadAllSignals();
  u8g2.clearBuffer();
}

bool buttonPressed(uint8_t pin) {
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
  static bool lastReading[4] = {HIGH, HIGH, HIGH, HIGH};
  static bool buttonState[4] = {HIGH, HIGH, HIGH, HIGH};
  static const uint8_t pinMap[4] = {BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK};
  uint8_t idx = 255;
  for (int i = 0; i < 4; i++) {
    if (pinMap[i] == pin) { idx = i; break; }
  }
  if (idx == 255) return false;
  bool reading = digitalRead(pin);
  if (reading != lastReading[idx]) {
    lastDebounceTime[idx] = millis();
  }
  if ((millis() - lastDebounceTime[idx]) > BUTTON_DEBOUNCE_MS) {
    if (reading != buttonState[idx]) {
      buttonState[idx] = reading;
      if (buttonState[idx] == LOW) {
        lastReading[idx] = reading;
        return true;
      }
    }
  }
  lastReading[idx] = reading;
  return false;
}

void checkBackInterruptFlag() {
  static bool backWasPressed = false;
  static unsigned long backPressTime = 0;
  bool backNow = (digitalRead(BTN_BACK) == LOW);
  if (backNow && !backWasPressed) {
    backWasPressed = true;
    backPressTime = millis();
  }
  if (!backNow && backWasPressed) {
    backWasPressed = false;
    unsigned long pressDuration = millis() - backPressTime;
    if (pressDuration > 50 && pressDuration < 800) {
      back_pressed = true;
    }
  }
  if (back_pressed) {
    back_pressed = false;
    // CORRECAO CRITICA: Para qualquer ataque WiFi ativo antes de voltar ao menu
    pararAtaqueAtual();
    current_screen = 0;
    radio.stopListening();
    ELECHOUSE_cc1101.setSidle();
    resetCC1101State();
    drawMenu();
  }
}

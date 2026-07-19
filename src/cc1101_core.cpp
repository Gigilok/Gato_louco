#include "config.h"
#include <driver/rmt.h>

namespace {
  CCState ccState = CC_IDLE;
  bool txPlaying = false;
  int txProgress = 0;
  int captureIndex = 0;
  uint32_t captureStartTime = 0;
  uint32_t detectedFreq = 0;

  volatile uint32_t lastEdgeUs = 0;
  volatile uint8_t lastPinState = 0;

  uint8_t paTableMax[8] = {
    0x00, CC1101_PA_TABLE_MAX, CC1101_PA_TABLE_MAX,
    CC1101_PA_TABLE_MAX, CC1101_PA_TABLE_MAX,
    CC1101_PA_TABLE_MAX, CC1101_PA_TABLE_MAX,
    CC1101_PA_TABLE_MAX
  };

  // RMT / Capture state
  bool rmtInitialized = false;
  bool rmtCapturing = false;
  RingbufHandle_t rmtRingbuf = nullptr;
}

// ==========================================
// FREQUENCIAS DE SCAN
// ==========================================
const uint32_t SCAN_FREQUENCIES[] = {
  300000, 303875, 304250, 310000, 315000, 318000, 330000, 345000, 348000,
  350000, 387000, 390000, 418000, 433050, 433420, 433920, 434420, 434775,
  438900, 439000, 464000, 467700, 468000, 779000, 868350, 868400, 868800,
  915000, 921000, 928000
};
const int NUM_FREQS = sizeof(SCAN_FREQUENCIES) / sizeof(SCAN_FREQUENCIES[0]);

const uint32_t BRUTE_FREQS[] = {
  300000, 303875, 304250, 310000, 315000, 318000, 330000, 345000, 348000,
  350000, 387000, 390000, 418000, 433050, 433420, 433920, 434420, 434775,
  438900, 439000, 464000, 467700, 468000, 779000, 868350, 868400, 868800,
  915000, 921000, 928000
};
const int NUM_BRUTE_FREQS = sizeof(BRUTE_FREQS) / sizeof(BRUTE_FREQS[0]);

// ==========================================
// 60+ PROTOCOLOS DECODIFICADOS
// ==========================================
const RCSwitchProtocol protocols[] = {
  {"PT2262",      350, {1,31}, {1, 3}, {3, 1}, false},
  {"EV1527",      320, {1,10}, {1, 3}, {3, 1}, false},
  {"HT12E",       400, {1,20}, {1, 3}, {3, 1}, false},
  {"SC2262",      350, {1,31}, {1, 3}, {3, 1}, false},
  {"HCS301",      420, {1, 5}, {1, 2}, {2, 1}, false},
  {"Keeloq",      400, {1,15}, {1, 2}, {2, 1}, false},
  {"Nice",        700, {1, 2}, {1, 2}, {2, 1}, false},
  {"BFT",         500, {1, 3}, {1, 2}, {2, 1}, false},
  {"Came",        320, {1,15}, {1, 3}, {3, 1}, false},
  {"FAAC",        650, {1, 2}, {1, 2}, {2, 1}, false},
  {"Chamberlain", 300, {1,10}, {1, 3}, {3, 1}, false},
  {"Linear",      400, {1,12}, {1, 3}, {3, 1}, false},
  {"Dickert",     500, {1, 5}, {1, 2}, {2, 1}, false},
  {"Hormann",     500, {1, 3}, {1, 2}, {2, 1}, false},
  {"Somfy",       600, {1, 4}, {1, 2}, {2, 1}, false},
  {"DeLOCK",      400, {1,10}, {1, 3}, {3, 1}, false},
  {"Intertechno", 300, {1,10}, {1, 3}, {3, 1}, false},
  {"Elro",        300, {1,10}, {1, 3}, {3, 1}, false},
  {"Kaku",        300, {1,10}, {1, 3}, {3, 1}, false},
  {"HomeEasy",    300, {1,10}, {1, 3}, {3, 1}, false},
  {"Byron",       300, {1,10}, {1, 3}, {3, 1}, false},
  {"Visonic",     300, {1,10}, {1, 3}, {3, 1}, false},
  {"Ansonic",     400, {1,15}, {1, 3}, {3, 1}, false},
  {"Heidemann",   350, {1,12}, {1, 3}, {3, 1}, false},
  {"Kangtai",     330, {1,10}, {1, 3}, {3, 1}, false},
  {"Stanley",     400, {1, 8}, {1, 2}, {2, 1}, false},
  {"Genie",       620, {1, 2}, {1, 2}, {2, 1}, false},
  {"Marantec",    500, {1, 3}, {1, 2}, {2, 1}, false},
  {"MultiCode",   400, {1,10}, {1, 3}, {3, 1}, false},
  {"LinearDelta", 350, {1,12}, {1, 3}, {3, 1}, false},
  {"WayneDalton", 500, {1, 3}, {1, 2}, {2, 1}, false},
  {"MightyMule",  450, {1, 5}, {1, 2}, {2, 1}, false},
  {"GTO",         450, {1, 5}, {1, 2}, {2, 1}, false},
  {"ALEKO",       400, {1,10}, {1, 3}, {3, 1}, false},
  {"GhostControls",500,{1, 3}, {1, 2}, {2, 1}, false},
  {"CAME_TW",     320, {1,15}, {1, 3}, {3, 1}, false},
  {"DOOYA",       400, {1,10}, {1, 3}, {3, 1}, false},
  {"SMARSECUR",   350, {1,10}, {1, 3}, {3, 1}, false},
  {"SONOFF",      300, {1,10}, {1, 3}, {3, 1}, false},
  {"TUYA",        320, {1,10}, {1, 3}, {3, 1}, false},
  {"XIAOMI",      400, {1, 8}, {1, 2}, {2, 1}, false},
  {"BROADLINK",   350, {1,10}, {1, 3}, {3, 1}, false},
  {"ORVIBO",      330, {1,10}, {1, 3}, {3, 1}, false},
  {"LIVOLO",      300, {1,10}, {1, 3}, {3, 1}, false},
  {"KERUI",       400, {1,10}, {1, 3}, {3, 1}, false},
  {"HOLTEC",      340, {1,10}, {1, 3}, {3, 1}, false},
  {"SYN115",      400, {1,10}, {1, 3}, {3, 1}, false},
  {"WL101",       350, {1,10}, {1, 3}, {3, 1}, false},
  {"MXT",         380, {1,10}, {1, 3}, {3, 1}, false},
  {"PHILIO",      360, {1,10}, {1, 3}, {3, 1}, false},
  {"SITERWELL",   340, {1,10}, {1, 3}, {3, 1}, false},
  {"DIGOO",       330, {1,10}, {1, 3}, {3, 1}, false},
  {"BLITZWOLF",   350, {1,10}, {1, 3}, {3, 1}, false},
  {"TECKIN",      320, {1,10}, {1, 3}, {3, 1}, false},
  {"GOSUND",      340, {1,10}, {1, 3}, {3, 1}, false},
  {"PARADOX",     400, {1,10}, {1, 3}, {3, 1}, false},
  {"DSC",         500, {1, 8}, {1, 2}, {2, 1}, false},
  {"HONEYWELL",   450, {1,10}, {1, 3}, {3, 1}, false},
  {"CONCORD",     400, {1,10}, {1, 3}, {3, 1}, false},
  {"ADEMCO",      400, {1,10}, {1, 3}, {3, 1}, false},
  {"Custom",      400, {1,10}, {1, 3}, {3, 1}, false}
};
const int NUM_PROTOCOLS = sizeof(protocols) / sizeof(protocols[0]);

const uint32_t COMMON_CODES[] = {
  0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
  0x00000010, 0x00000011, 0x00000012, 0x00000013, 0x00000014,
  0x00000020, 0x00000021, 0x00000022, 0x00000023, 0x00000024,
  0x00000030, 0x00000031, 0x00000032, 0x00000033, 0x00000034,
  0x00000100, 0x00000101, 0x00000102, 0x00000103, 0x00000104,
  0x00001000, 0x00001001, 0x00001002, 0x00001003, 0x00001004,
  0x00010000, 0x00010001, 0x00010002, 0x00010003, 0x00010004,
  0x00100000, 0x00100001, 0x00100002, 0x00100003, 0x00100004,
  0x01000000, 0x01000001, 0x01000002, 0x01000003, 0x01000004,
  0x10000000, 0x10000001, 0x10000002, 0x10000003, 0x10000004
};
const int NUM_COMMON_CODES = sizeof(COMMON_CODES) / sizeof(COMMON_CODES[0]);

void resetCC1101State() {
  ccState = CC_IDLE;
  txPlaying = false;
  txProgress = 0;
  captureIndex = 0;
  captureStartTime = 0;
  detectedFreq = 0;
  ELECHOUSE_cc1101.setSidle();
}

// ==========================================
// POTENCIA MAXIMA CC1101
// Configura PA_TABLE e FREND0 para maxima saida
// ==========================================
void cc1101SetMaxPower() {
  // PA_TABLE: index 0 = 0x00 (shutdown), index 1-7 = max power
  ELECHOUSE_cc1101.SpiWriteBurstReg(0x3E, paTableMax, 8);
  // FREND0: use PA_TABLE index 1 para TX, enable PA ramping
  ELECHOUSE_cc1101.SpiWriteReg(0x22, CC1101_FREND0_MAX);
  Serial.println("[CC1101] PA_TABLE + FREND0 configured for MAX POWER");
}

// ==========================================
// RMT CAPTURE - Compatibilidade ESP-IDF 5.x
// Usa driver legacy via driver/rmt.h
// ==========================================
void initRMTCapture() {
  if (rmtInitialized) return;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  // ESP-IDF 5.x: usar driver legacy se disponivel
  // Configuracao compativel via rmt_config_t legacy
  rmt_config_t rmt_rx = {};
  rmt_rx.channel = RMT_RX_CHANNEL;
  rmt_rx.gpio_num = (gpio_num_t)CC_GDO0;
  rmt_rx.clk_div = RMT_CLK_DIV;
  rmt_rx.mem_block_num = 4;
  rmt_rx.rmt_mode = RMT_MODE_RX;
  rmt_rx.rx_config.filter_en = true;
  rmt_rx.rx_config.filter_ticks_thresh = RMT_FILTER_TICKS;
  rmt_rx.rx_config.idle_threshold = RMT_IDLE_THRESH;
#else
  rmt_config_t rmt_rx;
  rmt_rx.channel = RMT_RX_CHANNEL;
  rmt_rx.gpio_num = (gpio_num_t)CC_GDO0;
  rmt_rx.clk_div = RMT_CLK_DIV;
  rmt_rx.mem_block_num = 4;
  rmt_rx.rmt_mode = RMT_MODE_RX;
  rmt_rx.rx_config.filter_en = true;
  rmt_rx.rx_config.filter_ticks_thresh = RMT_FILTER_TICKS;
  rmt_rx.rx_config.idle_threshold = RMT_IDLE_THRESH;
#endif

  esp_err_t err = rmt_config(&rmt_rx);
  if (err != ESP_OK) {
    Serial.printf("[RMT] Config error: %d\n", err);
    return;
  }

  err = rmt_driver_install(RMT_RX_CHANNEL, 4000, 0);
  if (err != ESP_OK) {
    Serial.printf("[RMT] Driver install error: %d\n", err);
    return;
  }

  err = rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &rmtRingbuf);
  if (err != ESP_OK || !rmtRingbuf) {
    Serial.printf("[RMT] Ringbuf error: %d\n", err);
    return;
  }

  rmtInitialized = true;
  Serial.println("[RMT] Capture driver initialized (1us precision)");
}

void stopRMTCapture() {
  if (!rmtInitialized) return;
  rmt_rx_stop(RMT_RX_CHANNEL);
  rmtCapturing = false;
}

bool isRMTCapturing() {
  return rmtCapturing;
}

void captureRawSignalRMT() {
  if (!rmtInitialized) {
    initRMTCapture();
  }
  if (!rmtInitialized) {
    Serial.println("[RMT] Failed to initialize, using bit-bang fallback");
    return;
  }

  size_t rx_size = 0;
  rmt_item32_t* dummy;
  while ((dummy = (rmt_item32_t*)xRingbufferReceive(rmtRingbuf, &rx_size, 0)) != NULL) {
    vRingbufferReturnItem(rmtRingbuf, (void*)dummy);
  }

  rmt_rx_start(RMT_RX_CHANNEL, true);
  rmtCapturing = true;
  captureStartTime = millis();
  Serial.println("[RMT] Capturing...");
}

void processRMTRawData() {
  if (!rmtCapturing) return;

  size_t rx_size = 0;
  rmt_item32_t* items = (rmt_item32_t*)xRingbufferReceive(rmtRingbuf, &rx_size, 0);

  if (items) {
    int num_items = rx_size / sizeof(rmt_item32_t);
    for (int i = 0; i < num_items && captureIndex < MAX_RAW_DATA - 2; i++) {
      if (items[i].duration0 > 0) {
        currentSignal.timings[captureIndex] = items[i].duration0;
        currentSignal.levels[captureIndex] = items[i].level0;
        captureIndex++;
      }
      if (items[i].duration1 > 0 && captureIndex < MAX_RAW_DATA - 1) {
        currentSignal.timings[captureIndex] = items[i].duration1;
        currentSignal.levels[captureIndex] = items[i].level1;
        captureIndex++;
      }
    }
    vRingbufferReturnItem(rmtRingbuf, (void*)items);
  }

  if (millis() - captureStartTime > CAPTURE_TIMEOUT_MS || captureIndex >= MAX_RAW_DATA - 10) {
    stopRMTCapture();
    if (captureIndex >= 20) {
      currentSignal.dataLength = captureIndex;
      currentSignal.timestamp = millis();
      hasSavedSignal = true;
      Serial.printf("[RMT] Capturado: %d pulsos em %.3f MHz\n",
                    captureIndex, detectedFreq / 1000.0);
    } else {
      resetCC1101State();
      Serial.println("[RMT] Timeout sem sinal suficiente");
    }
  }
}

// ==========================================
// CAPTURA BIT-BANGING (fallback robusto)
// ==========================================
void startCapture(uint32_t frequency) {
  resetCC1101State();
  ELECHOUSE_cc1101.setMHZ(frequency / 1000.0);
  ELECHOUSE_cc1101.SetRx();
  ccState = CC_CAPTURING;
  captureStartTime = millis();
  captureIndex = 0;
  detectedFreq = frequency;
  memset(&currentSignal, 0, sizeof(CC1101Signal));
  currentSignal.frequency = frequency;
  lastEdgeUs = micros();
  lastPinState = digitalRead(CC_GDO0);
  captureRawSignalRMT();
}

void stopCapture() {
  stopRMTCapture();
  ELECHOUSE_cc1101.setSidle();
  ccState = CC_IDLE;
  if (captureIndex > 0) {
    currentSignal.dataLength = captureIndex;
    currentSignal.timestamp = millis();
    hasSavedSignal = true;
    Serial.printf("[CC1101] Capturado: %d pulsos em %.3f MHz\n",
                  captureIndex, detectedFreq / 1000.0);
  }
}

void captureStep() {
  if (rmtCapturing) {
    processRMTRawData();
    return;
  }

  if (ccState != CC_CAPTURING) return;
  if (captureIndex >= MAX_RAW_DATA) {
    stopCapture();
    return;
  }

  uint8_t pinState = digitalRead(CC_GDO0);
  uint32_t now = micros();

  if (pinState != lastPinState) {
    uint32_t duration = now - lastEdgeUs;
    if (duration >= 50) {
      if (captureIndex < MAX_RAW_DATA) {
        currentSignal.levels[captureIndex] = lastPinState;
        currentSignal.timings[captureIndex] = (uint16_t)(duration > 65535 ? 65535 : duration);
        captureIndex++;
      }
      lastEdgeUs = now;
    }
    lastPinState = pinState;
  }

  if (millis() - captureStartTime > CAPTURE_TIMEOUT_MS) {
    if (captureIndex >= 10) {
      stopCapture();
    } else {
      resetCC1101State();
      Serial.println("[CC1101] Timeout sem sinal");
    }
  }
}

// ==========================================
// TRANSMISSAO
// ==========================================
void transmitBit(const RCSwitchProtocol& p, bool bit) {
  uint16_t high = bit ? p.one[0] : p.zero[0];
  uint16_t low = bit ? p.one[1] : p.zero[1];
  digitalWrite(CC_GDO0, HIGH);
  delayMicroseconds(p.pulseLength * high);
  digitalWrite(CC_GDO0, LOW);
  delayMicroseconds(p.pulseLength * low);
}

void transmitSync(const RCSwitchProtocol& p) {
  digitalWrite(CC_GDO0, HIGH);
  delayMicroseconds(p.pulseLength * p.syncFactor[0]);
  digitalWrite(CC_GDO0, LOW);
  delayMicroseconds(p.pulseLength * p.syncFactor[1]);
}

void transmitCode(uint32_t code, int bits, const RCSwitchProtocol& p) {
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(currentSignal.frequency / 1000.0);
  ELECHOUSE_cc1101.setModulation(2);
  cc1101SetMaxPower();
  pinMode(CC_GDO0, OUTPUT);
  digitalWrite(CC_GDO0, LOW);

  for (int repeat = 0; repeat < CC1101_TX_REPEATS; repeat++) {
    transmitSync(p);
    for (int i = bits - 1; i >= 0; i--) {
      bool bit = (code >> i) & 1;
      transmitBit(p, bit);
    }
    delayMicroseconds(p.pulseLength * 31);
  }

  digitalWrite(CC_GDO0, LOW);
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.SpiWriteReg(0x00, 0x2E);
}

void transmitRawSignal() {
  if (!hasSavedSignal || currentSignal.dataLength == 0) {
    Serial.println("[CC1101] Nenhum sinal salvo");
    return;
  }

  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(currentSignal.frequency / 1000.0);
  ELECHOUSE_cc1101.setModulation(2);
  cc1101SetMaxPower();
  pinMode(CC_GDO0, OUTPUT);

  for (int repeat = 0; repeat < 10; repeat++) {
    for (int i = 0; i < currentSignal.dataLength; i++) {
      digitalWrite(CC_GDO0, currentSignal.levels[i] ? HIGH : LOW);
      delayMicroseconds(currentSignal.timings[i]);
    }
    digitalWrite(CC_GDO0, LOW);
    delayMicroseconds(10000);
  }

  digitalWrite(CC_GDO0, LOW);
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setModulation(0);
}

// ==========================================
// JAMMING CC1101
// ==========================================
void jamChannel(uint32_t freqKHz) {
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(freqKHz / 1000.0);
  ELECHOUSE_cc1101.setModulation(2);
  cc1101SetMaxPower();
  ELECHOUSE_cc1101.SpiWriteReg(0x07, 0x00);
  ELECHOUSE_cc1101.SpiWriteReg(0x08, 0x02);
  for (int i = 0; i < 64; i++) {
    ELECHOUSE_cc1101.SpiWriteReg(0x3F, random(256));
  }
  ELECHOUSE_cc1101.SetTx();
  delay(100);
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.SpiWriteReg(0x07, 0x0E);
  ELECHOUSE_cc1101.SpiWriteReg(0x08, 0x45);
}

void jamChannelContinuous(uint32_t freqKHz) {
  static bool jammerActive = false;
  static uint32_t jamFreq = 0;

  if (freqKHz == 0) {
    if (jammerActive) {
      ELECHOUSE_cc1101.setSidle();
      jammerActive = false;
      Serial.println("[CC1101] Continuous jammer STOPPED");
    }
    return;
  }

  if (!jammerActive || jamFreq != freqKHz) {
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(freqKHz / 1000.0);
    ELECHOUSE_cc1101.setModulation(2);
    cc1101SetMaxPower();
    ELECHOUSE_cc1101.SpiWriteReg(0x08, 0x02);
    ELECHOUSE_cc1101.SpiWriteReg(0x07, 0x00);
    for (int i = 0; i < 64; i++) {
      ELECHOUSE_cc1101.SpiWriteReg(0x3F, random(256));
    }
    ELECHOUSE_cc1101.SetTx();
    jammerActive = true;
    jamFreq = freqKHz;
    Serial.printf("[CC1101] Continuous jammer at %.3f MHz (MAX POWER)\n", freqKHz / 1000.0);
  }

  static unsigned long lastRefill = 0;
  if (millis() - lastRefill > 50) {
    for (int i = 0; i < 32; i++) {
      ELECHOUSE_cc1101.SpiWriteReg(0x3F, random(256));
    }
    lastRefill = millis();
  }
}

// ==========================================
// DE BRUIJN SEQUENCE
// ==========================================
static void db(int t, int p, int k, int n, uint8_t* a, uint32_t* output, int* idx, int maxLen) {
  if (*idx >= maxLen) return;
  if (t > n) {
    if (n % p == 0) {
      for (int j = 1; j <= p && *idx < maxLen; j++) {
        output[(*idx)++] = a[j];
      }
    }
  } else {
    a[t] = a[t - p];
    db(t + 1, p, k, n, a, output, idx, maxLen);
    for (int j = a[t - p] + 1; j < k && *idx < maxLen; j++) {
      a[t] = j;
      db(t + 1, t, k, n, a, output, idx, maxLen);
    }
  }
}

void generateDeBruijnSequence(uint8_t k, int n, uint32_t* output, int maxLen) {
  uint8_t* a = (uint8_t*)calloc(k * n, sizeof(uint8_t));
  int idx = 0;
  db(1, 1, k, n, a, output, &idx, maxLen);
  free(a);
}

int getDeBruijnLength(int k, int n) {
  int result = 1;
  for (int i = 0; i < n; i++) result *= k;
  return result;
}

// ==========================================
// FORMATO .sub FLIPPER ZERO
// ==========================================
bool exportSignalToSubFormat(const CC1101Signal& sig, String& output) {
  output = "Filetype: Flipper SubGHz RAW File\n";
  output += "Version: 1\n";
  output += "Frequency: " + String(sig.frequency) + "\n";
  output += "Preset: FuriHalSubGhzPresetOok650Async\n";
  output += "Protocol: RAW\n";
  output += "RAW_Data: ";

  for (int i = 0; i < sig.dataLength && i < 512; i++) {
    if (i > 0) output += " ";
    int timing = sig.timings[i];
    if (sig.levels[i] == 0) timing = -timing;
    output += String(timing);
  }
  output += "\n";
  return true;
}

bool saveSignalToSubFile(const char* filename, const CC1101Signal& sig) {
  String content;
  if (!exportSignalToSubFormat(sig, content)) return false;
  // CORRECAO: Sem SPIFFS - usa NVS (Preferences)
  String key = String("sub_") + filename;
  key.replace("/", "_");
  key.replace(".", "_");
  if (key.length() > 15) key = key.substring(0, 15);  // NVS limite de chave
  prefs.putString(key.c_str(), content);
  Serial.printf("[SUB] Salvo em NVS: %s (%d bytes)\n", key.c_str(), content.length());
  return true;
}

bool loadSignalFromSubFile(const char* filename, CC1101Signal& sig) {
  // CORRECAO: Sem SPIFFS - usa NVS (Preferences)
  String key = String("sub_") + filename;
  key.replace("/", "_");
  key.replace(".", "_");
  if (key.length() > 15) key = key.substring(0, 15);

  if (!prefs.isKey(key.c_str())) return false;

  String content = prefs.getString(key.c_str(), "");
  if (content.isEmpty()) return false;

  memset(&sig, 0, sizeof(CC1101Signal));
  int pos = 0;
  while (pos < content.length()) {
    int end = content.indexOf('\n', pos);
    if (end == -1) end = content.length();
    String line = content.substring(pos, end);
    line.trim();
    if (line.startsWith("Frequency:")) {
      sig.frequency = line.substring(10).toInt();
    }
    else if (line.startsWith("RAW_Data:")) {
      String data = line.substring(9);
      data.trim();
      int idx = 0;
      int start = 0;
      while (start < data.length() && idx < MAX_RAW_DATA) {
        int space = data.indexOf(' ', start);
        if (space == -1) space = data.length();
        String val = data.substring(start, space);
        int timing = val.toInt();
        sig.levels[idx] = (timing >= 0) ? 1 : 0;
        sig.timings[idx] = abs(timing);
        idx++;
        start = space + 1;
      }
      sig.dataLength = idx;
    }
    pos = end + 1;
  }
  Serial.printf("[SUB] Carregado de NVS: %s (%d pulsos)\n", key.c_str(), sig.dataLength);
  return sig.dataLength > 0;
}

uint32_t autoDetectFrequency() {
  int bestRssi = -120;
  uint32_t bestFreq = 433920;
  for (int i = 0; i < NUM_FREQS; i++) {
    ELECHOUSE_cc1101.setMHZ(SCAN_FREQUENCIES[i] / 1000.0);
    ELECHOUSE_cc1101.SetRx();
    delay(20);
    int rssi = ELECHOUSE_cc1101.getRssi();
    if (rssi > bestRssi) {
      bestRssi = rssi;
      bestFreq = SCAN_FREQUENCIES[i];
    }
  }
  ELECHOUSE_cc1101.setSidle();
  Serial.printf("[CC1101] Auto: %.3f MHz (RSSI %d)\n", bestFreq / 1000.0, bestRssi);
  return bestFreq;
}

CCState getCCState() { return ccState; }
void setCCState(CCState state) { ccState = state; }
bool isTXPlaying() { return txPlaying; }
void setTXPlaying(bool val) { txPlaying = val; }
int getTXProgress() { return txProgress; }
int getCaptureIndex() { return captureIndex; }
void setCaptureIndex(int val) { captureIndex = val; }
void incrementCaptureIndex() { captureIndex++; }
uint32_t getCaptureStartTime() { return captureStartTime; }
uint32_t getDetectedFrequency() { return detectedFreq; }
void setDetectedFrequency(uint32_t freq) { detectedFreq = freq; }

DecodedSignal decodeCapturedSignal() {
  DecodedSignal result = {0, -1, 0, false};
  if (currentSignal.dataLength < 20) return result;

  for (int p = 0; p < NUM_PROTOCOLS; p++) {
    uint32_t code = 0;
    int bits = 0;
    int i = 0;

    while (i < currentSignal.dataLength - 1 && currentSignal.levels[i] == 1) i++;
    while (i < currentSignal.dataLength - 1 && currentSignal.levels[i] == 0) i++;

    while (bits < 32 && i < currentSignal.dataLength - 1) {
      uint16_t highTime = currentSignal.timings[i];
      uint16_t lowTime = (i + 1 < currentSignal.dataLength) ? currentSignal.timings[i + 1] : 0;

      float highRatio = (float)highTime / protocols[p].pulseLength;
      float lowRatio = (float)lowTime / protocols[p].pulseLength;

      if (abs(highRatio - protocols[p].one[0]) < 1.5 && abs(lowRatio - protocols[p].one[1]) < 1.5) {
        code = (code << 1) | 1;
        bits++;
      } else if (abs(highRatio - protocols[p].zero[0]) < 1.5 && abs(lowRatio - protocols[p].zero[1]) < 1.5) {
        code = (code << 1) | 0;
        bits++;
      } else {
        break;
      }
      i += 2;
    }

    if (bits >= 16) {
      result.code = code;
      result.protocolIndex = p;
      result.bits = bits;
      result.valid = true;
      return result;
    }
  }
  return result;
}

void analyzeProtocol() {
  DecodedSignal decoded = decodeCapturedSignal();
  if (decoded.valid) {
    Serial.printf("[CC1101] Protocolo: %s | Code: 0x%08X | Bits: %d\n",
                  protocols[decoded.protocolIndex].name, decoded.code, decoded.bits);
  } else {
    Serial.println("[CC1101] Protocolo desconhecido");
  }
}

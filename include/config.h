#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <U8g2lib.h>
#include <RF24.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <string>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_task_wdt.h>
#include <driver/rmt.h>
#include <BluetoothSerial.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <esp_gap_ble_api.h>
#include <PubSubClient.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/gcm.h>
#include <mbedtls/base64.h>
#include <Update.h>

// ==========================================
// VERSAO
// ==========================================
#define FIRMWARE_VERSION "Crazy Cat v3.3-POWER"
#define FIRMWARE_SHORT   "v3.3p"

// ==========================================
// PINOUT
// ==========================================
#define OLED_SDA 21
#define OLED_SCL 22

#define NRF_CE   26
#define NRF_CSN  25
#define NRF_SCK  18
#define NRF_MISO 19
#define NRF_MOSI 23

#define CC_CSN   27
#define CC_SCK   14
#define CC_MISO  12
#define CC_MOSI  13
#define CC_GDO0  4
#define CC_GDO2  16

#define BTN_UP     17
#define BTN_DOWN   15
#define BTN_SELECT 32
#define BTN_BACK   33

// ==========================================
// BUFFER RAW (Flipper Zero compativel)
// ==========================================
#define MAX_SAVED_SIGNALS 3
#define MAX_RAW_DATA 1024
#define RSSI_THRESHOLD -80
#define CAPTURE_TIMEOUT_MS 5000
#define BUTTON_DEBOUNCE_MS 180

// ==========================================
// RMT LEGACY (compatibilidade ESP-IDF 5.x via driver legacy)
// ==========================================
#define RMT_RX_CHANNEL    RMT_CHANNEL_0
#define RMT_CLK_DIV       80
#define RMT_MAX_PULSES    4096
#define RMT_FILTER_TICKS  50
#define RMT_IDLE_THRESH   10000

// ==========================================
// WiFi / REDE
// ==========================================
#define MAX_REDES 30
#define TAMANHO_SSID 33
#define TAMANHO_SENHA 65
#define TAMANHO_TIPO_SEG 16
#define NOME_DISPOSITIVO "CrazyCat"
#define SENHA_AP_PADRAO "12345678"
#define PORTA_DNS 53

// ==========================================
// PAREAMENTO
// ==========================================
#define PAIR_CODE_LENGTH 8
#define PAIR_TOKEN_LENGTH 64
#define PAIR_AES_KEY_SIZE 32
#define PAIR_TIMEOUT_MS 300000
#define PAIR_MAX_DEVICES 5
#define PAIR_CODE_REFRESH_MS 60000

// ==========================================
// MQTT
// ==========================================
#define MQTT_BROKER_PRIMARY "broker.hivemq.com"
#define MQTT_BROKER_SECONDARY "broker.emqx.io"
#define MQTT_PORT 1883
#define MQTT_TLS_PORT 8883
#define MQTT_TOPIC_PREFIX "crazycat/"

// ==========================================
// POTENCIA MAXIMA - ESP32 WiFi
// ==========================================
#define WIFI_MAX_TX_POWER 78
#define WIFI_MAX_TX_POWER_ARDUINO WIFI_POWER_19_5dBm

// ==========================================
// POTENCIA MAXIMA - nRF24 + PA+LNA
// RF_SETUP: CONT_WAVE=1, PLL_LOCK=1, RF_PWR=11 (+0dBm chip + 20dBm PA = +20dBm total)
// ==========================================
#define NRF24_RF_SETUP_VALUE  0x93
#define NRF24_PA_LEVEL        RF24_PA_MAX
#define NRF24_DATA_RATE       RF24_2MBPS
#define JAM_CARRIER_DWELL_MS  2

// ==========================================
// POTENCIA MAXIMA - CC1101
// PA_TABLE: 0xC0 = +12dBm @ 433MHz / +10dBm @ 868MHz (max do chip)
// FREND0: 0x11 = use PA_TABLE index 1 for TX (max power setting)
// ==========================================
#define CC1101_PA_TABLE_MAX   0xC0
#define CC1101_TX_REPEATS     20
#define CC1101_FREND0_MAX     0x11

// ==========================================
// DEAUTH HIGH POWER
// ==========================================
#define DEAUTH_BURST_SIZE 200
#define DEAUTH_BURST_INTERVAL_MS 5

// ==========================================
// FORMATOS COMPATIVEIS
// ==========================================
#define SUBGHZ_FILE_VERSION 1
#define SUBGHZ_MAX_NAME_LEN 64

// ==========================================
// ESTRUTURAS
// ==========================================
struct CC1101Signal {
  uint32_t frequency;
  uint16_t timings[MAX_RAW_DATA];
  uint8_t  levels[MAX_RAW_DATA];
  int      dataLength;
  int      modulation;
  int      rssi;
  uint32_t timestamp;
  char     name[SUBGHZ_MAX_NAME_LEN];
  uint8_t  preset;
};

struct SavedSignal {
  CC1101Signal signal;
  bool active;
  char name[16];
};

struct RCSwitchProtocol {
  const char* name;
  uint16_t pulseLength;
  uint8_t syncFactor[2];
  uint8_t zero[2];
  uint8_t one[2];
  bool inverted;
};

struct DecodedSignal {
  uint32_t code;
  int protocolIndex;
  int bits;
  bool valid;
};

struct SubGHzEntry {
  const char* name;
  float frequency;
  int protocolIndex;
  uint32_t code;
  int bits;
};

struct RedeWiFi {
  char ssid[TAMANHO_SSID];
  int rssi;
  uint8_t bssid[6];
  int canal;
  bool criptografada;
  bool wpsAtivo;
  char tipoSeguranca[TAMANHO_TIPO_SEG];
};

struct DispositivoPareado {
  char deviceId[33];
  char token[65];
  uint32_t timestamp;
  bool ativo;
  char nome[21];
};

enum PairingState {
  PAIR_IDLE,
  PAIR_SHOWING_CODE,
  PAIR_WAITING,
  PAIR_CONFIRMED,
  PAIR_FAILED
};

enum BFMode { BF_MODE_COMMON, BF_MODE_DEBRUIJN, BF_MODE_FULL, BF_MODE_ROLLJAM, BF_MODE_ROLLINGPWN, BF_MODE_SMART };
enum BFState { BF_IDLE, BF_RUNNING, BF_PAUSED };
enum CCState { CC_IDLE, CC_SCANNING, CC_CAPTURING, CC_TRANSMITTING };

// ==========================================
// EXTERNS GLOBAIS
// ==========================================
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern RF24 radio;
extern Preferences prefs;
extern BluetoothSerial SerialBT;
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;

extern SPIClass* vspi;

extern bool jamming_active;
extern uint8_t current_menu_item;
extern volatile uint8_t current_screen;
extern volatile bool back_pressed;

extern CC1101Signal currentSignal;
extern bool hasSavedSignal;
extern SavedSignal savedSignals[MAX_SAVED_SIGNALS];
extern int selectedSignalSlot;

// RollJam / RollingPwn
extern bool rolljamActive;
extern uint32_t rolljamCode1;
extern uint32_t rolljamCode2;
extern bool rolljamHasCode1;
extern bool rolljamHasCode2;
extern int rolljamStep;
extern unsigned long rolljamTimer;
extern int rolljamProtocol1;
extern int rolljamProtocol2;
extern int rolljamBits1;
extern int rolljamBits2;
extern bool rollingPwnActive;
extern int rollingPwnStep;
extern int rollingPwnCounter;
extern unsigned long rollingPwnTimer;
extern uint32_t capturedCodes[10];
extern int capturedCodeCount;
extern int capturedProtocols[10];
extern int capturedBits[10];
extern int32_t rollingPwnPattern;
extern int rollingPwnPatternType;

// BruteForce
extern BFMode bfMode;
extern BFState bfState;
extern int bfFreqIndex;
extern int bfProtocolIndex;
extern uint32_t bfCode;
extern unsigned long bfStartTime;
extern int bfCodesSent;

// WiFi Pentest
extern WebServer servidorWeb;
extern DNSServer dnsServer;
extern RedeWiFi redesDescobertas[MAX_REDES];
extern int numRedesDescobertas;
extern int redeSelecionada;
extern bool ataqueEmAndamento;
extern bool senhaRecebida;
extern char senhaCapturada[TAMANHO_SENHA];
extern char ssidAlvo[TAMANHO_SSID];
extern int pacotesInterceptados;
extern bool capturandoHandshake;
extern bool dnsAtivo;
extern bool evilTwinAtivo;
extern bool handshakeAtivo;
extern bool mitmAtivo;
extern bool deauthFloodAtivo;
extern int deauthAlvoIndex;
extern unsigned long deauthLastSend;
extern int deauthPacketCount;
extern bool celularConectado;
extern char wifiPassword[65];

// Pairing
extern DispositivoPareado dispositivosPareados[PAIR_MAX_DEVICES];
extern int numDispositivosPareados;
extern PairingState pairingState;
extern char currentPairCode[PAIR_CODE_LENGTH + 1];
extern unsigned long pairCodeGeneratedAt;
extern unsigned long pairTimeoutAt;
extern uint8_t pairAesKey[PAIR_AES_KEY_SIZE];
extern bool deviceIsPaired;
extern char deviceFingerprint[33];

// Crypto
extern uint8_t sessionAesKey[PAIR_AES_KEY_SIZE];
extern uint8_t sessionSalt[16];

// Protocolos / Bancos de dados
extern const uint32_t SCAN_FREQUENCIES[];
extern const int NUM_FREQS;
extern const uint32_t BRUTE_FREQS[];
extern const int NUM_BRUTE_FREQS;
extern const RCSwitchProtocol protocols[];
extern const int NUM_PROTOCOLS;
extern const uint32_t COMMON_CODES[];
extern const int NUM_COMMON_CODES;
extern const SubGHzEntry subGHzDatabase[];
extern const int SUBGHZ_DB_COUNT;

// ==========================================
// DECLARACOES DE FUNCOES
// ==========================================

void setupHardware();
extern "C" int __wrap_ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3);
bool buttonPressed(uint8_t pin);
void checkBackInterruptFlag();
void loadAllSignals();
void saveSignalToSlot(int slot, const CC1101Signal& sig);
void clearSignalSlot(int slot);

void initMenuSystem();
void drawMenu();
void handleMenu();
void runCurrentFunction();
void drawFunctionHeader(const char* title);
void drawHeader();

void initSerialControl();
void processSerialCommands();
void initBluetoothControl();
void processBluetoothCommands();
void simulateButtonUp();
void simulateButtonDown();
void simulateButtonSelect();
void simulateButtonBack();

void initUnifiedComm();
void processSerialCommandsUnified();
void processBluetoothCommandsUnified();
String processUnifiedCommand(const String& cmd, uint8_t source);

void initHttpServer();
void processHttpServer();
void stopHttpServer();
String processApiCommand(const char* cmd, JsonDocument* params);
String base64Encode(const uint8_t* data, size_t len);

void initCryptoManager();
void deriveKeyFromPairCode(const char* code, const uint8_t* salt, uint8_t* outKey);
String encryptAesGcm(const String& plaintext, const uint8_t* key);
String decryptAesGcm(const String& encrypted, const uint8_t* key);
void generateSecureToken(char* out, size_t len);
void setSessionKey(const uint8_t* key, const uint8_t* salt);
bool isCryptoReady();

void initMqttSecure();
void mqttSecureLoop();
void mqttSecureReconnect();
bool mqttSecurePublish(const char* topic, const char* payload, bool retained);
void destroyMqttSecure();

// nRF24
void nrfScannerSetup();
void nrfScannerLoop();
void nrfJammerSetup();
void nrfJammerLoop();

// CC1101 Core
void resetCC1101State();
void startCapture(uint32_t frequency);
void stopCapture();
void captureStep();
void transmitRawSignal();
void transmitBit(const RCSwitchProtocol& p, bool bit);
void transmitSync(const RCSwitchProtocol& p);
void transmitCode(uint32_t code, int bits, const RCSwitchProtocol& p);
void jamChannel(uint32_t freqKHz);
void jamChannelContinuous(uint32_t freqKHz);
uint32_t autoDetectFrequency();
CCState getCCState();
void setCCState(CCState state);
bool isTXPlaying();
void setTXPlaying(bool val);
int getTXProgress();
int getCaptureIndex();
void setCaptureIndex(int val);
void incrementCaptureIndex();
uint32_t getCaptureStartTime();
uint32_t getDetectedFrequency();
void setDetectedFrequency(uint32_t freq);
DecodedSignal decodeCapturedSignal();
void analyzeProtocol();
void cc1101SetMaxPower();

// CC1101 Attacks
void rollJamAttackStep();
void rollingPwnAttackStep();
void cc1101BruteForceSetup();
void cc1101BruteForceLoop();

// CC1101 UI
void cc1101ScannerSetup();
void cc1101ScannerLoop();
void cc1101TransceiverSetup();
void cc1101TransceiverLoop();
void cc1101FreqSweepSetup();
void cc1101FreqSweepLoop();

// RMT (legacy compat)
void initRMTCapture();
void captureRawSignalRMT();
void stopRMTCapture();
bool isRMTCapturing();
void processRMTRawData();

// De Bruijn
void generateDeBruijnSequence(uint8_t k, int n, uint32_t* output, int maxLen);
int getDeBruijnLength(int k, int n);

// Flipper .sub
bool saveSignalToSubFile(const char* filename, const CC1101Signal& sig);
bool loadSignalFromSubFile(const char* filename, CC1101Signal& sig);
bool exportSignalToSubFormat(const CC1101Signal& sig, String& output);

// BLE / WiFi
void bleScanSetup();
void bleScanLoop();
void sourAppleSetup();
void sourAppleLoop();
void wifiConnectSetup();
void wifiConnectLoop();
void wifiDeauthSetup();
void wifiDeauthLoop();
void wifiDeauthAttackSetup();
void wifiDeauthAttackLoop();

// WiFi Pentest
void wifiPentestInit();
void processarServidorWeb();
void escanearRedesWiFi();
void iniciarAtaqueEvilTwin();
void iniciarAtaqueWPS();
void iniciarCapturaHandshakeAlvo();
void iniciarAtaqueMITM();
void pararAtaqueAtual();
void verificarEstadoAtaques();
void aoReceberSenha(const char* ssid, const char* senha);
void handleNotFound();
bool ataqueDeauth(const uint8_t* bssid, int pacotes);
void deauthFloodHighPower();
void setWiFiMaxPower();
void setWiFiMaxDataRate();
void wifiSafeShutdown();
void wifiSafeSTA();
void wifiSafeAP(const char* ssid, const char* password);
void pararCaptura();

// Pairing
void pairingInit();
void pairingSetup();
void pairingLoop();
void generatePairCode();
bool validatePairCode(const char* code);
bool pairDevice(const char* deviceId, const char* token);
bool unpairDevice(const char* deviceId);
bool isDeviceAuthorized(const char* token);
void encryptPayload(uint8_t* input, uint8_t* output, size_t len, uint8_t* iv);
void decryptPayload(uint8_t* input, uint8_t* output, size_t len, uint8_t* iv);
void generateDeviceFingerprint(char* out, size_t len);
void mqttInit();
void mqttReconnect();
void mqttLoop();
void mqttPublishStatus();
void mqttHandleCommand(const char* topic, uint8_t* payload, unsigned int length);
void setupWiFiAPForPairing();
void stopWiFiAPForPairing();

// UI / Misc
void settingsSetup();
void settingsLoop();
void signalManagerSetup();
void signalManagerLoop();
void infoSetup();
void infoLoop();
void subGHzDBSetup();
void subGHzDBLoop();

#include "icons.h"

#endif

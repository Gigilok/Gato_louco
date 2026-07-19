#include "config.h"

namespace {
  bool btInitialized = false;
}

void initBluetoothControl() {
  if (SerialBT.begin("CrazyCat")) {
    btInitialized = true;
    Serial.println("[BT] Bluetooth iniciado: 'CrazyCat'");
    Serial.println("[BT] UUID SPP: 00001101-0000-1000-8000-00805F9B34FB");
  } else {
    Serial.println("[BT] Falha ao iniciar Bluetooth");
  }
}

// ==========================================
// PROCESSAMENTO BLUETOOTH - LEGADO
// Mantido para compatibilidade, mas delega para unified_comm
// ==========================================
void processBluetoothCommands() {
  // Delega para o sistema unificado
  processBluetoothCommandsUnified();
}

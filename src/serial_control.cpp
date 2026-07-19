#include "config.h"

void initSerialControl() {
  Serial.println("[SERIAL] Serial control initialized (unified mode)");
}

// ==========================================
// PROCESSAMENTO SERIAL - LEGADO
// Mantido para compatibilidade, mas delega para unified_comm
// ==========================================
void processSerialCommands() {
  // Delega para o sistema unificado
  processSerialCommandsUnified();
}

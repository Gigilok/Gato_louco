// ============================================
// DEAUTH BYPASS — stub (ESP-IDF 5.1)
// ============================================
// O wrap do linker foi removido do platformio.ini porque causa
// boot loop no ESP-IDF 5.1 (assinatura da funcao mudou).
// Frame 0xC0 eh bloqueado em firmware. Usamos 0xA0 (Disassociation)
// em wifi_pentest.cpp como alternativa.
// ============================================

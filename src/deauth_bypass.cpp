// ============================================
// DEAUTH BYPASS — ieee80211_raw_frame_sanity_check wrapper
// CORREÇÃO CRÍTICA: assinatura com 4 parâmetros (ESP-IDF 5.x / Arduino-ESP32 v2.0.17+)
// ============================================
#include <stdint.h>
#include <stdbool.h>

// NÃO declare __real_ explicitamente — o linker cria automaticamente.
// Apenas declare o wrapper com extern "C" para evitar name mangling.

extern "C" int __wrap_ieee80211_raw_frame_sanity_check(int ifx, const void *buffer, int len, bool auto_seq) {
  // Retorna 0 = frame é válido, pode ser transmitido
  (void)ifx;
  (void)buffer;
  (void)len;
  (void)auto_seq;
  return 0;
}

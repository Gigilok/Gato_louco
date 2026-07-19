// ============================================
// DEAUTH BYPASS — ieee80211_raw_frame_sanity_check wrapper
// Assinatura ESP-IDF 5.x: (int ifx, const void *buffer, int len, bool auto_seq)
// ============================================

#include <stdint.h>

extern "C" int __wrap_ieee80211_raw_frame_sanity_check(int ifx, const void *buffer, int len, bool auto_seq) {
  // Retorna 0 = frame valido
  return 0;
}

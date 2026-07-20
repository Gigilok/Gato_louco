// ============================================
// DEAUTH BYPASS — ieee80211_raw_frame_sanity_check wrapper
// ESP-IDF 4.4 (espressif32@5.4.0) — assinatura: 3 parametros
// ============================================
// O ESP-IDF bloqueia propositalmente o envio de frames de
// deautenticacao (0xC0) via esp_wifi_80211_tx().
// Este wrapper intercepta a validacao e permite o envio.
//
// Requer no platformio.ini:
//   build_flags = -Wl,--wrap=ieee80211_raw_frame_sanity_check
//
// ⚠️ AVISO: Use apenas em redes que voce possui ou tem autorizacao.
// ============================================

#include <stdint.h>

// ESP-IDF 4.4: assinatura com 3 parametros
extern "C" int __wrap_ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  // Retorna 0 = frame e valido, pode ser transmitido
  return 0;
}

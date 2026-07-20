// ============================================
// DEAUTH BYPASS — ieee80211_raw_frame_sanity_check wrapper
// ============================================
// O ESP-IDF bloqueia propositalmente o envio de frames de deautenticação
// (0xC0) via esp_wifi_80211_tx() por questões legais/regulatórias.
// Este wrapper intercepta a validação e permite o envio de deauth frames.
//
// Requer no platformio.ini:
//   build_flags = -Wl,--wrap=ieee80211_raw_frame_sanity_check
//
// ⚠️ AVISO: Use apenas em redes que você possui ou tem autorização para testar.
// ============================================

#include <stdint.h>
#include <stdbool.h>

// Função original (será renomeada pelo linker)
// CORREÇÃO CRÍTICA: assinatura com 4 parâmetros (ESP-IDF 5.x / Arduino-ESP32 v2.0.17+)
extern "C" int __real_ieee80211_raw_frame_sanity_check(int ifx, const void *buffer, int len, bool auto_seq);

// Wrapper: sempre retorna 0 (OK), permitindo qualquer frame type
// CORREÇÃO CRÍTICA: assinatura deve bater EXATAMENTE com a original
extern "C" int __wrap_ieee80211_raw_frame_sanity_check(int ifx, const void *buffer, int len, bool auto_seq) {
  // Retorna 0 = frame é válido, pode ser transmitido
  return 0;
}

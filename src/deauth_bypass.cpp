// ============================================
// DEAUTH BYPASS — ieee80211_raw_frame_sanity_check wrapper
// ============================================
// O ESP-IDF 5.x bloqueia propositalmente o envio de frames de
// deautenticação (0xC0) via esp_wifi_80211_tx().
// Este wrapper intercepta a validação e permite o envio.
//
// Requer no platformio.ini:
//   build_flags = -Wl,--wrap=ieee80211_raw_frame_sanity_check
//
// ⚠️ AVISO: Use apenas em redes que você possui ou tem autorização.
// ============================================

#include <stdint.h>
#include <Arduino.h>

// CORREÇÃO CRÍTICA v3.3-POWER:
// ESP-IDF 5.x alterou a assinatura de 3 para 4 parâmetros:
//   int ieee80211_raw_frame_sanity_check(int ifx, const void *buffer, int len, bool auto_seq)
// O wrapper DEVE ter a mesma assinatura, senão o linker não intercepta
// a chamada e o frame 0xC0 continua sendo rejeitado.

extern "C" int __wrap_ieee80211_raw_frame_sanity_check(int ifx, const void *buffer, int len, bool auto_seq) {
  // Retorna 0 = frame é válido, pode ser transmitido
  // Loga apenas a cada ~500ms para não floodar o Serial
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 500) {
    lastLog = millis();
    Serial.println("[BYPASS] sanity_check bypassed (ESP-IDF 5.x)");
  }
  return 0;
}

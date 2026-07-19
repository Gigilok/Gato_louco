#include "config.h"
#include <mbedtls/gcm.h>
#include <mbedtls/base64.h>

// ==========================================
// GERENCIADOR DE CRIPTOGRAFIA AES-256-GCM
// Compatível com CryptoManager.kt do APK
// ==========================================

namespace {
  bool cryptoInitialized = false;
}

// Variáveis globais de sessão (definidas aqui, declaradas como extern em config.h)
uint8_t sessionAesKey[PAIR_AES_KEY_SIZE] = {0};
uint8_t sessionSalt[16] = {0};

// ==========================================
// DERIVAÇÃO DE CHAVE PBKDF2-LIKE
// Compatível com deriveKeyFromPairCode() do APK
// ==========================================
void deriveKeyFromPairCode(const char* code, const uint8_t* salt, uint8_t* outKey) {
  if (!code || strlen(code) != PAIR_CODE_LENGTH) return;

  // Material: code(8 bytes) + salt(16 bytes) = 24 bytes
  uint8_t material[PAIR_CODE_LENGTH + 16];
  memcpy(material, code, PAIR_CODE_LENGTH);
  memcpy(material + PAIR_CODE_LENGTH, salt, 16);

  // PBKDF2-like com SHA-256, 10.000 iterações
  uint8_t hash[32];
  mbedtls_sha256_context ctx;

  // Iteração 0
  mbedtls_sha256_init(&ctx);
  // CORREÇÃO: Usar funções com sufixo _ret (ESP-IDF 5.x compatível)
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, material, sizeof(material));
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  // Iterações 1-9999
  for (int i = 1; i < 10000; i++) {
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, hash, 32);
    mbedtls_sha256_update(&ctx, material, sizeof(material));
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
  }

  // Extrai 32 bytes para AES-256
  memcpy(outKey, hash, 32);
}

// ==========================================
// AES-256-GCM ENCRYPT
// Formato: Base64(iv(12) + ciphertext + tag(16))
// Compatível com CryptoManager.encrypt() do APK
// ==========================================
String encryptAesGcm(const String& plaintext, const uint8_t* key) {
  if (!key) return "";

  // Gera IV aleatório de 12 bytes
  uint8_t iv[12];
  esp_fill_random(iv, 12);

  size_t plainLen = plaintext.length();
  size_t cipherLen = plainLen;
  uint8_t* ciphertext = (uint8_t*)malloc(cipherLen);
  uint8_t tag[16];

  if (!ciphertext) return "";

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (ret != 0) {
    free(ciphertext);
    mbedtls_gcm_free(&gcm);
    return "";
  }

  ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                   cipherLen,
                                   iv, 12,
                                   nullptr, 0,  // sem AAD
                                   (const uint8_t*)plaintext.c_str(),
                                   ciphertext,
                                   16, tag);
  mbedtls_gcm_free(&gcm);

  if (ret != 0) {
    free(ciphertext);
    return "";
  }

  // Monta: iv + ciphertext + tag
  size_t totalLen = 12 + cipherLen + 16;
  uint8_t* combined = (uint8_t*)malloc(totalLen);
  if (!combined) {
    free(ciphertext);
    return "";
  }

  memcpy(combined, iv, 12);
  memcpy(combined + 12, ciphertext, cipherLen);
  memcpy(combined + 12 + cipherLen, tag, 16);

  free(ciphertext);

  // Base64 encode
  size_t b64Len;
  mbedtls_base64_encode(nullptr, 0, &b64Len, combined, totalLen);
  uint8_t* b64 = (uint8_t*)malloc(b64Len);
  if (!b64) {
    free(combined);
    return "";
  }

  mbedtls_base64_encode(b64, b64Len, &b64Len, combined, totalLen);
  String result((char*)b64);
  free(b64);
  free(combined);

  return result;
}

// ==========================================
// AES-256-GCM DECRYPT
// Compatível com CryptoManager.decrypt() do APK
// ==========================================
String decryptAesGcm(const String& encrypted, const uint8_t* key) {
  if (!key || encrypted.isEmpty()) return "";

  // Base64 decode
  size_t combinedLen;
  mbedtls_base64_decode(nullptr, 0, &combinedLen, 
                        (const uint8_t*)encrypted.c_str(), encrypted.length());
  uint8_t* combined = (uint8_t*)malloc(combinedLen);
  if (!combined) return "";

  mbedtls_base64_decode(combined, combinedLen, &combinedLen,
                        (const uint8_t*)encrypted.c_str(), encrypted.length());

  if (combinedLen < 12 + 16) {  // IV + tag mínimo
    free(combined);
    return "";
  }

  // Extrai componentes
  uint8_t iv[12];
  memcpy(iv, combined, 12);

  size_t cipherLen = combinedLen - 12 - 16;
  uint8_t* ciphertext = combined + 12;
  uint8_t* tag = combined + 12 + cipherLen;

  uint8_t* plaintext = (uint8_t*)malloc(cipherLen + 1);
  if (!plaintext) {
    free(combined);
    return "";
  }

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (ret != 0) {
    free(plaintext);
    free(combined);
    mbedtls_gcm_free(&gcm);
    return "";
  }

  ret = mbedtls_gcm_auth_decrypt(&gcm, cipherLen,
                                  iv, 12,
                                  nullptr, 0,
                                  tag, 16,
                                  ciphertext, plaintext);
  mbedtls_gcm_free(&gcm);
  free(combined);

  if (ret != 0) {
    free(plaintext);
    return "";
  }

  plaintext[cipherLen] = '\0';
  String result((char*)plaintext);
  free(plaintext);

  return result;
}

// ==========================================
// GERAÇÃO DE TOKEN SEGURO
// Compatível com generateToken() do APK (32 bytes = 64 hex)
// ==========================================
void generateSecureToken(char* out, size_t len) {
  uint8_t bytes[32];
  esp_fill_random(bytes, 32);
  for (int i = 0; i < 32 && (i * 2) < (int)len - 1; i++) {
    snprintf(out + (i * 2), 3, "%02X", bytes[i]);
  }
  out[min((size_t)64, len - 1)] = '\0';
}

// ==========================================
// INICIALIZAÇÃO
// ==========================================
void initCryptoManager() {
  if (cryptoInitialized) return;
  memset(sessionAesKey, 0, 32);
  memset(sessionSalt, 0, 16);
  cryptoInitialized = true;
  Serial.println("[CRYPTO] AES-256-GCM manager initialized");
}

void setSessionKey(const uint8_t* key, const uint8_t* salt) {
  if (key) memcpy(sessionAesKey, key, 32);
  if (salt) memcpy(sessionSalt, salt, 16);
}

bool isCryptoReady() {
  return cryptoInitialized;
}

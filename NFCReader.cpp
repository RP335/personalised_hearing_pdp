/*
 * ============================================================================
 * NFCReader.cpp — PN532 NFC Audiogram Card Reader Implementation
 * ============================================================================
 */

#include "NFCReader.h"
#include "AudioGraph.h"
#include <Adafruit_PN532.h>

// ============================================================================
// HARDWARE INSTANCE (on Wire2)
// ============================================================================
static Adafruit_PN532 nfc(NFC_IRQ_PIN, NFC_RESET_PIN, &I2C_BUS);

// ============================================================================
// STATE
// ============================================================================
bool    nfcPresent      = false;
int8_t  leftThresholds[N_AUDIO_FREQS]  = {0};
int8_t  rightThresholds[N_AUDIO_FREQS] = {0};
char    currentUserUID[16] = {0};
bool    audiogramLoaded = false;

static uint8_t rawAudiogramData[NFC_DATA_BYTES];

// ============================================================================
// INIT
// ============================================================================

void nfcInit() {
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();

  if (!versiondata) {
    myTympan.println("[NFC] *** PN532 NOT FOUND *** (check wiring)");
    nfcPresent = false;
    return;
  }

  nfcPresent = true;
  myTympan.print("[NFC] PN532 FW v");
  myTympan.print((versiondata >> 24) & 0xFF);
  myTympan.print('.');
  myTympan.println((versiondata >> 16) & 0xFF);
  nfc.SAMConfig();
}

// ============================================================================
// UID → HEX STRING
// ============================================================================
static void uidToHex(uint8_t *uid, uint8_t len) {
  memset(currentUserUID, 0, sizeof(currentUserUID));
  for (uint8_t i = 0; i < len && i < 7; i++) {
    char tmp[3];
    snprintf(tmp, sizeof(tmp), "%02X", uid[i]);
    strcat(currentUserUID, tmp);
  }
}

// ============================================================================
// READ CARD — FULL AUDIOGRAM
// ============================================================================

bool nfcPollForCard() {
  if (!nfcPresent) return false;

  uint8_t uid[7];
  uint8_t uidLen;

  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen,
                                NFC_READ_TIMEOUT_MS))
    return false;

  myTympan.println("\n*** NFC Card Detected ***");
  uidToHex(uid, uidLen);
  myTympan.print("[NFC] UID: ");
  myTympan.println(currentUserUID);

  // Read pages 4–9 (24 bytes, we use first 22)
  uint8_t tempBuffer[24];
  memset(tempBuffer, 0, sizeof(tempBuffer));

  for (uint8_t page = NFC_START_PAGE; page <= NFC_END_PAGE; page++) {
    uint8_t pageBuf[4];
    memset(pageBuf, 0, sizeof(pageBuf));

    if (!nfc.mifareultralight_ReadPage(page, pageBuf)) {
      myTympan.print("[NFC] Read error on page ");
      myTympan.println(page);
      return false;
    }

    int offset = (page - NFC_START_PAGE) * 4;
    memcpy(tempBuffer + offset, pageBuf, 4);
    delay(10);  // I2C safety delay
  }

  // Transfer first 22 bytes
  memcpy(rawAudiogramData, tempBuffer, NFC_DATA_BYTES);

  // Debug: print raw hex
  myTympan.print("[NFC] Raw (");
  myTympan.print(NFC_DATA_BYTES);
  myTympan.print(" bytes): ");
  for (int i = 0; i < NFC_DATA_BYTES; i++) {
    if (rawAudiogramData[i] < 0x10) myTympan.print("0");
    myTympan.print(rawAudiogramData[i], HEX);
    myTympan.print(" ");
  }
  myTympan.println();

  // Parse: bytes 0–10 = left, bytes 11–21 = right
  for (int i = 0; i < N_AUDIO_FREQS; i++) {
    leftThresholds[i]  = (int8_t)rawAudiogramData[i];
    rightThresholds[i] = (int8_t)rawAudiogramData[N_AUDIO_FREQS + i];
  }

  audiogramLoaded = true;
  nfcPrintAudiogram();
  return true;
}

// ============================================================================
// TOGGLE TAP (card re-tap when already loaded)
// ============================================================================

bool nfcPollForToggle() {
  if (!nfcPresent) return false;

  uint8_t uid[7];
  uint8_t uidLen;
  return nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 30);
}

// ============================================================================
// PRINT AUDIOGRAM
// ============================================================================

void nfcPrintAudiogram() {
  myTympan.println("[NFC] Audiogram (dB HL):");
  myTympan.print("  Freq:  ");
  for (int i = 0; i < N_AUDIO_FREQS; i++) {
    myTympan.print(AUDIOGRAM_FREQS[i]);
    myTympan.print("\t");
  }
  myTympan.println();

  myTympan.print("  Left:  ");
  for (int i = 0; i < N_AUDIO_FREQS; i++) {
    myTympan.print(leftThresholds[i]);
    myTympan.print("\t");
  }
  myTympan.println();

  myTympan.print("  Right: ");
  for (int i = 0; i < N_AUDIO_FREQS; i++) {
    myTympan.print(rightThresholds[i]);
    myTympan.print("\t");
  }
  myTympan.println();
}

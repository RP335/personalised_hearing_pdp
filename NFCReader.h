/*
 * ============================================================================
 * NFCReader.h — PN532 NFC Audiogram Card Reader
 * ============================================================================
 * Reads personalized audiogram data from NTAG213 NFC cards.
 * Card format: 22 bytes starting at page 4
 *   Bytes 0–10:  left ear thresholds (int8, dB HL)
 *   Bytes 11–21: right ear thresholds (int8, dB HL)
 *   Frequencies: 125 250 500 750 1000 1500 2000 3000 4000 6000 8000 Hz
 * ============================================================================
 */

#ifndef NFC_READER_H
#define NFC_READER_H

#include "Config.h"

// --- State ---
extern bool     nfcPresent;
extern int8_t   leftThresholds[N_AUDIO_FREQS];
extern int8_t   rightThresholds[N_AUDIO_FREQS];
extern char     currentUserUID[16];    // hex string of NFC card UID
extern bool     audiogramLoaded;

// --- Lifecycle ---
void nfcInit();                         // call from setup(), after Wire2.begin()

// --- Polling (call from loop()) ---
// Returns true if a new audiogram was successfully read.
bool nfcPollForCard();

// Returns true if a card was tapped (used for toggle when already loaded).
bool nfcPollForToggle();

// --- Utility ---
void nfcPrintAudiogram();

#endif // NFC_READER_H

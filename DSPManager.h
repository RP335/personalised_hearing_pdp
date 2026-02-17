/*
 * ============================================================================
 * DSPManager.h — DSP Algorithm Manager
 * ============================================================================
 * Central controller for the processing chain:
 *   - FIR filterbank initialisation
 *   - Per-band WDRC configuration (bypass / NAL-R / custom)
 *   - Broadband limiter
 *   - Algorithm toggle (enable/disable individual stages)
 *   - Pre-gain control
 *
 * All DSP state is encapsulated here so the main firmware only calls
 * high-level functions like dspApplyProfile() or dspToggleWDRC().
 * ============================================================================
 */

#ifndef DSP_MANAGER_H
#define DSP_MANAGER_H

#include "Config.h"
#include "NALR.h"

// ============================================================================
// STATE (read by display, BLE, serial)
// ============================================================================

extern DSPFlags  dspFlags;               // current enable/disable state
extern float     currentNALRGains[N_CHAN]; // per-band gains (dB) for display
extern float     currentPreGainDB;        // pre-gain (dB)
extern bool      dspProfileLoaded;        // true after NFC card applied

// ============================================================================
// INITIALISATION
// ============================================================================

// Call once in setup() after audioGraphInit().
// Sets up FIR filterbank, configures all compressors to bypass/flat.
void dspInit();

// ============================================================================
// PROFILE APPLICATION
// ============================================================================

// Apply a full audiogram profile (from NFC card) to the DSP chain.
// Computes NAL-R gains and pushes them into per-band WDRC compressors.
void dspApplyProfile(const int8_t thresholds[N_AUDIO_FREQS]);

// Reset to flat bypass (no personalisation).
void dspSetBypass();

// ============================================================================
// ALGORITHM TOGGLES
// ============================================================================

// Toggle individual DSP stages on/off at runtime.
// When a stage is disabled, it passes audio through unchanged.

void dspToggleNALR();       // flip NAL-R gains on/off
void dspToggleWDRC();       // flip per-band compression on/off
void dspToggleLimiter();    // flip broadband limiter on/off

void dspSetNALR(bool on);
void dspSetWDRC(bool on);
void dspSetLimiter(bool on);

// ============================================================================
// PRE-GAIN CONTROL
// ============================================================================

void dspSetPreGain(float dB);
void dspAdjustPreGain(float deltadB);  // e.g. +3 or -3

// ============================================================================
// WDRC PARAMETER TWEAKS (advanced)
// ============================================================================

// Override compression ratio for all bands
void dspSetCompressionRatio(float cr);

// Override per-band output limit (BOLT)
void dspSetBandBOLT(float bolt_dBSPL);

// Override broadband limiter ceiling
void dspSetBBCeiling(float bolt_dBSPL);

// ============================================================================
// STATUS / DEBUG
// ============================================================================

// Print full DSP state to serial
void dspPrintStatus();

// Get per-band current signal levels (for display / logging)
void dspGetBandLevels(float levelsOut[N_CHAN]);

// Get broadband output level
float dspGetBBLevel();

#endif // DSP_MANAGER_H

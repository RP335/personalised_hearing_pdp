/*
 * ============================================================================
 * DSPManager.h — DSP Algorithm Manager (v2.1)
 * ============================================================================
 * Central controller for the processing chain:
 *   - IIR biquad filterbank design (replaces FIR)
 *   - Per-band NAL-R gain (separate gain blocks)
 *   - Per-band WDRC compression
 *   - Expansion / noise gate
 *   - Broadband limiter
 *   - Algorithm toggle (enable/disable individual stages)
 *   - Pre-gain control
 * ============================================================================
 */

#ifndef DSP_MANAGER_H
#define DSP_MANAGER_H

#include "Config.h"
#include "NALR.h"

// ============================================================================
// STATE (read by display, BLE, serial)
// ============================================================================

extern DSPFlags dspFlags;
extern float currentNALRGains_L[N_CHAN];
extern float currentNALRGains_R[N_CHAN];
extern float currentPreGainDB;
extern bool  dspProfileLoaded;

// ============================================================================
// INITIALISATION
// ============================================================================

void dspInit();

// ============================================================================
// PROFILE APPLICATION
// ============================================================================

void dspApplyProfile(const int8_t leftT[N_AUDIO_FREQS],
                     const int8_t rightT[N_AUDIO_FREQS]);
void dspSetBypass();

// ============================================================================
// ALGORITHM TOGGLES
// ============================================================================

void dspToggleNALR();
void dspToggleWDRC();
void dspToggleLimiter();
void dspToggleExpansion();   // NEW: noise gate toggle

void dspSetNALR(bool on);
void dspSetWDRC(bool on);
void dspSetLimiter(bool on);
void dspSetExpansion(bool on);  // NEW

// ============================================================================
// PRE-GAIN CONTROL
// ============================================================================

void dspSetPreGain(float dB);
void dspAdjustPreGain(float deltadB);

// ============================================================================
// WDRC PARAMETER TWEAKS (advanced)
// ============================================================================

void dspSetCompressionRatio(float cr);
void dspSetBandBOLT(float bolt_dBSPL);
void dspSetBBCeiling(float bolt_dBSPL);

// ============================================================================
// STATUS / DEBUG
// ============================================================================

void dspPrintStatus();
void dspGetBandLevels(float levelsOutL[N_CHAN], float levelsOutR[N_CHAN]);
float dspGetBBLevel();

#endif // DSP_MANAGER_H
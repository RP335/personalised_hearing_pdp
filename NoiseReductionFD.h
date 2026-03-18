/*
 * ============================================================================
 * NoiseReductionFD.h — Frequency-Domain Noise Reduction Module
 * ============================================================================
 * Wraps the Tympan AudioEffectNoiseReduction_FD_F32 for integration into
 * the personalized hearing protection signal chain.
 *
 * The algorithm estimates a stationary noise floor (long-term average of each
 * FFT bin) and attenuates bins whose level is close to that floor. Bins that
 * rise above the floor are treated as "signal" and passed through.
 *
 * ENABLE/DISABLE AT COMPILE TIME:
 *   In Config.h, define or comment out:
 *     #define ENABLE_NOISE_REDUCTION
 *
 * SIGNAL CHAIN INSERTION POINT:
 *   preGain_L/R  →  [noiseReduction_L/R]  →  filterbank_L/R
 *   When compiled out, preGain connects directly to filterbank (original path).
 * ============================================================================
 */

#ifndef NOISE_REDUCTION_FD_H
#define NOISE_REDUCTION_FD_H

#include "Config.h"

#ifdef ENABLE_NOISE_REDUCTION

#include <Tympan_Library.h>

// ============================================================================
// NR DEFAULTS (tweak these or override at runtime via serial)
// ============================================================================

// FFT overlap factor: 4 = 75% overlap (smoother), 2 = 50% overlap (cheaper)
#ifndef NR_FFT_OVERLAP
  #define NR_FFT_OVERLAP       4
#endif

// Noise estimation time constants
#ifndef NR_ATTACK_SEC
  #define NR_ATTACK_SEC        10.0f   // how fast noise estimate rises (slow = conservative)
#endif
#ifndef NR_RELEASE_SEC
  #define NR_RELEASE_SEC       3.0f    // how fast noise estimate falls
#endif

// Attenuation behaviour
#ifndef NR_MAX_ATTEN_DB
  #define NR_MAX_ATTEN_DB      15.0f   // max dB of noise suppression per bin
#endif
#ifndef NR_SNR_AT_MAX_ATTEN_DB
  #define NR_SNR_AT_MAX_ATTEN_DB  3.0f // SNR (dB) at which max attenuation applies
#endif
#ifndef NR_TRANSITION_WIDTH_DB
  #define NR_TRANSITION_WIDTH_DB  6.0f // width of transition from full atten to unity
#endif

// Gain smoothing (reduces "musical noise" / water-bubbling artefacts)
#ifndef NR_GAIN_SMOOTH_SEC
  #define NR_GAIN_SMOOTH_SEC   0.02f   // temporal smoothing of gain curve
#endif

// ============================================================================
// NR STATE
// ============================================================================

struct NRState {
  bool  enabled          = false;      // runtime enable/disable (bypass when false)
  bool  noiseEstUpdate   = true;       // freeze/unfreeze noise estimate
  float attackSec        = NR_ATTACK_SEC;
  float releaseSec       = NR_RELEASE_SEC;
  float maxAttenDB       = NR_MAX_ATTEN_DB;
  float snrAtMaxAttenDB  = NR_SNR_AT_MAX_ATTEN_DB;
  float transitionDB     = NR_TRANSITION_WIDTH_DB;
  float gainSmoothSec    = NR_GAIN_SMOOTH_SEC;
};

extern NRState nrState;

// ============================================================================
// PUBLIC API
// ============================================================================

/// Call once in setup(), after audioGraphInit() and dspInit()
void nrInit();

/// Toggle NR on/off
void nrToggle();
void nrSetEnabled(bool on);

/// Toggle noise-estimate updates (freeze the current noise profile)
void nrToggleNoiseEstUpdate();
void nrSetNoiseEstUpdate(bool on);

/// Parameter setters (also update nrState for status display)
void nrSetAttack(float sec);
void nrSetRelease(float sec);
void nrSetMaxAtten(float dB);
void nrSetSNRThreshold(float dB);
void nrSetTransitionWidth(float dB);
void nrSetGainSmoothing(float sec);

/// Print full NR status to Serial
void nrPrintStatus();

#endif // ENABLE_NOISE_REDUCTION
#endif // NOISE_REDUCTION_FD_H

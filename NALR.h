/*
 * ============================================================================
 * NALR.h — NAL-R Prescription Algorithm
 * ============================================================================
 * National Acoustic Laboratories – Revised (NAL-R) prescription rule.
 * Converts audiometric thresholds (dB HL) to recommended insertion gains.
 *
 * Reference:
 *   Byrne, D., & Dillon, H. (1986). The National Acoustic Laboratories'
 *   (NAL) new procedure for selecting the gain and frequency response of
 *   a hearing aid. Ear and Hearing, 7(4), 257–265.
 *
 * Simplified formula used here:
 *   G(f) = 0.31 × H(f) + K(f)
 * where
 *   H(f) = hearing threshold at frequency f (dB HL)
 *   K(f) = frequency-dependent correction (accounts for speech spectrum)
 * ============================================================================
 */

#ifndef NALR_H
#define NALR_H

#include "Config.h"

// --- Core computation ---

// Frequency-dependent correction factor K(f) for NAL-R
float nalrCorrectionFactor(float freqHz);

// Linearly interpolate audiogram threshold at an arbitrary frequency
// from the 11-point audiometric data stored on the NFC card.
float interpolateThreshold(const int8_t thresholds[N_AUDIO_FREQS], float freqHz);

// Compute NAL-R insertion gain for a single frequency, given the threshold.
// Result is clamped to [NALR_GAIN_FLOOR, NALR_GAIN_CEILING].
float calculateNALRGain(float thresholdHL, float freqHz);

// --- Batch computation ---

// Compute NAL-R gains for all N_CHAN bands, using interpolated thresholds.
// Results are written into gainsOut[N_CHAN].
void computeNALRGains(const int8_t thresholds[N_AUDIO_FREQS],
                       float gainsOut[N_CHAN]);

// Compute the 3-frequency average (3FA) of thresholds at 500, 1000, 2000 Hz.
// Used for NAL-R overall level correction.
float compute3FA(const int8_t thresholds[N_AUDIO_FREQS]);

#endif // NALR_H
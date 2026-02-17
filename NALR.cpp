/*
 * ============================================================================
 * NALR.cpp — NAL-R Prescription Algorithm Implementation
 * ============================================================================
 */

#include "NALR.h"
#include <Arduino.h>

// ============================================================================
// FREQUENCY CORRECTION TABLE
// ============================================================================
// NAL-R correction K(f) values, approximating the speech-spectrum weighting.
// The 1–2 kHz region gets the least correction (most gain relative to others)
// because that band carries the most speech intelligibility information.

float nalrCorrectionFactor(float freqHz) {
  // Piecewise lookup — matches published NAL-R tables
  if (freqHz <=  250.0f) return -17.0f;
  if (freqHz <=  500.0f) return  -8.0f;
  if (freqHz <=  750.0f) return  -3.0f;
  if (freqHz <= 1000.0f) return   0.0f;
  if (freqHz <= 1500.0f) return  -1.0f;
  if (freqHz <= 2000.0f) return  -1.0f;
  if (freqHz <= 3000.0f) return  -2.0f;
  if (freqHz <= 4000.0f) return  -2.0f;
  if (freqHz <= 6000.0f) return  -2.0f;
  return -2.0f;  // 6 kHz and above
}

// ============================================================================
// THRESHOLD INTERPOLATION
// ============================================================================
// Linear interpolation between the 11 audiometric points.
// Frequencies outside the [125, 8000] Hz range clamp to the nearest value.

float interpolateThreshold(const int8_t thresholds[N_AUDIO_FREQS], float freqHz) {
  // Clamp to audiogram range
  if (freqHz <= (float)AUDIOGRAM_FREQS[0])
    return (float)thresholds[0];
  if (freqHz >= (float)AUDIOGRAM_FREQS[N_AUDIO_FREQS - 1])
    return (float)thresholds[N_AUDIO_FREQS - 1];

  // Find bracketing pair and interpolate
  for (int i = 0; i < N_AUDIO_FREQS - 1; i++) {
    float fLow  = (float)AUDIOGRAM_FREQS[i];
    float fHigh = (float)AUDIOGRAM_FREQS[i + 1];
    if (freqHz >= fLow && freqHz <= fHigh) {
      float t = (freqHz - fLow) / (fHigh - fLow);
      return (float)thresholds[i] + t * ((float)thresholds[i + 1] - (float)thresholds[i]);
    }
  }
  return 0.0f;  // fallback (shouldn't reach here)
}

// ============================================================================
// SINGLE-FREQUENCY GAIN
// ============================================================================

float calculateNALRGain(float thresholdHL, float freqHz) {
  float gain = 0.31f * thresholdHL + nalrCorrectionFactor(freqHz);

  // Clamp
  if (gain < NALR_GAIN_FLOOR)   gain = NALR_GAIN_FLOOR;
  if (gain > NALR_GAIN_CEILING) gain = NALR_GAIN_CEILING;
  return gain;
}

// ============================================================================
// BATCH: COMPUTE ALL BAND GAINS
// ============================================================================

void computeNALRGains(const int8_t thresholds[N_AUDIO_FREQS],
                       float gainsOut[N_CHAN]) {
  for (int i = 0; i < N_CHAN; i++) {
    float freq      = BAND_CENTERS[i];
    float threshold  = interpolateThreshold(thresholds, freq);
    gainsOut[i]      = calculateNALRGain(threshold, freq);
  }
}

// ============================================================================
// 3-FREQUENCY AVERAGE
// ============================================================================
// PTA at 500, 1000, 2000 Hz — standard audiological metric.
// Indices into AUDIOGRAM_FREQS: 500=2, 1000=4, 2000=6

float compute3FA(const int8_t thresholds[N_AUDIO_FREQS]) {
  float sum = (float)thresholds[2]   // 500 Hz
            + (float)thresholds[4]   // 1000 Hz
            + (float)thresholds[6];  // 2000 Hz
  return sum / 3.0f;
}

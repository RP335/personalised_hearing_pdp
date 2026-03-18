/*
 * ============================================================================
 * NoiseReductionFD.cpp — Frequency-Domain Noise Reduction Implementation
 * ============================================================================
 */

#include "NoiseReductionFD.h"

#ifdef ENABLE_NOISE_REDUCTION

#include "AudioGraph.h"

// ============================================================================
// STATE
// ============================================================================

NRState nrState;

// ============================================================================
// INIT
// ============================================================================

void nrInit() {
  int N_FFT = APP_BLOCK_SAMPLES * NR_FFT_OVERLAP;

  // Setup both L and R NR blocks (allocates FFT buffers internally)
  noiseReduction_L.setup(audioSettings, N_FFT);
  noiseReduction_R.setup(audioSettings, N_FFT);

  // Apply default parameters
  nrSetAttack(nrState.attackSec);
  nrSetRelease(nrState.releaseSec);
  nrSetMaxAtten(nrState.maxAttenDB);
  nrSetSNRThreshold(nrState.snrAtMaxAttenDB);
  nrSetTransitionWidth(nrState.transitionDB);
  nrSetGainSmoothing(nrState.gainSmoothSec);
  nrSetNoiseEstUpdate(nrState.noiseEstUpdate);

  // Start disabled — user enables via serial or after profile load
  nrSetEnabled(false);

  myTympan.println("[NR] Initialised: N_FFT=" + String(N_FFT) +
                   ", overlap=" + String(NR_FFT_OVERLAP) + "x" +
                   ", maxAtten=" + String(nrState.maxAttenDB, 1) + " dB");
}

// ============================================================================
// ENABLE / DISABLE
// ============================================================================

void nrToggle() {
  nrSetEnabled(!nrState.enabled);
}

void nrSetEnabled(bool on) {
  nrState.enabled = on;
  noiseReduction_L.enable(on);
  noiseReduction_R.enable(on);
  myTympan.print("[NR] Noise reduction: ");
  myTympan.println(on ? "ON" : "OFF (passthrough)");
}

// ============================================================================
// NOISE ESTIMATE FREEZE/UNFREEZE
// ============================================================================

void nrToggleNoiseEstUpdate() {
  nrSetNoiseEstUpdate(!nrState.noiseEstUpdate);
}

void nrSetNoiseEstUpdate(bool on) {
  nrState.noiseEstUpdate = on;
  noiseReduction_L.setEnableNoiseEstimationUpdates(on);
  noiseReduction_R.setEnableNoiseEstimationUpdates(on);
  myTympan.print("[NR] Noise estimate updates: ");
  myTympan.println(on ? "ON (adapting)" : "FROZEN");
}

// ============================================================================
// PARAMETER SETTERS
// ============================================================================

void nrSetAttack(float sec) {
  if (sec < 0.1f) sec = 0.1f;
  if (sec > 60.0f) sec = 60.0f;
  nrState.attackSec = noiseReduction_L.setAttack_sec(sec);
  noiseReduction_R.setAttack_sec(sec);
  myTympan.print("[NR] Attack = ");
  myTympan.print(nrState.attackSec, 2);
  myTympan.println(" sec");
}

void nrSetRelease(float sec) {
  if (sec < 0.1f) sec = 0.1f;
  if (sec > 30.0f) sec = 30.0f;
  nrState.releaseSec = noiseReduction_L.setRelease_sec(sec);
  noiseReduction_R.setRelease_sec(sec);
  myTympan.print("[NR] Release = ");
  myTympan.print(nrState.releaseSec, 2);
  myTympan.println(" sec");
}

void nrSetMaxAtten(float dB) {
  if (dB < 0.0f) dB = 0.0f;
  if (dB > 60.0f) dB = 60.0f;
  nrState.maxAttenDB = noiseReduction_L.setMaxAttenuation_dB(dB);
  noiseReduction_R.setMaxAttenuation_dB(dB);
  // getMaxAttenuation_dB() returns the actual value after conversion
  nrState.maxAttenDB = noiseReduction_L.getMaxAttenuation_dB();
  myTympan.print("[NR] Max attenuation = ");
  myTympan.print(nrState.maxAttenDB, 1);
  myTympan.println(" dB");
}

void nrSetSNRThreshold(float dB) {
  nrState.snrAtMaxAttenDB = dB;
  noiseReduction_L.setSNRforMaxAttenuation_dB(dB);
  noiseReduction_R.setSNRforMaxAttenuation_dB(dB);
  myTympan.print("[NR] SNR at max atten = ");
  myTympan.print(dB, 1);
  myTympan.println(" dB");
}

void nrSetTransitionWidth(float dB) {
  if (dB < 1.0f) dB = 1.0f;
  nrState.transitionDB = dB;
  noiseReduction_L.setTransitionWidth_dB(dB);
  noiseReduction_R.setTransitionWidth_dB(dB);
  myTympan.print("[NR] Transition width = ");
  myTympan.print(dB, 1);
  myTympan.println(" dB");
}

void nrSetGainSmoothing(float sec) {
  if (sec < 0.001f) sec = 0.001f;
  nrState.gainSmoothSec = sec;
  noiseReduction_L.setGainSmoothing_sec(sec);
  noiseReduction_R.setGainSmoothing_sec(sec);
  myTympan.print("[NR] Gain smoothing = ");
  myTympan.print(sec, 3);
  myTympan.println(" sec");
}

// ============================================================================
// STATUS
// ============================================================================

void nrPrintStatus() {
  myTympan.println("\n=== NOISE REDUCTION STATUS ===");
  myTympan.print("  Enabled:        ");
  myTympan.println(nrState.enabled ? "ON" : "OFF");
  myTympan.print("  Noise est:      ");
  myTympan.println(nrState.noiseEstUpdate ? "ADAPTING" : "FROZEN");
  myTympan.print("  Attack:         ");
  myTympan.print(nrState.attackSec, 2);
  myTympan.println(" sec");
  myTympan.print("  Release:        ");
  myTympan.print(nrState.releaseSec, 2);
  myTympan.println(" sec");
  myTympan.print("  Max atten:      ");
  myTympan.print(nrState.maxAttenDB, 1);
  myTympan.println(" dB");
  myTympan.print("  SNR threshold:  ");
  myTympan.print(nrState.snrAtMaxAttenDB, 1);
  myTympan.println(" dB");
  myTympan.print("  Transition:     ");
  myTympan.print(nrState.transitionDB, 1);
  myTympan.println(" dB");
  myTympan.print("  Gain smooth:    ");
  myTympan.print(nrState.gainSmoothSec, 3);
  myTympan.println(" sec");
  myTympan.print("  FFT size:       ");
  myTympan.println(APP_BLOCK_SAMPLES * NR_FFT_OVERLAP);
  myTympan.println("==============================\n");
}

#endif // ENABLE_NOISE_REDUCTION

/*
 * ============================================================================
 * AudioGraph.h — Audio Object & Patch Cord Declarations (extern)
 * ============================================================================
 *
 * SIGNAL FLOW (v2.1 — IIR Biquad Filterbank):
 *
 *                           ┌─► Gain[0] ─► WDRC[0] ─┐
 *   LineIn ─► PreGain ─► BiquadFB ─► Gain[1] ─► WDRC[1] ─┤
 *                           ├─► Gain[2] ─► WDRC[2] ─┤
 *                           ├─► Gain[3] ─► WDRC[3] ─├─► Mixer ─► BB Limiter ─► Out
 *                           ├─► Gain[4] ─► WDRC[4] ─┤
 *                           ├─► Gain[5] ─► WDRC[5] ─┤
 *                           ├─► Gain[6] ─► WDRC[6] ─┤
 *                           └─► Gain[7] ─► WDRC[7] ─┘
 *
 *   LineIn ─► A-Weight ─► CalcLevel  (metrics branch, parallel)
 *
 * KEY CHANGE: AudioFilterbankBiquad_F32 replaces AudioFilterFIR_F32 arrays.
 *   - IIR biquads handle low-frequency crossovers cleanly
 *   - Separate gain blocks allow NAL-R gains to be applied independently of WDRC
 *   - Easier debugging: can bypass gain or WDRC separately
 * ============================================================================
 */

#ifndef AUDIO_GRAPH_H
#define AUDIO_GRAPH_H

#include "Config.h"
#include <Tympan_Library.h>

// --- Global audio settings ---
extern AudioSettings_F32 audioSettings;

// --- Hardware ---
extern Tympan myTympan;

// --- I/O ---
extern AudioInputI2S_F32  i2s_in;
extern AudioOutputI2S_F32 i2s_out;

// --- DSP chain (Left) ---
extern AudioEffectGain_F32        preGain_L;
extern AudioFilterbankBiquad_F32  filterbank_L;        // REPLACES firFilt_L[]
extern AudioEffectGain_F32        bandGain_L[N_CHAN];   // NEW: separate NAL-R gain stage
extern AudioEffectCompWDRC_F32    compPerBand_L[N_CHAN];
extern AudioMixer16_F32           mixer_L;              // Mixer16 (supports >8 inputs)
extern AudioEffectCompWDRC_F32    compBroadband_L;

// --- DSP chain (Right) ---
extern AudioEffectGain_F32        preGain_R;
extern AudioFilterbankBiquad_F32  filterbank_R;
extern AudioEffectGain_F32        bandGain_R[N_CHAN];
extern AudioEffectCompWDRC_F32    compPerBand_R[N_CHAN];
extern AudioMixer16_F32           mixer_R;
extern AudioEffectCompWDRC_F32    compBroadband_R;

// --- Metrics branch ---
extern AudioFilterFreqWeighting_F32 freqWeightA;
extern AudioCalcLevel_F32           calcLevelFast;
extern AudioCalcLevel_F32           calcLevelSlow;

// --- SD Writer ---
extern AudioSDWriter_F32 sdWriter;

// --- Initialisation ---
void audioGraphInit();

#endif // AUDIO_GRAPH_H


#ifdef ENABLE_NOISE_REDUCTION
  #include <AudioEffectNoiseReduction_FD_F32.h>  // from Tympan_Library
  extern AudioEffectNoiseReduction_FD_F32 noiseReduction_L;
  extern AudioEffectNoiseReduction_FD_F32 noiseReduction_R;
#endif
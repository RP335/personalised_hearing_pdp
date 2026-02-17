/*
 * ============================================================================
 * AudioGraph.cpp — Audio Object Definitions & Patch Cords
 * ============================================================================
 */

#include "AudioGraph.h"
#include "Config.h"

// ============================================================================
// AUDIO SETTINGS
// ============================================================================
AudioSettings_F32 audioSettings(SAMPLE_RATE_HZ, APP_BLOCK_SAMPLES);

// ============================================================================
// HARDWARE
// ============================================================================
Tympan myTympan(TYMPAN_REV, audioSettings);

// ============================================================================
// I/O
// ============================================================================
AudioInputI2S_F32 i2s_in(audioSettings);
AudioOutputI2S_F32 i2s_out(audioSettings);

// ============================================================================
// DSP PROCESSING CHAIN
// ============================================================================
AudioEffectGain_F32 preGain;
AudioFilterFIR_F32 firFilt[N_CHAN];
AudioEffectCompWDRC_F32 compPerBand[N_CHAN];
AudioMixer8_F32 mixer1;
AudioEffectCompWDRC_F32 compBroadband;

// ============================================================================
// METRICS BRANCH (parallel path for sound level measurement)
// ============================================================================
AudioFilterFreqWeighting_F32 freqWeightA(audioSettings);
AudioCalcLevel_F32 calcLevelFast(audioSettings);
AudioCalcLevel_F32 calcLevelSlow(audioSettings);

// ============================================================================
// SD WRITER (raw audio capture, stereo)
// ============================================================================
AudioSDWriter_F32 sdWriter(audioSettings);

// ============================================================================
// PATCH CORDS — Main DSP Chain
// ============================================================================

// Input → PreGain
AudioConnection_F32 pc_in(i2s_in, 0, preGain, 0);

// PreGain → FIR filterbank (fan-out to 8 parallel bands)
AudioConnection_F32 pc_f0(preGain, 0, firFilt[0], 0);
AudioConnection_F32 pc_f1(preGain, 0, firFilt[1], 0);
AudioConnection_F32 pc_f2(preGain, 0, firFilt[2], 0);
AudioConnection_F32 pc_f3(preGain, 0, firFilt[3], 0);
AudioConnection_F32 pc_f4(preGain, 0, firFilt[4], 0);
AudioConnection_F32 pc_f5(preGain, 0, firFilt[5], 0);
AudioConnection_F32 pc_f6(preGain, 0, firFilt[6], 0);
AudioConnection_F32 pc_f7(preGain, 0, firFilt[7], 0);

// FIR → per-band WDRC compressors
AudioConnection_F32 pc_c0(firFilt[0], 0, compPerBand[0], 0);
AudioConnection_F32 pc_c1(firFilt[1], 0, compPerBand[1], 0);
AudioConnection_F32 pc_c2(firFilt[2], 0, compPerBand[2], 0);
AudioConnection_F32 pc_c3(firFilt[3], 0, compPerBand[3], 0);
AudioConnection_F32 pc_c4(firFilt[4], 0, compPerBand[4], 0);
AudioConnection_F32 pc_c5(firFilt[5], 0, compPerBand[5], 0);
AudioConnection_F32 pc_c6(firFilt[6], 0, compPerBand[6], 0);
AudioConnection_F32 pc_c7(firFilt[7], 0, compPerBand[7], 0);

// Per-band WDRC → mixer (recombine)
AudioConnection_F32 pc_m0(compPerBand[0], 0, mixer1, 0);
AudioConnection_F32 pc_m1(compPerBand[1], 0, mixer1, 1);
AudioConnection_F32 pc_m2(compPerBand[2], 0, mixer1, 2);
AudioConnection_F32 pc_m3(compPerBand[3], 0, mixer1, 3);
AudioConnection_F32 pc_m4(compPerBand[4], 0, mixer1, 4);
AudioConnection_F32 pc_m5(compPerBand[5], 0, mixer1, 5);
AudioConnection_F32 pc_m6(compPerBand[6], 0, mixer1, 6);
AudioConnection_F32 pc_m7(compPerBand[7], 0, mixer1, 7);

// Mixer → broadband limiter → stereo output
AudioConnection_F32 pc_bb(mixer1, 0, compBroadband, 0);
AudioConnection_F32 pc_outL(compBroadband, 0, i2s_out, 0);
AudioConnection_F32 pc_outR(compBroadband, 0, i2s_out, 1);

// ============================================================================
// PATCH CORDS — Metrics Branch
// ============================================================================

// Input → A-weighting → level calculators (fast + slow)
AudioConnection_F32 pc_aw(i2s_in, 0, freqWeightA, 0);
AudioConnection_F32 pc_lf(freqWeightA, 0, calcLevelFast, 0);
AudioConnection_F32 pc_ls(freqWeightA, 0, calcLevelSlow, 0);

// ============================================================================
// PATCH CORDS — SD Writer (raw audio, both channels)
// ============================================================================
AudioConnection_F32 pc_sd0(i2s_in, 0, sdWriter, 0);
AudioConnection_F32 pc_sd1(i2s_in, 1, sdWriter, 1);

// ============================================================================
// INITIALISATION
// ============================================================================

void audioGraphInit() {
  // Allocate audio memory
  AudioMemory(AUDIO_MEM_I16);
  AudioMemory_F32(AUDIO_MEM_F32, audioSettings);

  // Start Tympan codec
  myTympan.enable();
  myTympan.inputSelect(DEFAULT_INPUT_SOURCE);
  myTympan.volume_dB(DEFAULT_HEADPHONE_DB);
  myTympan.setInputGain_dB(DEFAULT_INPUT_GAIN_DB);

  // Configure metrics time constants
  freqWeightA.setWeightingType(A_WEIGHT);
  calcLevelFast.setTimeConst_sec(TIME_CONST_FAST_SEC);
  calcLevelSlow.setTimeConst_sec(TIME_CONST_SLOW_SEC);

  // SD writer setup
  sdWriter.setSerial(&myTympan);
  sdWriter.setNumWriteChannels(2);

  myTympan.println("[AudioGraph] Initialised: Fs=" + String(SAMPLE_RATE_HZ, 0) +
                   " Hz, block=" + String(AUDIO_BLOCK_SAMPLES));
}

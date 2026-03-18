/*
 * ============================================================================
 * AudioGraph.cpp — Audio Object Definitions & Patch Cords (v2.1)
 * ============================================================================
 * CHANGE: FIR filterbank → IIR Biquad Filterbank (AudioFilterbankBiquad_F32)
 *         Added per-band gain blocks between filterbank and WDRC
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
AudioInputI2S_F32  i2s_in(audioSettings);
AudioOutputI2S_F32 i2s_out(audioSettings);

// ============================================================================
// DSP PROCESSING CHAIN (Left)
// ============================================================================
AudioEffectGain_F32        preGain_L;
AudioFilterbankBiquad_F32  filterbank_L(audioSettings);   // <-- IIR biquad filterbank
AudioEffectGain_F32        bandGain_L[N_CHAN];             // <-- NAL-R gain per band
AudioEffectCompWDRC_F32    compPerBand_L[N_CHAN];
AudioMixer16_F32           mixer_L(audioSettings);
AudioEffectCompWDRC_F32    compBroadband_L;

// ============================================================================
// DSP PROCESSING CHAIN (Right)
// ============================================================================
AudioEffectGain_F32        preGain_R;
AudioFilterbankBiquad_F32  filterbank_R(audioSettings);
AudioEffectGain_F32        bandGain_R[N_CHAN];
AudioEffectCompWDRC_F32    compPerBand_R[N_CHAN];
AudioMixer16_F32           mixer_R(audioSettings);
AudioEffectCompWDRC_F32    compBroadband_R;

// ============================================================================
// METRICS BRANCH
// ============================================================================
AudioFilterFreqWeighting_F32 freqWeightA(audioSettings);
AudioCalcLevel_F32           calcLevelFast(audioSettings);
AudioCalcLevel_F32           calcLevelSlow(audioSettings);

// ============================================================================
// SD WRITER
// ============================================================================
AudioSDWriter_F32 sdWriter(audioSettings);

// ============================================================================
// PATCH CORDS — Left Channel
// ============================================================================

// Input L → PreGain L → Filterbank L
AudioConnection_F32 pc_in_L(i2s_in, 0, preGain_L, 0);
// AudioConnection_F32 pc_fb_L(preGain_L, 0, filterbank_L, 0);



// Filterbank L outputs → per-band gain L
AudioConnection_F32 pc_bg0_L(filterbank_L, 0, bandGain_L[0], 0);
AudioConnection_F32 pc_bg1_L(filterbank_L, 1, bandGain_L[1], 0);
AudioConnection_F32 pc_bg2_L(filterbank_L, 2, bandGain_L[2], 0);
AudioConnection_F32 pc_bg3_L(filterbank_L, 3, bandGain_L[3], 0);
AudioConnection_F32 pc_bg4_L(filterbank_L, 4, bandGain_L[4], 0);
AudioConnection_F32 pc_bg5_L(filterbank_L, 5, bandGain_L[5], 0);
AudioConnection_F32 pc_bg6_L(filterbank_L, 6, bandGain_L[6], 0);
AudioConnection_F32 pc_bg7_L(filterbank_L, 7, bandGain_L[7], 0);

// Per-band gain L → WDRC compressors L
AudioConnection_F32 pc_c0_L(bandGain_L[0], 0, compPerBand_L[0], 0);
AudioConnection_F32 pc_c1_L(bandGain_L[1], 0, compPerBand_L[1], 0);
AudioConnection_F32 pc_c2_L(bandGain_L[2], 0, compPerBand_L[2], 0);
AudioConnection_F32 pc_c3_L(bandGain_L[3], 0, compPerBand_L[3], 0);
AudioConnection_F32 pc_c4_L(bandGain_L[4], 0, compPerBand_L[4], 0);
AudioConnection_F32 pc_c5_L(bandGain_L[5], 0, compPerBand_L[5], 0);
AudioConnection_F32 pc_c6_L(bandGain_L[6], 0, compPerBand_L[6], 0);
AudioConnection_F32 pc_c7_L(bandGain_L[7], 0, compPerBand_L[7], 0);

// WDRC L → mixer L
AudioConnection_F32 pc_m0_L(compPerBand_L[0], 0, mixer_L, 0);
AudioConnection_F32 pc_m1_L(compPerBand_L[1], 0, mixer_L, 1);
AudioConnection_F32 pc_m2_L(compPerBand_L[2], 0, mixer_L, 2);
AudioConnection_F32 pc_m3_L(compPerBand_L[3], 0, mixer_L, 3);
AudioConnection_F32 pc_m4_L(compPerBand_L[4], 0, mixer_L, 4);
AudioConnection_F32 pc_m5_L(compPerBand_L[5], 0, mixer_L, 5);
AudioConnection_F32 pc_m6_L(compPerBand_L[6], 0, mixer_L, 6);
AudioConnection_F32 pc_m7_L(compPerBand_L[7], 0, mixer_L, 7);

// Mixer L → broadband limiter L → output L
AudioConnection_F32 pc_bb_L(mixer_L, 0, compBroadband_L, 0);
AudioConnection_F32 pc_out_L(compBroadband_L, 0, i2s_out, 0);

// ============================================================================
// PATCH CORDS — Right Channel
// ============================================================================

AudioConnection_F32 pc_in_R(i2s_in, 1, preGain_R, 0);
// AudioConnection_F32 pc_fb_R(preGain_R, 0, filterbank_R, 0);

#ifdef ENABLE_NOISE_REDUCTION
  // NR audio objects
  AudioEffectNoiseReduction_FD_F32 noiseReduction_L(audioSettings);
  AudioEffectNoiseReduction_FD_F32 noiseReduction_R(audioSettings);
 
  // Left: preGain → NR → filterbank
  AudioConnection_F32 pc_nr_in_L(preGain_L,         0, noiseReduction_L, 0);
  AudioConnection_F32 pc_fb_L   (noiseReduction_L,  0, filterbank_L,     0);
 
  // Right: preGain → NR → filterbank
  AudioConnection_F32 pc_nr_in_R(preGain_R,         0, noiseReduction_R, 0);
  AudioConnection_F32 pc_fb_R   (noiseReduction_R,  0, filterbank_R,     0);
#else
  // Original direct path (no NR)
  AudioConnection_F32 pc_fb_L(preGain_L, 0, filterbank_L, 0);
  AudioConnection_F32 pc_fb_R(preGain_R, 0, filterbank_R, 0);
#endif



AudioConnection_F32 pc_bg0_R(filterbank_R, 0, bandGain_R[0], 0);
AudioConnection_F32 pc_bg1_R(filterbank_R, 1, bandGain_R[1], 0);
AudioConnection_F32 pc_bg2_R(filterbank_R, 2, bandGain_R[2], 0);
AudioConnection_F32 pc_bg3_R(filterbank_R, 3, bandGain_R[3], 0);
AudioConnection_F32 pc_bg4_R(filterbank_R, 4, bandGain_R[4], 0);
AudioConnection_F32 pc_bg5_R(filterbank_R, 5, bandGain_R[5], 0);
AudioConnection_F32 pc_bg6_R(filterbank_R, 6, bandGain_R[6], 0);
AudioConnection_F32 pc_bg7_R(filterbank_R, 7, bandGain_R[7], 0);

AudioConnection_F32 pc_c0_R(bandGain_R[0], 0, compPerBand_R[0], 0);
AudioConnection_F32 pc_c1_R(bandGain_R[1], 0, compPerBand_R[1], 0);
AudioConnection_F32 pc_c2_R(bandGain_R[2], 0, compPerBand_R[2], 0);
AudioConnection_F32 pc_c3_R(bandGain_R[3], 0, compPerBand_R[3], 0);
AudioConnection_F32 pc_c4_R(bandGain_R[4], 0, compPerBand_R[4], 0);
AudioConnection_F32 pc_c5_R(bandGain_R[5], 0, compPerBand_R[5], 0);
AudioConnection_F32 pc_c6_R(bandGain_R[6], 0, compPerBand_R[6], 0);
AudioConnection_F32 pc_c7_R(bandGain_R[7], 0, compPerBand_R[7], 0);

AudioConnection_F32 pc_m0_R(compPerBand_R[0], 0, mixer_R, 0);
AudioConnection_F32 pc_m1_R(compPerBand_R[1], 0, mixer_R, 1);
AudioConnection_F32 pc_m2_R(compPerBand_R[2], 0, mixer_R, 2);
AudioConnection_F32 pc_m3_R(compPerBand_R[3], 0, mixer_R, 3);
AudioConnection_F32 pc_m4_R(compPerBand_R[4], 0, mixer_R, 4);
AudioConnection_F32 pc_m5_R(compPerBand_R[5], 0, mixer_R, 5);
AudioConnection_F32 pc_m6_R(compPerBand_R[6], 0, mixer_R, 6);
AudioConnection_F32 pc_m7_R(compPerBand_R[7], 0, mixer_R, 7);

AudioConnection_F32 pc_bb_R(mixer_R, 0, compBroadband_R, 0);
AudioConnection_F32 pc_out_R(compBroadband_R, 0, i2s_out, 1);

// ============================================================================
// PATCH CORDS — Metrics Branch
// ============================================================================
AudioConnection_F32 pc_aw(i2s_in, 0, freqWeightA, 0);
AudioConnection_F32 pc_lf(freqWeightA, 0, calcLevelFast, 0);
AudioConnection_F32 pc_ls(freqWeightA, 0, calcLevelSlow, 0);

// ============================================================================
// PATCH CORDS — SD Writer
// ============================================================================
AudioConnection_F32 pc_sd0(i2s_in, 0, sdWriter, 0);
AudioConnection_F32 pc_sd1(i2s_in, 1, sdWriter, 1);

// ============================================================================
// INITIALISATION
// ============================================================================

void audioGraphInit() {
  AudioMemory(AUDIO_MEM_I16);
  AudioMemory_F32(AUDIO_MEM_F32, audioSettings);

  myTympan.enable();
  myTympan.inputSelect(DEFAULT_INPUT_SOURCE);
  myTympan.volume_dB(DEFAULT_HEADPHONE_DB);
  myTympan.setInputGain_dB(DEFAULT_INPUT_GAIN_DB);

  // Metrics
  freqWeightA.setWeightingType(A_WEIGHT);
  calcLevelFast.setTimeConst_sec(TIME_CONST_FAST_SEC);
  calcLevelSlow.setTimeConst_sec(TIME_CONST_SLOW_SEC);

  // SD writer
  sdWriter.setSerial(&myTympan);
  sdWriter.setNumWriteChannels(2);

  myTympan.println("[AudioGraph] Initialised: Fs=" + String(SAMPLE_RATE_HZ, 0) +
                   " Hz, block=" + String(APP_BLOCK_SAMPLES) +
                   " (IIR biquad filterbank)");
}
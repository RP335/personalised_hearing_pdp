/*
 * ============================================================================
 * AudioGraph.h — Audio Object & Patch Cord Declarations (extern)
 * ============================================================================
 * All Tympan/OpenAudio objects live in AudioGraph.cpp.
 * Other modules reference them through these extern declarations.
 *
 * SIGNAL FLOW:
 *
 *   LineIn ──► PreGain ──┬──► FIR[0] ──► WDRC[0] ──┐
 *                        ├──► FIR[1] ──► WDRC[1] ──┤
 *                        ├──► FIR[2] ──► WDRC[2] ──┤
 *                        ├──► FIR[3] ──► WDRC[3] ──├──► Mixer ──► BB Limiter ──► Out L
 *                        ├──► FIR[4] ──► WDRC[4] ──┤                          ──► Out R
 *                        ├──► FIR[5] ──► WDRC[5] ──┤
 *                        ├──► FIR[6] ──► WDRC[6] ──┤
 *                        └──► FIR[7] ──► WDRC[7] ──┘
 *
 *   LineIn ──► A-Weight ──► CalcLevel  (metrics branch, parallel)
 *
 * ============================================================================
 */

#ifndef AUDIO_GRAPH_H
#define AUDIO_GRAPH_H

#include <Tympan_Library.h>
#include "Config.h"

// --- Global audio settings (defined in AudioGraph.cpp) ---
extern AudioSettings_F32 audioSettings;

// --- Hardware ---
extern Tympan myTympan;

// --- I/O ---
extern AudioInputI2S_F32        i2s_in;
extern AudioOutputI2S_F32       i2s_out;

// --- DSP chain ---
extern AudioEffectGain_F32      preGain;
extern AudioFilterFIR_F32       firFilt[N_CHAN];
extern AudioEffectCompWDRC_F32  compPerBand[N_CHAN];
extern AudioMixer8_F32          mixer1;
extern AudioEffectCompWDRC_F32  compBroadband;

// --- Metrics branch ---
extern AudioFilterFreqWeighting_F32  freqWeightA;
extern AudioCalcLevel_F32            calcLevelFast;
extern AudioCalcLevel_F32            calcLevelSlow;

// --- SD Writer (raw audio, optional) ---
extern AudioSDWriter_F32        sdWriter;

// --- Initialisation (call from setup()) ---
void audioGraphInit();

#endif // AUDIO_GRAPH_H

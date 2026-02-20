/*
 * ============================================================================
 * DSPManager.cpp — DSP Algorithm Manager Implementation (v2.1)
 * ============================================================================
 * CHANGES FROM v2.0:
 *   - Filterbank: FIR → IIR biquad (AudioFilterbankBiquad_F32::designFilters)
 *   - NAL-R gain applied via separate bandGain_L/R[] blocks (not as WDRC tkgain)
 *   - Expansion (noise gate) support with runtime toggle
 *   - WDRC compressors use tkgain=0 (dynamics only, gain is handled upstream)
 * ============================================================================
 */

#include "DSPManager.h"
#include "AudioGraph.h"
#include "NALR.h"

// ============================================================================
// STATE
// ============================================================================

DSPFlags dspFlags;
float currentNALRGains_L[N_CHAN] = {0};
float currentNALRGains_R[N_CHAN] = {0};
float currentPreGainDB = 0.0f;
bool  dspProfileLoaded = false;

// Internal: last-applied thresholds
static int8_t lastThresholds_L[N_AUDIO_FREQS] = {0};
static int8_t lastThresholds_R[N_AUDIO_FREQS] = {0};

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Configure a single per-band WDRC compressor
// NOTE: tkgain is always 0 now — NAL-R gain lives in bandGain_L/R[] blocks
static void configureBandWDRC(AudioEffectCompWDRC_F32 &comp,
                              float cr, float tk, float bolt) {
  float expCR   = dspFlags.expansionEnabled ? WDRC_EXP_CR       : 1.0f;
  float expKnee = dspFlags.expansionEnabled ? WDRC_EXP_END_KNEE : 0.0f;

  comp.setSampleRate_Hz(audioSettings.sample_rate_Hz);
  comp.setParams(
    WDRC_ATTACK_MS, WDRC_RELEASE_MS, WDRC_MAXDB,
    expCR, expKnee,   // expansion (noise gate)
    0.0f,             // tkgain = 0 (gain handled by bandGain blocks)
    cr, tk, bolt
  );
}

// Push NAL-R gains into the separate gain blocks + reconfigure compressors
static void pushGainsAndCompressors() {
  float cr   = dspFlags.wdrcEnabled ? WDRC_CR_ACTIVE   : WDRC_CR_BYPASS;
  float tk   = dspFlags.wdrcEnabled ? WDRC_TK_ACTIVE   : WDRC_TK_BYPASS;
  float bolt = dspFlags.wdrcEnabled ? WDRC_BOLT_ACTIVE  : WDRC_BOLT_BYPASS;

  for (int i = 0; i < N_CHAN; i++) {
    // --- Gain stage: NAL-R insertion gain ---
    float gain_L = dspFlags.nalrEnabled ? currentNALRGains_L[i] : 0.0f;
    float gain_R = dspFlags.nalrEnabled ? currentNALRGains_R[i] : 0.0f;
    bandGain_L[i].setGain_dB(gain_L);
    bandGain_R[i].setGain_dB(gain_R);

    // --- WDRC: dynamics only (no makeup gain) ---
    configureBandWDRC(compPerBand_L[i], cr, tk, bolt);
    configureBandWDRC(compPerBand_R[i], cr, tk, bolt);
  }
}

// Set all bands to flat/bypass
static void setAllBandsFlat() {
  for (int i = 0; i < N_CHAN; i++) {
    bandGain_L[i].setGain_dB(0.0f);
    bandGain_R[i].setGain_dB(0.0f);
    configureBandWDRC(compPerBand_L[i], WDRC_CR_BYPASS, WDRC_TK_BYPASS, WDRC_BOLT_BYPASS);
    configureBandWDRC(compPerBand_R[i], WDRC_CR_BYPASS, WDRC_TK_BYPASS, WDRC_BOLT_BYPASS);
    currentNALRGains_L[i] = 0.0f;
    currentNALRGains_R[i] = 0.0f;
  }
}

// Configure the broadband output limiter
static void configureBBLimiter() {
  float fs = audioSettings.sample_rate_Hz;
  compBroadband_L.setSampleRate_Hz(fs);
  compBroadband_R.setSampleRate_Hz(fs);

  if (dspFlags.limiterEnabled) {
    compBroadband_L.setParams(BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
                              1.0f, 0.0f, 0.0f, BB_CR, BB_TK, BB_BOLT);
    compBroadband_R.setParams(BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
                              1.0f, 0.0f, 0.0f, BB_CR, BB_TK, BB_BOLT);
  } else {
    compBroadband_L.setParams(BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
                              1.0f, 0.0f, 0.0f, 1.0f, 119.0f, 119.0f);
    compBroadband_R.setParams(BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
                              1.0f, 0.0f, 0.0f, 1.0f, 119.0f, 119.0f);
  }
}

// ============================================================================
// INITIALISATION
// ============================================================================

void dspInit() {
  // --- Design IIR biquad filterbank ---
  float xfreqs[N_CHAN - 1];
  for (int i = 0; i < N_CHAN - 1; i++)
    xfreqs[i] = CROSSOVER_FREQS[i];

  int ret_L = filterbank_L.designFilters(N_CHAN, FILTERBANK_ORDER,
                SAMPLE_RATE_HZ, APP_BLOCK_SAMPLES, xfreqs);
  int ret_R = filterbank_R.designFilters(N_CHAN, FILTERBANK_ORDER,
                SAMPLE_RATE_HZ, APP_BLOCK_SAMPLES, xfreqs);

  if (ret_L < 0 || ret_R < 0) {
    myTympan.println("[DSP] *** FILTERBANK DESIGN FAILED ***");
  } else {
    myTympan.println("[DSP] Biquad filterbank OK (8 bands, order "
                     + String(FILTERBANK_ORDER) + ")");
  }

  // --- Initialise all gain blocks to 0 dB ---
  for (int i = 0; i < N_CHAN; i++) {
    bandGain_L[i].setGain_dB(0.0f);
    bandGain_R[i].setGain_dB(0.0f);
  }

  // --- Set all compressors to bypass ---
  setAllBandsFlat();
  configureBBLimiter();

  // --- Pre-gain ---
  preGain_L.setGain_dB(currentPreGainDB);
  preGain_R.setGain_dB(currentPreGainDB);

  myTympan.println("[DSP] Filterbank + WDRC initialised (bypass mode)");
}

// ============================================================================
// PROFILE APPLICATION
// ============================================================================

void dspApplyProfile(const int8_t leftT[N_AUDIO_FREQS],
                     const int8_t rightT[N_AUDIO_FREQS]) {
  memcpy(lastThresholds_L, leftT, sizeof(lastThresholds_L));
  memcpy(lastThresholds_R, rightT, sizeof(lastThresholds_R));

  computeNALRGains(leftT, currentNALRGains_L);
  computeNALRGains(rightT, currentNALRGains_R);

  dspFlags.nalrEnabled = true;
  dspFlags.wdrcEnabled = true;
  dspProfileLoaded = true;

  pushGainsAndCompressors();
  configureBBLimiter();

  myTympan.println("\n[DSP] Profile applied — per-band NAL-R gains (L/R):");
  for (int i = 0; i < N_CHAN; i++) {
    myTympan.print("  Band "); myTympan.print(i);
    myTympan.print(" (");      myTympan.print(BAND_CENTERS[i], 0);
    myTympan.print(" Hz): L="); myTympan.print(currentNALRGains_L[i], 1);
    myTympan.print(" dB, R="); myTympan.print(currentNALRGains_R[i], 1);
    myTympan.println(" dB");
  }
}

void dspSetBypass() {
  dspFlags.nalrEnabled = false;
  dspFlags.wdrcEnabled = false;
  dspProfileLoaded = false;

  setAllBandsFlat();
  configureBBLimiter();

  myTympan.println("[DSP] Full bypass — flat response");
}

// ============================================================================
// ALGORITHM TOGGLES
// ============================================================================

void dspToggleNALR()      { dspSetNALR(!dspFlags.nalrEnabled); }
void dspToggleWDRC()      { dspSetWDRC(!dspFlags.wdrcEnabled); }
void dspToggleLimiter()   { dspSetLimiter(!dspFlags.limiterEnabled); }
void dspToggleExpansion() { dspSetExpansion(!dspFlags.expansionEnabled); }

void dspSetNALR(bool on) {
  dspFlags.nalrEnabled = on;
  pushGainsAndCompressors();
  myTympan.print("[DSP] NAL-R: ");
  myTympan.println(on ? "ON" : "OFF");
}

void dspSetWDRC(bool on) {
  dspFlags.wdrcEnabled = on;
  pushGainsAndCompressors();
  myTympan.print("[DSP] WDRC: ");
  myTympan.println(on ? "ON" : "OFF");
}

void dspSetLimiter(bool on) {
  dspFlags.limiterEnabled = on;
  configureBBLimiter();
  myTympan.print("[DSP] Limiter: ");
  myTympan.println(on ? "ON" : "OFF");
}

void dspSetExpansion(bool on) {
  dspFlags.expansionEnabled = on;
  pushGainsAndCompressors();  // reconfigure all compressors with new expansion setting
  myTympan.print("[DSP] Expansion (noise gate): ");
  myTympan.println(on ? "ON" : "OFF");
}

// ============================================================================
// PRE-GAIN
// ============================================================================

void dspSetPreGain(float dB) {
  if (dB < -40.0f) dB = -40.0f;
  if (dB >  40.0f) dB =  40.0f;
  currentPreGainDB = dB;
  preGain_L.setGain_dB(dB);
  preGain_R.setGain_dB(dB);
  myTympan.print("[DSP] Pre-gain = ");
  myTympan.print(dB);
  myTympan.println(" dB");
}

void dspAdjustPreGain(float deltadB) {
  dspSetPreGain(currentPreGainDB + deltadB);
}

// ============================================================================
// ADVANCED WDRC TWEAKS
// ============================================================================

void dspSetCompressionRatio(float cr) {
  for (int i = 0; i < N_CHAN; i++) {
    configureBandWDRC(compPerBand_L[i], cr, WDRC_TK_ACTIVE, WDRC_BOLT_ACTIVE);
    configureBandWDRC(compPerBand_R[i], cr, WDRC_TK_ACTIVE, WDRC_BOLT_ACTIVE);
  }
  myTympan.print("[DSP] Compression ratio = ");
  myTympan.println(cr);
}

void dspSetBandBOLT(float bolt_dBSPL) {
  float cr = dspFlags.wdrcEnabled ? WDRC_CR_ACTIVE : WDRC_CR_BYPASS;
  for (int i = 0; i < N_CHAN; i++) {
    configureBandWDRC(compPerBand_L[i], cr, WDRC_TK_ACTIVE, bolt_dBSPL);
    configureBandWDRC(compPerBand_R[i], cr, WDRC_TK_ACTIVE, bolt_dBSPL);
  }
  myTympan.print("[DSP] Band BOLT = ");
  myTympan.print(bolt_dBSPL);
  myTympan.println(" dB SPL");
}

void dspSetBBCeiling(float bolt_dBSPL) {
  float fs = audioSettings.sample_rate_Hz;
  compBroadband_L.setSampleRate_Hz(fs);
  compBroadband_R.setSampleRate_Hz(fs);
  compBroadband_L.setParams(BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
                            1.0f, 0.0f, 0.0f, BB_CR, BB_TK, bolt_dBSPL);
  compBroadband_R.setParams(BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
                            1.0f, 0.0f, 0.0f, BB_CR, BB_TK, bolt_dBSPL);
  myTympan.print("[DSP] BB ceiling = ");
  myTympan.print(bolt_dBSPL);
  myTympan.println(" dB SPL");
}

// ============================================================================
// STATUS / DEBUG
// ============================================================================

void dspPrintStatus() {
  myTympan.println("\n=== DSP STATUS ===");
  myTympan.print("  Profile loaded: ");
  myTympan.println(dspProfileLoaded ? "YES" : "NO");
  myTympan.print("  NAL-R:     ");
  myTympan.println(dspFlags.nalrEnabled ? "ON" : "OFF");
  myTympan.print("  WDRC:      ");
  myTympan.println(dspFlags.wdrcEnabled ? "ON" : "OFF");
  myTympan.print("  Expansion: ");
  myTympan.println(dspFlags.expansionEnabled ? "ON" : "OFF");
  myTympan.print("  Limiter:   ");
  myTympan.println(dspFlags.limiterEnabled ? "ON" : "OFF");
  myTympan.print("  Pre-gain:  ");
  myTympan.print(currentPreGainDB);
  myTympan.println(" dB");

  myTympan.print("  CPU: ");
  myTympan.print(AudioProcessorUsage());
  myTympan.print("% | Mem: ");
  myTympan.println(AudioMemoryUsage());

  for (int i = 0; i < N_CHAN; i++) {
    myTympan.print("  Band "); myTympan.print(i);
    myTympan.print(" (");      myTympan.print(BAND_CENTERS[i], 0);
    myTympan.print(" Hz): gain L="); myTympan.print(currentNALRGains_L[i], 1);
    myTympan.print(" / R=");   myTympan.print(currentNALRGains_R[i], 1);
    myTympan.print(" dB, Lvl L=");
    myTympan.print(compPerBand_L[i].getCurrentLevel_dB(), 1);
    myTympan.print(" / R=");
    myTympan.print(compPerBand_R[i].getCurrentLevel_dB(), 1);
    myTympan.println(" dB");
  }

  myTympan.print("  BB level L: ");
  myTympan.print(compBroadband_L.getCurrentLevel_dB(), 1);
  myTympan.print(" dB, R: ");
  myTympan.print(compBroadband_R.getCurrentLevel_dB(), 1);
  myTympan.println(" dB");
  myTympan.println("==================\n");
}

void dspGetBandLevels(float levelsOutL[N_CHAN], float levelsOutR[N_CHAN]) {
  for (int i = 0; i < N_CHAN; i++) {
    levelsOutL[i] = compPerBand_L[i].getCurrentLevel_dB();
    levelsOutR[i] = compPerBand_R[i].getCurrentLevel_dB();
  }
}

float dspGetBBLevel() {
  return (compBroadband_L.getCurrentLevel_dB() +
          compBroadband_R.getCurrentLevel_dB()) / 2.0f;
}
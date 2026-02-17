/*
 * ============================================================================
 * DSPManager.cpp — DSP Algorithm Manager Implementation
 * ============================================================================
 */

#include "DSPManager.h"
#include "AudioGraph.h"
#include "NALR.h"

// ============================================================================
// STATE
// ============================================================================

DSPFlags  dspFlags;
float     currentNALRGains[N_CHAN] = {0};
float     currentPreGainDB         = 0.0f;
bool      dspProfileLoaded         = false;

// Internal: last-applied thresholds (so we can re-apply after toggle)
static int8_t lastThresholds[N_AUDIO_FREQS] = {0};

// FIR coefficient storage
static float firCoeffs[N_CHAN][N_FIR];

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Configure a single per-band compressor with given parameters
static void configureBandWDRC(int band, float tkgain, float cr,
                               float tk, float bolt) {
  float fs = audioSettings.sample_rate_Hz;
  compPerBand[band].setSampleRate_Hz(fs);
  compPerBand[band].setParams(
    WDRC_ATTACK_MS,
    WDRC_RELEASE_MS,
    WDRC_MAXDB,
    WDRC_EXP_CR,
    WDRC_EXP_END_KNEE,
    tkgain,              // linear gain below knee
    cr,                  // compression ratio
    tk,                  // compression knee (dB SPL)
    bolt                 // output limit (dB SPL)
  );
}

// Set all bands to flat/bypass (unity gain, no compression)
static void setAllBandsFlat() {
  for (int i = 0; i < N_CHAN; i++) {
    configureBandWDRC(i, 0.0f, WDRC_CR_BYPASS, WDRC_TK_BYPASS, WDRC_BOLT_BYPASS);
    currentNALRGains[i] = 0.0f;
  }
}

// Push NAL-R gains into compressors (with current WDRC settings)
static void pushNALRGains() {
  float cr   = dspFlags.wdrcEnabled ? WDRC_CR_ACTIVE   : WDRC_CR_BYPASS;
  float tk   = dspFlags.wdrcEnabled ? WDRC_TK_ACTIVE   : WDRC_TK_BYPASS;
  float bolt = dspFlags.wdrcEnabled ? WDRC_BOLT_ACTIVE  : WDRC_BOLT_BYPASS;

  for (int i = 0; i < N_CHAN; i++) {
    float gain = dspFlags.nalrEnabled ? currentNALRGains[i] : 0.0f;
    configureBandWDRC(i, gain, cr, tk, bolt);
  }
}

// Configure the broadband output limiter
static void configureBBLimiter() {
  float fs = audioSettings.sample_rate_Hz;
  compBroadband.setSampleRate_Hz(fs);

  if (dspFlags.limiterEnabled) {
    compBroadband.setParams(
      BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
      1.0f, 0.0f,    // expansion off
      0.0f,           // no linear gain
      BB_CR,          // linear below knee
      BB_TK,          // limiter knee
      BB_BOLT          // output ceiling
    );
  } else {
    // Limiter disabled: high ceiling, no limiting
    compBroadband.setParams(
      BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
      1.0f, 0.0f,
      0.0f,
      1.0f,           // unity ratio
      119.0f,         // knee above any real signal
      119.0f          // effectively no limit
    );
  }
}

// ============================================================================
// INITIALISATION
// ============================================================================

void dspInit() {
  // --- Generate FIR filterbank coefficients ---
  float xfreqs[N_CHAN - 1];
  for (int i = 0; i < N_CHAN - 1; i++) xfreqs[i] = CROSSOVER_FREQS[i];

  AudioConfigFIRFilterBank_F32 makeFIR(
    N_CHAN, N_FIR,
    audioSettings.sample_rate_Hz,
    xfreqs,
    (float *)firCoeffs
  );

  // Load coefficients into each FIR filter
  for (int i = 0; i < N_CHAN; i++) {
    firFilt[i].begin(firCoeffs[i], N_FIR, audioSettings.audio_block_samples);
  }

  // --- Set all compressors to flat/bypass ---
  setAllBandsFlat();
  configureBBLimiter();

  // --- Pre-gain ---
  preGain.setGain_dB(currentPreGainDB);

  myTympan.println("[DSP] Filterbank + WDRC initialised (bypass mode)");
}

// ============================================================================
// PROFILE APPLICATION
// ============================================================================

void dspApplyProfile(const int8_t thresholds[N_AUDIO_FREQS]) {
  // Store thresholds for re-application
  memcpy(lastThresholds, thresholds, sizeof(lastThresholds));

  // Compute NAL-R gains for all bands
  computeNALRGains(thresholds, currentNALRGains);

  // Enable NAL-R + WDRC
  dspFlags.nalrEnabled = true;
  dspFlags.wdrcEnabled = true;
  dspProfileLoaded = true;

  // Push into compressors
  pushNALRGains();
  configureBBLimiter();

  // Log
  myTympan.println("\n[DSP] Profile applied — per-band NAL-R gains:");
  for (int i = 0; i < N_CHAN; i++) {
    myTympan.print("  Band ");
    myTympan.print(i);
    myTympan.print(" (");
    myTympan.print(BAND_CENTERS[i], 0);
    myTympan.print(" Hz): ");
    myTympan.print(currentNALRGains[i], 1);
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

void dspToggleNALR() { dspSetNALR(!dspFlags.nalrEnabled); }
void dspToggleWDRC() { dspSetWDRC(!dspFlags.wdrcEnabled); }
void dspToggleLimiter() { dspSetLimiter(!dspFlags.limiterEnabled); }

void dspSetNALR(bool on) {
  dspFlags.nalrEnabled = on;
  pushNALRGains();
  myTympan.print("[DSP] NAL-R: ");
  myTympan.println(on ? "ON" : "OFF");
}

void dspSetWDRC(bool on) {
  dspFlags.wdrcEnabled = on;
  pushNALRGains();
  myTympan.print("[DSP] WDRC: ");
  myTympan.println(on ? "ON" : "OFF");
}

void dspSetLimiter(bool on) {
  dspFlags.limiterEnabled = on;
  configureBBLimiter();
  myTympan.print("[DSP] Limiter: ");
  myTympan.println(on ? "ON" : "OFF");
}

// ============================================================================
// PRE-GAIN
// ============================================================================

void dspSetPreGain(float dB) {
  if (dB < -40.0f) dB = -40.0f;
  if (dB >  40.0f) dB =  40.0f;
  currentPreGainDB = dB;
  preGain.setGain_dB(dB);
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
    float gain = dspFlags.nalrEnabled ? currentNALRGains[i] : 0.0f;
    configureBandWDRC(i, gain, cr, WDRC_TK_ACTIVE, WDRC_BOLT_ACTIVE);
  }
  myTympan.print("[DSP] Compression ratio = ");
  myTympan.println(cr);
}

void dspSetBandBOLT(float bolt_dBSPL) {
  for (int i = 0; i < N_CHAN; i++) {
    float gain = dspFlags.nalrEnabled ? currentNALRGains[i] : 0.0f;
    float cr   = dspFlags.wdrcEnabled ? WDRC_CR_ACTIVE : WDRC_CR_BYPASS;
    configureBandWDRC(i, gain, cr, WDRC_TK_ACTIVE, bolt_dBSPL);
  }
  myTympan.print("[DSP] Band BOLT = ");
  myTympan.print(bolt_dBSPL);
  myTympan.println(" dB SPL");
}

void dspSetBBCeiling(float bolt_dBSPL) {
  float fs = audioSettings.sample_rate_Hz;
  compBroadband.setSampleRate_Hz(fs);
  compBroadband.setParams(
    BB_ATTACK_MS, BB_RELEASE_MS, BB_MAXDB,
    1.0f, 0.0f, 0.0f, BB_CR, BB_TK, bolt_dBSPL
  );
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
  myTympan.print("  NAL-R:    ");
  myTympan.println(dspFlags.nalrEnabled ? "ON" : "OFF");
  myTympan.print("  WDRC:     ");
  myTympan.println(dspFlags.wdrcEnabled ? "ON" : "OFF");
  myTympan.print("  Limiter:  ");
  myTympan.println(dspFlags.limiterEnabled ? "ON" : "OFF");
  myTympan.print("  Pre-gain: ");
  myTympan.print(currentPreGainDB);
  myTympan.println(" dB");

  myTympan.print("  CPU: ");
  myTympan.print(AudioProcessorUsage());
  myTympan.print("% | Mem: ");
  myTympan.println(AudioMemoryUsage());

  for (int i = 0; i < N_CHAN; i++) {
    myTympan.print("  Band ");
    myTympan.print(i);
    myTympan.print(" (");
    myTympan.print(BAND_CENTERS[i], 0);
    myTympan.print(" Hz): gain=");
    myTympan.print(currentNALRGains[i], 1);
    myTympan.print(" dB, level=");
    myTympan.print(compPerBand[i].getCurrentLevel_dB(), 1);
    myTympan.println(" dB");
  }

  myTympan.print("  BB level: ");
  myTympan.print(compBroadband.getCurrentLevel_dB(), 1);
  myTympan.println(" dB");
  myTympan.println("==================\n");
}

void dspGetBandLevels(float levelsOut[N_CHAN]) {
  for (int i = 0; i < N_CHAN; i++) {
    levelsOut[i] = compPerBand[i].getCurrentLevel_dB();
  }
}

float dspGetBBLevel() {
  return compBroadband.getCurrentLevel_dB();
}

/*
 * ============================================================================
 * Metrics.cpp — Sound Level Metrics Engine Implementation
 * ============================================================================
 *
 * LAeq is computed using the energy-average principle:
 *   LAeq,T = 10 * log10( (1/N) * Σ 10^(Li/10) )
 * where Li are individual SPL samples and N is the number of samples.
 *
 * Noise dose follows NIOSH (1998) criterion:
 *   D = Σ (Ci / Ti) × 100%
 * where Ci = actual exposure time at level Li,
 *       Ti = allowable time = 8 / 2^((Li - 85)/3) hours
 * ============================================================================
 */

#include "Metrics.h"
#include "AudioGraph.h"
#include "DSPManager.h"
#include <math.h>

// ============================================================================
// INTERNAL STATE
// ============================================================================

static MetricsSnapshot latest;

// LAeq accumulators (store sum of 10^(L/10) for energy averaging)
static double  laeq1min_accum   = 0.0;
static int     laeq1min_count   = 0;
static unsigned long laeq1min_start = 0;

static double  laeq1hr_accum    = 0.0;
static int     laeq1hr_count    = 0;
static unsigned long laeq1hr_start  = 0;

// Noise dose accumulator
static double  doseAccum        = 0.0;    // fractional dose (0.0 = 0%, 1.0 = 100%)
static unsigned long doseStartMs = 0;
static unsigned long lastUpdateMs = 0;

// Peak tracker
static float   peakSPL          = -120.0f;

// ============================================================================
// INIT
// ============================================================================

void metricsInit() {
  memset(&latest, 0, sizeof(latest));
  metricsReset();
  myTympan.println("[Metrics] Initialised");
}

void metricsReset() {
  unsigned long now = millis();

  laeq1min_accum = 0.0;
  laeq1min_count = 0;
  laeq1min_start = now;

  laeq1hr_accum  = 0.0;
  laeq1hr_count  = 0;
  laeq1hr_start  = now;

  doseAccum   = 0.0;
  doseStartMs = now;
  peakSPL     = -120.0f;

  myTympan.println("[Metrics] Accumulators reset");
}

// ============================================================================
// UPDATE (call frequently from loop)
// ============================================================================

void metricsUpdate(unsigned long nowMs) {
  // Rate-limit to ~10 Hz
  if (nowMs - lastUpdateMs < 100) return;
  unsigned long dtMs = nowMs - lastUpdateMs;
  lastUpdateMs = nowMs;

  // --- Read raw levels from audio engine ---
  float rawFast = calcLevelFast.getCurrentLevel_dB();
  float rawSlow = calcLevelSlow.getCurrentLevel_dB();

  // Apply calibration offset (dBFS → dB SPL)
  float splFast = rawFast + CAL_OFFSET_DB;
  float splSlow = rawSlow + CAL_OFFSET_DB;

  // Floor at noise floor
  if (splFast < 0.0f) splFast = 0.0f;
  if (splSlow < 0.0f) splSlow = 0.0f;

  // --- Peak tracking ---
  if (splFast > peakSPL) peakSPL = splFast;

  // --- LAeq accumulation (energy domain) ---
  // Add 10^(L/10) to running sum
  double energySample = pow(10.0, (double)splFast / 10.0);

  // 1-minute LAeq
  laeq1min_accum += energySample;
  laeq1min_count++;
  if (nowMs - laeq1min_start >= LAEQ_SHORT_MS) {
    // Period complete — compute and restart
    if (laeq1min_count > 0) {
      latest.laeq1min_dBA = (float)(10.0 * log10(laeq1min_accum / (double)laeq1min_count));
    }
    laeq1min_accum = 0.0;
    laeq1min_count = 0;
    laeq1min_start = nowMs;
  } else if (laeq1min_count > 0) {
    // Running estimate
    latest.laeq1min_dBA = (float)(10.0 * log10(laeq1min_accum / (double)laeq1min_count));
  }

  // 1-hour LAeq
  laeq1hr_accum += energySample;
  laeq1hr_count++;
  if (nowMs - laeq1hr_start >= LAEQ_LONG_MS) {
    if (laeq1hr_count > 0) {
      latest.laeq1hr_dBA = (float)(10.0 * log10(laeq1hr_accum / (double)laeq1hr_count));
    }
    laeq1hr_accum = 0.0;
    laeq1hr_count = 0;
    laeq1hr_start = nowMs;
  } else if (laeq1hr_count > 0) {
    latest.laeq1hr_dBA = (float)(10.0 * log10(laeq1hr_accum / (double)laeq1hr_count));
  }

  // --- Noise dose (NIOSH) ---
  // At level L, allowable time T(L) = 8 / 2^((L - 85) / 3) hours
  // Dose increment for dt seconds at level L:
  //   dD = dt / (T(L) * 3600)   (convert T from hours to seconds)
  if (splFast >= 80.0f) {  // only accumulate above threshold
    float dtSec = (float)dtMs / 1000.0f;
    double allowableHours = (double)DOSE_CRITERION_HR /
                            pow(2.0, ((double)splFast - (double)DOSE_CRITERION_DB)
                                      / (double)DOSE_EXCHANGE_RATE);
    double allowableSec = allowableHours * 3600.0;
    if (allowableSec > 0.0) {
      doseAccum += (double)dtSec / allowableSec;
    }
  }

  // Projected 8-hr dose
  float elapsedHours = (float)(nowMs - doseStartMs) / 3600000.0f;
  float projected8hr = 0.0f;
  if (elapsedHours > 0.001f) {
    projected8hr = (float)(doseAccum * (8.0 / (double)elapsedHours) * 100.0);
  }

  // --- Per-band levels ---
  float bandLvls[N_CHAN];
  dspGetBandLevels(bandLvls);

  // --- Assemble snapshot ---
  latest.timestampMs    = nowMs;
  latest.splFast_dBA    = splFast;
  latest.splSlow_dBA    = splSlow;
  latest.peakSPL_dBA    = peakSPL;
  latest.noiseDosePct   = (float)(doseAccum * 100.0);
  latest.projectedDose8hr = projected8hr;
  memcpy(latest.bandLevels, bandLvls, sizeof(latest.bandLevels));
  latest.cpuPercent     = AudioProcessorUsage();
  latest.memUsed        = AudioMemoryUsage();
}

// ============================================================================
// ACCESS
// ============================================================================

MetricsSnapshot metricsGetSnapshot() {
  return latest;  // struct copy
}

float metricsGetSPLFast()   { return latest.splFast_dBA; }
float metricsGetSPLSlow()   { return latest.splSlow_dBA; }
float metricsGetLAeq1min()  { return latest.laeq1min_dBA; }
float metricsGetNoiseDose() { return latest.noiseDosePct; }

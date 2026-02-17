/*
 * ============================================================================
 * Metrics.h — Sound Level Metrics Engine
 * ============================================================================
 * Computes real-time noise exposure metrics from the A-weighted signal:
 *
 *   - SPL dBA (Fast: 125 ms, Slow: 1 s time constants)
 *   - LAeq,1min  — 1-minute equivalent continuous A-weighted level
 *   - LAeq,1hr   — 1-hour equivalent continuous A-weighted level
 *   - Lpeak      — peak SPL since last reset
 *   - Noise dose — % of NIOSH criterion (85 dBA / 8 hr / 3 dB exchange)
 *
 * Metrics are read by SDLogger for file writing and BLESync for streaming.
 *
 * IEC 61672 compliant time weighting (via Tympan AudioCalcLevel_F32).
 * ============================================================================
 */

#ifndef METRICS_H
#define METRICS_H

#include "Config.h"

// ============================================================================
// METRICS SNAPSHOT (immutable struct, safe to copy)
// ============================================================================

struct MetricsSnapshot {
  unsigned long timestampMs;      // millis() at capture
  float  splFast_dBA;             // current SPL, fast time weighting
  float  splSlow_dBA;             // current SPL, slow time weighting
  float  laeq1min_dBA;            // LAeq over last 1 minute
  float  laeq1hr_dBA;             // LAeq over last 1 hour
  float  peakSPL_dBA;             // peak since last reset
  float  noiseDosePct;            // cumulative noise dose (%)
  float  projectedDose8hr;        // projected 8-hr dose at current rate (%)
  float  bandLevels[N_CHAN];      // per-band levels (dB) from DSP chain
  float  cpuPercent;              // audio CPU usage
  int    memUsed;                 // audio memory blocks used
};

// ============================================================================
// LIFECYCLE
// ============================================================================

void metricsInit();
void metricsUpdate(unsigned long nowMs);   // call from loop(), ~every 100 ms
void metricsReset();                        // reset accumulators (dose, peak, etc.)

// ============================================================================
// ACCESS
// ============================================================================

MetricsSnapshot metricsGetSnapshot();       // thread-safe copy of latest
float metricsGetSPLFast();
float metricsGetSPLSlow();
float metricsGetLAeq1min();
float metricsGetNoiseDose();

#endif // METRICS_H

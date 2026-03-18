/*
 * ============================================================================
 * Config.h — Master Configuration
 * ============================================================================
 * All hardware pins, DSP parameters, feature flags, and system constants.
 * Edit this file to reconfigure the entire system.
 *
 * CHANGELOG (v2.1):
 *   - Sample rate: 24000 → 44100 Hz (better filter resolution)
 *   - Block size: 32 → 128 (standard, stable with biquad filterbank)
 *   - Filterbank: FIR (N_FIR taps) → IIR biquad (AudioFilterbankBiquad_F32)
 *   - Expansion (noise gate): ENABLED by default
 *   - WDRC release: was 50 ms in original, now 100 ms
 *   - WDRC threshold: was 65 dB in original, now 45 dB
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Tympan_Library.h>

// ============================================================================
// HARDWARE REVISION
// ============================================================================
#define TYMPAN_REV TympanRev::F

// ============================================================================
// AUDIO ENGINE
// ============================================================================
constexpr float SAMPLE_RATE_HZ =
    44100.0f;                          // 44.1 kHz — better filter resolution
constexpr int APP_BLOCK_SAMPLES = 128; // standard block (stable with biquad FB)
constexpr int AUDIO_MEM_I16 = 20;      // Teensy integer audio blocks
constexpr int AUDIO_MEM_F32 = 90; // F32 processing blocks (more for biquad FB)

// ============================================================================
// INPUT CONFIGURATION
// ============================================================================
#define DEFAULT_INPUT_SOURCE TYMPAN_INPUT_JACK_AS_LINEIN
constexpr float DEFAULT_INPUT_GAIN_DB = 3.75f;
constexpr float DEFAULT_HEADPHONE_DB = 0.0f;

// ============================================================================
// Level calibration
// ============================================================================

// Signal level at 0 dB input gain (line-in), 94 dB SPL = -XX.XX dB FS
// ref:
// https://github.com/Tympan/Tympan_Library/blob/a4f4238e78f8e8f6261422efb41eade14847a19b/examples/02-Utility/SoundLevelMeter/SoundLevelMeter.ino#L44
// --> adjust later accoding to actual measurements
// constexpr float MIC_CAL_DBFS_AT_94SPL = -47.4f + 9.2175f;
constexpr float MIC_CAL_DBFS_AT_94SPL = -16.0f;
constexpr float CAL_OFFSET_DB =
    -MIC_CAL_DBFS_AT_94SPL + 94.0f - DEFAULT_INPUT_GAIN_DB;
// constexpr float MAX_DB_SPL =
//     CAL_OFFSET_DB + 0.0f; // dB SPL equivalent of 0 dB FS
constexpr float MAX_DB_SPL = 101.35;

// ============================================================================
// DSP — FILTERBANK (IIR BIQUAD)
// ============================================================================
#define N_CHAN 8           // number of frequency bands
#define FILTERBANK_ORDER 6 // biquad cascade order (3rd-order per slope)

// 7 crossover frequencies → 8 bands
// Bands: [<200] [200-500] [500-1k] [1k-1.5k] [1.5k-2.5k] [2.5k-4k] [4k-6.5k]
// [>6.5k]
constexpr float CROSSOVER_FREQS[N_CHAN - 1] = {
    200.0f, 500.0f, 1000.0f, 1500.0f, 2500.0f, 4000.0f, 6500.0f};

// Approximate geometric centre of each band (for NAL-R interpolation)
constexpr float BAND_CENTERS[N_CHAN] = {125.0f,  350.0f,  750.0f,  1250.0f,
                                        2000.0f, 3250.0f, 5250.0f, 7500.0f};

// ============================================================================
// DSP — NAL-R PRESCRIPTION
// ============================================================================
constexpr float NALR_GAIN_FLOOR = 0.0f;
constexpr float NALR_GAIN_CEILING = 40.0f;

// Standard audiometric frequencies (matches NFC card data layout)
constexpr uint16_t AUDIOGRAM_FREQS[11] = {125,  250,  500,  750,  1000, 1500,
                                          2000, 3000, 4000, 6000, 8000};
#define N_AUDIO_FREQS 11

// ============================================================================
// DSP — WDRC COMPRESSOR DEFAULTS
// ============================================================================

// Per-band compressor
constexpr float WDRC_ATTACK_MS = 5.0f;
constexpr float WDRC_RELEASE_MS = 100.0f; // smoother than 50 ms
constexpr float WDRC_MAXDB = MAX_DB_SPL;

// Expansion (noise gate) — suppresses codec noise floor
constexpr float WDRC_EXP_CR = 2.0f;        // expansion ratio (2:1 below knee)
constexpr float WDRC_EXP_END_KNEE = 10.0f; // below 35 dB SPL → attenuate

constexpr float WDRC_CR_ACTIVE = 2.0f;  // compression ratio when active
constexpr float WDRC_CR_BYPASS = 1.0f;  // unity (linear)
constexpr float WDRC_TK_ACTIVE = 75.0f; // compression knee (dB SPL)
constexpr float WDRC_TK_BYPASS = 75.0f;
// BOLT stands for broadband output limiting threshold, i.e. the effective
// ceiling of the compressor output
constexpr float WDRC_BOLT_ACTIVE = 95.0f; // per-band output limit
constexpr float WDRC_BOLT_BYPASS = 95.0f;

// dB FS equivalents
constexpr float WDRC_TK_ACTIVE_DBFS = WDRC_TK_ACTIVE - CAL_OFFSET_DB;
constexpr float WDRC_BOLT_ACTIVE_DBFS = WDRC_BOLT_ACTIVE - CAL_OFFSET_DB;
constexpr float WDRC_EXP_END_KNEE_DBFS = WDRC_EXP_END_KNEE - CAL_OFFSET_DB;

// Broadband output limiter (safety ceiling — always on)
constexpr float BB_ATTACK_MS = 1.0f;
constexpr float BB_RELEASE_MS = 50.0f;
constexpr float BB_MAXDB =
    MAX_DB_SPL; // absolute ceiling (dB SPL equivalent of 0 dB FS)
constexpr float BB_CR = 1.0f;
constexpr float BB_TK = 24.0f;
constexpr float BB_BOLT = 100.0f;

// ============================================================================
// DSP — ALGORITHM FEATURE FLAGS  (runtime-toggleable via serial/BLE)
// ============================================================================
struct DSPFlags {
  bool nalrEnabled;      // NAL-R insertion gains
  bool wdrcEnabled;      // per-band compression
  bool limiterEnabled;   // broadband output limiter
  bool preGainEnabled;   // input pre-gain stage
  bool expansionEnabled; // noise gate (expansion below threshold)

  DSPFlags()
      : nalrEnabled(true), wdrcEnabled(false), limiterEnabled(true),
        preGainEnabled(true), expansionEnabled(true) {}
};

#define ENABLE_NOISE_REDUCTION

// ============================================================================
// NFC — PN532 on Wire2
// ============================================================================
#define NFC_IRQ_PIN (-1)
#define NFC_RESET_PIN (-1)
#define NFC_POLL_COOLDOWN_MS 2000
#define NFC_READ_TIMEOUT_MS 50

#define NFC_DATA_BYTES 22
#define NFC_START_PAGE 4
#define NFC_END_PAGE 9

// ============================================================================
// OLED — SSD1306 on Wire2
// ============================================================================
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET_PIN (-1)
#define OLED_I2C_ADDR 0x3C
#define OLED_REFRESH_MS 100

// ============================================================================
// I2C BUS
// ============================================================================
#define I2C_BUS Wire2
#define I2C_CLOCK_HZ 100000

// ============================================================================
// SD CARD LOGGING
// ============================================================================
#define SD_LOG_INTERVAL_MS 1000
#define SD_PURGE_AGE_MS 86400000UL
#define SD_PURGE_CHECK_MS 60000UL
#define SD_MAX_FILENAME 32
#define SD_LOG_DIR "/logs"

// ============================================================================
// BLUETOOTH (Tympan built-in BLE)
// ============================================================================
#define BLE_ADVERTISE_MS 50000
#define BLE_METRICS_SEND_MS 20000

#define BLE_PREFIX_METRICS "M:"
#define BLE_PREFIX_SYNC "S:"
#define BLE_PREFIX_CMD "C:"
#define BLE_PREFIX_USER "U:"

// ============================================================================
// SOUND LEVEL METRICS
// ============================================================================

constexpr float TIME_CONST_FAST_SEC = 0.125f;
constexpr float TIME_CONST_SLOW_SEC = 1.0f;

constexpr unsigned long LAEQ_SHORT_MS = 60000UL;
constexpr unsigned long LAEQ_LONG_MS = 3600000UL;

constexpr float DOSE_CRITERION_DB = 85.0f;
constexpr float DOSE_CRITERION_HR = 8.0f;
constexpr float DOSE_EXCHANGE_RATE = 3.0f;

// ============================================================================
// SERIAL
// ============================================================================
#define SERIAL_STATS_INTERVAL_MS 60000
#define POT_SERVICE_INTERVAL_MS 100

#endif // CONFIG_H
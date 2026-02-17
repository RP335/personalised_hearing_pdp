/*
 * ============================================================================
 * Config.h — Master Configuration
 * ============================================================================
 * All hardware pins, DSP parameters, feature flags, and system constants.
 * Edit this file to reconfigure the entire system.
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
constexpr float SAMPLE_RATE_HZ = 24000.0f; // 24 kHz — low-latency, CPU-friendly
constexpr int APP_BLOCK_SAMPLES = 32; // small block → low latency (~1.3 ms)
constexpr int AUDIO_MEM_I16 = 10;     // Teensy integer audio blocks
constexpr int AUDIO_MEM_F32 = 60;     // F32 processing blocks

// ============================================================================
// INPUT CONFIGURATION
// ============================================================================
// Select ONE active input source:
//   TYMPAN_INPUT_JACK_AS_LINEIN   — 3.5 mm jack, line level (comms controller)
//   TYMPAN_INPUT_ON_BOARD_MIC     — MEMS microphones on Tympan PCB
//   TYMPAN_INPUT_JACK_AS_MIC      — 3.5 mm jack, mic level + bias
#define DEFAULT_INPUT_SOURCE TYMPAN_INPUT_JACK_AS_LINEIN
constexpr float DEFAULT_INPUT_GAIN_DB = 15.0f; // ADC PGA gain (0–47.5 dB)
constexpr float DEFAULT_HEADPHONE_DB = 0.0f;   // headphone amp initial volume

// ============================================================================
// DSP — FILTERBANK
// ============================================================================
#define N_CHAN 8 // number of frequency bands
#define N_FIR 96 // FIR filter taps per band

// 7 crossover frequencies → 8 bands
// Bands: [0–200] [200–400] [400–800] [800–1.5k] [1.5k–2.5k] [2.5k–4k] [4k–6.5k]
// [6.5k–12k]
constexpr float CROSSOVER_FREQS[N_CHAN - 1] = {
    200.0f, 400.0f, 800.0f, 1500.0f, 2500.0f, 4000.0f, 6500.0f};

// Approximate geometric centre of each band (for NAL-R interpolation)
constexpr float BAND_CENTERS[N_CHAN] = {125.0f,  300.0f,  600.0f,  1150.0f,
                                        2000.0f, 3250.0f, 5250.0f, 7500.0f};

// ============================================================================
// DSP — NAL-R PRESCRIPTION
// ============================================================================
constexpr float NALR_GAIN_FLOOR = 0.0f;    // minimum insertion gain (dB)
constexpr float NALR_GAIN_CEILING = 40.0f; // maximum insertion gain (dB)

// Standard audiometric frequencies (matches NFC card data layout)
constexpr uint16_t AUDIOGRAM_FREQS[11] = {125,  250,  500,  750,  1000, 1500,
                                          2000, 3000, 4000, 6000, 8000};
#define N_AUDIO_FREQS 11

// ============================================================================
// DSP — WDRC COMPRESSOR DEFAULTS
// ============================================================================
// Per-band compressor (when NAL-R is active)
constexpr float WDRC_ATTACK_MS = 5.0f;
constexpr float WDRC_RELEASE_MS = 50.0f;
constexpr float WDRC_MAXDB = 119.0f;
constexpr float WDRC_EXP_CR = 1.0f; // expansion ratio (1 = off)
constexpr float WDRC_EXP_END_KNEE = 0.0f;
constexpr float WDRC_CR_ACTIVE = 2.0f;  // compression ratio when active
constexpr float WDRC_CR_BYPASS = 1.0f;  // unity (linear) when bypassed
constexpr float WDRC_TK_ACTIVE = 40.0f; // compression knee (dB SPL)
constexpr float WDRC_TK_BYPASS = 55.0f;
constexpr float WDRC_BOLT_ACTIVE = 95.0f; // per-band output limit (dB SPL)
constexpr float WDRC_BOLT_BYPASS = 90.0f;

// Broadband output limiter (safety ceiling — always on)
constexpr float BB_ATTACK_MS = 1.0f; // fast attack for brick-wall limiting
constexpr float BB_RELEASE_MS = 50.0f;
constexpr float BB_MAXDB = 119.0f;
constexpr float BB_CR = 1.0f;    // linear below knee
constexpr float BB_TK = 24.0f;   // limiter knee
constexpr float BB_BOLT = 91.0f; // absolute output ceiling (dB SPL)

// ============================================================================
// DSP — ALGORITHM FEATURE FLAGS  (runtime-toggleable via serial/BLE)
// ============================================================================
struct DSPFlags {
  bool nalrEnabled;    // NAL-R insertion gains
  bool wdrcEnabled;    // per-band compression
  bool limiterEnabled; // broadband output limiter
  bool preGainEnabled; // input pre-gain stage

  DSPFlags()
      : nalrEnabled(true), wdrcEnabled(false), limiterEnabled(false),
        preGainEnabled(true) {}
};

// ============================================================================
// NFC — PN532 on Wire2
// ============================================================================
#define NFC_IRQ_PIN (-1)
#define NFC_RESET_PIN (-1)
#define NFC_POLL_COOLDOWN_MS 2000 // debounce between card taps
#define NFC_READ_TIMEOUT_MS 50    // readPassiveTargetID timeout

// NFC card format: NTAG213 pages 4–9, 22 bytes
// Bytes 0–10:  left ear thresholds  (int8, dB HL) at 11 frequencies
// Bytes 11–21: right ear thresholds (int8, dB HL) at 11 frequencies
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
#define OLED_REFRESH_MS 100 // ~10 FPS

// ============================================================================
// I2C BUS
// ============================================================================
#define I2C_BUS Wire2       // NFC + OLED share Wire2
#define I2C_CLOCK_HZ 100000 // 100 kHz (safe for breadboard)
// Bump to 400000 after soldering

// ============================================================================
// SD CARD LOGGING
// ============================================================================
#define SD_LOG_INTERVAL_MS 1000    // write one metrics row every 1 s
#define SD_PURGE_AGE_MS 86400000UL // 24 hours in milliseconds
#define SD_PURGE_CHECK_MS 60000UL  // check for old files every 60 s
#define SD_MAX_FILENAME 32
#define SD_LOG_DIR "/logs"

// ============================================================================
// BLUETOOTH (Tympan built-in BLE)
// ============================================================================
#define BLE_ADVERTISE_MS 5000    // re-check advertising interval
#define BLE_METRICS_SEND_MS 2000 // push metrics to app/web every 2 s

// BLE protocol prefixes
#define BLE_PREFIX_METRICS "M:" // M:{json}
#define BLE_PREFIX_SYNC "S:"    // S:START / S:DATA:... / S:END
#define BLE_PREFIX_CMD "C:"     // C:command
#define BLE_PREFIX_USER "U:"    // U:user_uid_hex

// ============================================================================
// SOUND LEVEL METRICS
// ============================================================================
// Microphone calibration (dBFS → dB SPL)
// Adjust for your specific mic + gain chain.
// Formula: SPL = level_dBFS + CAL_OFFSET
// CAL_OFFSET = -mic_cal_dBFS_at94dBSPL_at_0dB_gain + 94 - input_gain_dB
constexpr float MIC_CAL_DBFS_AT_94SPL = -47.4f + 9.2175f; // PCB mic baseline
constexpr float CAL_OFFSET_DB =
    -MIC_CAL_DBFS_AT_94SPL + 94.0f - DEFAULT_INPUT_GAIN_DB;

// Time constants (IEC 61672)
constexpr float TIME_CONST_FAST_SEC = 0.125f; // 125 ms
constexpr float TIME_CONST_SLOW_SEC = 1.0f;   // 1000 ms

// LAeq integration periods
constexpr unsigned long LAEQ_SHORT_MS = 60000UL;  //  1 min
constexpr unsigned long LAEQ_LONG_MS = 3600000UL; // 60 min

// Noise dose reference (NIOSH criterion)
constexpr float DOSE_CRITERION_DB = 85.0f; // 85 dBA
constexpr float DOSE_CRITERION_HR = 8.0f;  // 8 hours
constexpr float DOSE_EXCHANGE_RATE = 3.0f; // 3 dB exchange rate

// ============================================================================
// SERIAL
// ============================================================================
#define SERIAL_STATS_INTERVAL_MS 3000
#define POT_SERVICE_INTERVAL_MS 100

#endif // CONFIG_H

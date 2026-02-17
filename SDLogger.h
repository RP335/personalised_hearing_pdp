/*
 * ============================================================================
 * SDLogger.h — SD Card Metrics Logger
 * ============================================================================
 * Periodically writes noise exposure metrics to CSV files on the SD card.
 *
 * File structure:
 *   /logs/<USER_UID>_<bootID>.csv
 *
 * CSV columns:
 *   timestamp_ms, spl_fast, spl_slow, laeq_1min, laeq_1hr, peak,
 *   dose_pct, dose_8hr_proj, band0..band7, cpu, mem
 *
 * Features:
 *   - Auto-creates /logs directory
 *   - New file per user + boot session
 *   - Auto-purge files older than 24 hours (configurable)
 *   - Raw audio recording toggle (via AudioSDWriter)
 *   - File listing for BLE sync
 * ============================================================================
 */

#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include "Config.h"
#include "Metrics.h"

// ============================================================================
// LIFECYCLE
// ============================================================================

void sdLoggerInit();                           // call from setup()
void sdLoggerService(unsigned long nowMs);     // call from loop()

// ============================================================================
// CONTROL
// ============================================================================

void sdLoggerStart(const char *userUID);       // begin logging for a user
void sdLoggerStop();                           // stop logging
bool sdLoggerIsActive();

// ============================================================================
// RAW AUDIO RECORDING (uses AudioSDWriter)
// ============================================================================

void sdRawRecordStart();
void sdRawRecordStop();
bool sdRawRecordIsActive();

// ============================================================================
// FILE ACCESS (for BLE sync)
// ============================================================================

// Get number of log files on SD
int  sdLoggerGetFileCount();

// Get filename at index (for enumeration during BLE sync)
bool sdLoggerGetFileName(int index, char *buf, int bufLen);

// Read a chunk of a file (for BLE transfer)
// Returns bytes read, 0 on EOF, -1 on error.
int  sdLoggerReadFileChunk(const char *filename, uint32_t offset,
                            uint8_t *buf, int maxLen);

// ============================================================================
// PURGE
// ============================================================================

// Delete log files older than SD_PURGE_AGE_MS
void sdLoggerPurgeOld(unsigned long nowMs);

#endif // SD_LOGGER_H

/*
 * ============================================================================
 * SDLogger.cpp — SD Card Metrics Logger Implementation
 * ============================================================================
 */

#include "SDLogger.h"
#include "AudioGraph.h"
#include "Metrics.h"
#include <SD.h>
#include <TimeLib.h>

// ============================================================================
// STATE
// ============================================================================

static bool sdAvailable = false;
static bool logging = false;
static File logFile;
static char logFilename[SD_MAX_FILENAME + 16];
static unsigned long lastLogWrite = 0;
static unsigned long lastPurgeCheck = 0;
static uint32_t bootID = 0;

// Track files for enumeration
#define MAX_LOG_FILES 64
static char fileList[MAX_LOG_FILES][SD_MAX_FILENAME];
static int fileCount = 0;

// ============================================================================
// HELPERS
// ============================================================================

static void ensureLogDir() {
  if (!SD.exists(SD_LOG_DIR)) {
    SD.mkdir(SD_LOG_DIR);
    myTympan.println("[SD] Created " SD_LOG_DIR);
  }
}

static void writeCSVHeader(File &f) {
  f.println("timestamp_ms,spl_fast_dBA,spl_slow_dBA,laeq_1min,laeq_1hr,"
            "peak_dBA,dose_pct,dose_8hr_proj,"
            "band0,band1,band2,band3,band4,band5,band6,band7,"
            "cpu_pct,mem_blocks");
}

static void writeCSVRow(File &f, const MetricsSnapshot &m) {
  if (timeStatus() == timeSet) {
    f.print(now()); // Unix timestamp if time is set
  } else {
    f.print(m.timestampMs); // Fallback to millis
  }
  f.print(',');
  f.print(m.splFast_dBA, 1);
  f.print(',');
  f.print(m.splSlow_dBA, 1);
  f.print(',');
  f.print(m.laeq1min_dBA, 1);
  f.print(',');
  f.print(m.laeq1hr_dBA, 1);
  f.print(',');
  f.print(m.peakSPL_dBA, 1);
  f.print(',');
  f.print(m.noiseDosePct, 2);
  f.print(',');
  f.print(m.projectedDose8hr, 1);
  f.print(',');
  for (int i = 0; i < N_CHAN; i++) {
    f.print(m.bandLevels[i], 1);
    f.print(',');
  }
  f.print(m.cpuPercent, 1);
  f.print(',');
  f.println(m.memUsed);
}

static void refreshFileList() {
  fileCount = 0;
  File dir = SD.open(SD_LOG_DIR);
  if (!dir)
    return;

  while (fileCount < MAX_LOG_FILES) {
    File entry = dir.openNextFile();
    if (!entry)
      break;
    if (!entry.isDirectory()) {
      strncpy(fileList[fileCount], entry.name(), SD_MAX_FILENAME - 1);
      fileList[fileCount][SD_MAX_FILENAME - 1] = '\0';
      fileCount++;
    }
    entry.close();
  }
  dir.close();
}

// ============================================================================
// INIT
// ============================================================================

void sdLoggerInit() {
  // Teensy 4.1 built-in SD — typically initialised by Tympan library.
  // We just check if it's accessible.
  if (SD.begin(BUILTIN_SDCARD)) { // uses built-in SDIO on Teensy 4.1
    sdAvailable = true;
    ensureLogDir();
    refreshFileList();
    myTympan.print("[SD] Card OK, ");
    myTympan.print(fileCount);
    myTympan.println(" log files found");
  } else {
    sdAvailable = false;
    myTympan.println("[SD] *** Card not found *** (logging disabled)");
  }

  // Generate a simple boot ID from millis
  bootID = millis() & 0xFFFF;
}

// ============================================================================
// START / STOP
// ============================================================================

void sdLoggerStart(const char *userUID) {
  if (!sdAvailable) {
    myTympan.println("[SD] Cannot start — no card");
    return;
  }
  if (logging)
    sdLoggerStop(); // close any open file first

  ensureLogDir();

  // Filename: /logs/AABBCCDD_B1234.csv
  snprintf(logFilename, sizeof(logFilename), SD_LOG_DIR "/%s_B%04X.csv",
           (userUID && userUID[0]) ? userUID : "UNKNOWN", (unsigned int)bootID);

  logFile = SD.open(logFilename, FILE_WRITE);
  if (!logFile) {
    myTympan.print("[SD] Failed to open ");
    myTympan.println(logFilename);
    return;
  }

  writeCSVHeader(logFile);
  logFile.flush();
  logging = true;

  myTympan.print("[SD] Logging started: ");
  myTympan.println(logFilename);
}

void sdLoggerStop() {
  if (logging && logFile) {
    logFile.flush();
    logFile.close();
    myTympan.println("[SD] Logging stopped");
  }
  logging = false;
}

bool sdLoggerIsActive() { return logging; }

// ============================================================================
// SERVICE (call from loop)
// ============================================================================

void sdLoggerService(unsigned long nowMs) {
  if (!sdAvailable)
    return;

  // --- Write metrics row ---
  if (logging && (nowMs - lastLogWrite >= SD_LOG_INTERVAL_MS)) {
    lastLogWrite = nowMs;
    MetricsSnapshot m = metricsGetSnapshot();
    writeCSVRow(logFile, m);

    // Flush periodically (every 10 rows) to avoid data loss
    static int rowsSinceFlush = 0;
    if (++rowsSinceFlush >= 10) {
      logFile.flush();
      rowsSinceFlush = 0;
    }
  }

  // --- Purge old files ---
  if (nowMs - lastPurgeCheck >= SD_PURGE_CHECK_MS) {
    lastPurgeCheck = nowMs;
    sdLoggerPurgeOld(nowMs);
  }
}

// ============================================================================
// RAW AUDIO RECORDING
// ============================================================================

void sdRawRecordStart() {
  sdWriter.startRecording();
  myTympan.println("[SD] Raw audio recording started");
}

void sdRawRecordStop() {
  sdWriter.stopRecording();
  myTympan.println("[SD] Raw audio recording stopped");
}

bool sdRawRecordIsActive() {
  return (sdWriter.getState() == AudioSDWriter::STATE::RECORDING);
}

// ============================================================================
// FILE ACCESS (for BLE sync)
// ============================================================================

int sdLoggerGetFileCount() {
  refreshFileList();
  return fileCount;
}

bool sdLoggerGetFileName(int index, char *buf, int bufLen) {
  if (index < 0 || index >= fileCount)
    return false;
  strncpy(buf, fileList[index], bufLen - 1);
  buf[bufLen - 1] = '\0';
  return true;
}

int sdLoggerReadFileChunk(const char *filename, uint32_t offset, uint8_t *buf,
                          int maxLen) {
  char fullPath[64];
  snprintf(fullPath, sizeof(fullPath), SD_LOG_DIR "/%s", filename);

  File f = SD.open(fullPath, FILE_READ);
  if (!f)
    return -1;

  if (!f.seek(offset)) {
    f.close();
    return -1;
  }

  int bytesRead = f.read(buf, maxLen);
  f.close();
  return bytesRead;
}

// ============================================================================
// PURGE OLD FILES
// ============================================================================
// Strategy: since we don't have RTC, we track creation time relative to
// boot using the bootID embedded in filenames. Files from previous boots
// are always candidates for purging if total file count exceeds threshold.
// Within current boot, we purge based on file age (creation millis stored
// in the first data row).

void sdLoggerPurgeOld(unsigned long nowMs) {
  // Simple strategy: if more than MAX_LOG_FILES/2 files, delete oldest ones
  refreshFileList();

  if (fileCount <= MAX_LOG_FILES / 2)
    return; // plenty of space

  // Delete excess files (oldest first — they appear first in directory listing)
  int toDelete = fileCount - (MAX_LOG_FILES / 2);
  for (int i = 0; i < toDelete && i < fileCount; i++) {
    char fullPath[64];
    snprintf(fullPath, sizeof(fullPath), SD_LOG_DIR "/%s", fileList[i]);

    // Don't delete the currently active log file
    if (logging && strcmp(fullPath, logFilename) == 0)
      continue;

    if (SD.remove(fullPath)) {
      myTympan.print("[SD] Purged: ");
      myTympan.println(fileList[i]);
    }
  }
  refreshFileList();
}

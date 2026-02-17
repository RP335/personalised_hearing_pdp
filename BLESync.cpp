/*
 * ============================================================================
 * BLESync.cpp — Bluetooth Data Sync Implementation
 * ============================================================================
 */

#include "BLESync.h"
#include "AudioGraph.h"
#include "DSPManager.h"
#include "Metrics.h"
#include "SDLogger.h"
#include "NFCReader.h"

// ============================================================================
// STATE
// ============================================================================

static BLE     *ble = nullptr;
static bool     metricsStreamEnabled = true;
static unsigned long lastMetricsSend = 0;

// File transfer state
static bool     transferActive = false;
static char     transferFilename[SD_MAX_FILENAME];
static uint32_t transferOffset = 0;

// Input buffer for commands
#define BLE_CMD_BUF_LEN 128
static char cmdBuf[BLE_CMD_BUF_LEN];
static int  cmdBufIdx = 0;

// ============================================================================
// HELPERS
// ============================================================================

static void sendBLE(const String &msg) {
  if (ble) ble->sendMessage(msg);
}

// Base64 encode a byte buffer (minimal implementation for BLE transfer)
static const char b64chars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String base64Encode(const uint8_t *data, int len) {
  String out;
  out.reserve((len * 4) / 3 + 4);
  for (int i = 0; i < len; i += 3) {
    uint32_t val = ((uint32_t)data[i]) << 16;
    if (i + 1 < len) val |= ((uint32_t)data[i + 1]) << 8;
    if (i + 2 < len) val |= data[i + 2];
    out += b64chars[(val >> 18) & 0x3F];
    out += b64chars[(val >> 12) & 0x3F];
    out += (i + 1 < len) ? b64chars[(val >> 6) & 0x3F] : '=';
    out += (i + 2 < len) ? b64chars[val & 0x3F] : '=';
  }
  return out;
}

// ============================================================================
// SEND METRICS JSON
// ============================================================================

static void sendMetricsJSON() {
  MetricsSnapshot m = metricsGetSnapshot();

  // Compact JSON — keep under BLE MTU
  String json = "{\"t\":";
  json += String(m.timestampMs);
  json += ",\"sf\":";
  json += String(m.splFast_dBA, 1);
  json += ",\"ss\":";
  json += String(m.splSlow_dBA, 1);
  json += ",\"eq\":";
  json += String(m.laeq1min_dBA, 1);
  json += ",\"pk\":";
  json += String(m.peakSPL_dBA, 1);
  json += ",\"d\":";
  json += String(m.noiseDosePct, 1);
  json += ",\"d8\":";
  json += String(m.projectedDose8hr, 1);
  json += ",\"cpu\":";
  json += String(m.cpuPercent, 0);
  json += ",\"uid\":\"";
  json += String(currentUserUID);
  json += "\",\"nalr\":";
  json += dspFlags.nalrEnabled ? "1" : "0";
  json += ",\"wdrc\":";
  json += dspFlags.wdrcEnabled ? "1" : "0";
  json += ",\"lim\":";
  json += dspFlags.limiterEnabled ? "1" : "0";
  json += "}";

  sendBLE(String(BLE_PREFIX_METRICS) + json);
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

static void processCommand(const char *cmd) {
  String c = String(cmd);
  c.trim();

  myTympan.print("[BLE] CMD: ");
  myTympan.println(c);

  if (c == "C:NALR") {
    dspToggleNALR();
  }
  else if (c == "C:WDRC") {
    dspToggleWDRC();
  }
  else if (c == "C:LIM") {
    dspToggleLimiter();
  }
  else if (c.startsWith("C:GAIN:")) {
    float delta = c.substring(7).toFloat();
    dspAdjustPreGain(delta);
  }
  else if (c == "C:BYPASS") {
    dspSetBypass();
  }
  else if (c == "C:STATUS") {
    // Send DSP status over BLE
    String status = "{\"profile\":";
    status += dspProfileLoaded ? "1" : "0";
    status += ",\"nalr\":";
    status += dspFlags.nalrEnabled ? "1" : "0";
    status += ",\"wdrc\":";
    status += dspFlags.wdrcEnabled ? "1" : "0";
    status += ",\"lim\":";
    status += dspFlags.limiterEnabled ? "1" : "0";
    status += ",\"gain\":";
    status += String(currentPreGainDB, 1);
    status += ",\"uid\":\"";
    status += currentUserUID;
    status += "\",\"bands\":[";
    for (int i = 0; i < N_CHAN; i++) {
      if (i > 0) status += ",";
      status += String(currentNALRGains[i], 1);
    }
    status += "]}";
    sendBLE("S:" + status);
  }
  else if (c == "C:FLIST") {
    // Send file list
    int count = sdLoggerGetFileCount();
    sendBLE("F:LIST:" + String(count));
    char fname[SD_MAX_FILENAME];
    for (int i = 0; i < count; i++) {
      if (sdLoggerGetFileName(i, fname, sizeof(fname))) {
        sendBLE("F:NAME:" + String(i) + ":" + String(fname));
      }
    }
  }
  else if (c.startsWith("C:FGET:")) {
    // Start file transfer
    String fname = c.substring(7);
    fname.toCharArray(transferFilename, sizeof(transferFilename));
    transferOffset = 0;
    transferActive = true;
    myTympan.print("[BLE] Starting file transfer: ");
    myTympan.println(transferFilename);
  }
  else if (c == "C:RESET") {
    metricsReset();
    sendBLE("S:{\"msg\":\"Metrics reset\"}");
  }
  else if (c == "C:STREAM:ON") {
    metricsStreamEnabled = true;
  }
  else if (c == "C:STREAM:OFF") {
    metricsStreamEnabled = false;
  }
  else {
    myTympan.print("[BLE] Unknown command: ");
    myTympan.println(c);
  }
}

// ============================================================================
// FILE TRANSFER SERVICE
// ============================================================================

static void serviceFileTransfer() {
  if (!transferActive) return;

  // Send one chunk per loop iteration (avoid blocking)
  uint8_t chunk[120];  // keep base64 output under ~160 bytes (BLE MTU safe)
  int bytesRead = sdLoggerReadFileChunk(transferFilename, transferOffset,
                                         chunk, sizeof(chunk));

  if (bytesRead > 0) {
    sendBLE("F:DATA:" + base64Encode(chunk, bytesRead));
    transferOffset += bytesRead;
  }

  if (bytesRead <= 0 || bytesRead < (int)sizeof(chunk)) {
    // Transfer complete
    sendBLE("F:END");
    transferActive = false;
    myTympan.print("[BLE] Transfer complete: ");
    myTympan.print(transferOffset);
    myTympan.println(" bytes");
  }
}

// ============================================================================
// INIT
// ============================================================================

void bleSyncInit() {
  ble = &myTympan.getBLE();
  delay(500);
  myTympan.setupBLE();
  delay(500);
  myTympan.println("[BLE] Initialised");
}

// ============================================================================
// SERVICE (call from loop)
// ============================================================================

void bleSyncService(unsigned long nowMs) {
  if (!ble) return;

  // --- Advertising keepalive ---
  ble->updateAdvertising(nowMs, BLE_ADVERTISE_MS);

  // --- Receive commands ---
  if (ble->available() > 0) {
    String msg;
    int len = ble->recvBLE(&msg);
    for (int i = 0; i < len; i++) {
      char ch = msg[i];
      if (ch == '\n' || ch == '\r') {
        if (cmdBufIdx > 0) {
          cmdBuf[cmdBufIdx] = '\0';
          processCommand(cmdBuf);
          cmdBufIdx = 0;
        }
      } else if (cmdBufIdx < BLE_CMD_BUF_LEN - 1) {
        cmdBuf[cmdBufIdx++] = ch;
      }
    }
    // If no newline but buffer has content, process it
    if (cmdBufIdx > 0) {
      cmdBuf[cmdBufIdx] = '\0';
      processCommand(cmdBuf);
      cmdBufIdx = 0;
    }
  }

  // --- Stream metrics ---
  if (metricsStreamEnabled && (nowMs - lastMetricsSend >= BLE_METRICS_SEND_MS)) {
    lastMetricsSend = nowMs;
    sendMetricsJSON();
  }

  // --- File transfer ---
  serviceFileTransfer();
}

// ============================================================================
// CONTROL
// ============================================================================

void bleSyncSetMetricsStream(bool enable) {
  metricsStreamEnabled = enable;
}

bool bleSyncIsStreaming() {
  return metricsStreamEnabled;
}

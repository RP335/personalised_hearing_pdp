// /*
//  * ============================================================================
//  * BLESync.cpp — Bluetooth Data Sync Implementation
//  * ============================================================================
//  */

// #include "BLESync.h"
// #include "AudioGraph.h"
// #include "DSPManager.h"
// #include "Metrics.h"
// #include "NFCReader.h"
// #include "SDLogger.h"

// //
// ============================================================================
// // STATE
// //
// ============================================================================

// static BLE *ble = nullptr;
// static bool metricsStreamEnabled = true;
// static unsigned long lastMetricsSend = 0;

// // File transfer state
// static bool transferActive = false;
// static char transferFilename[SD_MAX_FILENAME];
// static uint32_t transferOffset = 0;

// // Input buffer for commands
// #define BLE_CMD_BUF_LEN 128
// static char cmdBuf[BLE_CMD_BUF_LEN];
// static int cmdBufIdx = 0;

// //
// ============================================================================
// // HELPERS
// //
// ============================================================================

// static void sendBLE(const String &msg) {
//   if (ble)
//     ble->sendMessage(msg + "\n");
// }

// // Base64 encode a byte buffer (minimal implementation for BLE transfer)
// static const char b64chars[] =
//     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// static String base64Encode(const uint8_t *data, int len) {
//   String out;
//   out.reserve((len * 4) / 3 + 4);
//   for (int i = 0; i < len; i += 3) {
//     uint32_t val = ((uint32_t)data[i]) << 16;
//     if (i + 1 < len)
//       val |= ((uint32_t)data[i + 1]) << 8;
//     if (i + 2 < len)
//       val |= data[i + 2];
//     out += b64chars[(val >> 18) & 0x3F];
//     out += b64chars[(val >> 12) & 0x3F];
//     out += (i + 1 < len) ? b64chars[(val >> 6) & 0x3F] : '=';
//     out += (i + 2 < len) ? b64chars[val & 0x3F] : '=';
//   }
//   return out;
// }

// //
// ============================================================================
// // SEND METRICS JSON
// //
// ============================================================================

// static void sendMetricsJSON() {
//   MetricsSnapshot m = metricsGetSnapshot();

//   // Compact JSON — keep under BLE MTU
//   String json = "{\"t\":";
//   json += String(m.timestampMs);
//   json += ",\"sf\":";
//   json += String(m.splFast_dBA, 1);
//   json += ",\"ss\":";
//   json += String(m.splSlow_dBA, 1);
//   json += ",\"eq\":";
//   json += String(m.laeq1min_dBA, 1);
//   json += ",\"pk\":";
//   json += String(m.peakSPL_dBA, 1);
//   json += ",\"d\":";
//   json += String(m.noiseDosePct, 1);
//   json += ",\"d8\":";
//   json += String(m.projectedDose8hr, 1);
//   json += ",\"cpu\":";
//   json += String(m.cpuPercent, 0);
//   json += ",\"uid\":\"";
//   json += String(currentUserUID);
//   json += "\",\"nalr\":";
//   json += dspFlags.nalrEnabled ? "1" : "0";
//   json += ",\"wdrc\":";
//   json += dspFlags.wdrcEnabled ? "1" : "0";
//   json += ",\"lim\":";
//   json += dspFlags.limiterEnabled ? "1" : "0";
//   json += "}";

//   sendBLE(String(BLE_PREFIX_METRICS) + json);
// }

// //
// ============================================================================
// // COMMAND PROCESSING
// //
// ============================================================================

// static void processCommand(const char *cmd) {
//   String c = String(cmd);
//   c.trim();

//   myTympan.print("[BLE] CMD: ");
//   myTympan.println(c);

//   if (c == "C:NALR") {
//     dspToggleNALR();
//   } else if (c == "C:WDRC") {
//     dspToggleWDRC();
//   } else if (c == "C:LIM") {
//     dspToggleLimiter();
//   } else if (c.startsWith("C:GAIN:")) {
//     float delta = c.substring(7).toFloat();
//     dspAdjustPreGain(delta);
//   } else if (c == "C:BYPASS") {
//     dspSetBypass();
//   } else if (c == "C:STATUS") {
//     // Send DSP status over BLE
//     String status = "{\"profile\":";
//     status += dspProfileLoaded ? "1" : "0";
//     status += ",\"nalr\":";
//     status += dspFlags.nalrEnabled ? "1" : "0";
//     status += ",\"wdrc\":";
//     status += dspFlags.wdrcEnabled ? "1" : "0";
//     status += ",\"lim\":";
//     status += dspFlags.limiterEnabled ? "1" : "0";
//     status += ",\"gain\":";
//     status += String(currentPreGainDB, 1);
//     status += ",\"uid\":\"";
//     status += currentUserUID;
//     status += "\",\"bands\":[";
//     for (int i = 0; i < N_CHAN; i++) {
//       if (i > 0)
//         status += ",";
//       status += String(currentNALRGains[i], 1);
//     }
//     status += "]}";
//     sendBLE("S:" + status);
//   } else if (c == "C:FLIST") {
//     // Send file list
//     int count = sdLoggerGetFileCount();
//     sendBLE("F:LIST:" + String(count));
//     char fname[SD_MAX_FILENAME];
//     for (int i = 0; i < count; i++) {
//       if (sdLoggerGetFileName(i, fname, sizeof(fname))) {
//         sendBLE("F:NAME:" + String(i) + ":" + String(fname));
//       }
//     }
//   } else if (c.startsWith("C:FGET:")) {
//     // Start file transfer
//     String fname = c.substring(7);
//     fname.toCharArray(transferFilename, sizeof(transferFilename));
//     transferOffset = 0;
//     transferActive = true;
//     myTympan.print("[BLE] Starting file transfer: ");
//     myTympan.println(transferFilename);
//   } else if (c == "C:RESET") {
//     metricsReset();
//     sendBLE("S:{\"msg\":\"Metrics reset\"}");
//   } else if (c == "C:STREAM:ON") {
//     metricsStreamEnabled = true;
//   } else if (c == "C:STREAM:OFF") {
//     metricsStreamEnabled = false;
//   } else {
//     myTympan.print("[BLE] Unknown command: ");
//     myTympan.println(c);
//   }
// }

// //
// ============================================================================
// // FILE TRANSFER SERVICE
// //
// ============================================================================

// static void serviceFileTransfer() {
//   if (!transferActive)
//     return;

//   // Send one chunk per loop iteration (avoid blocking)
//   uint8_t chunk[120]; // keep base64 output under ~160 bytes (BLE MTU safe)
//   int bytesRead = sdLoggerReadFileChunk(transferFilename, transferOffset,
//   chunk,
//                                         sizeof(chunk));

//   if (bytesRead > 0) {
//     sendBLE("F:DATA:" + base64Encode(chunk, bytesRead));
//     transferOffset += bytesRead;
//   }

//   if (bytesRead <= 0 || bytesRead < (int)sizeof(chunk)) {
//     // Transfer complete
//     sendBLE("F:END");
//     transferActive = false;
//     myTympan.print("[BLE] Transfer complete: ");
//     myTympan.print(transferOffset);
//     myTympan.println(" bytes");
//   }
// }

// //
// ============================================================================
// // INIT
// //
// ============================================================================

// // COPY AND PASTE THIS INTO BLESync.cpp
// // REPLACES THE EXISTING bleSyncInit()

// void bleSyncInit() {
//   BLE_nRF52 &ble_nRF52 = myTympan.getBLE_nRF52();
//   ble = &ble_nRF52;

//   delay(1000);
//   myTympan.println("[BLE] MANUAL RESET: Sending AT+FACTORYRESET...");

//   // 1. DIRECT HARDWARE HACK: Open the port manually
//   // The nRF52 usually defaults to 9600 baud
//   Serial1.begin(9600);
//   delay(100);

//   // 2. Send the raw command to wipe the bluetooth module
//   // This bypasses the library wrapper
//   Serial1.println("AT+FACTORYRESET");

//   // 3. Wait for the chip to reboot and settle
//   delay(2000);

//   // 4. Now let the Tympan library take over normally
//   myTympan.println("[BLE] Booting nRF52 via Library...");
//   ble_nRF52.begin_basic();

//   // 5. Force Name Change
//   String devName = "Tympan-RevF";
//   myTympan.print("[BLE] Setting Name to: ");
//   myTympan.println(devName);

//   if (!ble_nRF52.setBleName(devName)) {
//     myTympan.println("[BLE] Name Set: FAILED (Module might be
//     unresponsive)");
//   } else {
//     myTympan.println("[BLE] Name Set: SUCCESS");
//   }

//   // 6. Start Advertising
//   ble_nRF52.enableAdvertising(true);

//   myTympan.println("[BLE] Initialised (Manual AT Mode)");
// }

// //
// ============================================================================
// // SERVICE (call from loop)
// //
// ============================================================================

// void bleSyncService(unsigned long nowMs) {
//   if (!ble)
//     return;

//   // --- Advertising keepalive ---
//   ble->updateAdvertising(nowMs, BLE_ADVERTISE_MS);

//   // --- Receive commands ---
//   if (ble->available() > 0) {
//     String msg;
//     int len = ble->recvBLE(&msg);
//     for (int i = 0; i < len; i++) {
//       char ch = msg[i];
//       if (ch == '\n' || ch == '\r') {
//         if (cmdBufIdx > 0) {
//           cmdBuf[cmdBufIdx] = '\0';
//           processCommand(cmdBuf);
//           cmdBufIdx = 0;
//         }
//       } else if (cmdBufIdx < BLE_CMD_BUF_LEN - 1) {
//         cmdBuf[cmdBufIdx++] = ch;
//       }
//     }
//     // If no newline but buffer has content, process it
//     if (cmdBufIdx > 0) {
//       cmdBuf[cmdBufIdx] = '\0';
//       processCommand(cmdBuf);
//       cmdBufIdx = 0;
//     }
//   }

//   // --- Stream metrics ---
//   if (metricsStreamEnabled &&
//       (nowMs - lastMetricsSend >= BLE_METRICS_SEND_MS)) {
//     lastMetricsSend = nowMs;
//     sendMetricsJSON();
//   }

//   // --- File transfer ---
//   serviceFileTransfer();
// }

// //
// ============================================================================
// // CONTROL
// //
// ============================================================================

// void bleSyncSetMetricsStream(bool enable) { metricsStreamEnabled = enable; }

// bool bleSyncIsStreaming() { return metricsStreamEnabled; }

#include "BLESync.h"
#include "AudioGraph.h"
#include "DSPManager.h"
#include "Metrics.h"
#include "NFCReader.h"
#include "SDLogger.h"
#include <TimeLib.h> // For RTC time syncing

// --- STATE ---
static bool metricsStreamEnabled = true;
static unsigned long lastMetricsSend = 0;

// File transfer state
static bool transferActive = false;
static char transferFilename[SD_MAX_FILENAME];
static uint32_t transferOffset = 0;

// --- HELPER: Base64 Encode ---
static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String base64Encode(const uint8_t *data, int len) {
  String out;
  out.reserve((len * 4) / 3 + 4);
  for (int i = 0; i < len; i += 3) {
    uint32_t val = ((uint32_t)data[i]) << 16;
    if (i + 1 < len)
      val |= ((uint32_t)data[i + 1]) << 8;
    if (i + 2 < len)
      val |= data[i + 2];
    out += b64chars[(val >> 18) & 0x3F];
    out += b64chars[(val >> 12) & 0x3F];
    out += (i + 1 < len) ? b64chars[(val >> 6) & 0x3F] : '=';
    out += (i + 2 < len) ? b64chars[val & 0x3F] : '=';
  }
  return out;
}

// --- HELPER: Send to USB instead of BLE ---
static void sendBLE(const String &msg) {
  // DIRECT TO USB SERIAL
  Serial.println(msg);
}

// --- INIT: Do almost nothing (Skip the broken radio) ---
void bleSyncInit() {
  // We just use the standard Serial which is already started in setup()
  myTympan.println("[BLE] BYPASSED: Using USB Serial for Dashboard instead.");
}

// --- METRICS JSON ---
static void sendMetricsJSON() {
  MetricsSnapshot m = metricsGetSnapshot();

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

// --- FILE TRANSFER SERVICE ---
static void serviceFileTransfer() {
  if (!transferActive)
    return;

  // Send one chunk per loop iteration (avoid blocking)
  uint8_t chunk[240]; // Larger chunk for USB Serial (was 120 for BLE)
  int bytesRead = sdLoggerReadFileChunk(transferFilename, transferOffset, chunk,
                                        sizeof(chunk));

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

// --- COMMAND PROCESSING (Read from USB) ---
void processSerialCommand(String c) {
  c.trim();
  if (c.length() == 0)
    return;

  // Echo for debugging
  // Serial.print("[CMD] "); Serial.println(c);

  if (c == "C:NALR")
    dspToggleNALR();
  else if (c == "C:WDRC")
    dspToggleWDRC();
  else if (c == "C:LIM")
    dspToggleLimiter();
  else if (c.startsWith("C:GAIN:"))
    dspAdjustPreGain(c.substring(7).toFloat());
  else if (c == "C:BYPASS")
    dspSetBypass();
  else if (c == "C:RESET")
    metricsReset();

  // Status Request
  else if (c == "C:STATUS") {
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
      if (i > 0)
        status += ",";
      status += String(currentNALRGains[i], 1);
    }
    status += "]}";
    sendBLE("S:" + status);
  }
  // File Listing
  else if (c == "C:FLIST") {
    int count = sdLoggerGetFileCount();
    sendBLE("F:LIST:" + String(count));
    char fname[SD_MAX_FILENAME];
    for (int i = 0; i < count; i++) {
      if (sdLoggerGetFileName(i, fname, sizeof(fname))) {
        sendBLE("F:NAME:" + String(i) + ":" + String(fname));
      }
    }
  }
  // File Transfer
  else if (c.startsWith("C:FGET:")) {
    String fname = c.substring(7);
    fname.toCharArray(transferFilename, sizeof(transferFilename));
    transferOffset = 0;
    transferActive = true;
    myTympan.print("[BLE] Starting file transfer (Serial): ");
    myTympan.println(transferFilename);
  }
  // Time Sync: C:TIME:1739989900
  else if (c.startsWith("C:TIME:")) {
    unsigned long epoch = c.substring(7).toInt();
    if (epoch > 1000000000) {
      Teensy3Clock.set(epoch);
      setTime(epoch);
      myTympan.print("[BLE] Time set to: ");
      myTympan.println(epoch);
    }
  }
}

// --- SERVICE LOOP ---
void bleSyncService(unsigned long nowMs) {
  // 1. Check for incoming USB commands (instead of BLE)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processSerialCommand(cmd);
  }

  // 2. Stream Metrics to USB
  if (metricsStreamEnabled &&
      (nowMs - lastMetricsSend >= BLE_METRICS_SEND_MS)) {
    lastMetricsSend = nowMs;
    sendMetricsJSON();
  }

  // 3. Service File Transfer
  serviceFileTransfer();
}

// --- CONTROL STUBS ---
void bleSyncSetMetricsStream(bool enable) { metricsStreamEnabled = enable; }
bool bleSyncIsStreaming() { return metricsStreamEnabled; }

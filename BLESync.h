/*
 * ============================================================================
 * BLESync.h — Bluetooth Data Sync & Command Handler
 * ============================================================================
 * Handles:
 *   - Real-time metrics streaming to connected BLE client (web dashboard)
 *   - SD card file sync protocol (list files, transfer file data)
 *   - Remote command reception (toggle algorithms, adjust params)
 *
 * BLE Protocol (text-based, over Tympan BLE UART):
 *
 *   OUTBOUND (Tympan → Client):
 *     M:{json}         Live metrics snapshot
 *     F:LIST:n         File count response
 *     F:NAME:idx:name  File name response
 *     F:DATA:b64chunk  File data chunk (base64)
 *     F:END            End of file transfer
 *     S:{json}         DSP status
 *
 *   INBOUND (Client → Tympan):
 *     C:NALR            Toggle NAL-R
 *     C:WDRC            Toggle WDRC
 *     C:LIM             Toggle limiter
 *     C:GAIN:+3         Adjust pre-gain
 *     C:BYPASS          Set full bypass
 *     C:STATUS          Request DSP status
 *     C:FLIST           Request file list
 *     C:FGET:filename   Request file transfer
 *     C:RESET           Reset metrics
 * ============================================================================
 */

#ifndef BLE_SYNC_H
#define BLE_SYNC_H

#include "Config.h"

// ============================================================================
// LIFECYCLE
// ============================================================================

void bleSyncInit();                          // call from setup()
void bleSyncService(unsigned long nowMs);    // call from loop()

// ============================================================================
// CONTROL
// ============================================================================

// Enable/disable live metrics streaming
void bleSyncSetMetricsStream(bool enable);
bool bleSyncIsStreaming();

#endif // BLE_SYNC_H

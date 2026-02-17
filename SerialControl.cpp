/*
 * ============================================================================
 * SerialControl.cpp — Serial Command Interface Implementation
 * ============================================================================
 */

#include "SerialControl.h"
#include "AudioGraph.h"
#include "DSPManager.h"
#include "NFCReader.h"
#include "Metrics.h"
#include "SDLogger.h"
#include "BLESync.h"
#include "OLEDDisplay.h"

// ============================================================================
// INIT
// ============================================================================

void serialControlInit() {
  myTympan.beginBothSerial();
}

// ============================================================================
// HELP
// ============================================================================

void serialPrintHelp() {
  myTympan.println();
  myTympan.println("╔══════════════════════════════════════════════════╗");
  myTympan.println("║   PERSONALIZED HEARING PROTECTION — COMMANDS    ║");
  myTympan.println("╠══════════════════════════════════════════════════╣");
  myTympan.println("║  DSP CONTROL:                                   ║");
  myTympan.println("║    b   = toggle full bypass                     ║");
  myTympan.println("║    n   = toggle NAL-R gains                     ║");
  myTympan.println("║    w   = toggle WDRC compression                ║");
  myTympan.println("║    l   = toggle broadband limiter               ║");
  myTympan.println("║    +/- = pre-gain ±3 dB                        ║");
  myTympan.println("║  DISPLAY:                                       ║");
  myTympan.println("║    d   = cycle OLED display page                ║");
  myTympan.println("║  SD CARD:                                       ║");
  myTympan.println("║    a   = toggle raw audio recording             ║");
  myTympan.println("║    f   = list SD log files                      ║");
  myTympan.println("║  METRICS:                                       ║");
  myTympan.println("║    m   = print current metrics snapshot         ║");
  myTympan.println("║    0   = reset metrics accumulators             ║");
  myTympan.println("║  SYSTEM:                                        ║");
  myTympan.println("║    s   = print full DSP status                  ║");
  myTympan.println("║    r   = reset (wait for new NFC card)          ║");
  myTympan.println("║    h/? = this help                              ║");
  myTympan.println("║                                                 ║");
  myTympan.println("║  NFC: Tap card to load / re-tap to toggle!      ║");
  myTympan.println("╚══════════════════════════════════════════════════╝");
  myTympan.println();
}

// ============================================================================
// SERVICE
// ============================================================================

void serialControlService() {
  while (Serial.available()) {
    char c = Serial.read();

    switch (c) {
      // --- DSP ---
      case 'b':
        if (dspProfileLoaded) {
          // Toggle between bypass and active
          if (dspFlags.nalrEnabled || dspFlags.wdrcEnabled) {
            dspSetBypass();
            displayFlash("BYPASS", "Flat response");
          } else {
            dspApplyProfile(leftThresholds);
            displayFlash("ACTIVE", "NAL-R applied");
          }
        } else {
          myTympan.println("[SYS] No profile loaded — scan NFC card first");
        }
        break;

      case 'n':
        dspToggleNALR();
        displayFlash("NAL-R", dspFlags.nalrEnabled ? "ON" : "OFF");
        break;

      case 'w':
        dspToggleWDRC();
        displayFlash("WDRC", dspFlags.wdrcEnabled ? "ON" : "OFF");
        break;

      case 'l':
        dspToggleLimiter();
        displayFlash("LIMITER", dspFlags.limiterEnabled ? "ON" : "OFF");
        break;

      case '+':
        dspAdjustPreGain(3.0f);
        break;

      case '-':
        dspAdjustPreGain(-3.0f);
        break;

      // --- Display ---
      case 'd':
        displayNextPage();
        myTympan.print("[OLED] Page: ");
        myTympan.println((int)displayGetPage());
        break;

      // --- SD Card ---
      case 'a':
        if (sdRawRecordIsActive()) {
          sdRawRecordStop();
          displayFlash("RAW REC", "STOPPED");
        } else {
          sdRawRecordStart();
          displayFlash("RAW REC", "STARTED");
        }
        break;

      case 'f': {
        int count = sdLoggerGetFileCount();
        myTympan.print("[SD] Log files: ");
        myTympan.println(count);
        char fname[SD_MAX_FILENAME];
        for (int i = 0; i < count; i++) {
          if (sdLoggerGetFileName(i, fname, sizeof(fname))) {
            myTympan.print("  ");
            myTympan.println(fname);
          }
        }
        break;
      }

      // --- Metrics ---
      case 'm': {
        MetricsSnapshot ms = metricsGetSnapshot();
        myTympan.println("\n=== METRICS SNAPSHOT ===");
        myTympan.print("  SPL Fast:   "); myTympan.print(ms.splFast_dBA, 1);
        myTympan.println(" dBA");
        myTympan.print("  SPL Slow:   "); myTympan.print(ms.splSlow_dBA, 1);
        myTympan.println(" dBA");
        myTympan.print("  LAeq,1min:  "); myTympan.print(ms.laeq1min_dBA, 1);
        myTympan.println(" dBA");
        myTympan.print("  LAeq,1hr:   "); myTympan.print(ms.laeq1hr_dBA, 1);
        myTympan.println(" dBA");
        myTympan.print("  Peak:       "); myTympan.print(ms.peakSPL_dBA, 1);
        myTympan.println(" dBA");
        myTympan.print("  Dose:       "); myTympan.print(ms.noiseDosePct, 2);
        myTympan.println(" %");
        myTympan.print("  8hr Proj:   "); myTympan.print(ms.projectedDose8hr, 1);
        myTympan.println(" %");
        myTympan.print("  CPU: "); myTympan.print(ms.cpuPercent, 1);
        myTympan.print("% | Mem: "); myTympan.println(ms.memUsed);
        myTympan.println("========================\n");
        break;
      }

      case '0':
        metricsReset();
        displayFlash("METRICS", "RESET");
        break;

      // --- System ---
      case 's':
        dspPrintStatus();
        break;

      case 'r':
        dspSetBypass();
        displaySetPage(PAGE_WAITING);
        sdLoggerStop();
        myTympan.println("[SYS] Reset → waiting for new NFC card");
        displayFlash("RESET", "Scan new card");
        break;

      case 'h':
      case '?':
        serialPrintHelp();
        break;

      default:
        break;  // ignore newlines, etc.
    }
  }
}

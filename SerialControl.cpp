/*
 * ============================================================================
 * SerialControl.cpp — Serial Command Interface Implementation
 * ============================================================================
 */

#include "SerialControl.h"
#include "AudioGraph.h"
#include "BLESync.h"
#include "DSPManager.h"
#include "Metrics.h"
#include "NFCReader.h"
#include "OLEDDisplay.h"
#include "SDLogger.h"

// Forward declaration for BLESync.cpp function
extern void processSerialCommand(String c);

static void parseAudiogram(String data, int8_t *thresholds) {
  unsigned int idx = 0;
  unsigned int start = 0;
  while (idx < N_AUDIO_FREQS && start < data.length()) {
    while (start < data.length() && isSpace(data.charAt(start)))
      start++;
    if (start >= data.length())
      break;
    unsigned int end = start;
    while (end < data.length() && !isSpace(data.charAt(end)))
      end++;
    thresholds[idx] = data.substring(start, end).toInt();
    start = end;
    idx++;
  }
}

// ============================================================================
// INIT
// ============================================================================

void serialControlInit() { myTympan.beginBothSerial(); }

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
  myTympan.println("║    g <G> = set pre-gain (dB)                    ║");
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
  myTympan.println("║  PROTOTYPE:                                     ║");
  myTympan.println("║    L <v1>...<v11> = set left thresholds         ║");
  myTympan.println("║    R <v1>...<v11> = set right thresholds        ║");
  myTympan.println("║    p   = apply custom L/R profile               ║");
  myTympan.println("║                                                 ║");
  myTympan.println("║  NFC: Tap card to load / re-tap to toggle!      ║");
  myTympan.println("╚══════════════════════════════════════════════════╝");
  myTympan.println();
}

// ============================================================================
// SERVICE
// ============================================================================

void serialControlService() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      return;

    // Route dashboard commands
    if (line.startsWith("C:")) {
      processSerialCommand(line);
      return;
    }

    // Process our local shortcuts
    char c = line.charAt(0);
    switch (c) {
    // --- DSP ---
    case 'b':
      if (dspProfileLoaded) {
        // Toggle between bypass and active
        if (dspFlags.nalrEnabled || dspFlags.wdrcEnabled) {
          dspSetBypass();
          displayFlash("BYPASS", "Flat response");
        } else {
          dspApplyProfile(leftThresholds, rightThresholds);
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
    
    case 'g': {
      float g = line.substring(1).toFloat();
      dspSetPreGain(g);
      myTympan.print("[DSP] Pre-gain set to ");
      myTympan.print(g, 1);
      myTympan.println(" dB");
      break;
    }

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
      myTympan.print("  SPL Fast:   ");
      myTympan.print(ms.splFast_dBA, 1);
      myTympan.println(" dBA");
      myTympan.print("  SPL Slow:   ");
      myTympan.print(ms.splSlow_dBA, 1);
      myTympan.println(" dBA");
      myTympan.print("  LAeq,1min:  ");
      myTympan.print(ms.laeq1min_dBA, 1);
      myTympan.println(" dBA");
      myTympan.print("  LAeq,1hr:   ");
      myTympan.print(ms.laeq1hr_dBA, 1);
      myTympan.println(" dBA");
      myTympan.print("  Peak:       ");
      myTympan.print(ms.peakSPL_dBA, 1);
      myTympan.println(" dBA");
      myTympan.print("  Dose:       ");
      myTympan.print(ms.noiseDosePct, 2);
      myTympan.println(" %");
      myTympan.print("  8hr Proj:   ");
      myTympan.print(ms.projectedDose8hr, 1);
      myTympan.println(" %");
      myTympan.print("  CPU: ");
      myTympan.print(ms.cpuPercent, 1);
      myTympan.print("% | Mem: ");
      myTympan.println(ms.memUsed);
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

    case 'L': {
      parseAudiogram(line.substring(1), leftThresholds);
      myTympan.println("[SYS] Left audiogram updated via serial.");
      break;
    }

    case 'R': {
      parseAudiogram(line.substring(1), rightThresholds);
      myTympan.println("[SYS] Right audiogram updated via serial.");
      break;
    }

    case 'p': {
      dspApplyProfile(leftThresholds, rightThresholds);
      transitionToProcessing();
      break;
    }

    default:
      break; // ignore newlines, etc.
    }
  }
}

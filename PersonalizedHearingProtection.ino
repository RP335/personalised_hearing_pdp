/*
 * ============================================================================
 * PersonalizedHearingProtection.ino
 * ============================================================================
 * MAIN FIRMWARE — First Responder Personalized Hearing Protection System
 *
 * Tympan Rev F (Teensy 4.1 + AIC3206 codec)
 *
 * ARCHITECTURE:
 *   Config.h         — All constants, hardware pins, DSP parameters
 *   AudioGraph.h/cpp — Audio objects, patch cords, codec init
 *   NALR.h/cpp       — NAL-R prescription algorithm (audiogram → gains)
 *   DSPManager.h/cpp — Filterbank, WDRC, limiter, algorithm toggles
 *   NFCReader.h/cpp  — PN532 NFC audiogram card reader
 *   Metrics.h/cpp    — SPL, LAeq, noise dose computation
 *   SDLogger.h/cpp   — SD card metrics CSV logging + raw audio
 *   BLESync.h/cpp    — Bluetooth metrics streaming + file sync
 *   OLEDDisplay.h/cpp— SSD1306 multi-page status display
 *   SerialControl.h/cpp — USB serial command interface
 *
 * SIGNAL FLOW (mono — left input → both ears):
 *   Input → PreGain → 8× FIR → 8× WDRC(NAL-R) → Mixer → BB Limiter → L+R
 *   Input → A-Weight → Level(Fast/Slow) → Metrics → SD Log + BLE Stream
 *
 * STATE MACHINE:
 *   INIT → WAITING → (NFC tap) → PROCESSING ↔ (NFC re-tap toggles bypass)
 *                ↑        ↑                          |
 *                └────────┴──── serial 'r' reset ────┘
 *
 * MIT License. Use at your own risk.
 * ============================================================================
 */

#include "AudioGraph.h"
#include "BLESync.h"
#include "Config.h"
#include "DSPManager.h"
#include "Metrics.h"
#include "NFCReader.h"
#include "OLEDDisplay.h"
#include "SDLogger.h"
#include "SerialControl.h"
#include "NoiseReductionFD.h"

// ============================================================================
// SYSTEM STATE
// ============================================================================

enum SystemState {
  SYS_INIT,
  SYS_WAITING,   // waiting for NFC card
  SYS_PROCESSING // profile loaded, DSP active
};

static SystemState sysState = SYS_INIT;

// Timing
static unsigned long lastNFCAction = 0;
static unsigned long lastSerialStats = 0;

void transitionToProcessing() {
  sysState = SYS_PROCESSING;
  displayFlash("PROFILE", "CUSTOM", 1500);
  displaySetPage(PAGE_GAINS);
  myTympan.println("[SYS] Custom Profile loaded -> PROCESSING");
}

// ============================================================================
// POTENTIOMETER — Headphone volume (Tympan onboard knob)
// ============================================================================

// void servicePotentiometer(unsigned long now) {
//   static unsigned long lastUpdate = 0;
//   static float prevVal = -1.0f;

//   if (now - lastUpdate < POT_SERVICE_INTERVAL_MS)
//     return;
//   lastUpdate = now;

//   float val = (float)myTympan.readPotentiometer() / 1023.0f;
//   val = 0.1f * (float)((int)(10.0f * val + 0.5f)); // quantise to 0.1 steps

//   if (fabsf(val - prevVal) > 0.05f) {
//     prevVal = val;
//     float vol_dB = -40.0f + 50.0f * val; // pot range: -40 to +10 dB
//     myTympan.volume_dB(vol_dB);
//   }
// }

// ============================================================================
// NFC STATE MACHINE
// ============================================================================

void serviceNFC(unsigned long now) {
  if (!nfcPresent)
    return;
  if (now - lastNFCAction < NFC_POLL_COOLDOWN_MS)
    return;

  if (sysState == SYS_WAITING) {
    // --- Try to read a new audiogram card ---
    if (nfcPollForCard()) {
      // Card read successfully — apply profile
      dspApplyProfile(leftThresholds, rightThresholds);

      // Start SD logging for this user
      sdLoggerStart(currentUserUID);

      // Update display
      displayFlash("PROFILE", "LOADED!", 1500);
      displaySetPage(PAGE_GAINS);

      sysState = SYS_PROCESSING;
      lastNFCAction = now;

      myTympan.println("[SYS] Profile loaded → PROCESSING");
    }
  } else if (sysState == SYS_PROCESSING) {
    // --- Re-tap card to toggle bypass ---
    if (nfcPollForToggle()) {
      if (dspFlags.nalrEnabled || dspFlags.wdrcEnabled) {
        dspSetBypass();
        displayFlash("BYPASS", nullptr, 800);
      } else {
        dspApplyProfile(leftThresholds, rightThresholds);
        displayFlash("NAL-R ON", nullptr, 800);
      }
      lastNFCAction = now;
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // --- Serial (USB + BT) ---
  serialControlInit();
  delay(1000);

  myTympan.println("============================================");
  myTympan.println("  PERSONALIZED HEARING PROTECTION");
  myTympan.println("  Tympan Rev F | Modular Firmware v2.0");
  myTympan.println("  8-Band FIR + WDRC + NAL-R + Metrics");
  myTympan.println("============================================");

  // --- Audio graph (codec, memory, I/O) ---
  audioGraphInit();

  // --- DSP chain (filterbank, compressors) ---
  dspInit();
  #ifdef ENABLE_NOISE_REDUCTION
    nrInit();
  #endif

  // --- I2C bus for NFC + OLED ---
  I2C_BUS.begin();
  I2C_BUS.setClock(I2C_CLOCK_HZ);

  // --- OLED display ---
  displayInit();

  // --- NFC reader ---
  nfcInit();
  if (nfcPresent) {
    displayFlash("NFC: OK", nullptr, 500);
  } else {
    displayFlash("NFC: MISSING", "Audio passthru OK", 1500);
  }

  // --- Sound level metrics ---
  metricsInit();

  // --- SD card logger ---
  sdLoggerInit();

  // --- BLE ---
  bleSyncInit();

  myTympan.volume_dB(10.0f);

  // --- Initial pot reading ---
  // servicePotentiometer(millis());

  // --- Ready ---
  sysState = SYS_WAITING;
  displaySetPage(PAGE_WAITING);

  myTympan.println("\n[SYS] Ready! Scan NFC card or send 'h' for commands.");
  myTympan.println("  Signal: Input → 8-Band WDRC → Headphones\n");
  serialPrintHelp();
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();

  // --- NFC polling (with cooldown) ---
  serviceNFC(now);

  // --- Sound level metrics update ---
  metricsUpdate(now);

  // --- SD card logging + purge ---
  sdLoggerService(now);

  // --- SD raw audio writer service ---
  sdWriter.serviceSD_withWarnings(i2s_in);

  // --- OLED display refresh (~10 FPS) ---
  displayUpdate(now);

  // --- Potentiometer (headphone volume) ---
  // servicePotentiometer(now);

  // --- BLE service (advertising, commands, metrics stream) ---
  bleSyncService(now);

  // --- Periodic serial stats (every 3 s when processing) ---
  if (sysState == SYS_PROCESSING &&
      (now - lastSerialStats > SERIAL_STATS_INTERVAL_MS)) {
    lastSerialStats = now;
    myTympan.printCPUandMemory(now, 0);
  }

  // --- Serial commands ---
  serialControlService();

  // --- LED heartbeat ---
  myTympan.serviceLEDs(now, sdRawRecordIsActive());
}

/*
 * ============================================================================
 * OLEDDisplay.cpp — SSD1306 OLED Display Implementation
 * ============================================================================
 */

#include "OLEDDisplay.h"
#include "AudioGraph.h"
#include "BLESync.h"
#include "DSPManager.h"
#include "Metrics.h"
#include "NFCReader.h"
#include "SDLogger.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// HARDWARE
// ============================================================================

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &I2C_BUS, OLED_RESET_PIN);
static bool oledOK = false;

// ============================================================================
// STATE
// ============================================================================

static DisplayPage currentPage = PAGE_SPLASH;
static unsigned long lastRefresh = 0;
static bool heartbeat = false;

// Flash overlay
static bool flashActive = false;
static char flashLine1[32] = {0};
static char flashLine2[32] = {0};
static unsigned long flashEnd = 0;

// ============================================================================
// DRAWING HELPERS
// ============================================================================

static void drawTitleBar(const char *title) {
  oled.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setCursor(4, 2);
  oled.print(title);
  oled.setTextColor(SSD1306_WHITE);
}

static void drawStatusIcons(int y) {
  // Small status indicators in top-right
  int x = 100;
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_BLACK);

  // SD indicator
  if (sdLoggerIsActive()) {
    oled.setCursor(x, 2);
    oled.print("SD");
    x += 14;
  }
  // BLE indicator
  if (bleSyncIsStreaming()) {
    oled.setCursor(x, 2);
    oled.print("BT");
  }
  oled.setTextColor(SSD1306_WHITE);
}

static void drawHeartbeat() {
  heartbeat = !heartbeat;
  if (heartbeat)
    oled.fillCircle(122, 6, 3, SSD1306_BLACK);
}

// ============================================================================
// PAGE RENDERERS
// ============================================================================

static void renderSplash() {
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("PERSONALIZED");
  oled.println("HEARING PROTECT");
  oled.println();
  oled.println("Tympan Rev F");
  oled.println("8-Band WDRC+NAL-R");
  oled.println();
  oled.print("Booting...");
}

static void renderWaiting() {
  drawTitleBar("HEARING PROTECT");
  drawStatusIcons(2);

  oled.setCursor(0, 18);
  oled.println("Scan NFC card...");
  oled.println();
  oled.println("8-Band WDRC+NAL-R");
  oled.println("Tympan Rev F");
  oled.println();
  if (!nfcPresent)
    oled.print("(NFC not found)");
}

static void renderReading() {
  oled.setTextSize(2);
  oled.setCursor(10, 20);
  oled.println("READING");
  oled.println("  NFC...");
}

static void renderGains() {
  // Title bar with mode indicator
  drawTitleBar(dspFlags.nalrEnabled ? "ACTIVE  NAL-R" : "BYPASS");
  drawStatusIcons(2);
  drawHeartbeat();

  oled.setCursor(0, 16);
  oled.print("Gains (dB):");

  const int barX = 4;
  const int barW = 13;
  const int barGap = 2;
  const int barBase = 58;
  const int maxBarH = 28;

  for (int i = 0; i < N_CHAN; i++) {
    int barH =
        (int)(currentNALRGains_L[i] / NALR_GAIN_CEILING * (float)maxBarH);
    if (barH < 1 && currentNALRGains_L[i] > 0.5f)
      barH = 1;

    int x = barX + i * (barW + barGap);
    oled.drawRect(x, barBase - maxBarH, barW, maxBarH, SSD1306_WHITE);
    if (barH > 0)
      oled.fillRect(x, barBase - barH, barW, barH, SSD1306_WHITE);

    if (i % 2 == 0) {
      oled.setCursor(x, 26);
      oled.print((int)currentNALRGains_L[i]);
    }
  }

  oled.setCursor(0, barBase + 2);
  oled.print("CPU:");
  oled.print(AudioProcessorUsage(), 0);
  oled.print("%");
}

static void renderSPL() {
  drawTitleBar("SPL METER");
  drawStatusIcons(2);

  MetricsSnapshot m = metricsGetSnapshot();

  oled.setCursor(0, 16);
  oled.setTextSize(2);
  oled.print(m.splFast_dBA, 0);
  oled.setTextSize(1);
  oled.print(" dBA");

  oled.setTextSize(1);
  oled.setCursor(0, 36);
  oled.print("Slow: ");
  oled.print(m.splSlow_dBA, 1);
  oled.print(" dBA");

  oled.setCursor(0, 46);
  oled.print("LAeq: ");
  oled.print(m.laeq1min_dBA, 1);
  oled.print(" dBA");

  oled.setCursor(0, 56);
  oled.print("Peak: ");
  oled.print(m.peakSPL_dBA, 1);
  oled.print(" dBA");
}

static void renderDose() {
  drawTitleBar("NOISE DOSE");
  drawStatusIcons(2);

  MetricsSnapshot m = metricsGetSnapshot();

  oled.setCursor(0, 16);
  oled.setTextSize(2);
  oled.print(m.noiseDosePct, 0);
  oled.setTextSize(1);
  oled.print(" %");

  oled.setCursor(0, 36);
  oled.print("8hr proj: ");
  oled.print(m.projectedDose8hr, 0);
  oled.print("%");

  oled.setCursor(0, 48);
  oled.print("LAeq,1hr: ");
  oled.print(m.laeq1hr_dBA, 1);
  oled.print(" dBA");

  // Warning bar if dose > 50%
  if (m.noiseDosePct > 50.0f) {
    oled.setCursor(0, 58);
    oled.print("!! HIGH EXPOSURE !!");
  }
}

static void renderStatus() {
  drawTitleBar("SYSTEM STATUS");

  oled.setCursor(0, 16);
  oled.print("NAL-R: ");
  oled.print(dspFlags.nalrEnabled ? "ON" : "OFF");
  oled.print("  WDRC: ");
  oled.println(dspFlags.wdrcEnabled ? "ON" : "OFF");

  oled.print("Limit: ");
  oled.print(dspFlags.limiterEnabled ? "ON" : "OFF");
  oled.print("  Gain: ");
  oled.print(currentPreGainDB, 0);
  oled.println("dB");

  oled.println();
  oled.print("CPU: ");
  oled.print(AudioProcessorUsage(), 1);
  oled.print("% Mem: ");
  oled.println(AudioMemoryUsage());

  oled.print("SD: ");
  oled.print(sdLoggerIsActive() ? "LOG" : "---");
  oled.print("  BLE: ");
  oled.println(bleSyncIsStreaming() ? "STREAM" : "---");

  oled.print("User: ");
  oled.println(currentUserUID[0] ? currentUserUID : "---");
}

static void renderFlash() {
  oled.setTextSize(2);
  oled.setCursor(4, 16);
  oled.println(flashLine1);
  if (flashLine2[0]) {
    oled.setCursor(4, 36);
    oled.println(flashLine2);
  }
}

// ============================================================================
// INIT
// ============================================================================

void displayInit() {
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    myTympan.println("[OLED] *** INIT FAILED ***");
    oledOK = false;
    return;
  }

  oledOK = true;
  oled.clearDisplay();
  renderSplash();
  oled.display();
  myTympan.println("[OLED] OK");
}

// ============================================================================
// UPDATE
// ============================================================================

void displayUpdate(unsigned long nowMs) {
  if (!oledOK)
    return;
  if (nowMs - lastRefresh < OLED_REFRESH_MS)
    return;
  lastRefresh = nowMs;

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  // Flash overlay takes priority
  if (flashActive) {
    if (nowMs >= flashEnd) {
      flashActive = false;
    } else {
      renderFlash();
      oled.display();
      return;
    }
  }

  switch (currentPage) {
  case PAGE_SPLASH:
    renderSplash();
    break;
  case PAGE_WAITING:
    renderWaiting();
    break;
  case PAGE_READING:
    renderReading();
    break;
  case PAGE_GAINS:
    renderGains();
    break;
  case PAGE_SPL:
    renderSPL();
    break;
  case PAGE_DOSE:
    renderDose();
    break;
  case PAGE_STATUS:
    renderStatus();
    break;
  default:
    renderGains();
    break;
  }

  oled.display();
}

// ============================================================================
// CONTROL
// ============================================================================

void displaySetPage(DisplayPage page) { currentPage = page; }

void displayNextPage() {
  int p = (int)currentPage + 1;
  // Skip splash/waiting/reading in rotation
  if (p <= PAGE_READING)
    p = PAGE_GAINS;
  if (p >= PAGE_COUNT)
    p = PAGE_GAINS;
  currentPage = (DisplayPage)p;
}

DisplayPage displayGetPage() { return currentPage; }

void displayFlash(const char *line1, const char *line2,
                  unsigned long durationMs) {
  strncpy(flashLine1, line1, sizeof(flashLine1) - 1);
  flashLine1[sizeof(flashLine1) - 1] = '\0';

  if (line2) {
    strncpy(flashLine2, line2, sizeof(flashLine2) - 1);
    flashLine2[sizeof(flashLine2) - 1] = '\0';
  } else {
    flashLine2[0] = '\0';
  }

  flashEnd = millis() + durationMs;
  flashActive = true;
}

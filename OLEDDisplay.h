/*
 * ============================================================================
 * OLEDDisplay.h — SSD1306 OLED Status Display
 * ============================================================================
 * Manages the 128×64 OLED on Wire2.
 * Multiple display pages, auto-rotated or manually selectable.
 * ============================================================================
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "Config.h"

// Display pages (cycle through with serial command or auto-rotate)
enum DisplayPage {
  PAGE_SPLASH,       // boot screen
  PAGE_WAITING,      // waiting for NFC card
  PAGE_READING,      // reading NFC card
  PAGE_GAINS,        // per-band NAL-R gain bar chart
  PAGE_SPL,          // real-time SPL + LAeq meter
  PAGE_DOSE,         // noise dose summary
  PAGE_STATUS,       // system status (CPU, memory, flags)
  PAGE_COUNT
};

// ============================================================================
// LIFECYCLE
// ============================================================================

void displayInit();                            // call from setup(), after Wire2.begin()
void displayUpdate(unsigned long nowMs);       // call from loop()

// ============================================================================
// CONTROL
// ============================================================================

void displaySetPage(DisplayPage page);
void displayNextPage();
DisplayPage displayGetPage();

// One-shot messages (shown briefly, then returns to normal page)
void displayFlash(const char *line1, const char *line2 = nullptr,
                   unsigned long durationMs = 1500);

#endif // OLED_DISPLAY_H

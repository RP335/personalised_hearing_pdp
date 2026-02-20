/*
 * ============================================================================
 * SerialControl.h — Serial Command Interface
 * ============================================================================
 * Extended command set for controlling all system features via USB serial.
 * ============================================================================
 */

#ifndef SERIAL_CONTROL_H
#define SERIAL_CONTROL_H

#include "Config.h"

void serialControlInit();
void serialControlService(); // call from loop()
void serialPrintHelp();

extern void transitionToProcessing();

#endif // SERIAL_CONTROL_H

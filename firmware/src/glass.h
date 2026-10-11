// SPDX-License-Identifier: MIT
// The segment LCD the firmware draws on. Each glass (the LaunchPad's for development, the GDC04212 on the
// board) has its own glass_<name>.c with its pin assignment and segment map; the Makefile's GLASS picks one.
#ifndef GLASS_H
#define GLASS_H

#include <stdint.h>

// Hand the glass's pins to the LCD controller. Needed after every reset, including each wake from LPM3.5,
// before the pins are unlocked.
void glass_pins(void);

// Set up the LCD controller from scratch and blank the display, leaving the colon blinking at 1 Hz in
// hardware. Only needed when the controller isn't already running.
void glass_init(void);

// Draw HH:MM. digits[] holds the four digits, tens of hours first, each 0-9.
void glass_show(const uint8_t digits[4]);

#endif

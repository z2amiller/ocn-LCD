// SPDX-License-Identifier: MIT
// The MSP-EXP430FR4133 LaunchPad's FH-1138P glass, from SLAU595B Table 4. It is 4-mux with COM0-COM3 on
// L0-L3. Each LCDMEM byte covers two pins: the odd pin's COM3..COM0 in bits 7..4, the even pin's in bits 3..0.
// HH:MM uses characters A1 A2 : A3 A4, with the colon that sits after A2.
#include <msp430.h>
#include "glass.h"

// TI's header declares LCDMEM and LCDBMEM as pointers to a one-byte register, so indexing past 0 trips
// -Warray-bounds even though the memory is there.
#pragma GCC diagnostic ignored "-Warray-bounds"

static const uint8_t pos[4] = {4, 6, 8, 10};    // LCDMEM byte holding the 7-segment part of A1..A4

// Segments A B C D (L odd) and E F G M (L even); G and M are the two halves of the middle bar.
static const uint8_t digit[10] = {0xFC, 0x60, 0xDB, 0xF3, 0x67, 0xB7, 0xBF, 0xE4, 0xFF, 0xF7};

#define COLON_BYTE 7                             // A2COL: LCDM7, L14 COM2
#define COLON_BIT  0x04

void glass_pins(void)
{
    SYSCFG2 |= LCDPCTL;                          // R13, R23, R33, LCDCAP0 and LCDCAP1 to the LCD
    LCDPCTL0 = 0xFFFF;                           // every pin the glass uses: L0-L26 and L36-L39, so unused
    LCDPCTL1 = 0x07FF;                           // segments are driven off rather than left with DC on them
    LCDPCTL2 = 0x00F0;
}

void glass_init(void)
{
    // f(LCD) = 32768 / ((LCDDIV + 1) x 16) = 256 Hz at 4-mux, so the frame rate is 256 / 8 = 32 Hz: the low,
    // low-current end of the 25-80 Hz range (SLAU445I 17.2.4). LCDDIV resets to 7; spelt out so it can't be
    // lost (an earlier LCDCTL0 = 0 made it divide by 1, eight times faster).
    // LCDLP is the low-power waveform: measured on the LaunchPad it took the sleep current from 4.12 to
    // 2.85 uA, with no visible change to the display.
    LCDCTL0 = LCDDIV_7 | LCDSSEL_0 | LCD4MUX | LCDLP;   // XT1 clock, 4-mux, low-power waveform, still off
    // Internal 2.96 V bias from the charge pump at its lowest clock. LCDREFMODE switches the bias reference
    // instead of running it continuously (static mode is for large panels): 2.85 to 2.20 uA, display unchanged.
    LCDVCTL = LCDCPEN | LCDREFEN | LCDREFMODE | VLCD_6 | LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3;
    LCDMEMCTL |= LCDCLRM | LCDCLRBM;             // clears the COM assignments too, so set them after
    LCDCSSEL0 = 0x000F;                          // L0-L3 are COMs
    LCDCSSEL1 = 0;
    LCDCSSEL2 = 0;
    LCDM0 = 0x21;                                // L0 = COM0, L1 = COM1
    LCDM1 = 0x84;                                // L2 = COM2, L3 = COM3
    // f(BLINK) = f(LCD) / ((LCDMX + 1) x 2^(LCDBLKPRE + 2)) = 256 / (4 x 64) = 1 Hz (SLAU445I 17.2.6.3).
    // Mode 1 blinks only segments that are on in both the display memory and the blink memory: here, just
    // the colon. The prescaler may only change while blinking is off, so it goes in first.
    LCDBMEM[COLON_BYTE] = COLON_BIT;
    LCDBLKCTL = LCDBLKPRE_4;
    LCDBLKCTL |= LCDBLKMOD_1;
    LCDCTL0 |= LCDON;
}

void glass_show(const uint8_t digits[4])
{
    for (uint8_t i = 0; i < 4; i++) {
        LCDMEM[pos[i]] = digit[digits[i]];
    }
    LCDMEM[COLON_BYTE] = COLON_BIT;
}

// SPDX-License-Identifier: MIT
// The board's glass: Good Display GDC04212, 4 digits with a colon, 1/4 duty, 1/3 bias, 3.0 V. Derived from the
// spec sheet's pin table (as encoded in the GDC04212 symbol in ../lib/LowPower.kicad_sym) and the board's
// netlist; not yet checked on real glass.
//
// Glass pins 1-4 are COM1-COM4, wired to P7.0-P7.3 = L0-L3 (MCU COM0-COM3), as on the LaunchPad. The segment
// pins are wired in order to P3.0-P3.7 = L8-L15:
//
//   glass pin   MCU pin   segment on COM1  COM2  COM3  COM4
//       5        L8              1F        1G    1E    P1
//       6        L9              1A        1B    1C    1D
//       7        L10             2F        2G    2E    P2 (colon)
//       8        L11             2A        2B    2C    2D
//       9        L12             3F        3G    3E    P3
//      10        L13             3A        3B    3C    3D
//      11        L14             4F        4G    4E    P4
//      12        L15             4A        4B    4C    4D
//
// Each LCDMEM byte covers two pins: the even pin's COM3..COM0 in bits 3..0, the odd pin's in bits 7..4. So digit n
// is LCDMEM[3 + n], and every digit's byte has the same layout. P1, P3 and P4 are decimal points.
#include <msp430.h>
#include "glass.h"

// TI's header declares LCDMEM and LCDBMEM as pointers to a one-byte register, so indexing past 0 trips
// -Warray-bounds even though the memory is there.
#pragma GCC diagnostic ignored "-Warray-bounds"

#define SEG_F 0x01                               // even pin, COM1
#define SEG_G 0x02                               // even pin, COM2
#define SEG_E 0x04                               // even pin, COM3
#define SEG_P 0x08                               // even pin, COM4: decimal point, or the colon on digit 2
#define SEG_A 0x10                               // odd pin, COM1
#define SEG_B 0x20                               // odd pin, COM2
#define SEG_C 0x40                               // odd pin, COM3
#define SEG_D 0x80                               // odd pin, COM4

static const uint8_t digit[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,             // 0
    SEG_B | SEG_C,                                             // 1
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                     // 2
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                     // 3
    SEG_B | SEG_C | SEG_F | SEG_G,                             // 4
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                     // 5
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,             // 6
    SEG_A | SEG_B | SEG_C,                                     // 7
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,     // 8
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,             // 9
};

#define DIGIT_BYTE(n) (3 + (n))                  // n = 1..4
#define COLON_BYTE    DIGIT_BYTE(2)
#define COLON_BIT     SEG_P

void glass_pins(void)
{
    SYSCFG2 |= LCDPCTL;                          // R13, R23, R33, LCDCAP0 and LCDCAP1 to the LCD
    LCDPCTL0 = 0xFF0F;                           // L0-L3 (COMs) and L8-L15 (segments); nothing else
    LCDPCTL1 = 0;
    LCDPCTL2 = 0;
}

void glass_init(void)
{
    // Same timing and bias as the LaunchPad glass (see glass_launchpad.c): 32 Hz frames, the low-power waveform
    // and the switched bias reference. VLCD_6 is 2.96 V for the glass's 3.0 V; check the contrast on real glass,
    // and raise the frame rate (LCDDIV_3, 64 Hz) if it flickers.
    LCDCTL0 = LCDDIV_7 | LCDSSEL_0 | LCD4MUX | LCDLP;
    LCDVCTL = LCDCPEN | LCDREFEN | LCDREFMODE | VLCD_6 | LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3;
    LCDMEMCTL |= LCDCLRM | LCDCLRBM;             // clears the COM assignments too, so set them after
    LCDCSSEL0 = 0x000F;                          // L0-L3 are COMs
    LCDCSSEL1 = 0;
    LCDCSSEL2 = 0;
    LCDM0 = 0x21;                                // L0 = COM0, L1 = COM1
    LCDM1 = 0x84;                                // L2 = COM2, L3 = COM3
    LCDBMEM[COLON_BYTE] = COLON_BIT;             // only the colon blinks, at 1 Hz
    LCDBLKCTL = LCDBLKPRE_4;
    LCDBLKCTL |= LCDBLKMOD_1;
    LCDCTL0 |= LCDON;
}

void glass_show(const uint8_t digits[4])
{
    for (uint8_t i = 0; i < 4; i++) {
        LCDMEM[DIGIT_BYTE(i + 1)] = digit[digits[i]];
    }
    LCDMEM[COLON_BYTE] |= COLON_BIT;
}

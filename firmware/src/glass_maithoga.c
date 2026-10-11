// SPDX-License-Identifier: MIT
// An alternative glass for the board: the 12-pin 4-digit TN panel sold on AliExpress as "12PIN TN Positive
// 4-Digits Segment LCD Panel" (brand maithoga), 50.8 x 22.86 x 2.8 mm, 1/4 duty, 1/3 bias, 3.0 V. Its 12 pins
// sit at 2.54 mm, centred on the long edge like the GDC04212's, so it fits the same footprint; the glass is
// 4.4 mm longer at each end. Derived from the listing's drawing and pin table; not yet checked on real glass.
//
// Glass pins 1-4 are its COM3-COM0, in reverse order, wired to P7.0-P7.3 = L0-L3. They are set up as MCU
// COM0-COM3 as for the other glasses, so the glass's COM3 is the MCU's COM0. The segment pins are wired in
// order to P3.0-P3.7 = L8-L15:
//
//   glass pin   MCU pin   segment on COM3  COM2  COM1  COM0   (glass numbering)
//       5        L8              1A        1F    1E    -
//       6        L9              1B        1G    1C    1D
//       7        L10             2A        2F    2E    2H
//       8        L11             2B        2G    2C    2D
//       9        L12             3A        3F    3E    3H (separator)
//      10        L13             3B        3G    3C    3D
//      11        L14             4A        4F    4E    4H
//      12        L15             4B        4G    4C    4D
//
// Each LCDMEM byte covers two pins: the even pin's MCU COM3..COM0 in bits 3..0, the odd pin's in bits 7..4. So
// digit n is LCDMEM[3 + n], as on the GDC04212. H is a decimal point; the listing's photos put 2H-4H to the
// left of their digit, between it and the one before. There is no colon, so 3H, the point between the hours
// and the minutes, stands in for it and blinks: the display reads 12.34.
#include <msp430.h>
#include "glass.h"

// TI's header declares LCDMEM and LCDBMEM as pointers to a one-byte register, so indexing past 0 trips
// -Warray-bounds even though the memory is there.
#pragma GCC diagnostic ignored "-Warray-bounds"

#define SEG_A 0x01                               // even pin, MCU COM0 (glass COM3)
#define SEG_F 0x02                               // even pin, MCU COM1 (glass COM2)
#define SEG_E 0x04                               // even pin, MCU COM2 (glass COM1)
#define SEG_H 0x08                               // even pin, MCU COM3 (glass COM0): point left of the digit
#define SEG_B 0x10                               // odd pin, MCU COM0 (glass COM3)
#define SEG_G 0x20                               // odd pin, MCU COM1 (glass COM2)
#define SEG_C 0x40                               // odd pin, MCU COM2 (glass COM1)
#define SEG_D 0x80                               // odd pin, MCU COM3 (glass COM0)

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
#define SEP_BYTE      DIGIT_BYTE(3)              // 3H, between digits 2 and 3
#define SEP_BIT       SEG_H

void glass_pins(void)
{
    SYSCFG2 |= LCDPCTL;                          // R13, R23, R33, LCDCAP0 and LCDCAP1 to the LCD
    LCDPCTL0 = 0xFF0F;                           // L0-L3 (COMs) and L8-L15 (segments); nothing else
    LCDPCTL1 = 0;
    LCDPCTL2 = 0;
}

void glass_init(void)
{
    // Same timing and bias as the other glasses (see glass_launchpad.c): 32 Hz frames, the low-power waveform
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
    LCDBMEM[SEP_BYTE] = SEP_BIT;                 // only the separator blinks, at 1 Hz
    LCDBLKCTL = LCDBLKPRE_4;
    LCDBLKCTL |= LCDBLKMOD_1;
    LCDCTL0 |= LCDON;
}

void glass_show(const uint8_t digits[4])
{
    for (uint8_t i = 0; i < 4; i++) {
        LCDMEM[DIGIT_BYTE(i + 1)] = digit[digits[i]];
    }
    LCDMEM[SEP_BYTE] |= SEP_BIT;
}

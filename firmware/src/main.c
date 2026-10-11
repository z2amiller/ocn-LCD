// SPDX-License-Identifier: MIT
// Elapsed-time counter: counts minutes from power-up and shows HH:MM, freezing at 99:59.
//
// The chip spends its time in LPM3.5, where only the RTC, the LCD controller and the backup memory keep
// running. The RTC, clocked from the 32.768 kHz crystal, wakes it once a minute. Waking from LPM3.5 is a
// reset: the program starts again from main(), and SYSRSTIV says it was a wake rather than a power-up. RAM
// doesn't survive, so the count lives in backup memory. The LCD keeps showing the last drawing throughout,
// and blinks the colon by itself.
//
// Power-up (the magnet coming off the board, or the LaunchPad's supply coming back) starts again from 00:00.
//
// Test builds: FAST=1 counts once a second instead of once a minute; START=hhmm starts from that count,
// for example START=0058 to see the hour carry, or START=9958 to see it freeze.
#include <msp430.h>
#include <stdint.h>
#include "glass.h"

#ifndef FAST
#define FAST 0
#endif
#ifndef START
#define START 0
#endif

// RTC tick = 1024 / 32768 s = 31.25 ms; the interrupt fires every RTCMOD + 1 ticks.
#define TICKS_PER_COUNT (FAST ? 32 : 60 * 32)

// The four digits, tens of hours first, as BCD in backup memory (32 bytes, kept in LPM3.5).
#define DIGITS ((volatile uint8_t *)BKMEM_BASE)

// RSTLOG=1 builds keep a log of reset causes in the rest of backup memory, for reading back with DSLite:
// byte 4 is the next write position, bytes 5-31 a ring of SYSRSTIV values with a 0 after each start-up's.
#ifndef RSTLOG
#define RSTLOG 0
#endif
#define LOG_POS  (((volatile uint8_t *)BKMEM_BASE)[4])
#define LOG      (((volatile uint8_t *)BKMEM_BASE) + 5)
#define LOG_LEN  27

static void log_cause(uint8_t v)
{
    if (!RSTLOG) return;
    uint8_t p = LOG_POS;
    if (p >= LOG_LEN) p = 0;
    LOG[p] = v;
    LOG_POS = (uint8_t)((p + 1) % LOG_LEN);
}

// SYSRSTIV holds every reset cause since it was last emptied. Each read returns the highest-priority one and
// clears only that, so causes pile up unless it's emptied every time: a flash left the RST-pin reset and an
// old LPM3.5 wake side by side. Emptying it at every start-up keeps it current, and a start-up is a wake only
// when the LPM3.5 wake is the one cause in it; anything else (power-up, the RST pin, a software reset) starts
// over from 00:00.
static uint8_t woke_from_lpm35(void)
{
    uint8_t wake = 0, other = 0;
    for (uint8_t i = 0; i < 32; i++) {
        uint16_t iv = SYSRSTIV;
        if (iv == SYSRSTIV_NONE) break;
        if (iv == SYSRSTIV_LPM5WU) wake = 1; else other = 1;
        log_cause((uint8_t)iv);
    }
    log_cause(0);
    return wake && !other;
}

static void gpio_init(void)
{
    // Unused pins are driven low so they don't float. P1.1 (RX) stays an input with its pull-up: the
    // programmer's serial line drives it when connected and idles high, and without the pull-up it floats
    // when the line isn't there. The LCD controller takes its own pins over whatever is set here.
    P1OUT = BIT1; P1REN = BIT1; P1DIR = (uint8_t)~BIT1;
    P2OUT = 0; P2DIR = 0xFF;
    P3OUT = 0; P3DIR = 0xFF;
    P4OUT = 0; P4DIR = (uint8_t)~(BIT1 | BIT2);
    P5OUT = 0; P5DIR = 0xFF;
    P6OUT = 0; P6DIR = 0xFF;
    P7OUT = 0; P7DIR = 0xFF;
    P8OUT = 0; P8DIR = 0xFF;
    P4SEL0 |= BIT1 | BIT2;                       // XIN/XOUT to the crystal
}

static void load_digits(uint8_t d[4])
{
    for (uint8_t i = 0; i < 4; i++) {
        d[i] = DIGITS[i];
    }
}

static void save_digits(const uint8_t d[4])
{
    for (uint8_t i = 0; i < 4; i++) {
        DIGITS[i] = d[i];
    }
}

// Advances one minute. Returns 0, leaving the digits alone, once they read 99:59.
static uint8_t advance(uint8_t d[4])
{
    if (d[0] == 9 && d[1] == 9 && d[2] == 5 && d[3] == 9) {
        return 0;
    }
    if (++d[3] < 10) return 1;
    d[3] = 0;
    if (++d[2] < 6) return 1;
    d[2] = 0;
    if (++d[1] < 10) return 1;
    d[1] = 0;
    ++d[0];
    return 1;
}

static void sleep_lpm35(void)
{
    PMMCTL0_H = PMMPW_H;
    PMMCTL0_L = (PMMCTL0_L & ~SVSHE) | PMMREGOFF;   // supervisor off (saves ~0.2 uA), regulator off = LPM3.5
    PMMCTL0_H = 0;
    __bis_SR_register(LPM3_bits | GIE);
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;
    uint8_t woke = woke_from_lpm35();

    gpio_init();
    glass_pins();
    PM5CTL0 &= ~LOCKLPM5;                        // pins take the settings above

    do {                                         // the crystal: already running after a wake
        CSCTL7 &= ~(XT1OFFG | DCOFFG);
        SFRIFG1 &= ~OFIFG;
    } while (SFRIFG1 & OFIFG);

    if (woke) {
        if (!(LCDCTL0 & LCDON)) {                // in case the LCD controller didn't keep its setup
            uint8_t d[4];
            load_digits(d);
            glass_init();
            glass_show(d);
        }
        __enable_interrupt();                    // the RTC interrupt that woke us runs now
        __no_operation();
    } else {
        const uint8_t start[4] = {START / 1000 % 10, START / 100 % 10, START / 10 % 10, START % 10};
        save_digits(start);
        glass_init();
        glass_show(start);
        RTCMOD = TICKS_PER_COUNT - 1;
        RTCCTL = RTCSS__XT1CLK | RTCSR | RTCPS__1024 | RTCIE;
    }
    sleep_lpm35();                               // doesn't return: the next wake restarts main()
    for (;;) {
    }
}

void __attribute__((interrupt(RTC_VECTOR))) rtc_isr(void)
{
    if (RTCIV == RTCIV_RTCIF) {
        log_cause(0xEE);                         // RSTLOG marker: the RTC interrupt ran
        uint8_t d[4];
        load_digits(d);
        if (advance(d)) {
            save_digits(d);
            glass_show(d);
        } else {
            RTCCTL = 0;                          // frozen at 99:59: no more wakes
        }
    }
}

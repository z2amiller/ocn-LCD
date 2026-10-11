# ocn-LCD firmware

Firmware for the ocn-LCD board's MSP430FR4131 and its GDC04212 LCD. It counts minutes from power-up and
shows HH:MM, freezing at 99:59. The colon blinks at 1 Hz from the LCD controller itself (on the alternative
glass, which has no colon, the decimal point in its place blinks).

Developed on TI's MSP-EXP430FR4133 LaunchPad, whose FR4133 is the board's FR4131 with more memory. The same
code builds for both. The board build is 632 bytes; the FR4131 has 3,968 bytes of FRAM and 512 bytes of RAM.

Status: tested on the LaunchPad and its own LCD. The GDC04212 driver is written from the glass's spec sheet
and the board's netlist, but hasn't run on real glass yet.

## How it works

- The chip sleeps in LPM3.5, where only the RTC, the LCD controller and the 32-byte backup memory keep
  running.
- The RTC, clocked from the 32.768 kHz crystal, wakes it once a minute. That wake is a reset: `main()` runs
  again. The count lives in backup memory as four BCD digits, since RAM doesn't survive.
- The LCD keeps showing the last drawing through the sleep and the wake. If its controller ever comes back
  off, the wake path sets it up again and redraws.
- At 99:59 the RTC is stopped, so nothing wakes the chip again.
- Any other reset, such as power returning when the magnet is taken away, starts from 00:00.

**Telling a wake from a power-up.** `SYSRSTIV` keeps every reset cause since it was last emptied; each read
returns the highest-priority one and clears only that. The debugger leaves extras behind (attaching wakes the
chip, 0x08, then resets it through the RST pin, 0x04), so a single read can misreport a later wake. The code
empties it at every start-up and counts a start-up as a wake only when the LPM3.5 wake is its one cause.

**The LCD.** It runs from XT1 at f(LCD) = 256 Hz: a 32 Hz frame rate and a 1 Hz blink (SLAU445I 17.2.4 and
17.2.6.3). `LCDDIV` resets to 7; the code sets it explicitly, because clearing `LCDCTL0` makes it divide by 1
and runs both eight times too fast. The low-power waveform (`LCDLP`) and the switched bias reference
(`LCDREFMODE`) are on; neither changed how the LaunchPad's glass looks.

## Measured on the LaunchPad

A Nordic PPK2 supplying the target at 3.3 V, with every J101 jumper off except GND and the two Spy-Bi-Wire
ones:

| | Current |
|---|---|
| Asleep, as shipped | 2.15 µA median, 2.32 µA mean |
| Asleep, LCD off (chip and board only) | 1.28 µA |
| Asleep before `LCDLP` and `LCDREFMODE` | 4.12 µA |
| Each minute's wake | 1.3 ms, 0.33 µC (0.006 µA averaged) |
| Power-up (crystal start) | 204 ms, 61 µC |

The LaunchPad's glass has six alphanumeric characters and a row of symbols, much more than the GDC04212, so
the board should come in lower. Even 2.3 µA would run a CR2032 for about 11 years, longer than the cell's
shelf life. Lowering the crystal drive (`XT1DRIVE`) made no difference.

## Source

- `src/main.c`: the count, the wake handling and LPM3.5.
- `src/glass.h`: what the count needs from a display: `glass_pins()`, `glass_init()`, `glass_show()`.
- `src/glass_gdc04212.c`: the board's GDC04212, from Good Display's spec v2.0 pin table and the board's
  netlist (COM1-COM4 on L0-L3, segment pins 5-12 on L8-L15, so digit n is LCDMEM[3 + n] and the colon is
  bit 3 of LCDMEM[5]). When it first runs on real glass, check the contrast (VLCD_6, 2.96 V) and any flicker
  (32 Hz frames).
- `src/glass_maithoga.c`: an alternative glass for the same footprint, the 12-pin 4-digit TN panel sold on
  AliExpress as "12PIN TN Positive 4-Digits Segment LCD Panel" (brand maithoga, 50.8 × 22.86 mm), from the
  listing's drawing and pin table. Its pins line up with the GDC04212's, its glass is 4.4 mm longer at each end,
  and its commons run in reverse (pin 1 is COM3), which the segment table absorbs. It has no colon, so the
  decimal point between the hours and minutes blinks instead and the display reads 12.34. Not yet run on real
  glass.
- `src/glass_launchpad.c`: the LaunchPad's FH-1138P glass (SLAU595B Table 4), with HH:MM on characters A1
  to A4.
- `tools/ppk.py`: drives a Nordic PPK2 as the supply and measures the current (see below).
- `targets/msp430fr413{1,2,3}.ccxml`: DSLite target configs for a LaunchPad's eZ-FET programmer.

## Toolchain (macOS)

- **Compiler:** TI/Mitto msp430-gcc 9.3.1.11, unpacked to `~/ti/msp430-gcc` (x86_64, under Rosetta 2).
- **Device headers and linker scripts:** TI's msp430-gcc support files 1.212, in
  `~/ti/msp430-gcc-support-files/include`.
- **Flasher:** DSLite from Code Composer Studio 21.0.1, at
  `/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite` (x86_64).
- **PPK2 (optional):** `uv venv .venv && uv pip install --python .venv/bin/python ppk2-api numpy`, in this
  folder. No driver is needed on macOS; the PPK2 shows up as a USB serial port.

Other locations work too: set `TI=`, `GCC=`, `SUPPORT=` or `DSLITE=` on the `make` command line.

## Use

Run these from this folder.

```
make GLASS=gdc04212 MCU=msp430fr4131        # the board: its glass and its chip
make GLASS=gdc04212 MCU=msp430fr4131 flash  # build, program, verify, hard reset
make GLASS=maithoga MCU=msp430fr4131 flash  # the board with the alternative glass
make                              # the LaunchPad: FR4133 and its own glass
make flash                        # build, program the LaunchPad, verify, hard reset
make cycle                        # PPK2 power off and on: the clean start the chip needs after flashing
make measure SECONDS=130          # power-cycle, then measure current for 130 s
make FAST=1 START=0058 flash      # test build: one count per second, starting at 00:58
make RSTLOG=1 flash               # log reset causes in backup memory (read back with DSLite memory)
make EXTRA=-DSOMETHING            # extra compiler flags, for experiments
```

`FAST=1` counts once a second instead of once a minute. `START=hhmm` sets the starting count: `START=0058`
shows the hour carry and `START=9958` the freeze. Each variant builds into its own folder under `build/`; use
`make -B` after changing `EXTRA`.

`RSTLOG=1` keeps a log in backup memory: byte 4 is the next write position, bytes 5-31 a ring of `SYSRSTIV`
values with a 0 after each start-up's, and 0xEE each time the RTC interrupt runs. Read it with
`DSLite memory --config=targets/msp430fr4133.ccxml --range=0x0660,16 --size=16 --output=bk.bin`, but note
that attaching halts the chip.

`make` also writes `compile_flags.txt` so clangd resolves `msp430.h` and the device registers.

To program the board itself, wire its J1 to the LaunchPad's J101 as in the board's
[README](../README.md#programming), with the cell out.

## Testing on the LaunchPad

- **After flashing, press RST or run `make cycle`.** The chip stays in the debugger's session after DSLite
  finishes, and in that state it doesn't wake from LPM3.5: a flashed board counted for 15 s and stopped. Any
  reset without the debugger (the RST button, a power cycle) clears it.
- **The magnet.** On the board, the reed switch cuts the MCU's power; it never signals it. `make cycle` does
  the same with the PPK2; without one, unplug and replug the USB. Pulling only the 3V3 jumper on J101 isn't
  a clean power cut, because the programmer's other J101 lines can still feed the chip through its pins.
- **Current.** Pull every J101 jumper except GND and the two Spy-Bi-Wire ones (SBW RST, SBW TST), and wire
  the PPK2's VOUT to the target-side 3V3 pin and its GND to GND. Run the PPK2 at 3.3 V so the Spy-Bi-Wire
  lines, which idle at the programmer's 3.3 V, don't feed the chip; that also lets DSLite flash without
  moving any jumpers.

## License

The firmware is under the [MIT License](LICENSE); the rest of the repository (the board) is under the
CERN-OHL-P v2. `tools/ppk.py` uses [ppk2-api](https://github.com/IRNAS/ppk2-api-python) (GPL-2.0), which
isn't included here; install it separately as above.

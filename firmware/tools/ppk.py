#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Power the board from a Nordic PPK2 and measure its current.

The PPK2 runs as a source meter: it supplies the board (default 3.0 V, like the cell) through VOUT and
measures what it draws, 100,000 samples a second. Prints one line a second (average and peak), lists the
wake spikes with their times, and ends with the average over the whole run.

    .venv/bin/python tools/ppk.py --seconds 130            # measure for 130 s
    .venv/bin/python tools/ppk.py --cycle 3 --seconds 130  # power off 3 s first: the magnet coming off

Wiring on the LaunchPad: pull every J101 jumper except GND, SBW RST and SBW TST; PPK2 VOUT to the
target-side 3V3 pin, GND to GND. Run it at 3.3 V (`--volts 3.3`, as the Makefile does) so the Spy-Bi-Wire
lines, which idle at the programmer's 3.3 V, don't feed the chip.

ppk2-api supplies the serial protocol and the PPK2's calibration, but decodes one sample at a time in
Python, which can't keep up with 400 kB/s: the port overflows and the 4-byte framing slips. This decodes
whole reads with numpy instead, using the same calibration formula (ppk2_api get_adc_result, without its
range-switch spike smoothing).
"""
import argparse
import sys
import time

import numpy as np
from ppk2_api.ppk2_api import PPK2_API

RATE = 100_000.0                              # PPK2 samples per second


def calibration(ppk):
    """Per-range calibration arrays from the PPK2's own memory, indexed by measurement range 0-4."""
    m = ppk.modifiers
    return {k: np.array([float(m[k][str(r)]) for r in range(5)]) for k in ("O", "R", "UG", "GS", "GI", "S", "I")}


def decode(words, cal, vdd_v, adc_mult):
    """Raw 32-bit PPK2 words to current in uA."""
    rng = np.minimum((words >> 14) & 0x7, 4).astype(np.int64)
    adc = (words & 0x3FFF).astype(np.float64) * 4
    raw = (adc - cal["O"][rng]) * (adc_mult / cal["R"][rng])
    amps = cal["UG"][rng] * (raw * (cal["GS"][rng] * raw + cal["GI"][rng]) + (cal["S"][rng] * vdd_v + cal["I"][rng]))
    return amps * 1e6


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="PPK2 serial port (default: the first PPK2 found)")
    ap.add_argument("--volts", type=float, default=3.0, help="supply voltage (default 3.0)")
    ap.add_argument("--seconds", type=float, default=10.0, help="how long to measure (default 10)")
    ap.add_argument("--cycle", type=float, default=0.0,
                    help="first switch the supply off for this many seconds, then on (a clean power-up)")
    ap.add_argument("--spike-ua", type=float, default=50.0,
                    help="current that counts as a wake spike, in uA (default 50)")
    ap.add_argument("--quiet", action="store_true", help="skip the once-a-second lines")
    ap.add_argument("--off", action="store_true", help="leave the supply off at the end")
    args = ap.parse_args()

    port = args.port
    if not port:
        found = PPK2_API.list_devices()
        if not found:
            print("No PPK2 found.", file=sys.stderr)
            return 1
        port = found[0]

    ppk = PPK2_API(port, timeout=1, write_timeout=1, exclusive=True)
    ppk.get_modifiers()
    cal = calibration(ppk)
    ppk.set_source_voltage(int(round(args.volts * 1000)))
    ppk.use_source_meter()

    if args.cycle > 0:
        ppk.toggle_DUT_power("OFF")
        print(f"supply off for {args.cycle:g} s")
        time.sleep(args.cycle)
    ppk.toggle_DUT_power("ON")
    print(f"supply on at {args.volts:g} V, port {port}")
    ppk.start_measuring()

    leftover = b""
    chunks = []                                # one float32 array per read, kept for the spike scan
    sec_report = 1
    seen = 0
    t0 = time.monotonic()
    try:
        while time.monotonic() - t0 < args.seconds:
            data = ppk.ser.read(ppk.ser.in_waiting or 1)
            if not data:
                continue
            data = leftover + data
            cut = len(data) - len(data) % 4
            leftover = data[cut:]
            ua = decode(np.frombuffer(data[:cut], dtype="<u4"), cal, args.volts, ppk.adc_mult).astype(np.float32)
            chunks.append(ua)
            seen += len(ua)
            if not args.quiet:
                while seen >= sec_report * RATE:
                    allv = np.concatenate(chunks)
                    s = allv[int((sec_report - 1) * RATE):int(sec_report * RATE)]
                    print(f"{sec_report:6d} s  avg {s.mean():10.2f} uA  peak {s.max():10.1f} uA")
                    sec_report += 1
    finally:
        ppk.stop_measuring()
        if args.off:
            ppk.toggle_DUT_power("OFF")

    allv = np.concatenate(chunks) if chunks else np.zeros(0, np.float32)
    print()
    print(f"samples: {len(allv)} ({len(allv) / RATE:.1f} s of data in {args.seconds:g} s)")
    if not len(allv):
        return 1

    # Wake spikes: runs of samples over the threshold, merging runs less than 2 ms apart.
    over = np.flatnonzero(allv >= args.spike_ua)
    spikes = []
    if len(over):
        breaks = np.flatnonzero(np.diff(over) > int(0.002 * RATE))
        starts = np.concatenate(([over[0]], over[breaks + 1]))
        ends = np.concatenate((over[breaks], [over[-1]]))
        for a, b in zip(starts, ends):
            seg = allv[a:b + 1]
            spikes.append((a / RATE, (b - a + 1) / RATE, float(seg.max()), float(seg.sum() / RATE)))
    print(f"wake spikes (>= {args.spike_ua:g} uA): {len(spikes)}")
    for t, dur, peak, charge in spikes[:40]:
        print(f"  at {t:8.3f} s  for {dur * 1000:7.2f} ms  peak {peak:9.1f} uA  charge {charge:8.3f} uC")
    if len(spikes) > 40:
        print(f"  ... {len(spikes) - 40} more")
    if len(spikes) >= 2:
        gaps = np.diff([s[0] for s in spikes])
        print(f"spacing: median {np.median(gaps):.3f} s, min {gaps.min():.3f} s, max {gaps.max():.3f} s")

    # Sleep current: the run's median, and the mean of the quiet stretches between spikes.
    quiet = allv < args.spike_ua
    print(f"average over the run: {allv.mean():.3f} uA")
    print(f"median: {np.median(allv):.3f} uA   mean of samples under {args.spike_ua:g} uA: {allv[quiet].mean():.3f} uA")
    return 0


if __name__ == "__main__":
    sys.exit(main())

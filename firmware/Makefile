# SPDX-License-Identifier: MIT
# Build with the TI/Mitto msp430-gcc in ~/ti; flash with CCS's DSLite (both x86_64, run under Rosetta).
#
#   make                              build the counter for the FR4133 (the LaunchPad's part) and its glass
#   make flash                        build, then program and verify the LaunchPad, and run
#   make MCU=msp430fr4131             build for the FR4131 instead (checks the code still fits)
#   make GLASS=gdc04212               draw on the board's GDC04212 instead of the LaunchPad's glass
#   make GLASS=maithoga               the alternative 12-pin glass (no colon; its middle decimal point blinks)
#   make FAST=1 START=0058 flash      test build: count once a second, starting from 00:58
#   make EX=msp430fr413x_LCDE_01      build one of TI's SLAC625 examples instead of src/
#   make EX=msp430fr413x_LCDE_01 flash

MCU     ?= msp430fr4133
GLASS   ?= launchpad
TI      ?= $(HOME)/ti
GCC     ?= $(TI)/msp430-gcc/bin
SUPPORT ?= $(TI)/msp430-gcc-support-files/include
EXAMPLES = $(TI)/examples/MSP430FR413x_MSP430FR203x_Code_Examples/C
DSLITE  ?= /Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite

ifdef EX
SRC := $(EXAMPLES)/$(EX).c
BUILD = build/$(MCU)
else
SRC ?= src/main.c src/glass_$(GLASS).c
BUILD = build/$(MCU)/$(GLASS)$(if $(FAST),-fast)$(if $(START),-from$(START))$(if $(RSTLOG),-rstlog)
endif

# Test builds. START is hhmm; leading zeros are dropped so C doesn't read it as octal.
DEFS = $(EXTRA) $(if $(RSTLOG),-DRSTLOG=1) $(if $(FAST),-DFAST=1) $(if $(START),-DSTART=$(shell echo '$(START)' | sed 's/^0*\([0-9]\)/\1/'))

CC   = $(GCC)/msp430-elf-gcc
SIZE = $(GCC)/msp430-elf-size
CFLAGS  = -mmcu=$(MCU) -I$(SUPPORT) -Os -g -Wall -Wextra -std=gnu11 -ffunction-sections -fdata-sections $(DEFS)
LDFLAGS = -L$(SUPPORT) -Wl,--gc-sections

NAME = $(basename $(notdir $(firstword $(SRC))))
ELF  = $(BUILD)/$(NAME).elf

.PHONY: all size flash cycle measure clean

all: $(ELF) size compile_flags.txt

# Lets clangd (editor/LSP) resolve msp430.h and the device registers the way msp430-gcc -mmcu does.
compile_flags.txt: Makefile
	printf '%s\n' --target=msp430 -D__$(shell echo $(MCU) | tr a-z A-Z)__ -I$(SUPPORT) -I$(TI)/msp430-gcc/msp430-elf/include -std=gnu11 > $@

$(ELF): $(SRC) $(wildcard src/*.h) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

size: $(ELF)
	$(SIZE) $<

# The chip stays in the debugger's session after flashing and won't wake from LPM3.5 until it's reset
# without the debugger: press RST, cycle the power, or run `make cycle` with the PPK2 supplying it.
flash: $(ELF)
	$(DSLITE) flash --config=targets/$(MCU).ccxml --verbose --flash --verify --reset 1 $<
	@echo "Flashed. Press RST or run 'make cycle' before testing anything that sleeps."

# PPK2 as the supply (see tools/ppk.py): `make cycle` switches it off and on, a clean power-up like the magnet
# coming off; `make measure SECONDS=130` power-cycles and measures. VOLTS is the supply voltage.
VOLTS   ?= 3.3
SECONDS ?= 10
PY       = .venv/bin/python

cycle:
	$(PY) tools/ppk.py --volts $(VOLTS) --cycle 1 --seconds 1 --quiet > /dev/null

measure:
	$(PY) tools/ppk.py --volts $(VOLTS) --cycle 1 --seconds $(SECONDS)

clean:
	rm -rf build compile_flags.txt

# sfc2md

This program for AVR microcontrollers allows playing Genesis/Mega Drive with a
Super Nintendo/Super Famicom controller.

# Features

This program emulates a 6-button controller by default.  It was created
primarily to play Xeno Crisis, so the default button mapping is optimized for
that game: face buttons shoot directionally, R rolls, L throws grenade, Select
drops weapon, Start pauses.

For games that use A as primary action and B to jump (and C for a secondary
action, if applicable), hold Left on the controller when powering on the
console.  This maps action and jump to Y and B on the controller, and A to the
secondary action, which is typical and intuitive for SNES/SFC games.

For games that use B as the primary action and C to jump (and A for a secondary
action), hold Right when powering on instead.

In either of the above mappings, L, X, and R on the controller map to
Genesis/MD buttons X, Y, and Z respectively.  Start and Select always map to
Start and Mode.

To act as a 3-button controller, hold Select when powering on the console.
This can be combined with one of the above directions.  Some games may require
this, although in my testing so far I've found this adapter to be more
compatible than the official 6-button controller.

The adapter introduces about 1.67 milliseconds of input latency on NTSC region
(60 Hz) games.  It introduces 5 milliseconds on PAL region (50 Hz) games -- not
ideal, but so is playing Mega Drive games at 50 Hz.

# Building

A simple `Makefile` is provided.  The `upload` target will program an
ATmega32U4 on Linux with `avrdude`.  You may need to modify the file for your
MCU and OS.

# Requirements

It was prototyped on an ATmega32U4 board.  The MCU needs to run at 16MHz or
more to keep up.  Pin assignments may need to be changed depending on your MCU
and wiring.

**TO DO**: describe how to build hardware.  For now, an enterprising individual
can look at the pin assignments in the code and pinout diagrams for the console
controller ports.

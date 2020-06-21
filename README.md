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

# Building the Code

A simple `Makefile` is provided.  The `upload` target will program an
ATmega32U4 on Linux with `avrdude`.  You may need to modify the file for your
MCU, OS, and programmer.

# Hardware and Requirements

The prototype uses an ATmega32U4 board and some spare controller/extension cables
spliced together.  I used Dupont connectors and put it in an old AC adapter housing
to keep things a bit more clean.

![Adapter photograph](adapter.jpg)

The MCU needs to run at 16MHz or more to keep up with the Genesis/MD.  Pin
assignments may need to be changed depending on your MCU and wiring.

The best way to get the appropriate plugs is to buy extension cables and cut
them up.  You will want to wire the plugs to your MCU as follows:

## Genesis/Mega Drive connector (DE-9)

Looking into the connector on the controller cable (be sure not to get this
reversed!):

![Genesis (DE-9) connector](de9.png)

| Pin | Name   |
|-----|--------|
| 1   | D0     |
| 2   | D1     |
| 3   | D2     |
| 4   | D3     |
| 5   | +5V    |
| 6   | D4     |
| 7   | Select |
| 8   | GND    |
| 9   | D5     |

## SNES/Super Famicom connector (proprietary)

The shape of the connector makes orientation unambiguous, thankfully:

![SNES connector](sfcplug.png)

| Pin | Name   |
|-----|--------|
| 1   | GND    |
| 2   | Unused |
| 3   | Unused |
| 4   | Data   |
| 5   | Latch  |
| 6   | Clock  |
| 7   | +5V    |

## MCU

The default pin assignments using the same names as in the above tables are
found prominently in the [source code](sfc2md.c).  If you aren't using the same
board that I am, you'll most likely have to change them to match your wiring.
All Genesis/MD data lines (D0-D5) must be on the same port register so that
they can be updated in a single instruction.

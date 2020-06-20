CPU_FLAGS := -DF_CPU=16000000UL -mmcu=atmega32u4
CFLAGS := -Wall -Werror $(CPU_FLAGS) -Os

OBJECTS = sfc2md.o
HEADERS =

all: sfc2md.hex

asm: sfc2md.s

upload: sfc2md.hex
	avrdude -v -patmega32u4 -cavr109 -P/dev/ttyACM0 -b57600 -D -Uflash:w:$<:i

clean:
	rm -f $(OBJECTS) sfc2md.hex sfc2md sfc2md.s

sfc2md.hex: sfc2md
	avr-objcopy -O ihex -R .eeprom $< $@

sfc2md: $(OBJECTS)
	avr-gcc $(CFLAGS) -o $@ $(OBJECTS)

.c.s:
	avr-gcc $(CFLAGS) -c -S -o $@ $<

.c.o:
	avr-gcc $(CFLAGS) -c -o $@ $<

$(OBJECTS): $(HEADERS)

.PHONY: upload clean asm

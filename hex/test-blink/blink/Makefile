MF := $(MAKEFILE_LIST)
SOURCE = blink.c
LDSECTIONS = -Wl,-section-start=.text=0
AVRGCCROOT = ~/Library/Arduino15/packages/MultiX-Zinnia/tools/avr-gcc/7.3.0-avr8-gnu-toolchain/bin
CC = $(AVRGCCROOT)/avr-gcc
OPTIMIZE = -Os -fno-split-wide-types -mrelax
JOINING  = -j .text -j .data -j --set-section-flags
CFLAGS   = -g -Wall $(OPTIMIZE) -save-temps=obj
LDFLAGS  = $(LDSECTIONS) -Wl,--relax -nostartfiles -nostdlib
OBJCOPY = $(AVRGCCROOT)/avr-objcopy
OBJDUMP = $(AVRGCCROOT)/avr-objdump
LISTING = $(OBJDUMP) -S
OPTIONS = -mmcu=$*

blink_vcp_%.hex: blink_vcp_%.elf
	$(OBJCOPY) $(JOINING) -O ihex $< $@
blink_vcp_%.elf: $(SOURCE) FORCE
	$(CC) $(CFLAGS) $(OPTIONS) $(LDFLAGS) -o $@ $<
	$(LISTING) $@ > blink_vcp_$*.lst
atmega%:
	$(MAKE) -f $(MF) blink_vcp_atmega$*.hex
avr%:
	$(MAKE) -f $(MF) blink_vcp_avr$*.hex
all:
	$(MAKE) -f $(MF) $(MCU_TARGET)
FORCE:
clean:
	@touch __temp_.o __temp_.map __temp_.i __temp_.s __temp_.sym __temp_.lss __temp_.srec
	@touch __temp_.bin __temp_.lst __temp_.eep __temp_.elf __temp_.tmp.sh __temp_.tmp
	rm -rf *.o *.elf *.lst *.map *.i *.s *.sym *.lss *.eep *.srec *.bin *.tmp.sh *.tmp
%.lst: %.elf FORCE
	$(OBJDUMP) -h -S $< > $@
%.srec: %.elf FORCE
	$(OBJCOPY) $(JOINING) -O srec $< $@

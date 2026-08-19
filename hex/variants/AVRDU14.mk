###
### make -f AVRDU14.mk [build|compile|clean]
###

SKETCH  = UPDI4AVR-USB
VARIANT = AVRDU_14P
TARGET  = 54_AVR16DU14

CODE_HEX = $(SKETCH).ino.hex

# Additional environment variables can be specified here.

BUILD_OPT = --build-property "build.buildopt=-DHAL_PROFILE=\"HAL/$(VARIANT).h\""

### arduino-cli @1.0.x is required. ###

ACLIPATH=
SDKURL = --additional-urls https://askn37.github.io/package_multix_zinnia_index.json

# FQBN : You only need to specify the menu items that differ from the defaults.

FQBN = "MultiX-Zinnia:modernAVR:AVRDU_noloader:\
01_variant=$(TARGET),\
02_clock=11_20MHz,\
21_resetpin=02_gpio,\
27_fusefile=03_upload,\
54_console_select=12_UART1_D6_LC3,\
90_console_baud=14_500000bps"

### Make rule ###

MF := $(MAKEFILE_LIST)

build: FORCE
	$(MAKE) -f $(MF) compile
#	@$(eval TOOLS := $(shell dirname $(shell cat ../build/compile_commands.json | grep toolchain | head -n1) | tr -d '"'))
#	@$(TOOLS)/avr-size -A ../build/$(SKETCH).ino.elf
	@mv ./build/$(CODE_HEX) hex/$(VARIANT).hex
	@ls -lh hex/$(VARIANT).hex
	@$(MAKE) -f $(MF) clean

compile:
	$(MAKE) -f $(MF) clean
	$(ACLIPATH)arduino-cli compile --fqbn $(FQBN) $(BUILD_OPT) $(SDKURL) --build-path ./build ../..

clean:
	@touch ./build/__temp
	@rm -rf ./build/*

FORCE:

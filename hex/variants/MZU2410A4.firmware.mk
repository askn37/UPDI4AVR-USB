###
### make -f MZU2410A4.mk [build|compile|clean]
###

SKETCH  = UPDI4AVR-USB
VARIANT = MZU2410A4
TARGET  = 33_AVR32DU28

CODE_HEX = $(SKETCH).ino.hex
FUSE_HEX = $(SKETCH).ino.fuse
BOOT_HEX = $(SKETCH).ino.with_bootloader.hex

# Additional environment variables can be specified here.

BUILD_OPT = --build-property "build.buildopt=-DHAL_PROFILE=\"HAL/$(VARIANT).h\""

### arduino-cli @1.0.x is required. ###

ACLIPATH=
SDKURL = --additional-urls https://askn37.github.io/package_multix_zinnia_index.json

# FQBN : You only need to specify the menu items that differ from the defaults.

FQBN = "MultiX-Zinnia:modernAVR:AVRDU_usbloader:\
01_variant=$(TARGET),\
02_clock=11_20MHz,\
90_console_baud=14_500000bps,\
96_usbbootloader=11_euboot_LC3_SF6"

### Make rule ###

MF := $(MAKEFILE_LIST)

build: FORCE
	$(MAKE) -f $(MF) compile
	@mv ./build/$(CODE_HEX) hex/$(VARIANT).firmware.hex
	@mv ./build/$(FUSE_HEX) hex/$(VARIANT).bootloader.fuse
	@mv ./build/$(BOOT_HEX) hex/$(VARIANT).firmware_withboot.hex
	@ls -lh hex/$(VARIANT)*.hex
	@$(MAKE) -f $(MF) clean

compile:
	$(MAKE) -f $(MF) clean
	$(ACLIPATH)arduino-cli compile --fqbn $(FQBN) $(BUILD_OPT) $(SDKURL) --build-path ./build ../..

clean:
	@touch ./build/__temp
	@rm -rf ./build/*

FORCE:

# UPDI4AVR-USB : OSS/OSHW Programmer for UPDI/TPI

*Switching document languages* : [日本語](README_jp.md), __English__

- Open source software/firmware that transforms the AVR-DU family into a USB-connected programmer.
- Can read/erase/write NVM (non-volatile memory) of UPDI and TPI type AVR series.
- AVRDUDE is assumed as the programming application on the host PC. It looks like "PICKit4" or "Curiosity Nano".
- Equipped with VCP-UART transfer function.
- All results are distributed under the MIT license.

The conventional *USB4AVR* is designed to use a USB-serial conversion circuit, but this *UPDI4AVR-USB* is a one-chip complete design that uses the USB peripheral circuit built into the MCU.

## Quick Start

The pre-built binaries can be uploaded to the ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a) product for easy setup.

[->Click Here](https://github.com/askn37/UPDI4AVR-USB/tree/main/hex/updi4avr-usb)

## Introduction

The existence of the AVR-DU family was announced in the spring of 2021, but was soon put on hold. While that was stalled, the AVR-Ex series was released first, and after some time, the first production AVR64DU32 (with unfortunate errata) was finally released in May 2024. It will still take about half a year for the 14P/20P products to be released.

The biggest feature is that it is the only AVR-Dx series family that has built-in USB 2.0 "Full-Speed" device peripheral functions. This is a function inherited from the ATxmega AU family (expensive and minor), and is much more powerful than that of the ATmega32U4 family. The AVR-DU family also has SOIC-14P and 3mm square VQFN-20P packages, and can be made compact, so it has great potential.

On the other hand, since it is such a new product, open source compatibility has not progressed. The official development environment is MPLAB-X, but because of the elaborate build environment based on HAL, it takes up a very large amount of flash capacity. Moreover, the throughput is not sufficient. Do you want to cook a crucian carp with a battle axe?

What I wanted first from the AVR-DU family was the realization of VCP transfer based on USB-CDC/ACM that is normally recognized by the OS, and UPDI/TPI programming function based on USB-HID that can be handled from AVRDUDE. It naturally has to be a USB composite device. On the other hand, I am not considering using it from MPLAB-X due to licensing issues. So let's forget about the existence of dWire and OCD.

The only documents that provide clues are the [USB-IF public specification](https://www.usb.org/document-library/class-definitions-communication-devices-12), the [AVR-DU family data sheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/AVR32-16DU-14-20-28-32-Prelim-DataSheet-DS40002576.pdf), and the [ATxmega AU family manual](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8331-8-and-16-bit-AVR-Microcontroller-XMEGA-AU_Manual.pdf). This is a completely clean room development. I started by building a USB protocol stack from scratch using only AVR-GCC and AVR-LIBC, without any involvement in other projects. I condensed the main operations of the USB composite device to about 3KiB, and it took me 20 days to make it freely applicable. (Of course, I can also make a DFU bootloader, but that's a different plan.)

Next, I started investigating the AVRDUDE source code "jtag3.c". It was not difficult because I had already contributed to improving "jtagmkII.c" and "serialupdi.c". I was familiar with most of the UPDI NVM control from hand-making the *UPDI4AVR* series, and I have also been involved in supporting the AVR-Dx/Ex series. I created an emulator application that mimicked the response of "Curiosity Nano" at first, but I ran into a problem where I couldn't freely change the USB-VID:PID, so I had to strengthen "usb_hidapi.c". In addition, since it only requires a small amount of additional code, it can handle not only UPDI devices but also TPI devices. This is where my experience of making *TPI4AVR* came in handy.

It took 10 days to create a scenario and get it to work, and another 20 days to check the operation of over 20 types of UPDI devices I had on hand one by one and get a satisfactory result. But even more than that, it was a tedious time to prepare the outline!

After all that, I finally release the first open source branch that can be used with ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a). Some of the planned features, such as HV control, have not yet been implemented, but it should be no problem to try it out for general use.


## What you can and can't do

It is mostly a replacement for the traditional *JTAG2UPDI* and *microUPDI*. If you are already using them, you can start using it right away.

This software can:

- Only install on the AVR-DU family.
- Operates all released UPDI type AVR series:
  - All tinyAVR-0/1/2, megaAVR-0, AVR-Dx, AVR-Ex products
- Operates all released TPI type AVR series:
  - ATtiny4 ATtiny5 ATtiny9 ATtiny10 ATtiny20 ATtiny40 ATtiny102 ATtiny104 (8 types in total)
- No additional driver installation is required because it uses the standard OS driver for Windows/macos/Linux.
  - If additional driver/Inf file is already installed, the device vendor that matches VID:PID will be displayed. (Beware of license infringement)
- Includes VCP (Virtual Communication Port) including RS232 control signals.
  - RS232 control signals are assigned to surplus pins, so functions are limited in 14P/20P package varieties.
- A push switch can be used to reset the target device while the VCP is operating.
  - The reset state is maintained while the switch is pressed. Disabled while UPDI/TPI programs are running.
  - Can reset target devices (UPDI type) that do not have a hardware reset terminal as standard, such as the tinyAVR series, without turning off the power.
  - Useful for forcibly restarting application code such as bootloaders written to the device. Compatible with Arduino IDE.
- Can lock and unlock target devices. (LOCK bit FUSE operation and forced chip erase)

Planned but not yet realized: (As of August 2024)

- 2-types of UPDI type high voltage writing support. *Requires additional dedicated hardware circuitry.*
- 1-type of TPI type high voltage writing support. *Requires additional dedicated hardware circuitry. Also requires additional modification of AVRDUDE.*
- Bootloader protocol VCP bridge, such as "DFU for Serial" and "arduino/urclock". *It is not yet decided whether to integrate it into AVRDUDE.*

This software cannot:

- Does not work with devices other than the AVR-DU family, as the necessary USB peripheral functions are not implemented.
  - Porting to the ATxmega AU family is probably possible, as the USB peripheral functions are similar. (No plans: Fork required)
- Supports only USB 2.0 "Full-Speed". The AVR-DU family does not support "High-Speed". (Not possible)
- Does not support ISP/PP/HVPP type devices. The hardware requirements are different and there is no commonality in GPIO, so it will be a different software. (Fork required)
- Does not support PDI type AVR series. *However, there are plans to add support in the future, as the differences in hardware requirements are small.*
- JTAG communication, SWD/SWO, dWire, and OCD functions are not supported. (No plans)
- High-voltage programming is not supported on the 14P package (AVR16-32DU14) because there are no extra pins. To use all functions simultaneously, a 28P/32P package is required.
- DEBUG build (PRINTF) cannot be used on 14P/20P models because there are insufficient pins, and on 16KiB models because there is no free space.

For details on pinout/signal assignments for each package type of the AVR-DU family, see [<configuration.h>](src/configuration.h).

> [!NOTE]
> Previous generation devices such as the ATmega328P family are usually handled by ISP serial programming (SPI technology), but some models cannot be restored to factory settings once the fuses are destroyed unless HVPP high-voltage parallel programming (parallel technology) is used. In this case, a high-level programmer such as *UPDI4AVR*, whose primary objective is to "restore the device to its factory settings", needs to be able to process all of ISP/PP/HVPP.

## Practical Usage

Below is a simple usage example for "AVR64DU32 Curiosity Nano".
This product series can be mounted on a breadboard without soldering using the included pin headers.

### UPDI Control

For UPDI control, the target device requires three wires: "VCC", "GND", and "UPDI (TDAT)". Optionally, three more wires can be added: "nRST (TRST)", "VCP-TxD", and "VCP-RxD". Unless the device sets the GPIO to push-pull output, all connections are open-drain with built-in pull-up resistors. If you are concerned about GPIO contention, you can insert a 330Ω series resistor.

> The electrical characteristics are based on 5V/225kbps, and you should pay attention to the slew rate in the VCCx0.2 to 0.8 range.

<img src="https://askn37.github.io/product/UPDI4AVR/images/IMG_3832.jpg" width="400"> <img src="https://askn37.github.io/product/UPDI4AVR/images/U4AU_UPDI.drawio.svg" width="400">

The following signal arrangement is recommended for converting to the AVR-ICSP MIL/6P connector. This is compatible with TPI control and 3-types of HV control methods. (However, HV control is not possible without a dedicated circuit.)

<img src="https://askn37.github.io/svg/AVR-ICSP-M6P-UPDI4AVR.drawio.svg" width="320">

If the target device is `AVR64DU28`, a minimum connection test can be performed with the following command line.

```console
avrdude -c pickit4_updi -p avr64du28 -v -U sib:r:-:r
```

```
avrdude: Version 7.3
         Copyright the AVRDUDE authors;
         see https://github.com/avrdudes/avrdude/blob/main/AUTHORS

         System wide configuration file is C:\usr\avrdude-v7.3-windows_mingw-x64\avrdude.conf

         Using port            : usb
         Using programmer      : pickit4_updi
         AVR Part              : AVR64DU28
         Programming modes     : UPDI, SPM
         Programmer Type       : JTAGICE3_UPDI
         Description           : MPLAB(R) PICkit 4 in UPDI mode
         ICE HW version        : 0
         ICE FW version        : 1.32 (rel. 40)
         Serial number         : **********
         Vtarget               : 5.02 V
         PDI/UPDI clk          : 225 kHz

avrdude: partial Family_ID returned: "AVR "
avrdude: silicon revision: 1.3
avrdude: AVR device initialized and ready to accept instructions
avrdude: device signature = 0x1e9622 (probably avr64du28)

avrdude: processing -U sib:r:-:r
avrdude: reading sib memory ...
avrdude: writing output file <stdout>
AVR     P:4D:1-3M2 (A3.KV00S.0)
avrdude done.  Thank you.
```

### TPI Control

For TPI control, the target device requires five wires: "VCC", "GND", "TDAT", "TCLK", and "TRST".
As a result, for the 6P devices ATtiny4/5/9/10, only one unused pin remains.
All connections are open-drain with built-in pull-up resistors.

<img src="https://askn37.github.io/product/UPDI4AVR/images/IMG_3839.jpg" width="400"> <img src="https://askn37.github.io/product/UPDI4AVR/images/U4AU_TPI.drawio.svg" width="400">

If the target device is `ATiny10`, a minimum connection test can be performed with the following command line.

```console
avrdude -c pickit4_tpi -p attiny10 -v
```

```
avrdude: Version 7.3
         Copyright the AVRDUDE authors;
         see https://github.com/avrdudes/avrdude/blob/main/AUTHORS

         System wide configuration file is C:\usr\avrdude-v7.3-windows_mingw-x64\avrdude.conf

         Using port            : usb
         Using programmer      : pickit4_tpi
         AVR Part              : ATtiny10
         Programming modes     : TPI
         Programmer Type       : JTAGICE3_TPI
         Description           : MPLAB(R) PICkit 4 in TPI mode
         ICE HW version        : 0
         ICE FW version        : 1.32 (rel. 40)
         Serial number         : **********
         Vtarget               : 5.01 V

avrdude: AVR device initialized and ready to accept instructions
avrdude: device signature = 0x1e9003 (probably t10)

avrdude: processing -U flash:r:-:I
avrdude: reading flash memory ...
Reading | ################################################## | 100% 0.91 s
avrdude: writing output file <stdout>
:200000000AC011C010C00FC00EC00DC00CC00BC00AC009C008C011271FBFCFE5D0E0DEBF02 // 00000> .@.@.@.@.@.@.@.@.@.@.@.'.?OeP`^?
:20002000CDBF02D016C0ECCF48ED50E04CBF56BF4FEF47BB7894619A0A9ABA98029A4FEF35 // 00020> M?.P.@lOHmP`L?V?OoG;x.a...:...Oo
:1600400059E668E1415050406040E1F700C00000F5CFF894FFCFAB                     // 00040> YfhaAPP@`@aw.@..uOx..O
:00000001FF

avrdude done.  Thank you.
```

> In this example, the *Blinking LED* sketch binary for the PB2 terminal is read. \
> If you want to pull out the UART of the ATtiny102 and ATtiny104 to the 6P connector and connect it to the VCP, you will need to use some ingenuity. PA0/TCLK and PB3/RXD must be shorted, and PA0 must be left unused as a GPIO in principle.

### LED blinking

The orange LED can have several different states depending on the situation.

- Heartbeat - or deep breath. USB connection established with host OS. Ready for use.
- Short flash - Waiting for USB connection. Not seen by host OS.
- Long blink - SW0 is pressed down. Not programming. Target device is resetting (if possible).
- Short blink - Programming in progress. VCP communication is disabled.

> No LED control terminals are provided for VCP TxD/RxD communication indication.

### Other pinouts

For detailed pinout/signal assignments, see [<configuration.h>](src/configuration.h).

## High-Voltage control

Planning to support this. A dedicated control circuit will be required to be attached externally. Technically, this has already been achieved with the previous model, [UPDI4AVR](https://askn37.github.io/product/UPDI4AVR/README_en.html).

Currently, two projects are underway.

- A candy box sized all-in-one model.
- An expansion board model that allows you to attach a CNANO as a daughter board.

<img src="https://askn37.github.io/product/UPDI4AVR/images/U4AU_VIEW_MZU2410A.drawio.svg" width="400"> <img src="https://askn37.github.io/product/UPDI4AVR/images/U4AU_VIEW_MZU2411A.drawio.svg" width="400">

## Build and installation

By installing the SDK at the following link into the Arduino IDE, you can easily build and install for all AVR-DU family chips, including bare metal chips.

- https://github.com/askn37/multix-zinnia-sdk-modernAVR

For build options, see [<UPDI4AVR-USB.ino>](UPDI4AVR-USB.ino).

## Related link and documentation

- Repository front page (This page): We're looking for contributors to help us out.
  - [日本語(Native)](README_jp.md), [English](README.md)

- [UPDI4AVR-USB QUICK INSTALLATION GUIDE](hex/updi4avr/README.md)
- [UPDI4AVR](https://askn37.github.io/product/UPDI4AVR/README_en.html) : USB to serial converter

- [AVRDUDE](https://github.com/avrdudes/avrdude)

- [USB-IF public specification](https://www.usb.org/document-library/class-definitions-communication-devices-12)
- [AVR-DU family data sheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/AVR32-16DU-14-20-28-32-Prelim-DataSheet-DS40002576.pdf)
- [ATxmega AU family manual](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8331-8-and-16-bit-AVR-Microcontroller-XMEGA-AU_Manual.pdf)

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)

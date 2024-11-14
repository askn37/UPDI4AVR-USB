# UPDI4AVR-USB QUICK INSTALLATION GUIDE

The pre-built files found here can be installed on the following products:

- [AVR64DU32 Curiosity Nano : EV59F82A](https://www.microchip.com/en-us/development-tool/ev59f82a)

> [!NOTE]
> Don't buy/use the wrong product. Only this model number is correct.

AVRDUDE>=8.0 is required for this to work. Install it on your host PC in your preferred way.

- [AVRDUDE releases](https://github.com/avrdudes/avrdude/releases) @8.0+

> [!TIP]
> AVRDUDE<=7.3 actually works, but it doesn't come with part definitions for the AVR-DU family.
> You need to edit the configuration file yourself.

## Setting up Curiosity Nano

Are you ready?

Orient the AVR logo so it is readable, then connect a USB-C type cable between the programming port on the left and the host PC. Upload the file using the following command line:

```sh
avrdude -c pkobn_updi -p avr64du32 -e -U fuses:w:AVR64DU32_CNANO.fuse:i -U flash:w:AVR64DU32_CNANO.hex:i
```

Then run an additional command line. This parameter tells AVRDUDE what kind of writer you want to emulate. Let's say it's PICKit4. This is required if you want to use AVRDUDE<=7.3.

```sh
avrdude -c pkobn_updi -p avr64du32 -U eeprom:w:0xEB,0x03,0x77,0x21:m
```

The Green LED will flash while programming is in progress. If successful the Orange LED will flash. Press and hold SW0 next to the Orange LED. Does the blinking pattern change? If so, this is OK.

## Next Step

Reconnect the USB-C cable to the application port on the right. The adjacent jumper header should be closed.

For Windows, it will take some time to adjust the driver at first. If it fails, please try connecting again.

If everything works perfectly, the Orange LED will slowly light up and you will begin to take deep breaths.

Now try entering the following command. It will stop with an error because the target device is not yet connected, but you should be able to read the UPDI4AVR's firmware version, unique serial number, and operating voltage from the host PC.

```sh
avrdude -c pickit4_updi -p avr64du32 -v
```

```console
Avrdude version 8.0-20241010 (0b92721a)
Copyright see https://github.com/avrdudes/avrdude/blob/main/AUTHORS

System wide configuration file is /usr/local/etc/avrdude.conf
User configuration file is /Users/user/.avrduderc

Using port            : usb
Using programmer      : pickit4_updi
AVR part              : AVR64DU32
Programming modes     : SPM, UPDI
Programmer type       : JTAGICE3_UPDI
Description           : MPLAB(R) PICkit 4 in UPDI mode
ICE HW version        : 0
ICE FW version        : 1.33 (rel. 46)
Serial number         : **********
Vtarget               : 5.01 V
PDI/UPDI clk          : 225 kHz

Error: hid_read_timeout(usb, 64, 10000) failed
Retrying with external reset applied
Error: unable to write 64 bytes to USB
Jtag3_edbg_send(): unable to send command to serial port
Error: unable to write 64 bytes to USB
Jtag3_edbg_recv_frame(): unable to send CMSIS-DAP vendor command
Retrying with external reset applied
Error: initialization failed  (rc = -1)
 - double check the connections and try again
 - use -B to set lower the bit clock frequency, e.g. -B 125kHz
 - use -F to override this check
...

Avrdude done.  Thank you.
```

*Congratulations!* Your "Curiosity Nano" has new life!

----

If you are using AVRDUDE>=8.0, you can use `-Pusb:vid:pid` syntax to identify the USB port without rewriting the VID:PID in an additional HEX file. In this case, the `-c` option allows you to choose between different options such as `jtag3updi`, `atmelice_updi`, `xplainedmini_updi`, `pickit4_updi`, `pkobn_updi`, etc.

```sh
avrdude -P usb:04d8:0b15 -c jtag3updi ...
```

Essentially the behavior and results are exactly the same, but there are differences in the available `-x<opt>` options.

## Practical Usage

To actually use UPDI4AVR, you need to prepare the target device, breadboard, jumper wires, etc., and wire them correctly and reliably. Wiring information is on the [front page](../../README.md).

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)

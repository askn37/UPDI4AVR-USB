/**
 * @file UPDI4AVR-USB.ino
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.32.40+
 * @date 2024-07-10
 * @copyright Copyright (c) 2024 askn37 at github.com
 * @link Potal : https://askn37.github.io/
 *       MIT License : https://askn37.github.io/LICENSE.html
 */

/*
 * This file is always empty.
 * It tells the Arduino IDE to recognize the fileset as a valid sketch.
 * It is assumed that the following SDK is used:
 *
 *   - https://github.com/askn37/multix-zinnia-sdk-modernAVR
 *
 * The SDK menu options are as follows for "AVR64DU32 Curiosity Nano":
 *
 *     Board Manager - AVR DU w/o Bootloader
 *           Variant - 32pin AVR64DU32 (64KiB+8KiB)
 *         Clock(Dx) - Internal 20 MHz (recommend)  : Maximum speed available on die with Errata.
 *          FUSE PF6 - PF6 pin=GPIO (input only)    : Not change this, SW0 will not be usable.
 *      Build option - Build Release (default)      : or DEBUG=1, DEBUG=2
 *   Console and LED - UART1 TX:PD6 RX:PD7 LED=PF2 SW=PF6 (AVR64DU32 Curiosity Nano)
 *           Console - CONSOLE_BAUD=500000 bps      : (using DEBUG) The maximum speed that can be used with the on-board debugger.
 *        Proggramer - Curiosity Nano (nEDBG: ATSAMD21E18)
 */

// end of code

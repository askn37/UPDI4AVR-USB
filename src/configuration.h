/**
 * @file configuration.h
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.35.50+
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#pragma once
#include <avr/io.h>
#include <api/macro_api.h>  /* interrupts, initVariant */
#include <variant.h>

/********************************
 * Hardware layer configuration *
 ********************************/

/*** How to use "usrdef.h" ***/
/*
 * Create a file named "usrdef.h" and declare the HAL profile
 * you wish to use for `HAL_PROFILE` within it.
 * The HAL profiles are located in the `HAL` directory.

  // Contents of usrdef.h (example)
  #define HAL_PROFILE "HAL/AVRDU_CNANO.h"

* You can also declare or redefine other macros (such as `CONSOLE_BAUD`) here.
* You may also add your own custom HAL profile.
* To do so, copy and modify an existing profile, then save it under a new name.
* If you do not follow this procedure, the HAL profile
* corresponding to the currently selected MCU will be used.
*/


/************************
 * Global configuration *
 ***********************/

/*
 * When the DEBUG symbol is enabled, DBG-COM is enabled.
 *
 * Not available on 14P/20P.
 *
 * The DEBUG=0 output is not normally used,
 * but can be used to filter only user-defined output.
 */

// #define DEBUG 2


/*
 * UPDI/PDI/ISP Program interface operating clock.
 * In avrdude this can be changed with `-B125khz` etc.
 */

#define UPDI_CLK 225
#define PDI_CLK  2500
#define ISP_CLK  200


/*
 * TPI Program interface operating clock.
 * This cannot be changed with avrdude and will always use this value.
 */

#define TPI_CLK  250


/*** CONFIG_SYS ***/

/*
 * JTAGICE3 FW versions:
 *
 * This is notified to MPLAB-X etc.
 * Set it a bit higher than the real "Curiosity Nano" value.
 * This will almost always avoid notifications of incompatible version upgrades,
 * but it's not perfect.
 *
 * Columns: HW_VER, FW_MAJOR, FW_MINOR, FW_REL(word,LE) 
 */

#define HW_VER   0
#define FW_MAJOR 1
#define FW_MINOR 35
#define FW_REL   53


/*** CONFIG_USB ***/

/*
 * USB VID:PID Pair Selection:
 *
 *   Enable any of the following if desired.
 *   If all are disabled, the defaults will be selected.
 *   This information is stored in the file for the EEPROM.
 *
 *   The default value is `0x04D8,0x0B15`.
 *
 * WARNING:
 *
 *   AVRDUDE<=7.3 will not work with any VID other than 0x03EB==ATML.
 *   For AVRDUDE>=8.0 this is not necessary as you can use the `-P usb:vid:pid` format.
 *
 * For more details, see the NOTE in <usb.cpp> and <eeprom.cpp>.
 */

#ifndef CONFIG_USB_VIDPID
  /* ATMEL license group : Can be used with older AVRDUDE<=7.3 */
//#define CONFIG_USB_VIDPID 0x03EB,0x2177   /* Compatible pickit4_updi/tpi : PICKit4 nEDBG */
//#define CONFIG_USB_VIDPID 0x03EB,0x2175   /* Compatible pkobn_updi : Curiosity Nano series nEDBG */
//#define CONFIG_USB_VIDPID 0x03EB,0x2145   /* Compatible xplainedmini_updi/tpi : Xplained Mini series mEDBG */
//#define CONFIG_USB_VIDPID 0x03EB,0x2111   /* Compatible xplainedpro_updi : Xplained Pro series mEDBG */
//#define CONFIG_USB_VIDPID 0x03EB,0x2110   /* Compatible jtag3updi : ATMELICE3 EDBG */

  /* V-USB license group : AVRDUDE>=8.0 and additional `-P usb:vid:pid` */
//#define CONFIG_USB_VIDPID 0x16C0,0x05DC   /* V-USB(VOTI) ObjDev's free shared PID for libusb */
//#define CONFIG_USB_VIDPID 0x16C0,0x05DF   /* V-USB(VOTI) ObjDev's free shared PID for HID */
//#define CONFIG_USB_VIDPID 0x16C0,0x05E1   /* V-USB(VOTI) ObjDev's free shared PID for CDC-ACM */

  /* Other license group : AVRDUDE>=8.0 and additional `-P usb:vid:pid` */
//#define CONFIG_USB_VIDPID 0x04D8,0x0B15   /* Default: Allocated for Microchip Tech. AVR-DU series CDC-ACM (no license violations) */
//#define CONFIG_USB_VIDPID 0x04D8,0x0B12   /* Default: Allocated for Microchip Tech. AVR-DU series HID (no license violations) */
//#define CONFIG_USB_VIDPID 0x04D8,0x000A   /* Allocated for Microchip Tech. USB Serial / CDC RS-232 Emulation Demo */
//#define CONFIG_USB_VIDPID 0x04D8,0x9012   /* Compatible pickit4_updi/tpi : PICKit4 nEDBG (Final shipping product?) */
#endif


/*
 * USB Serial Number : 8-digit hexadecimal number
 *
 * If nothing is specified or if the value is 0xFFFFFFFF,
 * a random value will be generated automatically from the chip's factory settings.
 *
 * This information is stored in the EEPROM.
 */

#ifndef CONFIG_USB_SERIALNUMBER
//#define CONFIG_USB_SERIALNUMBER 0x12345678
#endif


/*** CONFIG_VCP ***/

/*
 * When VCP-DTR changes, it sends a RESET signal to the target device.
 *
 * This reboots the Arduino-IDE compatible bootloader.
 * Ignored during Program mode.
 * Disable this if you don't want to reboot when hot swapping.
 */

#define CONFIG_VCP_DTR_RESET


/*
 * Supports VCP interrupts.
 *
 * Used to notify the host PC of VCP errors and RS232 contact status.
 * There is little harm in disabling it.
 */

#define CONFIG_VCP_INTERRUPT_SUPPRT


/*** CONFIG_HVC ***/

/*
 * Enables HV control.
 *
 * Not available in 14P package.
 * It will not function without external support circuitry.
 */

#define CONFIG_HVC_ENABLE


/*** CONFIG_PGM ***/

/*
 * Enable PDI type programming support.
 *
 * 3.3V operating voltage support is required. 5V operation is prohibited.
 * CNANO must be pre-configured for 3.3V operation.
 */

#define CONFIG_PGM_PDI_ENABLE


/*
 * Enable ISP type programming support.
 *
 * It supports ISP programming via the AVR-SPI
 * (4-line Serial Programming Interface) method.
 * For some device models, HV=12V programming may be
 * possible (requiring hardware support).
 * In any case, AVR-PP/HVPP is not supported.
 */

#define CONFIG_PGM_ISP_ENABLE


/*
 * External Clock Supply for ISP Control
 *
 * When enabled, a 2 MHz clock is output on PA6 during ISP operations.
 * This can be connected to the XTAL1 pin of the ISP target device.
 * An additional delay of at least 250 ms is required for the clock
 * to stabilize and be accepted.
 * Access the target device using a setting of `-B125` or slower.
 *
 * The minimum specifiable value is `4`. This value must be an even number;
 * consequently, CLK_PER/4 represents the maximum frequency. Since the AVR-DU
 * family is limited to operating its USB peripherals at 12,16,20, or 24 MHz,
 * the default value is set to their greatest common divisor.
 */

#define CONFIG_PGM_EXCLK_ENABLE (F_CPU / 2000000L)


/*
 * Enable the VTG-Power switch.
 *
 * Attempts to discharge target power just before HV control.
 * This ensures that power-on reset disables GPIOs.
 *
 * It will not function without external support circuitry.
 *
 *   Typical design is to discharge a 15mA load.
 *   Any external power source stronger than this will not turn off.
 */

#define CONFIG_PGM_VPOWER_ENABLE


/********************************
 * Do not change it after this. *
 ********************************/

#ifndef HAL_PROFILE
  #include "usrdef.h"
#endif
#ifndef HAL_PROFILE
  #if defined(__AVR_AVR64DU32__) && (LED_BUILTIN == PIN_PF2) && (SW_BUILTIN == PIN_PF6)
    #define HAL_PROFILE "HAL/AVRDU_CNANO.h"
  #elif defined(AVR_AVRDU14)
    #define HAL_PROFILE "HAL/AVRDU_14P.h"
  #elif defined(AVR_AVRDU20)
    #define HAL_PROFILE "HAL/AVRDU_20P.h"
  #elif defined(AVR_AVRDU28)
    #define HAL_PROFILE "HAL/AVRDU_28P.h"
  #elif defined(AVR_AVRDU32)
    #define HAL_PROFILE "HAL/AVRDU_32P.h"
  #else
    #error There are no hardware profiles to select.
    #include BUILD_STOP
  #endif
#endif

#include HAL_PROFILE

// end of header

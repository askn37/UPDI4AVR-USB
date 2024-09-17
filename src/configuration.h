/**
 * @file configuration.h
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.32.40+
 * @date 2024-07-10
 * @copyright Copyright (c) 2024 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#pragma once
#include <avr/io.h>
#include "api/macro_api.h"  /* interrupts, initVariant */
#include "variant.h"

/********************************
 * Hardware layer configuration *
 ********************************/

/* The following hardware layer are preset: */

#define HAL_BAREMETAL_14P 14  /* AVR16-32DU14 */
#define HAL_BAREMETAL_20P 20  /* AVR16-32DU20 */
#define HAL_BAREMETAL_28P 28  /* AVR16-64DU28 */
#define HAL_BAREMETAL_32P 32  /* AVR16-64DU32 */
#define HAL_CNANO         33  /* AVR64DU32 Curiosity Nano: LED=PF2 SW=PF6 DBG_UART=PD6 */

/*** Choose a symbol from the list above. *+*/
/* If disabled, it will be automatically detected */

// #define CONFIG_HAL_TYPE HAL_CNANO

/*
 * HAL_BAREMETAL_14P: The AVR32-16DU14 has the following limitations:
 * 
 * - Only UPDI mode is available. TPI mode is not available.
 * - HV and POWER control is not available.
 * - The following I/O signal cannot be used: DSR, RTS, CTS, RI, DCD, nRST.
 * - When using DEBUG-COM port, the following control signals are not available: DTR, VBD.
 * 
 *                             AVR32-16DU14
 *                               +------+
 *                         GND - |1   14| - VDD
 *                   SW0 - PF6 - |2   13| - PD7 - VBD     (or DBG-RxD)
 *   (Self program UPDI) - PF7 - |3   12| - PD6 - VCP-DTR (or DBG-TxD)
 *               VCP-RxD - PA0 - |4   11| - PD5 - TRST    (to Target nRST program pad)
 *               VCP-TxD - PA1 - |5   10| - PD4 - TDAT    (to Target UPDI program pad)
 *                      | VUSB - |6    9| - PC3 - LED0
 *  for Application USB |   DM - |7    8| - DP  - for Application USB
 *                               +------+
 * 
 * - If CONFIG_SYS_SW0_ALT_14P is enabled, the pin definitions go to:
 * 
 *                PF6 - nRST
 *                PD7 - VBD
 *                PD6 - SW0
 *               (DEBUG is Disable)
 */

/*
 * HAL_BAREMETAL_20P: The AVR32-16DU20 has the following limitations:
 * 
 * - The following I/O signal cannot be used: DSR, RI, DCD.
 * - When using DEBUG-COM port, the following control signals are not available: RTS, VBD.
 * 
 *                                          AVR32-16DU20
 *                                            +------+
 *                                      GND - |1   20| - VDD
 *                                SW0 - PF6 - |2   19| - PD7 - VBD     (or DBG-RxD)
 *                (Self program UPDI) - PF7 - |3   18| - PD6 - VCP-RTS (or DBG-TxD)
 *  (to Target UPDI program pad) TDAT - PA0 - |4   17| - PD5 - VCP-DTR
 *  (to Target nRST program pad) TRST - PA1 - |5   16| - PD4 - VCP-CTS 
 *                      TCLK, VCP-TxD - PA2 - |6   15| - PC3 - HVFB
 *                            VCP-RxD - PA3 - |7   14| - DP   |
 *                               HVCP - PA4 - |8   13| - DM   | for Application USB
 *                               HVSL - PA5 - |9   12| - VUSB |
 *                               LED0 - PA6 - |10  11| - PA7 - HVSW
 *                                            +------+
 * 
 * - If CONFIG_VCP_RS232C_ENABLE is disabled, the pin definitions go to:
 * 
 *                PF6 - nRST
 *                PD7 - DBG-RxD or N.C.
 *                PD6 - DBG-TxD or N.C.
 *                PD5 - SW0
 *                PD4 - VBD
 */

/*
 * HAL_BAREMETAL_28P: The AVR64-16DU28 has the following limitations:
 * 
 *                            AVR64-16DU28
 *                              +------+
 *                 HVSW - PA7 - |1   28| - PA6 - LED0
 *                     | VUSB - |2   27| - PA5 - HVSL
 * for Application USB |   DM - |3   26| - PA4 - HVCP
 *                     |   DP - |4   25| - PA3 - VCP-RxD
 *                 HVFB - PC3 - |5   24| - PA2 - VCP-TxD, TCLK
 *              VCP-DCD - PD0 - |6   23| - PA1 - TRST (to Target nRST program pad)
 *              VCP-DSR - PD1 - |7   22| - PA0 - TDAT (to Target UPDI program pad)
 *              VCP-CTS - PD2 - |8   21| - GND
 *              VCP-RI  - PD3 - |9   20| - VDD
 *              VCP-DTR - PD4 - |10  19| - PF7 - (Self program UPDI)
 *              VCP-RTS - PD5 - |11  18| - PF6 - (Self program nRST)
 *              DBG-TxD - PD6 - |12  17| - PF1 - SW0
 *              DBG-RxD - PD7 - |13  16| - PF0 - VBD
 *                        VDD - |14  15| - GND
 *                              +------+
 */

/*
 * HAL_BAREMETAL_32P: Almost the same as HAL_BAREMETAL_28P
 *
 *        PF0,PF1,PF2 - not used (N.C.)
 *                PF3 - HVPW
 *                PF4 - VBD
 *                PF5 - SW0
 */

/*
 * HAL_CNANO: The AVR64DU32 Curiosity Nano has the following limitations:
 * 
 *                                    AVR64DU32 Curiosity Nano
 * 
 *                                      for Programming USB-C
 *                                            +--| |--+
 *                                            | nEDBG |
 *                                            +-------+
 *  (to Target UPDI program pad) TDAT - PA0 - |7    32| - PF4 - VBD
 *  (to Target nRST program pad) TRST - PA1 - |8    31| - PF5 - N.C. (or SW0)
 *                      TCLK, VCP-TxD - PA2 - |9    30| - PD0 - VCP-DCD
 *                            VCP-RxD - PA3 - |10   29| - PD1 - VCP-DSR
 *                               HVCP - PA4 - |11   28| - PD2 - HVFB
 *                               HVSL - PA5 - |12   27| - PD3 - VCP-RI
 *                               HVPW - PA6 - |13   26| - PD4 - VCP-DTR
 *                               HVSW - PA7 - |14   25| - PD5 - VCP-RTS
 *                                      GND - |15   24| - GND
 *                            DBG-TxD - PD6 - |16   23| - PC3 - N.C.
 *                            DBG-RxD - PD7 - |17   22| - PF3 - VCP-CTS
 *        (or Self program nRST)  SW0 - PF6 - |18   21| - PF0 - N.C.
 *                  (Low active) LED0 - PF2 - |19   20| - PF1 - N.C.
 *                                            +--| |--+
 *                                      for Application USB-C
 * 
 * NOTE:
 * 
 * - PIN_PC3 is connected to a resistor divider as the AC0 input for on-board
 *   VBUS detection, but it is bus-powered only and will not function properly
 *   when two USB-C cables are connected, so it is not used in this application.
 * - PIN_PF0,1 are reserved for external 32k crystal oscillators and are not used.
 * - Because HVFW and HVSW have exclusive use of AC0 inputs and outputs,
 *   you cannot move the definitions from PIN_PD2 and PIN_PA7.
 */

/*
 * Signal name details
 * 
 *    PGM
 *        TDAT - PIN_PG_TDAT      TPI-Data or UPDI interface
 *        TRST - PIN_PG_TRST      TPI-Reset
 *        TCLK - PIN_PG_TCLK      TPI-Clock (Also used for VCP-TxD : Other than 14P)
 * 
 *    VCP
 *         TxD - PIN_VCP_TXD      Data OUT (Also used for TCLK)
 *         RxD - PIN_VCP_RXD      Data IN
 *         DCD - PIN_VCP_DCD      Device-Carrier-Detect IN (RxCarrier)
 *          RI - PIN_VCP_RI       Ring interrupt IN
 *         DSR - PIN_VCP_DSR      Flow-control IN (TxCarrier)
 *         DTR - PIN_VCP_DTR      Flow-control OUT
 *         RTS - PIN_VCP_RTS      Flow-control OUT
 *         CTS - PIN_VCP_CTS      Flow-control IN (TxCarrier)
 * 
 *            NOTE: DCD, DSR, and RI must use PIN_PD0, PD1, and PD3.
 * 
 *    SYS
 *     DBG-TxD -                  System self debug (default 500kbps)
 *     DBG-RxD -                  System self debug (reserved)
 *        UPDI -                  System self programing
 *        nRST -                  System self reset
 *        LED0 - PIN_SYS_LED      System indicator
 *         SW0 - PIN_SYS_SW0      System mode switch
 *         VBD - PIN_USB_VDETECT  USB live line detection
 * 
 *    HVCTRL
 *        HVFB - PIN_HV_FEEDBACK  Voltage Measurement Feedback (Analog-Comparator-IN-Positive)
 *        HVSW - PIN_HV_SWITCH    High Voltage Generation FET Switching
 *        HVCP - PIN_HV_CHGPUMP   Charge pump pulse output
 *        HVSL - PIN_HV_SELECT    Voltage application terminal selection
 *        HVPW - PIN_HV_POWER     Extra Target Power Control
 */

/************************
 * Global configuration *
 ***********************/

/*
 * When the DEBUG symbol is enabled, DBG-COM is enabled.
 *
 * Functions that use overlapping pins are disabled.
 * 
 * The DEBUG=0 output is not normally used,
 * but can be used to filter only user-defined output.
 */

// #define DEBUG 2

/*
 * UPDI/TPI Program interface operating clock: default is 225kHz.
 */

#define UPDI_CLK  225000L   /* The selection range is 40kHz to 240kHz. */

/*** CONFIG_SYS ***/

/*
 * JTAGICE3 FW versions:
 *
 * This is notified to MPLAB-X etc.
 * Set it a bit higher than the real "Curiosity Nano" value.
 * This will almost always avoid notifications of incompatible version upgrades,
 * but it's not perfect.
 *
 * Columns: HW_VER, FW_MAJOR, FW_MINOR, FW_RELL, FW_RELH (all 1-byte decimal)
 */

#define CONFIG_SYS_FWVER { 0, 1, 32, 44, 0 }

/*
 * For CNANO, you can change the detection of SW0 to PIN_PF5.
 *
 * The default value is PIN_PF6, but must be changed if used for system reset.
 * 
 * The SW0 pin is a pull-up input.
 * Should be equipped with a coupling capacitor and a series resistor to prevent chattering.
 */

// #define CONFIG_SYS_SW0_ALT_CNANO

/*
 * For HAL_BAREMETAL_14P you can change SW0 to PD6.
 *
 * This will disable DEBUG and replace PF6 with nRST.
 */

// #define CONFIG_SYS_SW0_ALT_14P

/*** CONFIG_USB ***/

/*
 * USB VID:PID Pair Selection:
 *
 *   Enable any of the following if desired.
 *   If all are disabled, the defaults will be selected.
 *   This information is stored in the file for the EEPROM.
 *
 * WARNING:
 * 
 *   AVRDUDE<=7.3 will not work with any VID other than 0x03EB==ATML.
 *   For AVRDUDE>=7.4 this is not necessary as you can use the `-P usb:vid:pid` format.
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

  /* V-USB license group : AVRDUDE>=7.4 and additional `-P usb:vid:pid` */
//#define CONFIG_USB_VIDPID 0x16C0,0x05DC   /* V-USB(VOTI) ObjDev's free shared PID for libusb */
//#define CONFIG_USB_VIDPID 0x16C0,0x05DF   /* V-USB(VOTI) ObjDev's free shared PID for HID */
//#define CONFIG_USB_VIDPID 0x16C0,0x05E1   /* V-USB(VOTI) ObjDev's free shared PID for CDC-ACM */

  /* Other license group : AVRDUDE>=7.4 and additional `-P usb:vid:pid` */
//#define CONFIG_USB_VIDPID 0x04D8,0x0B15   /* Default: Allocated for Microchip Tech. AVR-DU series CDC-ACM (no license violations) */
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

/*
 * Enable VBUS detection:
 * 
 * The VBD pin is a pull-up input, it should be kept LOW when the USB cable is unplugged.
 * A reset IC should be used, make sure the off period is at least 250 ms.
 */

#define CONFIG_USB_VDETECT

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
 * Enables RS232C signal GPIO support
 *
 * Sets the DCD, RI, DSR, DTR, RTS, and CTS signals for RS232C compatibility to GPIO.
 * If disabled, the corresponding pin terminals remain unused.
 * 
 * NOTE: DCD, DSR and RI always use the pin terminals specified
 * 　　　 in the VPOTRTD_IN register. (Parallel input)
 */

#define CONFIG_VCP_RS232C_ENABLE

/*
 * Enables the VCP-CTS input signal. Disabled by default.
 *
 * When enabled, VCP-TxD will not transmit data unless CTS is LOW.
 * When enabled and unused, CTS must be shorted to GND.
 * Affected by CONFIG_VCP_RS232C_ENABLE.
 */

// #define CONFIG_VCP_CTS_ENABLE

/*
 * Support VCP interrupts.
 *
 * If you disable it, you will lose some lesser used features in Windows.
 * Affected by CONFIG_VCP_RS232C_ENABLE.
 */

#define CONFIG_VCP_INTERRUPT_SUPPRT

/*
 * Make VCP-TxD port open-drain: default is push-pull.
 * 
 * We recommend that you enable this signal because it is also used
 * as the TCLK signal for TPI programming.
 * When the CDC host issues a BREAK command, this signal goes LOW during the command.
 */

#define CONFIG_VCP_TXD_ODM

/*
 * Experimental: Supports 9-bit character mode.
 *
 * The USB host must be able to send and receive data in 2-byte units.
 */

// #define CONFIG_VCP_9BIT_SUPPORT

/*** CONFIG_HVCTRL ***/

/*
 * Enables HV control. (Not yet implemented)
 * using the following I/O signals: HVFB, HVSW, HVSL, HVCP
 * 
 * Not available in 14P package.
 * Requires additional external circuitry to enable.
 */

// #define CONFIG_HVCTRL_ENABLE

/*
 * Enables control of the HVPW terminal: Can be used with 32P and CNANO
 *
 * When supplying power to a V-target, it can control power-off.
 * Not used when power is supplied from the V-target.
 * If the HVPW terminal is used directly as a power output, it must not exceed 15mA.
 * 
 * Not affected by CONFIG_HVCTRL_ENABLE.
 */

#define CONFIG_HVCTRL_POWER_ENABLE

/*** CONFIG_PGM ***/

/*
 * Enable TPI type programming support
 *
 * When disabled, the size is approximately 2KiB smaller.
 * Cannot be used with 14P model. 
 */

#define CONFIG_PGM_TPI_ENABLE

/*
 * Enable PDI type programming support. (Not yet implemented)
 *
 * Cannot be used with 14P model. 
 */

// #define CONFIG_PGM_PDI_ENABLE

/********************************
 * Do not change it after this. *
 ********************************/

#if (CONFIG_HAL_TYPE == HAL_CNANO) && !defined(AVR_AVRDU32)
  #undef CONFIG_HAL_TYPE
#endif
#ifndef CONFIG_HAL_TYPE
  #if defined(AVR_AVRDU14)
    #define CONFIG_HAL_TYPE HAL_BAREMETAL_14P
  #elif defined(AVR_AVRDU20)
    #define CONFIG_HAL_TYPE HAL_BAREMETAL_20P
  #elif defined(AVR_AVRDU28)
    #define CONFIG_HAL_TYPE HAL_BAREMETAL_28P
  #elif defined(AVR_AVRDU32)
    #if (LED_BUILTIN == PIN_PF2) && (SW_BUILTIN == PIN_PF6)
      #define CONFIG_HAL_TYPE HAL_CNANO
    #else
      #define CONFIG_HAL_TYPE HAL_BAREMETAL_32P
    #endif
  #else
    #error There are no hardware profiles to select.
    #include BUILD_STOP
  #endif
#endif

/*** To override the default value at the CLI level: ***/

#ifdef CONFIG_USB_VDETECT_DISABLE
  #undef CONFIG_USB_VDETECT
#endif
#ifdef CONFIG_VCP_DTR_RESET_DISABLE
  #undef CONFIG_VCP_DTR_RESET
#endif
#ifdef CONFIG_VCP_RS232C_DISABLE
  #undef CONFIG_VCP_RS232C_ENABLE
#endif
#ifdef CONFIG_VCP_CTS_DISABLE
  #undef CONFIG_VCP_CTS_ENABLE
#endif
#ifdef CONFIG_VCP_INTERRUPT_SUPPRT_DISABLE
  #undef CONFIG_VCP_INTERRUPT_SUPPRT
#endif
#ifdef CONFIG_VCP_TXD_ODM_DISABLE
  #undef CONFIG_VCP_TXD_ODM
#endif
#ifdef CONFIG_VCP_9BIT_SUPPORT_DISABLE
  #undef CONFIG_VCP_9BIT_SUPPORT
#endif
#ifdef CONFIG_HVCTRL_ENABLE_DISABLE
  #undef CONFIG_HVCTRL_ENABLE
#endif
#ifdef CONFIG_HVCTRL_POWER_DISABLE
  #undef CONFIG_HVCTRL_POWER_ENABLE
#endif
#ifdef CONFIG_PGM_TPI_DISABLE
  #undef CONFIG_PGM_TPI_ENABLE
#endif
#ifdef CONFIG_PGM_PDI_DISABLE
  #undef CONFIG_PGM_PDI_ENABLE
#endif

/*** Note: The pin numbers of VCP-DCD,DSR,RI are fixed as PIN_PD0,1,3. ***/

#if (CONFIG_HAL_TYPE == HAL_BAREMETAL_14P)
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_UPDI  (PORTMUX_USART0_ALT3_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD
  #define PIN_VCP_RXD         PIN_USART0_RXD
  #define PIN_PG_TDAT         PIN_USART0_TXD_ALT3
  #define PIN_PG_TRST         PIN_USART0_RXD_ALT3
  #define PIN_SYS_LED         PIN_PC3      /* PIN_LUT1_OUT */
  #undef CONFIG_VCP_CTS_ENABLE
  #undef CONFIG_PGM_TPI_ENABLE
  #undef CONFIG_HVCTRL_ENABLE
  #undef CONFIG_HVCTRL_POWER_ENABLE
  #ifdef CONFIG_SYS_SW0_ALT_14P
    #undef DEBUG
    #define PIN_USB_VDETECT   PIN_PD7
    #define PIN_SYS_SW0       PIN_PD6
  #else
    #define PIN_SYS_SW0       PIN_PF6
    #if !defined(DEBUG)
      #define PIN_USB_VDETECT PIN_PD7
      #define PIN_VCP_DTR     PIN_PD6
    #endif
  #endif
#else
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_UPDI  (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
  #define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
  #define PIN_PG_TDAT         PIN_USART0_TXD
  #define PIN_PG_TRST         PIN_USART0_RXD
  #define PIN_PG_TCLK         PIN_USART0_XCLK   /* alias PIN_VCP_TXD */
  #define PIN_HV_SWITCH       PIN_PA7
  #define PIN_HV_SELECT       PIN_PA5
  #define PIN_HV_CHGPUMP      PIN_PA4      /* PIN_TCA0_WO4 */
  #if (CONFIG_HAL_TYPE == HAL_BAREMETAL_20P)
    #ifdef CONFIG_VCP_RS232C_ENABLE
      #if !defined(DEBUG)
        #define PIN_USB_VDETECT PIN_PD7
        #define PIN_VCP_RTS     PIN_PD6
      #endif
      #define PIN_SYS_SW0     PIN_PF6
      #define PIN_VCP_DTR     PIN_PD5
      #define PIN_VCP_CTS     PIN_PD4
    #else
      #define PIN_SYS_SW0     PIN_PD5
      #define PIN_USB_VDETECT PIN_PD4
    #endif
    #define PIN_HV_FEEDBACK   PIN_PC3     /* PIN_AC0_AINP4 */
    #define PIN_SYS_LED0      PIN_PA6 /* PIN_LUT0_OUT_ALT1 */
  #elif (CONFIG_HAL_TYPE == HAL_BAREMETAL_28P)
    #define PIN_SYS_SW0       PIN_PF1
    #define PIN_USB_VDETECT   PIN_PF0
    #define PIN_VCP_RTS       PIN_PD5
    #define PIN_VCP_DTR       PIN_PD4
    #define PIN_VCP_RI        PIN_PD3
    #define PIN_VCP_CTS       PIN_PD2
    #define PIN_VCP_DSR       PIN_PD1
    #define PIN_VCP_DCD       PIN_PD0
    #define PIN_HV_FEEDBACK   PIN_PC3     /* PIN_AC0_AINP4 */
    #define PIN_SYS_LED0      PIN_PA6 /* PIN_LUT0_OUT_ALT1 */
  #elif (CONFIG_HAL_TYPE == HAL_BAREMETAL_32P)
    #define PIN_SYS_SW0       PIN_PF5
    #define PIN_USB_VDETECT   PIN_PF4
    #define PIN_HV_POWER      PIN_PF3
    #define PIN_VCP_RTS       PIN_PD5
    #define PIN_VCP_DTR       PIN_PD4
    #define PIN_VCP_RI        PIN_PD3
    #define PIN_VCP_CTS       PIN_PD2
    #define PIN_VCP_DSR       PIN_PD1
    #define PIN_VCP_DCD       PIN_PD0
    #define PIN_HV_FEEDBACK   PIN_PC3     /* PIN_AC0_AINP4 */
    #define PIN_SYS_LED0      PIN_PA6 /* PIN_LUT0_OUT_ALT1 */
  #else /* CNANO */
    #ifdef CONFIG_SYS_SW0_ALT_CNANO
      #define PIN_SYS_SW0     PIN_PF5
    #else
      #define PIN_SYS_SW0     PIN_PF6
    #endif
    #define PIN_USB_VDETECT   PIN_PF4
    #define PIN_VCP_CTS       PIN_PF3
    #define PIN_SYS_LED0      PIN_PF2        /* PIN_EVOUTF */
    #define PIN_VCP_RTS       PIN_PD5
    #define PIN_VCP_DTR       PIN_PD4
    #define PIN_VCP_RI        PIN_PD3
    #define PIN_HV_FEEDBACK   PIN_PD2     /* PIN_AC0_AINP0 */
    #define PIN_VCP_DSR       PIN_PD1
    #define PIN_VCP_DCD       PIN_PD0
    #define PIN_HV_POWER      PIN_PA6
  #endif
#endif
#ifndef CONFIG_VCP_CTS_ENABLE
  #undef PIN_VCP_CTS
#endif
#ifndef CONFIG_PGM_TPI_ENABLE
  #undef PIN_PG_TCLK
#endif
#ifndef CONFIG_HVCTRL_POWER_ENABLE
  #undef PIN_HV_POWER
#endif
#ifndef CONFIG_USB_VDETECT
  #undef PIN_USB_VDETECT
#endif
#ifndef CONFIG_HVCTRL_ENABLE
  #undef PIN_HV_FEEDBACK
  #undef PIN_HV_SWITCH
  #undef PIN_HV_SELECT
  #undef PIN_HV_CHGPUMP
#endif
#ifndef CONFIG_VCP_RS232C_ENABLE
  #undef PIN_VCP_DCD
  #undef PIN_VCP_DSR
  #undef PIN_VCP_RI
  #undef PIN_VCP_DTR
  #undef PIN_VCP_RTS
  #undef PIN_VCP_CTS
#endif

// end of header

/**
 * @file configuration.h
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.32.45+
 * @date 2024-08-26
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

#define HAL_BAREMETAL_14P 14  /* AVR16-32DU14 : without HV, Bus-powerd 5V only */
#define HAL_BAREMETAL_20P 20  /* AVR16-32DU20 */
#define HAL_BAREMETAL_28P 28  /* AVR16-64DU28 : UPDI4AVR-USB Type-F Reference Design */
#define HAL_BAREMETAL_32P 32  /* AVR16-64DU32 : alias 28P */
#define HAL_CNANO         33  /* AVR64DU32 Curiosity Nano, 5V only LED=PF2 SW=PF6 DBG_UART=PD6 */

/*** Choose a symbol from the list above. *+*/
/* If disabled, it will be automatically detected */

// #define CONFIG_HAL_TYPE HAL_CNANO

/*
 * Pin layout by package:
 *
 *         14P   20P   28P   32P   CNANO
 *    PA0  VTxD  TDAT  TDAT  TDAT  TDAT
 *    PA1  VRxD  TRST  TRST  TRST  TRST
 *    PA2  -     VTxD  VTxD  VTxD  VTxD
 *    PA3  -     VRxD  VRxD  VRxD  VRxD
 *    PA4  -     PDIS  SW0   SW0   HVCP1
 *    PA5  -     VPW   VBD   VBD   HVCP2
 *    PA6  -     LED0  LED0  LED0  N.C.
 *    PA7  -     HVSW  LED1  LED1  HVSW
 *    PC3  LED0  HVFB  HVFB  HVFB  Reserved
 *    PD0  -     -     HVSL1 HVSL1 DTR
 *    PD1  -     -     HVSL2 HVSL2 CTS
 *    PD2  -     -     HVSW  HVSW  HVFB
 *    PD3  -     -     VPW   VPW   N.C.
 *    PD4  TDAT  HVCP1 HVCP1 HVCP1 HVSL1
 *    PD5  TRST  HVCP2 HVCP2 HVCP2 HVSL2
 *    PD6  TCLK  HVSL1 DTxD  DTxD  DTxD
 *    PD7  PDIS  HVSL2 DRxD  DRxD  DRxD
 *    PF0  -     -     PDIS  PDIS  Reserved
 *    PF1  -     -     N.C.  N.C.  Reserved
 *    PF2  -     -     -     N.C.  LED0
 *    PF3  -     -     -     N.C.  LED1
 *    PF4  -     -     -     N.C.  VPW
 *    PF5  -     -     -     N.C.  PDIS
 *    PF6  SW0   SW0   DnRST DnRST SW0
 *    PF7  DUPDI DUPDI DUPDI DUPDI DUPDI
 *
 *    * SW0 refers to the SW1 component on the drawing except for CNANO.
 *    * DTR and CTS are only available on CNANO.
 *
 *
 * Peripheral Function Input/Output:
 *
 *         14P   20P   28P   32P   CNANO
 *    AC0I -     PC3   PC3   PC3   PD2
 *    AC0O -     EVOA  EVOD  EVOD  PA6
 *    CCL0 -     PA6   PA6   PA6   EVOF
 *    CCL1 PC3  -      -     -     -
 *    CCL2 SW0   SW0   SW0   SW0   SW0
 *    CCL3 -     -     EVOA  EVOA  PF3
 *    EVOA -     PA7   PA7   PA7   -
 *    EVOD -     -     PD2   PD2   -
 *    EVOF -     -     -     -     PF2
 *    WO4  -     PD4   PD4   PD4   PA4
 *    WO5  -     PD5   PD5   PD5   PA5
 *    PINT PF6   PF6   PA4   PA4   PF6
 *
 *
 * Signal name details:
 *
 *    PGM
 *        TDAT - PIN_PGM_TDAT     TPI-Data or UPDI interface
 *        TRST - PIN_PGM_TRST     TPI-Reset
 *        TCLK - PIN_PGM_TCLK     TPI-Clock or PDI-Clock and PDI-Reset
 *        PDIS - PIN_PGM_PDISEL   PDI-Clock Selector
 *         VPW - PIN_PGM_VPOWER   V-Target Power Control (negative logic)
 *
 *    VCP
 *        VTxD - PIN_VCP_TXD      Data OUT (Also used for TCLK)
 *        VRxD - PIN_VCP_RXD      Data IN
 *         DTR - PIN_VCP_DTR      Flow-control OUT negative logic
 *         CTS - PIN_VCP_CTS      Flow-control IN negative logic (TxCarrier)
 *
 *    SYS
 *        DTxD -                  System self debug (default 8N1,500kbps)
 *        DRxD -                  System self debug (reserved)
 *       DUPDI -                  System self programing
 *       DnRST -                  System self reset
 *        LED0 - PIN_SYS_LED0     System indicator (CNANO: negative logic)
 *        LED1 - PIN_SYS_LED1     VCP trafic indicator positive logic
 *         SW0 - PIN_SYS_SW0      System mode switch negative logic
 *         VBD - PIN_SYS_VDETECT  USB live line detection positive logic
 *
 *    HVC
 *        HVFB - PIN_HVC_FEEDBACK Voltage Measurement Feedback (Analog-Comparator-IN-Positive)
 *        HVSW - PIN_HVC_SWITCH   High Voltage Generation FET Switching output negative logic
 *       HVCP1 - PIN_HVC_CHGPUMP1 Charge pump pulse output positive logic
 *       HVCP2 - PIN_HVC_CHGPUMP2 Charge pump pulse output negative logic
 *       HVSL1 - PIN_HVC_SELECT1  Voltage application terminal select TDAT output negative logic
 *       HVSL2 - PIN_HVC_SELECT2  Voltage application terminal select TRST output negative logic
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
 * UPDI/TPI/TPI Program interface operating clock: default is 225kHz.
 */

#define PGM_CLK 225000L   /* The selection range is 40kHz to 240kHz. */

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

#define CONFIG_SYS_FWVER { 0, 1, 32, 45, 0 }

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
 * Changes VCP-TxD output to push-pull.
 *
 * Normally open-drain to avoid conflict with GPIO.
 * Enable if faster VCP communication is required.
 */

// #define CONFIG_VCP_TXD_PUSHPULL

/*
 * When VCP-DTR changes, it sends a RESET signal to the target device.
 *
 * This reboots the Arduino-IDE compatible bootloader.
 * Ignored during Program mode.
 * Disable this if you don't want to reboot when hot swapping.
 */

#define CONFIG_VCP_DTR_RESET

/*
 * Enables the VCP-CTS input signal. Disabled by default.
 *
 * CNANO only.
 * When enabled, VCP-TxD will not transmit data unless CTS is LOW.
 * If enabled and not used, CTS must be shorted to GND (by JP1).
 */

// #define CONFIG_VCP_CTS_ENABLE

/*
 * Supports VCP interrupts. Disabled by default.
 *
 * Used to notify the host PC of VCP errors and RS232 contact status.
 * Normally unused.
 */

// #define CONFIG_VCP_INTERRUPT_SUPPRT

/*
 * Experimental: Supports 9-bit character mode.
 *
 * USB host must be able to send and receive data in 2-byte chunks.
 * Rarely does this happen on host PCs.
 */

// #define CONFIG_VCP_9BIT_SUPPORT

/*** CONFIG_HVC ***/

/*
 * Enables HV control. (Not yet implemented)
 * using the following I/O signals: HVFB, HVSW, HVSL, HVCP
 *
 * Not available in 14P package.
 * It will not function without external support circuitry.
 */

#define CONFIG_HVC_ENABLE

/*** CONFIG_PGM ***/

/*
 * Enable TPI type programming support.
 *
 * VCP is available with limitations on ATtiny102/104.
 *
 *   The connection between VRxD and PB2 (TxD) always works.
 *   VTxD must be connected to PA0 (TCLK) and PB3 (RxD),
 *   and PA0 (TCLK) should not be used as a GPIO.
 */

#define CONFIG_PGM_TPI_ENABLE

/*
 * Enable PDI type programming support. (Not yet implemented)
 *
 * 3.3V operating voltage support is required. 5V operation is prohibited.
 * Not compatible with VCP without external support circuitry.
 *
 * On 14P models, LED1 and PDIS are mutually exclusive.
 */

// #define CONFIG_PGM_PDI_ENABLE

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

#ifdef CONFIG_VCP_DTR_RESET_DISABLE
  #undef CONFIG_VCP_DTR_RESET
#endif
#ifdef CONFIG_VCP_CTS_DISABLE
  #undef CONFIG_VCP_CTS_ENABLE
#endif
#ifdef CONFIG_VCP_INTERRUPT_DISABLE
  #undef CONFIG_VCP_INTERRUPT_SUPPRT
#endif
#ifdef CONFIG_VCP_9BIT_DISABLE
  #undef CONFIG_VCP_9BIT_SUPPORT
#endif
#ifdef CONFIG_HVCTRL_DISABLE
  #undef CONFIG_HVCTRL_ENABLE
#endif
#ifdef CONFIG_PGM_TPI_DISABLE
  #undef CONFIG_PGM_TPI_ENABLE
#endif
#ifdef CONFIG_PGM_PDI_DISABLE
  #undef CONFIG_PGM_PDI_ENABLE
#endif

/*** Note: The pin numbers of VCP-DCD,DSR,RI are fixed as PIN_PD0,1,3. ***/

#if (CONFIG_HAL_TYPE == HAL_BAREMETAL_14P)
  #undef DEBUG
  #undef CONFIG_HVCTRL_ENABLE
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_ALT3_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD
  #define PIN_VCP_RXD         PIN_USART0_RXD
  #define PIN_PGM_TDAT        PIN_USART0_TXD_ALT3
  #define PIN_PGM_TRST        PIN_USART0_RXD_ALT3
  #define PIN_PGM_TCLK        PIN_USART0_XCK_ALT3
  #define PIN_PGM_PDISEL      PIN_PD7
  #define PIN_SYS_LED0        PIN_LUT1_OUT
  #define PIN_SYS_LED1        PIN_PD7
  #define PIN_SYS_SW0         PIN_PF6

#elif (CONFIG_HAL_TYPE == HAL_BAREMETAL_20P)
  #undef DEBUG
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
  #define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
  #define PIN_PGM_TDAT        PIN_USART0_TXD
  #define PIN_PGM_TRST        PIN_USART0_RXD
  #define PIN_PGM_TCLK        PIN_USART0_XCK
  #define PIN_PGM_PDISEL      PIN_PA4
  #define PIN_PGM_VPOWER      PIN_PA5
  #define PIN_HVC_FEEDBACK    PIN_AC0_AINP4
  #define PIN_HVC_SWITCH      PIN_AC0_OUT
  #define PIN_HVC_SELECT1     PIN_PD6
  #define PIN_HVC_SELECT2     PIN_PD7
  #define PIN_HVC_CHGPUMP1    PIN_TCA0_WO4_ALT3
  #define PIN_HVC_CHGPUMP2    PIN_TCA0_WO5_ALT3
  #define PIN_SYS_LED0        PIN_LUT0_OUT_ALT1
  #define PIN_SYS_SW0         PIN_PF6

#elif (CONFIG_HAL_TYPE == HAL_CNANO)
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
  #define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
  #define PIN_VCP_CTS         PIN_PD0
  #define PIN_VCP_DTR         PIN_PD1
  #define PIN_PGM_TDAT        PIN_USART0_TXD
  #define PIN_PGM_TRST        PIN_USART0_RXD
  #define PIN_PGM_TCLK        PIN_USART0_XCK
  #define PIN_PGM_PDISEL      PIN_PF5
  #define PIN_PGM_VPOWER      PIN_PF4
  #define PIN_HVC_FEEDBACK    PIN_AC0_AINP0
  #define PIN_HVC_SWITCH      PIN_AC0_OUT
  #define PIN_HVC_SELECT1     PIN_PD4
  #define PIN_HVC_SELECT2     PIN_PD5
  #define PIN_HVC_CHGPUMP1    PIN_TCA0_WO4
  #define PIN_HVC_CHGPUMP2    PIN_TCA0_WO5
  #define PIN_SYS_LED0        PIN_EVOUTF
  #define PIN_SYS_LED1        PIN_LUT3_OUT
  #define PIN_SYS_SW0         PIN_PF6

#else /* (CONFIG_HAL_TYPE == HAL_BAREMETAL_28P) || (CONFIG_HAL_TYPE == HAL_BAREMETAL_32P) */
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
  #define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
  #define PIN_PGM_TDAT        PIN_USART0_TXD
  #define PIN_PGM_TRST        PIN_USART0_RXD
  #define PIN_PGM_TCLK        PIN_USART0_XCK
  #define PIN_PGM_PDISEL      PIN_PF0
  #define PIN_PGM_VPOWER      PIN_PD3
  #define PIN_HVC_FEEDBACK    PIN_AC0_AINP4
  #define PIN_HVC_SWITCH      PIN_EVOUTD
  #define PIN_HVC_SELECT1     PIN_PD0
  #define PIN_HVC_SELECT2     PIN_PD1
  #define PIN_HVC_CHGPUMP1    PIN_TCA0_WO4_ALT3
  #define PIN_HVC_CHGPUMP2    PIN_TCA0_WO5_ALT3
  #define PIN_SYS_LED0        PIN_LUT0_OUT_ALT1
  #define PIN_SYS_LED1        PIN_EVOUTA_ALT1
  #define PIN_SYS_SW0         PIN_PA4
  #define PIN_SYS_VDETECT     PIN_PA5

#endif

#ifndef PIN_VCP_CTS
  #undef CONFIG_VCP_CTS_ENABLE
#endif
#ifndef PIN_PGM_VPOWER
  #undef CONFIG_PGM_VPOWER_ENABLE
#endif

#ifndef PIN_HVC_FEEDBACK
  #undef CONFIG_HVC_ENABLE
#endif

// end of header

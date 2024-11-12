/**
 * @file configuration.h
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.33.46+
 * @date 2024-10-07
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
 * Pin layout by package: Design Type MZU2410A2 (28P/32P)
 *
 *         14P   20P   28P   32P   CNANO
 *    PA0  VTxD  TDAT  TDAT  TDAT  TDAT
 *    PA1  VRxD  TRST  VPW   VPW   TRST
 *    PA2  -     VTxD  VTxD  VTxD  VTxD     : Shared with TCLK
 *    PA3  -     VRxD  VRxD  VRxD  VRxD
 *    PA4  -     N.C.  N.C.  N.C.  PDAT
 *    PA5  -     HVSL1 SW0   SW0   VPW
 *    PA6  -     N.C.  TRST  TRST  PCLK
 *    PA7  -     HVSL2 N.C.  N.C.  N.C.
 *    PC3  LED1  LED1  LED1  LED1  N.A.
 *    PD0  -     -     HVSL1 HVSL1 HVSL1
 *    PD1  -     -     HVSL2 HVSL2 HVSL2
 *    PD2  -     -     HVSL3 HVSL3 HVSL3
 *    PD3  -     -     LED0  LED0  N.C.
 *    PD4  TDAT  HVCP1 HVCP1 HVCP1 HVCP1
 *    PD5  TRST  HVCP2 HVCP2 HVCP2 HVCP2
 *    PD6  TCLK  LED0  DTxD  DTxD  DTxD     : 14P TCLK is shared outside with VTxD
 *    PD7  LED0  HVSL3 DRxD  DRxD  DRxD
 *    PF0  -     -     N.C.  N.C.  N.A.
 *    PF1  -     -     N.C.  N.C.  N.A.
 *    PF2  -     -     -     N.C.  LED0
 *    PF3  -     -     -     N.C.  LED1
 *    PF4  -     -     -     N.C.  N.C.
 *    PF5  -     -     -     N.C.  N.C.
 *    PF6  SW0   SW0   DnRST DnRST SW0
 *    PF7  DUPDI DUPDI DUPDI DUPDI DUPDI
 *
 *    * SW0 refers to the SW1 component on the drawing except for CNANO.
 *
 *
 * Peripheral Function Input/Output:
 *
 *         14P   20P   28P   32P   CNANO
 *    CCL0 -     -     -     -     -        : SW0 Falling interrupt
 *    CCL1 PC3   PC3   PC3   PC3   (PF3)    : LED1
 *    CCL2 (PD7) (PA7) PD3   PD3   (PF2)    : LED0
 *    WO4  -     PD4   PD4   PD4   PD4
 *    WO5  -     PD5   PD5   PD5   PD5
 *
 *
 * Signal name details:
 *
 *    PGM
 *        TDAT - PIN_PGM_TDAT     TPI-Data or UPDI-Interface (open-drain, pull-up)
 *        TRST - PIN_PGM_TRST     TPI-Reset or UPDI-Reset (open-drain, pull-up)
 *        TCLK - PIN_PGM_TCLK     TPI-Clock (push-pull)
 *        PDAT - PIN_PGM_PDAT     PDI-Data (push-pull, no pull-up)
 *        PCLK - PIN_PGM_PCLK     PDI-Clock (push-pull)
 *         VPW - PIN_PGM_VPOWER   V-Target Power Control (negative logic, push-pull)
 *
 *    VCP
 *        VTxD - PIN_VCP_TXD      Data OUT (TCLK and Bridge, open-drain, pull-up)
 *        VRxD - PIN_VCP_RXD      Data IN  (open-drain, pull-up)
 *
 *    SYS
 *        DTxD -                  System self debug (default 8N1,500kbps)
 *        DRxD -                  System self debug (reserved)
 *       DUPDI -                  System self programing
 *       DnRST -                  System self reset
 *        LED0 - PIN_SYS_LED0     System indicator (CNANO: negative logic)
 *        LED1 - PIN_SYS_LED1     VCP trafic indicator positive logic
 *         SW0 - PIN_SYS_SW0      System mode switch negative logic (Physically, SW1)
 *
 *    HVC
 *       HVCP1 - PIN_HVC_CHGPUMP1 Charge pump pulse output positive logic
 *       HVCP2 - PIN_HVC_CHGPUMP2 Charge pump pulse output negative logic
 *       HVSL1 - PIN_HVC_SELECT1  Logic high applies 12V to TDAT for UPDI V0 (push-pull)
 *       HVSL2 - PIN_HVC_SELECT2  Logic high applies 12V to TRST for TPI (push-pull)
 *       HVSL3 - PIN_HVC_SELECT3  Logic high applies 7V5 to TRST for UPDI V2+ (push-pull)
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
 * UPDI/PDI Program interface operating clock.
 * In avrdude this can be changed with `-B125khz` etc.
 */

#define UPDI_CLK 225
#define PDI_CLK  500

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
 * Columns: HW_VER, FW_MAJOR, FW_MINOR, FW_RELL, FW_RELH (all 1-byte decimal)
 */

#define CONFIG_SYS_FWVER { 0, 1, 33, 46, 0 }

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

// #define CONFIG_VCP_INTERRUPT_SUPPRT

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
 * This option can currently only be enabled on CNANO.
 * 3.3V operating voltage support is required. 5V operation is prohibited.
 * CNANO must be pre-configured for 3.3V operation.
 */

#define CONFIG_PGM_PDI_ENABLE

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
#ifdef CONFIG_HVC_DISABLE
  #undef CONFIG_HVC_ENABLE
#endif
#ifdef CONFIG_PGM_PDI_DISABLE
  #undef CONFIG_PGM_PDI_ENABLE
#endif

#if (CONFIG_HAL_TYPE == HAL_BAREMETAL_14P)
  #undef DEBUG
  #undef CONFIG_HVC_ENABLE
  #undef CONFIG_PGM_PDI_ENABLE
  #define CONFIG_PGM_TYPE 1
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_ALT3_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD
  #define PIN_VCP_RXD         PIN_USART0_RXD
  #define PIN_PGM_TDAT        PIN_USART0_TXD_ALT3
  #define PIN_PGM_TRST        PIN_PD5
  #define PIN_PGM_TCLK        PIN_USART0_XCK_ALT3
  #define PIN_SYS_LED0        PIN_EVOUTD_ALT1
  #define PIN_SYS_LED1        PIN_LUT1_OUT
  #define PIN_SYS_SW0         PIN_PF6

#elif (CONFIG_HAL_TYPE == HAL_BAREMETAL_20P)
  #undef DEBUG
  #undef CONFIG_PGM_PDI_ENABLE
  #define CONFIG_PGM_TYPE 2
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
  #define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
  #define PIN_PGM_TDAT        PIN_USART0_TXD
  #define PIN_PGM_TRST        PIN_PA1
  #define PIN_PGM_TCLK        PIN_USART0_XCK
  #define PIN_HVC_SELECT1     PIN_PA5
  #define PIN_HVC_SELECT2     PIN_PA7
  #define PIN_HVC_SELECT3     PIN_PD7
  #define PIN_HVC_CHGPUMP1    PIN_TCA0_WO4_ALT3
  #define PIN_HVC_CHGPUMP2    PIN_TCA0_WO5_ALT3
  #define PIN_SYS_LED0        PIN_LUT2_OUT_ALT1
  #define PIN_SYS_LED1        PIN_LUT1_OUT
  #define PIN_SYS_SW0         PIN_PF6

#elif (CONFIG_HAL_TYPE == HAL_CNANO)
  #define CONFIG_PGM_TYPE 0
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PDI   (PORTMUX_USART0_ALT1_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
  #define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
  #define PIN_PGM_TDAT        PIN_USART0_TXD
  #define PIN_PGM_TRST        PIN_PA1
  #define PIN_PGM_TCLK        PIN_USART0_XCK
  #define PIN_PGM_PDAT        PIN_USART0_TXD_ALT1
  #define PIN_PGM_PCLK        PIN_USART0_XCK_ALT1
  #define PIN_PGM_VPOWER      PIN_PA5
  #define PIN_HVC_SELECT1     PIN_PD0
  #define PIN_HVC_SELECT2     PIN_PD1
  #define PIN_HVC_SELECT3     PIN_PD2
  #define PIN_HVC_CHGPUMP1    PIN_TCA0_WO4_ALT3
  #define PIN_HVC_CHGPUMP2    PIN_TCA0_WO5_ALT3
  #define PIN_SYS_LED0        PIN_EVOUTF
  #define PIN_SYS_LED1        PIN_EVOUTA_ALT1
  #define PIN_SYS_SW0         PIN_PF6

#else /* (CONFIG_HAL_TYPE == HAL_BAREMETAL_28P) || (CONFIG_HAL_TYPE == HAL_BAREMETAL_32P) */
  #undef CONFIG_PGM_PDI_ENABLE
  #define CONFIG_PGM_TYPE 2
  #define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
  #define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
  #define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
  #define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
  #define PIN_PGM_TDAT        PIN_USART0_TXD
  #define PIN_PGM_TCLK        PIN_USART0_XCK
  #define PIN_PGM_TRST        PIN_PA6
  #define PIN_PGM_VPOWER      PIN_PA1
  #define PIN_HVC_SELECT1     PIN_PD0
  #define PIN_HVC_SELECT2     PIN_PD1
  #define PIN_HVC_SELECT3     PIN_PD2
  #define PIN_HVC_CHGPUMP1    PIN_TCA0_WO4_ALT3
  #define PIN_HVC_CHGPUMP2    PIN_TCA0_WO5_ALT3
  #define PIN_SYS_LED0        PIN_LUT2_OUT
  #define PIN_SYS_LED1        PIN_LUT1_OUT
  #define PIN_SYS_SW0         PIN_PA5

#endif

#ifndef PIN_HVC_SELECT1
  #undef CONFIG_HVC_ENABLE
#endif

#ifndef PIN_PGM_VPOWER
  #undef CONFIG_PGM_VPOWER_ENABLE
#endif

// end of header

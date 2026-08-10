/**
 * @file MZU2410A2.h
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.34.49+
 * @date 2026-08-09
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#pragma once

/* This number is optional, provided it is not used elsewhere. */
#define HAL_MZU2410A2 28
#define HAL_PROFILE "HAL/MZU2410A2.h"

/*
 * Pin layout by design: MZU2410A2
 *
 *    This profile does not support PDI/ISP (it's an outdated design).
 *
 *                  -- Target-PGM Type --
 *         28P      UPDI  TPI
 *    PA0  TDAT     UPDI  DATA
 *    PA1  VPW      RESET RESET
 *    PA2  VTxD           CLK
 *    PA3  VRxD 
 *    PA4  PDAT
 *    PA5  SW0
 *    PA6  TRST
 *    PA7  N.C.
 *    PC3  LED1
 *    PD0  HVSL1
 *    PD1  HVSL2
 *    PD2  HVSL3
 *    PD3  LED0
 *    PD4  HVCP1
 *    PD5  HVCP2
 *    PD6  DTxD     : USB-CDC-RX
 *    PD7  DRxD     : USB-CDC-TX
 *    PF0  -
 *    PF1  -
 *    PF2  -
 *    PF3  -
 *    PF4  -
 *    PF5  -
 *    PF6  DnRST
 *    PF7  DUPDI
 *
 *
 * Peripheral Function Input/Output:
 *
 *         28P
 *    CCL0 -        : SW0 Falling interrupt
 *    CCL1 PC3      : LED1
 *    CCL2 PD3      : LED0
 *    WO4  PD4      : HVCP1
 *    WO5  PD5      : HVCP2
 *
 *
 * Signal name details:
 *
 *    PGM
 *        TDAT - PIN_PGM_TDAT     TPI-Data or UPDI-Interface (open-drain, pull-up)(HV=12V)
 *        TRST - PIN_PGM_TRST     TPI-Reset or UPDI-Reset (open-drain, pull-up)(HV=12V/7.5V)
 *        TCLK - PIN_PGM_TCLK     TPI-Clock (push-pull)
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

#ifndef __AVR_AVR32DU28__
  #error There are no hardware profiles to select AVR32DU28.
  #include BUILD_STOP
#endif

#undef CONFIG_PGM_PDI_ENABLE
#define CONFIG_HVC_ENABLE
#define CONFIG_PGM_VPOWER_ENABLE
#define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
#define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
#define PORTMUX_USART_PDI   (PORTMUX_USART0_ALT1_gc    | PORTMUX_USART1_ALT2_gc)
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

// end of code

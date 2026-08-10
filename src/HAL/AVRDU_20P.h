/**
 * @file AVRDU_20P.h
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
#define HAL_AVRDU_20P 20
#define HAL_PROFILE "HAL/AVRDU_20P.h"

/*
 * Pin layout by design: AVR16/32DU20
 *
 *                  -- Target-PGM Type --
 *         20P      UPDI    TPI     PDI
 *    PA0  TDAT     1:UPDI  1:DATA
 *    PA1  TRST     5:RESET 5:RESET
 *    PA2  VTxD     3:HTCR  3:CLK
 *    PA3  VRxD     4:HRCT
 *    PA4  PDAT                     1:DATA
 *    PA5  HVSL1
 *    PA6  PCLK                     5:CLK
 *    PA7  HVSL2
 *    PC3  LED1
 *    PD0  -
 *    PD1  -
 *    PD2  -
 *    PD3  -
 *    PD4  HVCP1
 *    PD5  HVCP2
 *    PD6  LED0     : USB-CDC-RX
 *    PD7  HVSL3    : USB-CDC-TX
 *    PF0  -
 *    PF1  -
 *    PF2  -
 *    PF3  -
 *    PF4  -
 *    PF5  -
 *    PF6  SW0
 *    PF7  DUPDI
 *
 *    - In this plan, PF6 is used as a GPIO.
 *
 *
 * Peripheral Function Input/Output:
 *
 *         20P
 *    CCL0 -        : SW0 Falling interrupt
 *    CCL1 PC3      : LED1
 *    CCL2 PD6(ALT) : LED0
 *    WO4  PD4      : HVCP1
 *    WO5  PD5      : HVCP2
 *
 *
 * Signal name details:
 *
 *    PGM
 *        TDAT - PIN_PGM_TDAT     TPI-Data or UPDI-Interface (open-drain, pull-up)(HV=12V)
 *        TRST - PIN_PGM_TRST     TPI-Reset or UPDI-Reset (open-drain, pull-up)(HV=12V/7V5)
 *        TCLK - PIN_PGM_TCLK     TPI-Clock (push-pull)
 *        PDAT - PIN_PGM_PDAT     PDI-Data (push-pull, no pull-up)
 *        PCLK - PIN_PGM_PCLK     PDI-Clock (push-pull)
 * 
 *    PGM-ISP : It is still in the planning stage.
 *        MOSI - PIN_PGM_MOSI
 *        MISO - PIN_PGM_MISO
 *         SCK - PIN_PGM_MSCK
 *       RESET - PIN_PGM_MRST     (HV=12V)
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

#if !defined(AVR_AVRDU) || !defined(PIN_PA2)
  #error The MCU family is not AVR_DU with 20-pads or more.
  #include BUILD_STOP
#endif

#undef CONFIG_PGM_ISP_ENABLE
#undef CONFIG_PGM_VPOWER_ENABLE
#define PORTMUX_USART_VCP   (PORTMUX_USART0_ALT2_gc    | PORTMUX_USART1_ALT2_gc)
#define PORTMUX_USART_PGM   (PORTMUX_USART0_DEFAULT_gc | PORTMUX_USART1_ALT2_gc)
#define PORTMUX_USART_PDI   (PORTMUX_USART0_ALT1_gc    | PORTMUX_USART1_ALT2_gc)
#define PORTMUX_USART_NONE  (PORTMUX_USART0_NONE_gc    | PORTMUX_USART1_ALT2_gc)
#define PIN_VCP_TXD         PIN_USART0_TXD_ALT2
#define PIN_VCP_RXD         PIN_USART0_RXD_ALT2
#define PIN_PGM_TDAT        PIN_USART0_TXD
#define PIN_PGM_TCLK        PIN_USART0_XCK
#define PIN_PGM_TRST        PIN_PA1
#define PIN_PGM_PDAT        PIN_USART0_TXD_ALT1
#define PIN_PGM_PCLK        PIN_USART0_XCK_ALT1
#define PIN_HVC_SELECT1     PIN_PA5
#define PIN_HVC_SELECT2     PIN_PA7
#define PIN_HVC_SELECT3     PIN_PD7
#define PIN_HVC_CHGPUMP1    PIN_TCA0_WO4_ALT3
#define PIN_HVC_CHGPUMP2    PIN_TCA0_WO5_ALT3
#define PIN_SYS_LED0        PIN_LUT2_OUT_ALT1
#define PIN_SYS_LED1        PIN_LUT1_OUT
#define PIN_SYS_SW0         PIN_PF6

// end of code

/**
 * @file AVRDU_14P.h
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.35.49+
 * @date 2026-08-09
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#pragma once

/* This number is optional, provided it is not used elsewhere. */
#define HAL_AVRDU_14P 14
#define HAL_PROFILE "HAL/AVRDU_14P.h"

/*
 * Pin layout by design: AVR16/32DU20
 *
 *                  -- Target-PGM Type --
 *         14P      UPDI   TPI
 *    PA0  VTxD     3:HTCR
 *    PA1  VRxD     4:HRCT
 *    PA2  -
 *    PA3  -
 *    PA4  -
 *    PA5  -
 *    PA6  -
 *    PA7  -
 *    PC3  LED1
 *    PD0  -
 *    PD1  -
 *    PD2  -
 *    PD3  -
 *    PD4  TDAT     1:UPDI  1:TPI
 *    PD5  TRST     5:RESET 5:RESET
 *    PD6  TCLK             3:CLK
 *    PD7  LED0
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
 *    - In this implementation, PD6 (TCLK) and PA0 (VTxD)
 *      must be shorted externally to the package.
 *
 *
 * Peripheral Function Input/Output:
 *
 *         14P
 *    CCL0 -        : SW0 Falling interrupt
 *    CCL1 PC3      : LED1
 *    CCL2 PD7      : LED0(EVOUTD_ALT)
 *    WO4  -
 *    WO5  -
 *
 *
 * Signal name details:
 *
 *    PGM
 *        TDAT - PIN_PGM_TDAT     TPI-Data or UPDI-Interface (open-drain, pull-up)
 *        TRST - PIN_PGM_TRST     TPI-Reset or UPDI-Reset (open-drain, pull-up)
 *        TCLK - PIN_PGM_TCLK     TPI-Clock (push-pull)
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
 */

#if !defined(AVR_AVRDU)
  #error The MCU family is not AVR_DU.
  #include BUILD_STOP
#endif

#undef DEBUG
#undef CONFIG_HVC_ENABLE
#undef CONFIG_PGM_PDI_ENABLE
#undef CONFIG_PGM_ISP_ENABLE
#undef CONFIG_PGM_VPOWER_ENABLE
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

// end of code

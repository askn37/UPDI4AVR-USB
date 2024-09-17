/**
 * @file fuse.c
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

#include <avr/io.h>
#include <avr/fuse.h>
#include "configuration.h"

/*
 * Note: 
 *
 * These are the recommended FUSE arrays.
 * 
 * - SYSCFG0->FUSE_UPDIPINCFG_bm is True by default
 * - SYSCFG0->FUSE_RSTPINCFG_bm varies depending on SW0 usage
 * - SYSCFG0->FUSE_EESAVE_bm is True to preserve VID:PID information
 * - PDICFG should not be changed from the default
 */

#if PIN_SYS_SW0 != PIN_PF6
  #define ENABLE_SYS_RESET FUSE_RSTPINCFG_bm
#else
  #define ENABLE_SYS_RESET 0
#endif

FUSES = {
    .WDTCFG   = FUSE0_DEFAULT,
    .BODCFG   = FUSE1_DEFAULT,
    .OSCCFG   = FUSE2_DEFAULT,
    .SYSCFG0  = FUSE5_DEFAULT | FUSE_EESAVE_bm | ENABLE_SYS_RESET,
    .SYSCFG1  = FUSE6_DEFAULT,
    .CODESIZE = FUSE7_DEFAULT,  /* 0=All application code */
    .BOOTSIZE = FUSE8_DEFAULT,  /* 0=No bootloader */
    .PDICFG   = FUSE10_DEFAULT  /* Never change it */
};

// end of code

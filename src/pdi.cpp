/**
 * @file pdi.cpp
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
#include "api/macro_api.h"  /* ATOMIC_BLOCK */
#include "api/capsule.h"    /* _CAPS macro */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * PDI command payloads use the JTAGICE3 standard.
 *
 * The PDI communications protocol uses the same access layer command set as UPDI.
 * The hardware layer also uses the same 12-bit frames USART as UPDI/TPI.
 * It is a single-wire bidirectional communication like RS485,
 * synchronized by the XCLK just like TPI.
 *
 * ACC handles 32-bit addresses:
 *   high order byte 0 specifies the code space.
 *   high order byte 1 specifies the data space.
 */

namespace PDI {
  /* STUB */
};

// end of code

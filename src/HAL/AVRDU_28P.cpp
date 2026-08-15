/**
 * @file AVRDU_28P.cpp
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

#include <avr/io.h>
#include <variant.h>
#include "../configuration.h"
#ifdef HAL_AVRDU_28P
#include "../prototype.h"

namespace SYS {

  /*
   * MPU Setup - AVR-DU28/32 standard HAL (sys.cpp)
   */

};  /* SYS */

#endif

// end of code

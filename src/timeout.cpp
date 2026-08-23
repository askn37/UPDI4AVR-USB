/**
 * @file timeout.cpp
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

#include <avr/io.h>
#include <setjmp.h>
#include <api/macro_api.h>  /* ATOMIC_BLOCK */
#include <peripheral.h>     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

namespace Timeout {

  /*
   * Timeout after the specified time.
   * To be precise, in 1/1024 sec units.
   */
  void start (uint16_t _ms) {
    while (RTC_STATUS);
    RTC_CMP = RTC_CNT + _ms;
    RTC_INTFLAGS = RTC_CMP_bm;
    RTC_INTCTRL  = RTC_CMP_bm;
  }

  /*
   * Exit from the timeout block.
   * The last RETI is required.
   */
  __attribute__((used, naked, noinline))
  void stop (void) {
    RTC_INTCTRL  = 0;
    RTC_INTFLAGS = RTC_CMP_bm;
    reti();
  }

  /*
   * Timeout extension.
   */
  void extend (uint16_t _ms) {
    while (RTC_STATUS);
    RTC_CMP = RTC_CNT + _ms;
  }

  /*
   * Timeout delay.
   */
  void delay_rtc_millis (uint16_t _ms) {
    while (RTC_STATUS);
    _ms += RTC_CNT;
    while (RTC_CNT != _ms) {
      wdt_reset();
    }
  }

  /*
   * Timeout block.
   * Does not work with interrupts disabled.
   * RETI must be called after the interrupt is suspended.
   */
  size_t command (size_t (*func_p)(void), size_t (*fail_p)(void), uint16_t _ms) {
    volatile size_t _result = 0;
    while (_result == 0) {
      if (setjmp(TIMEOUT_CONTEXT) == 0) {
        Timeout::start(_ms);
        _result = (*func_p)();
        Timeout::stop();
        break;
      }
      Timeout::stop();
      D1PRINTF("[TO]");
      if (!fail_p) break;
      wdt_reset();
      if (!(*fail_p)()) break;
    }
    return _result;
  }

};

/*
 * Timeout interrupt.
 * Note that it does not end with RETI.
 */
ISR(RTC_CNT_vect, ISR_NAKED) {
  /***
    This interrupt is a global escape due to timeout.
    There is no return to the source of the interrupt.
  ***/
  __asm__ __volatile__ ("EOR R1,R1");
  longjmp(TIMEOUT_CONTEXT, 2);
}

// end of code

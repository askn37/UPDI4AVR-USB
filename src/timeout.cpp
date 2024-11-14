/**
 * @file timeout.cpp
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

#include <avr/io.h>
#include <setjmp.h>
#include "api/macro_api.h"  /* ATOMIC_BLOCK */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

namespace Timeout {

  void setup (void) {
    RTC_PITEVGENCTRLA = RTC_EVGEN0SEL_DIV32_gc | RTC_EVGEN1SEL_DIV128_gc;
    EVSYS_CHANNEL0 = EVSYS_CHANNEL_RTC_EVGEN0_gc; /* 1024Hz periodic.  */
    EVSYS_CHANNEL1 = EVSYS_CHANNEL_RTC_EVGEN1_gc; /* 32Hz periodic.    */
    EVSYS_USERTCB0COUNT = EVSYS_USER_CHANNEL0_gc; /* TCB0_CLK = 1024Hz */
    EVSYS_USERTCB1COUNT = EVSYS_USER_CHANNEL1_gc; /* TCB1_CLK = 32Hz   */
    RTC_PITCTRLA = RTC_PITEN_bm;
  }

  /*
   * Timeout after the specified time.
   * To be precise, in 1/1024 sec units.
   */
  void start (uint16_t _ms) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TCB0_CNT = 0;
      TCB0_CCMP = _ms;
      TCB0_INTCTRL = TCB_CAPT_bm;
      TCB0_INTFLAGS = TCB_CAPT_bm;
      TCB0_CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc; /* for EVSYS_USERTCB0COUNT */
    }
  }

  /*
   * Exit from the timeout block.
   * The last RETI is required.
   */
  __attribute__((used, naked, noinline))
  void stop (void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TCB0_CTRLA = 0;
      TCB0_INTFLAGS = TCB_CAPT_bm;
    }
    reti();
  }

  /*
   * Timeout extension.
   */
  void extend (uint16_t _ms) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TCB0_CCMP = _ms;
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
ISR(TCB0_INT_vect, ISR_NAKED) {
  /***
    This interrupt is a global escape due to timeout.
    There is no return to the source of the interrupt.
  ***/
  __asm__ __volatile__ ("EOR R1,R1");
  TCB0_CTRLA = 0;
  TCB0_INTFLAGS = TCB_CAPT_bm;
  longjmp(TIMEOUT_CONTEXT, 2);
}

// end of code

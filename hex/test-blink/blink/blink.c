/**
 * @file blink.c
 * @author askn (K.Sato) multix.jp
 * @brief A small program for bare metal chips of type UPDI.
 *        It blinks an LED and outputs a count up message to the UART
 *        of a 9600,8N1. Any input will reset the chip.
 * @version 0.1
 * @date 2024-08-06
 * @copyright Copyright (c) 2024 askn37 at github.com
 * @link Potal : https://askn37.github.io/
 *       MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
/*
 * Target MCU: megaAVR-0, AVR-Dx, AVR-Ex series
 *
 * PA7 - LED
 * PA0 - TxD  to  VCP-RxD
 * PA1 - RxD from VCP-TxD
 * 
 * VCP Parameter: 9600,8N1
 */

#define RXSTAT _SFR_MEM8(0x001E)
#define RXDATA _SFR_MEM8(0x001F)

void send (const uint8_t _d) {
  /* TxD send */
  loop_until_bit_is_set(USART0_STATUS, USART_DREIF_bp);
  USART0_TXDATAL = _d;
}

__attribute__((used, naked, section(".init3")))
void setup (void) {
  uint8_t _c = 0, _d = 0;

  /*** UART setup ****/
#ifdef RSTCTRL_SWRE_bm
  /* Not AVR-Dx */
  uint8_t _f = 0;
  #define BAUD_SETTING_16 ((16000000L / 6 * 64) / (16L * 9600))
  #define BAUD_SETTING_20 ((20000000L / 6 * 64) / (16L * 9600))

  #ifdef EVSYS_STROBE
  /* megaAVR-0 */
  if (bit_is_set(FUSE_OSCCFG, FUSE_FREQSEL_0_bp)) {
    USART0_BAUD = BAUD_SETTING_16;
  }
  else {
    USART0_BAUD = BAUD_SETTING_20;
    _f = 1;
  }
  #else
  /* AVR-Ex */
  if (bit_is_set(FUSE_OSCCFG, FUSE_OSCHFFRQ_bp)) {
    USART0_BAUD = BAUD_SETTING_16;
  }
  else {
    USART0_BAUD = BAUD_SETTING_20;
    _f = 1;
  }
  #endif
#else
  /* AVR-Dx */
  #define BAUD_SETTING ((4000000L * 64) / (16L * 9600))
  USART0_BAUD = BAUD_SETTING;
#endif
  USART0_CTRLC = USART_CHSIZE_8BIT_gc;
  USART0_CTRLB = USART_RXEN_bm | USART_TXEN_bm;
  send('*');
  send('\r');
  send('\n');

  /* LED and TxD port output */
  VPORTA_DIR = _BV(7) | _BV(0);

  for (;;) {
    /* LED Toggle  */
    VPORTA_IN = _BV(7);

    /* RxD recv */
    if (bit_is_set(USART0_STATUS, USART_RXCIF_bp))
      _PROTECTED_WRITE(RSTCTRL_SWRR, 1);

    /* 2-digit decimal count up */
    if (++_c >= 10) {
      _c = 0;;
      if (++_d >= 10) _d = 0;
    }
    send(_d + '0'); send(_c + '0'); send('\r'); send('\n');

#ifdef RSTCTRL_SWRE_bm
    /* megaAVR-0, AVR-Ex */
    if (_f)
      __builtin_avr_delay_cycles(20000000L / 6);
    else
      __builtin_avr_delay_cycles(16000000L / 6);
#else
    /* AVR-Dx */
    __builtin_avr_delay_cycles(4000000L);
#endif
  }
}

// end of code

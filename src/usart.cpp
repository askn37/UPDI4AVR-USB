/**
 * @file usart.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.32.45+
 * @date 2024-08-26
 * @copyright Copyright (c) 2024 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include <string.h>         /* memcpy */
#include "api/macro_api.h"  /* ATOMIC_BLOCK */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

namespace USART {

  void setup (void) {
    SYS::LED_Fast();
    PGCONF = 0;
    disable_vcp();
    pinModeMacro(PIN_VCP_RXD, INPUT_PULLUP);  /* USART0_DEFAULT or USART0_ALT2 */
    pinModeMacro(PIN_PGM_TRST, INPUT_PULLUP); /* USART0_ALT3 or USART0_DEFAULT */
  }

  /*** Calculate the baud rate for VCP asynchronous mode. ***/
  uint16_t calk_baud_khz (uint16_t _khz) {
    uint32_t _baud = ((F_CPU / 1000L * 8L) / _khz + 1) / 2;
    if (_baud < 64) _baud = 64;
    else if (_baud > 0xFFFFU) _baud = 0xFFFF;
    return _baud;
  }

  void drain (size_t _delay) {
    do {
      if (bit_is_set(USART0_STATUS, USART_RXCIF_bp)) {
        __asm__ __volatile__ (
          "LDS R0, 0x0801\n"  /* drop USART0_RXDATAH */
          "LDS R0, 0x0800\n"  /* drop USART0_RXDATAL */
        );
      }
    } while (--_delay);
  }

  /*** Stop the VCP and release the ports in use. ***/
  void disable_vcp (void) {
    if (USART0_CTRLB) {
      D1PRINTF(" UART=OFF\r\n");
      /* Allow time to move USART0_TXDATA */
      delay_micros(4);
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        /* Simply clearing the CTRLB does not disable the USART completely.                    */
        /* This errata is not documented for the AVR-DU, but is the same as for tinyAVR-0 etc. */
        USART0_CTRLB = 0;
        USART0_CTRLA = 0;
        RXSTAT = 0;
        RXDATA = 0;
        PORTMUX_USARTROUTEA = PORTMUX_USART_VCP;
        bit_clear(GPCONF, GPCONF_VCP_bp);
      }
      pinModeMacro(PIN_PGM_TDAT, INPUT_PULLUP);   /* open-drain */
      pinModeMacro(PIN_VCP_TXD, INPUT_PULLUP);    /* open-drain */
      openDrainWriteMacro(PIN_VCP_TXD, HIGH);
  #if (PIN_PGM_TCLK != PIN_VCP_TXD)
      pinModeMacro(PIN_PGM_TCLK, INPUT_PULLUP);   /* open-drain */
      openDrainWriteMacro(PIN_PGM_TCLK, HIGH);
  #endif
    }
  }

  /*** Sets up single-wire asynchronous mode for UPDI operation. ***/
  void change_updi (void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      D1PRINTF(" UART=UPDI\r\n");
      PORTMUX_USARTROUTEA = PORTMUX_USART_PGM;
      USART0_STATUS = USART_DREIF_bm;
      USART0_BAUD  = calk_baud_khz(_xclk);
      USART0_CTRLC = USART_CHSIZE_8BIT_gc | USART_PMODE_EVEN_gc | USART_SBMODE_2BIT_gc;
      /* The RS485_INT:_BV(1) attribute is undocumented but works fine.     */
      /* Without it, an additional delay is required before sending a byte. */
      USART0_CTRLA = USART_LBME_bm | USART_RS485_INT_gc;
      USART0_CTRLB = USART_RXEN_bm | USART_TXEN_bm | USART_ODME_bm;
      D1PRINTF(" BAUD=%04X:%02X\r\n", USART0_BAUD, USART0_CTRLB);
    }
  }

  /*** Set up single-wire synchronous mode for TPI operation. ***/
  /* The VCD-TxD is repurposed to transmit the synchronous clock. */
  void change_tpi (void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      D1PRINTF(" UART=TPI\r\n");
      /* In synchronous mode the formula is different. */
      /* The target device typically runs at 1MHz, so a safe communication speed is 100kHz. */
      uint32_t _baud = ((F_CPU / 1000L) / 100 + 1) / 2;
      _baud <<= 6;
      if (_baud < 64) _baud = 64;
      PORTMUX_USARTROUTEA = PORTMUX_USART_PGM;
      pinModeMacro(PIN_PGM_TCLK, OUTPUT); /* push-pull */
      USART0_STATUS = USART_DREIF_bm;
      USART0_BAUD  = _baud;
      USART0_CTRLC = USART_CHSIZE_8BIT_gc | USART_PMODE_EVEN_gc | USART_CMODE_SYNCHRONOUS_gc | USART_SBMODE_2BIT_gc;
      USART0_CTRLA = USART_LBME_bm | USART_RS485_INT_gc | USART_RS485_EXT_gc;
      USART0_CTRLB = USART_RXEN_bm | USART_TXEN_bm | USART_ODME_bm;
      D1PRINTF(" BAUD=%04X:%02X\r\n", USART0_BAUD, USART0_CTRLB);
    }
  }

  /*** Activates VCP operation. ***/
  /* Detailed parameters are specified in SET_LINE_ENCODING. */
  void change_vcp (void) {
  #ifdef CONFIG_VCP_TXD_PUSHPULL
    uint8_t _ctrlb = USART_RXEN_bm | USART_TXEN_bm;
    digitalWrite(PIN_VCP_TXD, HIGH);
    pinModeMacro(PIN_VCP_TXD, OUTPUT);    /* push-pull */
  #else
    uint8_t _ctrlb = USART_RXEN_bm | USART_TXEN_bm | USART_ODME_bm;
  #endif
    uint32_t _baud = _set_line_encoding.dwDTERate;
    /* If the BAUD value is small, select double speed mode. */
    if (_baud) _baud = (((F_CPU * 8L) / _baud) + 1) >> 1;
    if (_baud < 96) {
      _baud <<= 1;
      _ctrlb |= USART_RXMODE_CLK2X_gc;
    }
    D1PRINTF(" BAUD=%08lX:%02X\r\n", _baud, _ctrlb);
    if (_baud < 0x10000UL && _baud >= 64) {
      uint8_t _bits = _set_line_encoding.bDataBits - 5;
      uint8_t _ctrlc = (uint8_t[]){
        USART_PMODE_DISABLED_gc, USART_PMODE_ODD_gc, USART_PMODE_EVEN_gc, USART_PMODE_DISABLED_gc
      }[_set_line_encoding.bParityType & 3]
      + _set_line_encoding.bCharFormat ? USART_SBMODE_2BIT_gc : USART_SBMODE_1BIT_gc;
      if (_bits < 4) {
        _ctrlc += _bits; /* USART_CHSIZE_[5,6,7,8]BIT_gc */
        #if defined(CONFIG_VCP_9BIT_SUPPORT)
        usart_receiver    = &USB::vcp_receiver;
        usart_transmitter = &USB::vcp_transceiver;
      }
      else {
        _ctrlc += USART_CHSIZE_9BITL_gc;
        usart_receiver    = &USB::vcp_receiver_9bit;
        usart_transmitter = &USB::vcp_transceiver_9bit;
        #else
      }
      else {
        _ctrlc += USART_CHSIZE_8BIT_gc;
        #endif
      }
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        PORTMUX_USARTROUTEA = PORTMUX_USART_VCP;
        USART0_STATUS = USART_DREIF_bm;
        USART0_BAUD = (uint16_t)_baud;
        USART0_CTRLC = _ctrlc;
        USART0_CTRLA = USART_RXCIF_bm;
        USART0_CTRLB = _ctrlb;
        bit_set(GPCONF, GPCONF_VCP_bp);
      }
      D1PRINTF(" UART=VCP\r\n");
      drain(32768);
    }
    else {
      /* If outside the supported range, the USART will remain in the BREAK state. */
      D1PRINTF(" UART=FAIL\r\n");
    }
    if (bit_is_set(GPCONF, GPCONF_USB_bp))
      SYS::LED_HeartBeat();
    else
      SYS::LED_Flash();
  }

  void set_line_encoding (LineEncoding_t* _buff) {
    /* The USART will not change unless a different setting is given. */
    if (0 == memcmp(&_set_line_encoding, _buff, sizeof(LineEncoding_t))) return;
    USB::read_drop();
    USART::setup();
    memcpy(&_set_line_encoding, _buff, sizeof(LineEncoding_t));
    change_vcp();
  }

  LineEncoding_t& get_line_encoding (void) {
    return _set_line_encoding;
  }

  void set_line_state (uint8_t _line_state) {

    /* If a physical port exists, it reflects DTR/RTS. */

  #if defined(PIN_VCP_DTR)
    if (bit_is_set(_line_state, 0))
      digitalWriteMacro(PIN_VCP_DTR, LOW);
    else
      digitalWriteMacro(PIN_VCP_DTR, HIGH);
  #endif

  #if defined(PIN_VCP_RTS)
    if (bit_is_set(_line_state, 1))
      digitalWriteMacro(PIN_VCP_RTS, LOW);
    else
      digitalWriteMacro(PIN_VCP_RTS, HIGH);
  #endif

  #if defined(CONFIG_VCP_DTR_RESET)
    /* If DTR is set, the device will reboot assuming the host has opened the port. */
    if (!_set_line_state.bStateDTR && bit_is_set(_line_state, 0)) {
      bit_set(GPCONF, GPCONF_FAL_bp);
      bit_set(GPCONF, GPCONF_RIS_bp);
    }
  #endif

    _set_line_state.bValue = _line_state;
  }

  LineState_t get_line_state (void) {
    return _set_line_state;
  }

};

/*** CMSIS-DAP VCOM,VCP transceiver ***/
ISR(USART0_RXC_vect) {
#if defined(CONFIG_VCP_9BIT_SUPPORT)
  usart_receiver();
#else
  USB::vcp_receiver();
#endif
}

// end of code

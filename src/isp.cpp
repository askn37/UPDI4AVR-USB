/**
 * @file isp.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.35.50+
 * @date 2026-08-24
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include <peripheral.h>     /* import Serial (Debug) */
#include <api/capsule.h>    /* _CAPS macro */
#include "configuration.h"
#include "prototype.h"

#ifdef CONFIG_PGM_ISP_ENABLE

/*
 * NOTE:
 *
 * ISP command payloads use the JTAGICE3 standard.
 *
 * Currently, this implementation supports only Low-Voltage SPI control;
 * High-Voltage debugWire control is not yet supported.
 * 
 * Only the ISP commands that might be sent by AVRDUDE are implemented.
 *
 * Only a very small number of chips have been tested;
 * operation with other chips is not guaranteed.
 * Please refer to the information on GitHub for test results.
 *
 * While much of the communication payload resembles the STK500v2
 * (JTAGICEmkII) -— specifically the AVR068 specification -—
 * some parts are unique to the `PICKit4`.
 *
 * For example, the parameters for `CMD_SET_SCK` and `CMD_GET_SCK`
 * are specified as the reciprocal of the frequency rather than the
 * frequency itself, and the responses also follow a special format.
 */

/*
 * Bit-banging is used for SPI communication with the chip.
 * Consequently, the pinout of the ICSP-6P connector is fully compatible
 * with UPDI/TPI. Conversely, VCP-UART communication can be established
 * by wiring TxD to MISO and RxD to SCK.
 * All pins used must belong to the same port group.
 */

/* ABd H C <- Arduino Board, ICSP-6P Header, CNumber */
/* D13 3 6 */#define PIN_PGM_MSCK PIN_VCP_TXD
/* D12 4 5 */#define PIN_PGM_MISO PIN_VCP_RXD
/* GND 6 4 */
/* VCC 2 3 (IOREF,VTG) */
/* D11 1 2 */#define PIN_PGM_MOSI PIN_PGM_TDAT
/* RST 5 1 */#define PIN_PGM_MRST PIN_PGM_TRST

/* These I/O ports must belong to the same port group. */
#define SPI_OUTPUT (pinBitmask(PIN_PGM_MOSI)|pinBitmask(PIN_PGM_MSCK))
#define SPI_INPUT  (pinBitmask(PIN_PGM_MISO)|pinBitmask(PIN_PGM_MRST))

namespace ISP {

  // MARK: ISP Low level

  NOINIT uint16_t _base_addr;
  NOINIT uint8_t _ser[4], _res[4], _pgm_retry;

  uint8_t spi_exchange (const uint8_t _data) {
    uint8_t _d = _data;
    for (uint8_t _i = 0; _i < 8; _i++) {
      if (_d & 0x80) digitalWriteMacro(PIN_PGM_MOSI, HIGH);
      else           digitalWriteMacro(PIN_PGM_MOSI, LOW);

      loop_until_bit_is_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
      digitalWriteMacro(PIN_PGM_MSCK, HIGH);
      _d <<= 1;
      bit_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
      if (digitalReadMacro(PIN_PGM_MISO)) _d |= 1;

      loop_until_bit_is_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
      digitalWriteMacro(PIN_PGM_MSCK, LOW);
      bit_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
    };
    return _d;
  }

  void spi_transaction (const uint8_t _cmd[]) {
    loop_until_bit_is_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
    bit_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      _res[0] = spi_exchange(_cmd[0]);
      _res[1] = spi_exchange(_cmd[1]);
      _res[2] = spi_exchange(_cmd[2]);
      _res[3] = spi_exchange(_cmd[3]);
    }
  #if !defined(NDEBUG) && defined(DEBUG)
    if (bit_is_clear(PGCONF, PGCONF_PGIA_bp)) {
      DPRINTF(" S/"); DPRINTHEX(_cmd, 4, ':');
      DPRINTF(" R/"); DPRINTHEX(_res, 4, ':');
    }
  #endif
  }

  // MARK: ISP NVM API

  size_t read_fuse_byte (void) {
    /* Send FUSE read command. */
    spi_transaction(&packet.out.isp.data[1]);

    /* Read and return the value at the specified position. */
    packet.in.isp.data[0] = _res[packet.out.isp.data[0] - 1];
    return 2;
  }

  size_t read_memories (void) {
    uint16_t _addr = _base_addr;
    size_t _length = bswap16(packet.out.isp.wValue);
    uint8_t _type = packet.out.isp.data[2];
    D1PRINTF(" M=$%04X, L=%d, T=$%02X\r\n", _addr, _length, _type);
    for (size_t _index = 0; _index < _length; _index++) {
      _ser[0] = _type;
      _ser[1] = _CAPS16(_addr)->bytes[1];
      _ser[2] = _CAPS16(_addr)->bytes[0];
      _ser[3] = 0;
      spi_transaction(_ser);
      packet.in.isp.data[_index] = _res[3];
      _addr += 1;
    }
    return _length + 1;
  }

  size_t read_flash (void) {
    uint16_t _addr   = _base_addr;
    size_t   _length = bswap16(packet.out.isp.wValue);
    uint8_t  _type   = packet.out.isp.data[2];
    D1PRINTF(" M=$%04X, L=%d, T=$%02X\r\n", _addr << 1, _length, _type);
    for (size_t _index = 0; _index < _length;) {
      _ser[0] = _type;
      _ser[1] = _CAPS16(_addr)->bytes[1];
      _ser[2] = _CAPS16(_addr)->bytes[0];
      _ser[3] = 0;
      spi_transaction(_ser);
      packet.in.isp.data[_index++] = _res[3];
      _ser[0] = _type | 0x08;
      spi_transaction(_ser);
      packet.in.isp.data[_index++] = _res[3];
      _addr += 1;
    }
    return _length + 1;
  }

  size_t chip_erase (void) {
    /* Send chip erase command. */
    spi_transaction(&packet.out.isp.data[2]);
    uint16_t _delay = packet.out.isp.data[1];
    if (_delay < 9) _delay = 9;
    Timeout::delay_rtc_millis(_delay);
    return 1;
  }

  size_t write_flash (void) {
    /* _length must be a power of 2 (not verified). */
    /* The lower bits of _addr must be zero-filled accordingly. */
    /* Ex) addr=$0000(word)                   */
    /* 00:20:C1:0A:40:4C:20:00:00:01:02:03:04 */
    /* lenBE|  |ms|WW|FF|RR|     |words...    */
    uint16_t _addr     = _base_addr;
    size_t   _length   = bswap16(packet.out.isp.wValue);
    uint16_t _delay    = packet.out.isp.data[3];
    uint8_t  _page_set = packet.out.isp.data[4];
    uint8_t  _commit   = packet.out.isp.data[5];
    uint8_t  _mask     = (_length >> 1) - 1;
    D1PRINTF(" M=$%04X, L=%d\r\n", _addr, _length);
    for (size_t _index = 0; _index < _length; _index += 2) {
      _ser[0] = _page_set;
      _ser[1] = _CAPS16(_addr)->bytes[1];
      _ser[2] = _CAPS16(_addr)->bytes[0] & _mask;
      _ser[3] = packet.out.isp.data[_index + 9];
      spi_transaction(_ser);
      _ser[0] = _page_set | 0x08;
      _ser[1] = _CAPS16(_addr)->bytes[1];
      _ser[2] = _CAPS16(_addr)->bytes[0] & _mask;
      _ser[3] = packet.out.isp.data[_index + 10];
      spi_transaction(_ser);
      _addr += 1;
    }
    _ser[0] = _commit;
    _ser[1] = _CAPS16(_base_addr)->bytes[1];
    _ser[2] = _CAPS16(_base_addr)->bytes[0] & ~_mask;
    _ser[3] = 0;
    spi_transaction(_ser);
    if (_delay < 5) _delay = 5;
    Timeout::delay_rtc_millis(_delay);
    return 1;
  }

  size_t write_eeprom (void) {
    /* _length must be a power of 2 (not verified). */
    /* The lower bits of _addr must be zero-filled accordingly. */
    /* Ex) addr=$0000(byte)                   */
    /* 00:04:C1:05:C1:C2:A0:00:00:01:02:03:04 (page type; m328p) */
    /* 00:04:84:14:C0:00:A0:FF:FF:FF:FF:50:41 (byte type; m8) */
    /* lenBE|tp|ms|WW|FF|RR|P1|P2|bytes...    */
    uint16_t _addr     = _base_addr;
    size_t   _length   = bswap16(packet.out.isp.wValue);
    uint16_t _type     = packet.out.data[2];      /* tp */
    uint16_t _delay    = packet.out.data[3];      /* ms */
    uint8_t  _page_set = packet.out.isp.data[4];  /* WW */
    uint8_t  _commit   = packet.out.isp.data[5];  /* FF */
    uint8_t  _read     = packet.out.isp.data[6];  /* RR */
    uint8_t  _verify   = packet.out.isp.data[7];  /* P1 */
    uint8_t  _mask     = _length - 1;
    if (_delay < 4) _delay = 4;
    D1PRINTF(" M=$%04X, L=%d\r\n", _addr, _length);
    if (bit_is_set(_type, 0)) {
      /* page type */
      for (size_t _index = 9; _index < _length + 9;) {
        _ser[0] = _page_set;
        _ser[1] = 0;
        _ser[2] = _CAPS16(_addr)->bytes[0] & _mask;
        _ser[3] = packet.out.isp.data[_index++];
        spi_transaction(_ser);
        _addr += 1;
      }
      _ser[0] = _commit;
      _ser[1] = _CAPS16(_base_addr)->bytes[1];
      _ser[2] = _CAPS16(_base_addr)->bytes[0] & ~_mask;
      _ser[3] = 0;
      spi_transaction(_ser);
      Timeout::delay_rtc_millis(_delay);
    }
    else {
      /* byte type */
      for (size_t _index = 9; _index < _length + 9;) {
        _ser[0] = _page_set;
        _ser[1] = _CAPS16(_addr)->bytes[1];
        _ser[2] = _CAPS16(_addr)->bytes[0];
        _ser[3] = packet.out.isp.data[_index++];
        spi_transaction(_ser);
        _addr += 1;
        if (bit_is_set(_type, 1)) {
          Timeout::delay_rtc_millis(_delay);
        }
        else if (bit_is_set(_type, 2)) {
          if (_ser[3] == 0xFF) {
            Timeout::delay_rtc_millis(9);
          }
          else {
            D1PRINTF(" S/"); D1PRINTHEX(_ser, 4, ':');
            do {
              _ser[0] = _read;
              spi_transaction(_ser);
            } while (_verify == _res[3]);
            D1PRINTF(" R/"); D1PRINTHEX(_res, 4, ':');
          }
        }
      }
    }
    return 1;
  }

  size_t write_fuse_byte (void) {
    /* Send FUSE write command. */
    spi_transaction(&packet.out.isp.data[0]);
    return 1;
  }

  // MARK: ISP Session

  /*
   * We will begin with low-voltage SPI control.
   * High-voltage dWire control is separate.
   *
   * 
   */
  size_t connect (void) {
    int _ret;
    PGCONF = PGCONF_PGIF_bm;
    USART::setup();
    TCA0_SPLIT_CTRLA = 0;
    _base_addr = 0;

    /* Change the necessary port settings. */
    portRegister(PIN_PGM_MRST).DIRCLR = SPI_INPUT;
    portRegister(PIN_PGM_MRST).OUTCLR = SPI_OUTPUT | SPI_INPUT;
    portRegister(PIN_PGM_MRST).DIRSET = SPI_OUTPUT;

    /* Bit-banging accuracy */
    uint8_t _period = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1_gc;
    _ret = (F_CPU / 2000) / _xclk;
    if (_ret < 0) _ret = 8;           /* Speed limit: 1250 kbps */
    else if (_ret >> 8) {
      _ret >>= 4;
      _period = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV16_gc;
      if (_ret >> 8) {
        _ret >>= 4;
        _period = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV256_gc;
        if (_ret >> 8) _ret = 255;
      }
    }

    /* TCA0_WO0 setup */
    TCA0_SPLIT_CTRLA = 0;
    TCA0_SPLIT_CTRLB = 0;
    TCA0_SPLIT_LCNT  = _ret - 1;
    TCA0_SPLIT_LPER  = _ret - 1;
    TCA0_SPLIT_CTRLA = _period;
    D1PRINTF(" XCK=%d>%d,%02X\r\n", _xclk, _ret, _period);

    for (uint8_t _i = 0; _i < 2; _i++) {
      pinLogicOpen(PIN_PGM_MRST);

      /* Pulse must be minimum 2 target CPU clock cycles
        so 100 usec is ok for CPU speeds above 20 KHz */
      SYS::delay_800us();
      pinLogicPull(PIN_PGM_MRST);
      SYS::delay_20ms();

      /* Programming start command. */
      spi_transaction(&packet.out.isp.data[7]);

      /* Verification after reading. */
      if (_res[packet.out.isp.data[6] - 1] == packet.out.isp.data[5]) {
        PGCONF = PGCONF_PGIA_bm | PGCONF_PROG_bm;
        D1PRINTF(" PGM OK\r\n");
        return 1;
      }
    }

    /* Time out on failure. */
    for (;;);
  }

  size_t timeout_fallback (void) {
    wdt_reset();
    D1PRINTF(" Fail\r\n");
    if (--_pgm_retry == 0) return 0;
    /* If a timeout occurs, the communication speed will be reduced. */
    _xclk -= _xclk >> 2;  /* next 75 % */
    return 1;
  }

  size_t sign_off (void) {
    if (PGCONF) {
      /* TCA0_WO0 reset */
      TCA0_SPLIT_CTRLA = 0;
      TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1024_gc;

      portRegister(PIN_PGM_TRST).OUTCLR = SPI_OUTPUT | SPI_INPUT;
      portRegister(PIN_PGM_TRST).DIRCLR = SPI_OUTPUT | SPI_INPUT;

      SYS::power_reset();
      PGCONF = 0;
      USART::setup();
      USART::change_vcp();
    }
    return 1;
  }

  // MARK: JTAG SCOPE

  /*** The ISP scope provides access to Low-Voltage SPI control AVR-8 chips. ***/
  /*
   * The packets within this scope are a subset of the STK500v2 protocol.
   * They also differ slightly from the AVRISPmkII (AVR069 document).
   */
  size_t jtag_scope_isp (void) {
    size_t  _rspsize = 0;
    uint8_t _length  = _packet_length - 6;
    uint8_t _cmd     = packet.out.cmd;
  #if !defined(NDEBUG) && defined(DEBUG)
    D1PRINTF(" L=%d\r\n  ", _length);
    if (_length)
      D1PRINTHEX(&packet.out.isp.data, _length, ':', 16, "\r\n  ");
    else
      D1PRINTF("\r\n");
  #endif
    if (_cmd == 0x10) {         /* CMD_ENTER_PROGMODE_ISP */
      D1PRINTF(" ISP_CMD_ENTER_PROGMODE\r\n");
      if (_length >= 11) {
        _pgm_retry = 4;
        _rspsize = Timeout::command(&connect, &timeout_fallback, packet.out.isp.data[0]);
        /* If STATUS_CMD_FAILED is returned here,
           CMD3_START_DW_DEBUG will be sent next. */
      }
    }
    else if (_cmd == 0x11) {    /* CMD_LEAVE_PROGMODE_ISP */
      D1PRINTF(" ISP_CMD_LEAVE_PROGMODE\r\n");
      _rspsize = sign_off();
    }
    else if (_cmd == 0x1D) {    /* CMD_SET_SCK */
      uint16_t _data = packet.out.isp.wValue;
      D1PRINTF(" PIC_CMD_SET_SCK=%d\r\n", _data);
      /* This command sends microseconds. */
      if (_data > 0) _xclk = 1000 / _data;  /* convert us to kbps */
      D1PRINTF(" XCLK=%d\r\n", _xclk);
      _rspsize = 1;
    }
    else if (_cmd == 0x1E) {    /* CMD_GET_SCK */
      D1PRINTF(" PIC_CMD_GET_SCK=%d\r\n", _xclk);
      /* This command gives a non-standard response. */
      packet.in.stk500v2.wValue = _xclk; /* kbps */
      D1PRINTF(" PIC_RES L=2\r\n  ");
      D1PRINTHEX(&packet.in.stk500v2.data, 3, ':');
      return 2;
    }
    else if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) {
      /* The steps below will not be executed unless
         program mode has been successfully entered. */
      packet.in.isp.res = 0xC0; /* STATUS_CMD_FAILED */
      return 1;
    }
    else if (_cmd == 0x06) {    /* CMD_LOAD_ADDRESS */
      _base_addr = bswap32(packet.out.isp.dwValue);
      D1PRINTF(" ISP_CMD_LOAD_ADDRESS=$%04X\r\n", _base_addr);
      _rspsize = 1;
    }
    else if (_cmd == 0x12) {    /* CMD_CHIP_ERASE_ISP */
      D1PRINTF(" ISP_CMD_CHIP_ERASE\r\n");
      _rspsize = Timeout::command(&chip_erase);
    }
    else if (_cmd == 0x13) {    /* CMD_PROGRAM_FLASH_ISP */
      D1PRINTF(" ISP_CMD_PROGRAM_FLASH\r\n");
      _rspsize = Timeout::command(&write_flash);
    }
    else if (_cmd == 0x14) {    /* CMD_READ_FLASH_ISP */
      D1PRINTF(" ISP_CMD_READ_FLASH\r\n");
      _rspsize = Timeout::command(&read_flash);
    }
    else if (_cmd == 0x15) {    /* CMD_PROGRAM_EEPROM_ISP */
      D1PRINTF(" ISP_CMD_PROGRAM_EEPROM\r\n");
      _rspsize = Timeout::command(&write_eeprom);
    }
    else if (_cmd == 0x16) {    /* CMD_READ_EEPROM_ISP */
      D1PRINTF(" ISP_CMD_READ_EEPROM\r\n");
      _rspsize = Timeout::command(&read_memories);
    }
    else if (_cmd == 0x17       /* CMD_PROGRAM_FUSE_ISP */
          || _cmd == 0x19) {    /* CMD_PROGRAM_LOCK_ISP */
      D1PRINTF(" ISP_CMD_PROGRAM_%s\r\n",
        _cmd == 0x17 ? "FUSE" : "LOCK"
      );
      _rspsize = Timeout::command(&write_fuse_byte);
      /* This command returns one extra byte in the response. */
      packet.in.isp.data[0] = 0;
      packet.in.isp.res = _rspsize ? 0x00 : 0x80;
  #if !defined(NDEBUG) && defined(DEBUG)
      D1PRINTF(" ISP_RES L=%d\r\n  ", 2);
      D1PRINTHEX(&packet.in.isp.cmd, 3, ':', 16, "\r\n  ");
  #endif
      return 2;
    }
    else if (_cmd == 0x18       /* CMD_READ_FUSE_ISP */
          || _cmd == 0x1A       /* CMD_READ_LOCK_ISP */
          || _cmd == 0x1B       /* CMD_READ_SIGNATURE_ISP */
          || _cmd == 0x1C) {    /* CMD_READ_OSCCAL_ISP */
      D1PRINTF(" ISP_CMD_READ_%s\r\n",
        _cmd == 0x18 ? "FUSE" :
        _cmd == 0x1A ? "LOCK" :
        _cmd == 0x1B ? "SIGNATURE" : "OSCCAL"
      );
      _rspsize = Timeout::command(&read_fuse_byte);
    }
    else {
      D1PRINTF(" ISP_CMD_?[%02X]\r\n", _cmd);
      packet.in.isp.res = 0xC9; /* STATUS_CMD_UNKNOWN */
      return 1;
    }

    if (_rspsize == 0) {
      packet.in.isp.res = 0x80; /* STATUS_CMD_TOUT */
      _rspsize = 1;
    }
    else {
      packet.in.data[_rspsize] = 0;
      if (_rspsize > 1) _rspsize += 1;
      packet.in.isp.res = 0x00; /* STATUS_CMD_OK */
    }

  #if !defined(NDEBUG) && defined(DEBUG)
    D1PRINTF(" ISP_RES L=%d\r\n  ", _rspsize);
    D1PRINTHEX(&packet.in.isp.cmd, _rspsize + 1, ':', 16, "\r\n  ");
  #endif
    return _rspsize;
  }

};  /* ISP */

#endif

// end of code

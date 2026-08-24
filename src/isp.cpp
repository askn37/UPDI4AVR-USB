/**
 * @file isp.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.35.50+
 * @date 2026-08-19
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
 */
/* AB H C <- Arduino Board, ICSP Header, CNumber */
/* 13 3 6 */#define PIN_PGM_MSCK PIN_VCP_TXD
/* 12 4 5 */#define PIN_PGM_MISO PIN_VCP_RXD
/* GN 6 4 */
/* VC 2 3 */
/* 11 1 2 */#define PIN_PGM_MOSI PIN_PGM_TDAT
/* NR 5 1 */#define PIN_PGM_MRST PIN_PGM_TRST

/* These I/O ports must belong to the same port group. */
#define SPI_MASK (pinBitmask(PIN_PGM_MOSI)|pinBitmask(PIN_PGM_MSCK)|pinBitmask(PIN_PGM_TRST))

namespace ISP {

  // MARK: ISP Low level

  NOINIT uint16_t _base_addr;
  NOINIT uint8_t _ser[4], _res[4];
  NOINIT uint8_t _pgm_retry;

  uint8_t spi_exchange (const uint8_t _data) {
    RXDATA = _data;
    for (uint8_t _i = 0; _i < 8; _i++) {
      if (RXDATA & 0x80) digitalWriteMacro(PIN_PGM_MOSI, HIGH);
      else               digitalWriteMacro(PIN_PGM_MOSI, LOW);

      loop_until_bit_is_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
      digitalWriteMacro(PIN_PGM_MSCK, HIGH);
      RXDATA <<= 1;
      bit_is_clear(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
      if (digitalReadMacro(PIN_PGM_MISO)) RXDATA |= 1;

      loop_until_bit_is_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
      digitalWriteMacro(PIN_PGM_MSCK, LOW);
      bit_is_clear(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_LUNF_bp);
    };
    return RXDATA;
  }

  void spi_transaction (uint8_t _cmd[]) {
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
    spi_transaction(&packet.out.data[1]);

    /* Read and return the value at the specified position. */
    packet.in.isp.data[0] = _res[packet.out.data[0] - 1];
    return 1;
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
      _ser[3] = 0xFF;
      spi_transaction(_ser);
      packet.in.isp.data[_index] = _res[3];
      _addr += 1;
    }
    return _length;
  }

  size_t read_flash (void) {
    uint16_t _addr = _base_addr;
    size_t _length = bswap16(packet.out.isp.wValue);
    uint8_t _type = packet.out.isp.data[2];
    D1PRINTF(" M=$%04X, L=%d, T=$%02X\r\n", _addr << 1, _length, _type);
    for (uint8_t _index = 0; _index < _length;) {
      _ser[0] = _type;
      _ser[1] = _CAPS16(_addr)->bytes[1];
      _ser[2] = _CAPS16(_addr)->bytes[0];
      _ser[3] = 0xFF;
      spi_transaction(_ser);
      packet.in.isp.data[_index++] = _res[3];
      _ser[0] |= 0x08;
      spi_transaction(_ser);
      packet.in.isp.data[_index++] = _res[3];
      _addr += 1;
    }
    return _length;
  }

  size_t chip_erase (void) {
    /* Send chip erase command. */
    spi_transaction(&packet.out.data[2]);
    uint16_t _delay = packet.out.data[1];
    if (_delay == 0) _delay = 9;
    Timeout::delay_rtc_millis(_delay);
    return 0;
  }

  size_t write_flash (void) {
    /* _length must be a power of 2 (not verified). */
    /* The lower bits of _addr must be zero-filled accordingly. */
    /* Ex) addr=$0000(word)                   */
    /* 00:20:C1:0A:40:4C:20:00:00:01:02:03:04 */
    /* lenBE|  |ms|WW|FF|RR|     |words...    */
    uint16_t _addr = _base_addr;
    size_t _length = bswap16(packet.out.isp.wValue);
    uint16_t _delay = packet.out.data[3];
    uint8_t _mask = (_length >> 1) - 1;
    D1PRINTF(" M=$%04X, L=%d\r\n", _addr, _length);
    for (uint8_t _index = 0; _index < _length; _index += 2) {
      _ser[0] = packet.out.isp.data[4];
      _ser[1] = 0;
      _ser[2] = _CAPS16(_addr)->bytes[0] & _mask;
      _ser[3] = packet.out.data[_index + 9];
      spi_transaction(_ser);
      _ser[0] = packet.out.isp.data[4] | 0x08;
      _ser[1] = 0;
      _ser[2] = _CAPS16(_addr)->bytes[0] & _mask;
      _ser[3] = packet.out.data[_index + 10];
      spi_transaction(_ser);
      _addr += 1;
    }
    _ser[0] = packet.out.isp.data[5];
    _ser[1] = _CAPS16(_base_addr)->bytes[1];
    _ser[2] = _CAPS16(_base_addr)->bytes[0] & ~_mask;
    _ser[3] = 0xFF;
    spi_transaction(_ser);
    if (_delay == 0) _delay = 5;
    Timeout::delay_rtc_millis(_delay);
    return 0;
  }

  size_t write_eeprom (void) {
    /* _length must be a power of 2 (not verified). */
    /* The lower bits of _addr must be zero-filled accordingly. */
    /* Ex) addr=$0000(byte)                   */
    /* 00:04:C1:05:C1:C2:A0:00:00:01:02:03:04 */
    /* lenBE|  |ms|WW|FF|RR|     |bytes...    */
    uint16_t _addr = _base_addr;
    size_t _length = bswap16(packet.out.isp.wValue);
    uint16_t _delay = packet.out.data[3];
    uint8_t _mask = _length - 1;
    D1PRINTF(" M=$%04X, L=%d\r\n", _addr, _length);
    for (uint8_t _index = 0; _index < _length; _index++) {
      _ser[0] = packet.out.isp.data[4];
      _ser[1] = 0;
      _ser[2] = _CAPS16(_addr)->bytes[0] & _mask;
      _ser[3] = packet.out.data[_index + 9];
      spi_transaction(_ser);
      _addr += 1;
    }
    _ser[0] = packet.out.isp.data[5];
    _ser[1] = _CAPS16(_base_addr)->bytes[1];
    _ser[2] = _CAPS16(_base_addr)->bytes[0] & ~_mask;
    _ser[3] = 0xFF;
    spi_transaction(_ser);
    if (_delay == 0) _delay = 4;
    Timeout::delay_rtc_millis(_delay);
    return 0;
  }

  size_t write_fuse_byte (void) {
    /* Send FUSE write command. */
    spi_transaction(&packet.out.data[0]);
    return 0;
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
    portRegister(PIN_PGM_TRST).DIRCLR = SPI_MASK | pinBitmask(PIN_PGM_MISO);
    pinControlRegister(PIN_PGM_TRST) = PORT_ISC_INTDISABLE_gc | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_MISO) = PORT_ISC_INTDISABLE_gc;
    pinControlRegister(PIN_PGM_MSCK) = PORT_ISC_INTDISABLE_gc;
    pinControlRegister(PIN_PGM_MOSI) = PORT_ISC_INTDISABLE_gc;

    portRegister(PIN_PGM_TRST).OUTCLR = SPI_MASK;
    portRegister(PIN_PGM_TRST).DIRSET = SPI_MASK;

    /* Bit-banging accuracy */
    uint8_t _period = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1_gc;
    _ret = (F_CPU / 2000) / _xclk;
    if (_ret == 0) _ret = 7;          /* Under-limit: 1250 kbps */
    else if (_ret > 255) {
      _ret >>= 4;
      _period = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV16_gc;
      if (_ret > 255) {
        _ret >>= 4;
        _period = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV256_gc;
        if (_ret > 255) _ret = 255;
      }
    }

    /* If reading back the BUS yields zero, it is normal. */
    if (portRegister(PIN_PGM_TRST).IN & SPI_MASK) {
      D1PRINTF(" SPI Bus Fail\r\n");
      return -1;
    }

    /* TCA0_WO0 setup */
    TCA0_SPLIT_CTRLB = 0;
    TCA0_SPLIT_LCNT  = _ret - 1;
    TCA0_SPLIT_LPER  = _ret - 1;
    TCA0_SPLIT_LCMP0 = _ret >> 1;
    TCA0_SPLIT_CTRLA = _period;
    D1PRINTF(" XCK=%d>%d,%02X\r\n", _xclk, _ret, _period);

    for (uint8_t _i = 0; _i < 3; _i++) {
      digitalWriteMacro(PIN_PGM_TRST, HIGH);
      while (!digitalReadMacro(PIN_PGM_TRST));

      /* Pulse must be minimum 2 target CPU clock cycles
        so 100 usec is ok for CPU speeds above 20 KHz */
      SYS::delay_100us();
      digitalWriteMacro(PIN_PGM_TRST, LOW);
      while (digitalReadMacro(PIN_PGM_TRST));
      SYS::delay_40ms();

      /* Programming start command. */
      spi_transaction(&packet.out.data[7]);

      /* Verification after reading. */
      if (_res[packet.out.data[6] - 1] == packet.out.data[5]) {
        PGCONF = PGCONF_PGIA_bm | PGCONF_PROG_bm;
        D1PRINTF(" PGM OK\r\n");
        return 0;
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
    if (_xclk > 100) _xclk -= 50;
    else if (_xclk > 1) _xclk = (_xclk >> 1) + (_xclk >> 2);
    return 1;
  }

  size_t sign_off (void) {
    if (PGCONF) {
      /* TCA0_WO0 reset */
      TCA0_SPLIT_CTRLA = 0;
      TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1024_gc;

      portRegister(PIN_PGM_TRST).OUTCLR = SPI_MASK;
      pinControlRegister(PIN_VCP_TXD)  = PORT_PULLUPEN_bm | PORT_ISC_INTDISABLE_gc;
      pinControlRegister(PIN_VCP_RXD)  = PORT_PULLUPEN_bm | PORT_ISC_INTDISABLE_gc;
      pinControlRegister(PIN_PGM_TDAT) = PORT_PULLUPEN_bm | PORT_ISC_INTDISABLE_gc;
      pinControlRegister(PIN_PGM_TRST) = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
      portRegister(PIN_PGM_TRST).DIRCLR = SPI_MASK;

      SYS::power_reset();
      SYS::delay_2500us();
      PGCONF = 0;
      USART::setup();
      USART::change_vcp();
    }
    return 0;
  }

  // MARK: JTAG SCOPE

  /*** The ISP scope provides access to Low-Voltage SPI control AVR-8 chips. ***/
  /*
   * The packets within this scope are a subset of the STK500v2 protocol.
   * They also differ slightly from the AVRISPmkII (AVR069 document).
   */
  size_t jtag_scope_isp (void) {
    int _rspsize = -2;
    /* Return 1 for _rspsize, whether the operation succeeds or fails.
       Indicate the size of the returned data by incrementing _rspsize by 1. */
    uint8_t _length = _packet_length - 6;
    uint8_t _cmd = packet.out.cmd;
  #if !defined(NDEBUG) && defined(DEBUG)
    D1PRINTF(" L=%d\r\n  ", _length);
    if (_length)
      D1PRINTHEX(&packet.out.data, _length, ':', 16, "\r\n  ");
    else
      D1PRINTF("\r\n");
  #endif
    if (_cmd == 0x10) {         /* CMD_ENTER_PROGMODE_ISP */
      D1PRINTF(" ISP_CMD_ENTER_PROGMODE\r\n");
      if (_length >= 11) {
        _pgm_retry = 4;
        _rspsize = Timeout::command(&connect, &timeout_fallback, 300);
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
      _rspsize = 0;
    }
    else if (_cmd == 0x1E) {    /* CMD_GET_SCK */
      D1PRINTF(" PIC_CMD_GET_SCK=%d\r\n", _xclk);
      /* This command gives a non-standard response. */
      packet.in.stk500v2.wValue = _xclk; /* kbps */
      D1PRINTF(" PIC_RES L=2\r\n  ");
      D1PRINTHEX(&packet.in.stk500v2.data, 2, ':');
      return 2;
    }
    else if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) {
      /* The steps below will not be executed unless
         program mode has been successfully entered. */
      _rspsize = -1;
    }
    else if (_cmd == 0x06) {    /* CMD_LOAD_ADDRESS */
      _base_addr = bswap32(packet.out.isp.dwValue);
      D1PRINTF(" ISP_CMD_LOAD_ADDRESS=$%04X\r\n", _base_addr);
      _rspsize = 0;
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
      if (_rspsize == 0) _rspsize = -3;
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

    if (_rspsize == -2) {
      packet.in.isp.res = 0x80; /* STATUS_CMD_TOUT */
      _rspsize = 1;
    }
    else if (_rspsize == -1) {
      packet.in.isp.res = 0xC0; /* STATUS_CMD_FAILED */
      _rspsize = 1;
    }
    else {
      packet.in.isp.res = 0x00; /* STATUS_CMD_OK */
      if (_rspsize > 0) {
        packet.in.data[_rspsize] = 0;
        _rspsize += 2;
      }
      else if (_rspsize == -3) {/* PROGRAM_[FUSE|LOCK] only */
        packet.in.data[0] = 0;
        packet.in.data[1] = 0;
        _rspsize = 2;
      }
      else {
        _rspsize = 1;
      }
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

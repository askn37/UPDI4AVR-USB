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
 * While much of the communication payload resembles the STK500v2
 * (JTAGICEmkII) -— specifically the AVR068 specification -—
 * some parts are unique to the `PICKit4`.
 *
 * For example; the parameters for `CMD_SET_SCK` and `CMD_GET_SCK`
 * are specified in milliseconds rather than frequency.
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

  NOINIT uint32_t _base_addr;
  NOINIT uint8_t _res[4];

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
    D1PRINTF(" S/"); D1PRINTHEX(_cmd, 4);
    D1PRINTF(" R/"); D1PRINTHEX(_res, 4);
  }

  // MARK: ISP NVM API

  size_t read_fuse_byte (void) {
    /* Send FUSE read command. */
    spi_transaction(&packet.out.data[1]);

    /* Read and return the value at the specified position. */
    packet.in.isp.data[0] = _res[packet.out.data[0] - 1];
    return 2; /* return size + 1 */
  }

  // MARK: ISP Session

  /*
   * We will begin with low-voltage SPI control.
   * High-voltage dWire control is separate.
   *
   * 
   */
  size_t connect (void) {
    size_t _ret;
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
    _ret = (F_CPU / 2000) / _xclk;
    if (_ret == 0) _ret = 7;          /* Under-limit: 1250 kbps */
    else if (_ret > 255) _ret = 255;  /* Over-limit: 39 kbps */

    /* If reading back the BUS yields zero, it is normal. */
    if (portRegister(PIN_PGM_TRST).IN & SPI_MASK) {
      D1PRINTF(" SPI Bus Fail\r\n");
      return 2;
    }

    /* TCA0_WO0 setup */
    TCA0_SPLIT_CTRLB = 0;
    TCA0_SPLIT_LCNT  = _ret - 1;
    TCA0_SPLIT_LPER  = _ret - 1;
    TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1_gc;
    D1PRINTF(" XCK=%d>%d\r\n", _xclk, _ret);

    for (uint8_t _i = 0; _i < 3; _i++) {
      digitalWriteMacro(PIN_PGM_TRST, HIGH);

      /* Pulse must be minimum 2 target CPU clock cycles
        so 100 usec is ok for CPU speeds above 20 KHz */
      SYS::delay_100us();
      digitalWriteMacro(PIN_PGM_TRST, LOW);
      SYS::delay_20ms();

      /* Programming start command. */
      spi_transaction(&packet.out.data[7]);

      /* Verification after reading. */
      if (_res[packet.out.data[6] - 1] == packet.out.data[5]) {
        PGCONF = PGCONF_PGIA_bm | PGCONF_PROG_bm;
        D1PRINTF(" PGM OK\r\n");
        return 1;
      }
    }

    /* Time out on failure. */
    for (;;);
  }

  size_t timeout_fallback (void) {
    D1PRINTF(" Fail\r\n");
    /* If a timeout occurs, the communication speed will be reduced. */
    _xclk -= _xclk > 100 ? 50 : 20;
    return _xclk < 40 ? 0 : 1;
  }

  uint8_t disconnect (void) {
    return 1;
  }

  uint8_t sign_off (void) {
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
    return 1;
  }

  // MARK: JTAG SCOPE

  /*** The ISP scope provides access to Low-Voltage SPI control AVR-8 chips. ***/
  /*
   * The packets within this scope are a subset of the STK500v2 protocol.
   * They also differ slightly from the AVRISPmkII (AVR069 document).
   */
  size_t jtag_scope_isp (void) {
    uint8_t _rspsize = 0;
    /* Return 1 for _rspsize, whether the operation succeeds or fails.
       Indicate the size of the returned data by incrementing _rspsize by 1. */
    uint8_t _length = _packet_length - 6;
    uint8_t _cmd = packet.out.cmd;
    D1PRINTF(" %d/", _length);
    D1PRINTHEX(&packet.out.data, _length);
    if (_cmd == 0x10) {             /* CMD_ENTER_PROGMODE_ISP */
      D1PRINTF(" ISP_CMD_ENTER_PROGMODE\r\n");
      if (_length >= 11) {
        _rspsize = Timeout::command(&connect, &timeout_fallback, packet.out.data[0]);
                  /* STATUS_CMD_FAILED : STATUS_CMD_OK : STATUS_CMD_TOUT */
        packet.in.isp.res = _rspsize == 2 ? 0xC0 : _rspsize ? 0x00 : 0x80;
        return 1;
        /* If STATUS_CMD_FAILED is returned here,
           CMD3_START_DW_DEBUG will be sent next. */
      }
    }
    else if (_cmd == 0x11) {        /* CMD_LEAVE_PROGMODE_ISP */
      D1PRINTF(" ISP_CMD_LEAVE_PROGMODE\r\n");
      _rspsize = sign_off();
    }
    else if (_cmd == 0x1D) {        /* CMD_SET_SCK */
      uint16_t _data = packet.out.isp.wValue;
      D1PRINTF(" ISP_CMD_SET_SCK=%d\r\n", _data);
      /* This command sends microseconds. */
      if (_data > 0) _xclk = 1000 / _data;  /* convert us to kbps */
      D1PRINTF(" XCLK=%d\r\n", _xclk);
      _rspsize = 1;
    }
    else if (_cmd == 0x1E) {        /* CMD_GET_SCK */
      D1PRINTF(" ISP_CMD_GET_SCK=%d\r\n", _xclk);
      /* This command gives a non-standard response. */
      _CAPS16(packet.in.stk500v2.data)->word = _xclk; /* kbps */
      return 2;
    }
    // else if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) { /* empty */ }
    else if (_cmd == 0x06) {        /* CMD_LOAD_ADDRESS */
      D1PRINTF(" ISP_CMD_LOAD_ADDRESS=%08lX\r\n", packet.out.isp.dwValue);
      _base_addr = packet.out.isp.dwValue;
      _rspsize = 1;
    }
    else if (_cmd == 0x12) {        /* CMD_CHIP_ERASE_ISP */
      D1PRINTF(" ISP_CMD_CHIP_ERASE\r\n");
    }
    else if (_cmd == 0x13) {        /* CMD_PROGRAM_FLASH_ISP */
      D1PRINTF(" ISP_CMD_PROGRAM_FLASH\r\n");
    }
    else if (_cmd == 0x14) {        /* CMD_READ_FLASH_ISP */
      D1PRINTF(" ISP_CMD_READ_FLASH\r\n");
    }
    else if (_cmd == 0x15) {        /* CMD_PROGRAM_EEPROM_ISP */
      D1PRINTF(" ISP_CMD_PROGRAM_EEPROM\r\n");
    }
    else if (_cmd == 0x16) {        /* CMD_READ_EEPROM_ISP */
      D1PRINTF(" ISP_CMD_READ_EEPROM\r\n");
    }
    else if (_cmd == 0x17           /* CMD_PROGRAM_FUSE_ISP */
          || _cmd == 0x19) {        /* CMD_PROGRAM_LOCK_ISP */
      D1PRINTF(" ISP_CMD_PROGRAM_%s\r\n",
        _cmd == 0x17 ? "FUSE" : "LOCK"
      );
    }
    else if (_cmd == 0x18           /* CMD_READ_FUSE_ISP */
          || _cmd == 0x1A           /* CMD_READ_LOCK_ISP */
          || _cmd == 0x1B           /* CMD_READ_SIGNATURE_ISP */
          || _cmd == 0x1C) {        /* CMD_READ_OSCCAL_ISP */
      D1PRINTF(" ISP_CMD_READ_%s\r\n",
        _cmd == 0x18 ? "FUSE" :
        _cmd == 0x1A ? "LOCK" :
        _cmd == 0x1B ? "SIGNATURE" : "OSCCAL"
      );
      _rspsize = Timeout::command(&read_fuse_byte);
    }
    else {
      D1PRINTF(" ISP_CMD_?[%02X]\r\n", _cmd);
      packet.in.isp.res = 0xC9;     /* STATUS_CMD_UNKNOWN */
      return 1;
    }
    if (_rspsize) {
      packet.in.isp.res = 0x00;     /* STATUS_CMD_OK */
    }
    else {
      packet.in.isp.res = 0xC0;     /* STATUS_CMD_FAILED */
      _rspsize = 1;
    }
    D1PRINTF(" ISP_RSP=%02x>", _rspsize);
    D1PRINTHEX(&packet.in.isp.res, _rspsize);
    return _rspsize;
  }

};  /* ISP */

#endif

// end of code

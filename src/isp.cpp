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

namespace ISP {

  // MARK: ISP Low level

  // MARK: ISP NVM API

  // MARK: ISP Session

  // MARK: JTAG SCOPE

  /*** The ISP scope provides access to Low-Voltage SPI control AVR-8 chips. ***/
  /*
   * The packets within this scope are a subset of the STK500v2 protocol.
   * They also differ slightly from the AVRISPmkII (AVR069 document).
   */
  size_t jtag_scope_isp (void) {
    size_t _rspsize = 0;
    uint8_t _cmd = packet.out.cmd;
    if (_cmd == 0x10) {             /* CMD_ENTER_PROGMODE_ISP */
    }
    else if (_cmd == 0x11) {        /* CMD_LEAVE_PROGMODE_ISP */
    }
    else if (_cmd == 0x1D) {        /* CMD_SET_SCK */
    }
    else if (_cmd == 0x1E) {        /* CMD_GET_SCK */
    }
    else if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) { /* empty */ }
    else if (_cmd == 0x12) {        /* CMD_CHIP_ERASE_ISP */
    }
    else if (_cmd == 0x13) {        /* CMD_PROGRAM_FLASH_ISP */
    }
    else if (_cmd == 0x14) {        /* CMD_READ_FLASH_ISP */
    }
    else if (_cmd == 0x15) {        /* CMD_PROGRAM_EEPROM_ISP */
    }
    else if (_cmd == 0x16) {        /* CMD_READ_EEPROM_ISP */
    }
    else if (_cmd == 0x17           /* CMD_PROGRAM_FUSE_ISP */
          || _cmd == 0x19) {        /* CMD_PROGRAM_LOCK_ISP */
    }
    else if (_cmd == 0x18           /* CMD_READ_FUSE_ISP */
          || _cmd == 0x1A           /* CMD_READ_LOCK_ISP */
          || _cmd == 0x1B           /* CMD_READ_SIGNATURE_ISP */
          || _cmd == 0x1C) {        /* CMD_READ_OSCCAL_ISP */
    }
    else {
      packet.in.res = 0xC9;         /* STATUS_CMD_UNKNOWN */
      return _rspsize;
    }              /* STATUS_CMD_OK : STATUS_CMD_FAILED */
    packet.in.res = _rspsize ? 0x00 : 0xC0;
    return _rspsize;
  }

};  /* ISP */

#endif

// end of code

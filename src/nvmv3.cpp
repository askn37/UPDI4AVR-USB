/**
 * @file nvmv3.cpp
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
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * UPDI NVM version 3 is used in the AVR-EA series.
 * It has the following features:
 *
 * - The data area is at the beginning of the 24-bit address space.
 *   The flash area is at the end of the 24-bit address space.
 *   All other memory types are in the data space.
 * 
 * - The signature is at address 0x1100.
 * 
 * - Flash and EEPROM each have their own dedicated page buffer memory.
 * 
 * - EEPROM can be written in 8-byte units.
 *   There is no page erase command.
 * 
 * - Flash can be written in 64-byte units.
 * 
 * - FUSE must be written in the same way as EEPROM.
 * 
 * - Erasing and rewriting a flash memory page are separate commands.
 * 
 * - USERROW is written in the same way as flash, so a page erase is required.
 */

namespace NVM::V3 {

  constexpr auto NVM_CTRL   = 0x1000;
  constexpr auto NVM_STATUS = 0x1006;
  constexpr auto PROD_SIG   = 0x1100;
  constexpr auto PROG_START = 0x800000;

  uint8_t nvm_wait (void) {
    do { UPDI::recv_byte(NVM_STATUS); } while (RXDATA & 3);
    return RXDATA;
  }

  bool nvm_ctrl_change (uint8_t _nvmcmd) {
    nvm_wait();
    if (UPDI::recv_byte(NVM_CTRL) && RXDATA == _nvmcmd) return true;
    if (!UPDI::nvm_ctrl(0x00)) return false;
    if (0 != _nvmcmd) return UPDI::nvm_ctrl(_nvmcmd);
    return true;
  }

  bool write_words_flash (uint32_t _dwAddr, size_t _wLength) {
    D2PRINTF(" NVM_V3_FLPERW=%06lX\r\n", _dwAddr);
    return (
      nvm_ctrl_change(0x00)
      && UPDI::send_words_block(_dwAddr, _wLength)
      && nvm_ctrl_change(0x05)  /* NVM_V3_CMD_FLPERW */
      && (nvm_wait() & 0x73) == 0
    );
  }

  bool write_bytes_flash (uint32_t _dwAddr, size_t _wLength) {
    D2PRINTF(" NVM_V3_FLPERW=%06lX\r\n", _dwAddr);
    return (
      nvm_ctrl_change(0x00)
      && UPDI::send_bytes_block(_dwAddr, _wLength)
      && nvm_ctrl_change(0x05)  /* NVM_V3_CMD_FLPERW */
      && (nvm_wait() & 0x73) == 0
    );
  }

  bool write_eeprom (uint32_t _dwAddr, size_t _wLength) {
    D2PRINTF(" NVM_V3_EEPERW=%06lX\r\n", _dwAddr);
    return (
      nvm_ctrl_change(0x10)
      && UPDI::send_bytes_block(_dwAddr, _wLength)
      && nvm_ctrl_change(0x15)  /* NVM_V3_CMD_EEPERW */
      && (nvm_wait() & 0x73) == 0
    );
  }

  size_t prog_init (void) {
    nvm_ctrl_change(0x0F);      /* NVM_V3_CMD_FLPBCLR */
    nvm_ctrl_change(0x1F);      /* NVM_V3_CMD_EEPBCLR */
    return nvm_ctrl_change(0x00);
  }

  size_t read_memory (void) {
    uint8_t   m_type = packet.out.bMType;
    uint32_t _dwAddr = packet.out.dwAddr;
    size_t  _wLength = packet.out.dwLength;
    if (bit_is_set(PGCONF, PGCONF_PROG_bp)) {
      if (m_type == 0xB0) {
        /* MTYPE_FLASH_PAGE (PROGMEM) */
        _dwAddr += PROG_START;
        return UPDI::recv_words_block(_dwAddr, _wLength) ? _wLength + 1 : 0;
      }
      if (m_type == 0xB4) {
        /* MTYPE_SIGN_JTAG */
        _dwAddr = PROD_SIG + ((uint8_t)_dwAddr & 0x7F);
      }
      if (UPDI::recv_bytes_block(_dwAddr, _wLength))
        return _wLength + 1;
    }
    return 0;
  }

  size_t erase_memory (void) {
    uint8_t e_type = packet.out.bEType;
    if (e_type == 0x00) {
      /* XMEGA_ERASE_CHIP */
      return UPDI::chip_erase();
    }
    /* Page erase will not be used if received. */
    return 1;
  }

  size_t write_memory (void) {
    uint8_t   m_type = packet.out.bMType;
    uint32_t _dwAddr = packet.out.dwAddr;
    size_t  _wLength = packet.out.dwLength;
    if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) return UPDI::write_userrow();
    if (m_type == 0x22 || m_type == 0xB2 || m_type == 0xB3 || m_type == 0xC4) {
      /* MTYPE_FUSE_BITS */
      /* MTYPE_LOCK_BITS */
      /* MTYPE_EEPROM */
      /* MTYPE_EEPROM_XMEGA */
      return write_eeprom(_dwAddr, _wLength);
    }
    else if (m_type == 0xC0 || m_type == 0xC5) {
      /* MTYPE_FLASH (alias) */
      /* MTYPE_USERSIG (USERROW) */
      return write_bytes_flash(_dwAddr, _wLength);
    }
    else if (m_type == 0xB0) {
      /* MTYPE_FLASH_PAGE (PROGMEM) */
      _dwAddr += PROG_START;
      return write_words_flash(_dwAddr, _wLength);
    }
    else {
      /* MTYPE_SRAM */
      return UPDI::send_bytes_block(_dwAddr, _wLength);
    }
  }

  bool setup (void) {
    Command_Table.prog_init    = &prog_init;
    Command_Table.read_memory  = &read_memory;
    Command_Table.erase_memory = &erase_memory;
    Command_Table.write_memory = &write_memory;
    return true;
  }

};

// end of code

/**
 * @file nvmv0.cpp
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
 * UPDI NVM version 0 is used in the tinyAVR-0, 1, 2 and megaAVR-0 series.
 * It has the following features:
 * 
 * - The total address range of the flash and data areas is 64KiB.
 *   Flash is mapped into the second half of the 64KiB address range.
 *   The offset value is different between tinyAVR and megaAVR.
 * 
 * - ACC only uses 16-bit addresses, but it can also use
 *   24-bit addresses with a zero high-order byte.
 *   The use of PDI-compatible 32-bit addresses is explicitly prohibited.
 * 
 * - The signature is located at address 0x1100.
 * 
 * - Fuse uses special commands to rewrite byte by byte.
 * 
 * - There is a special page buffer memory, it is not SRAM.
 * 
 * - EEPROM can be written in 32/64-byte units.
 *   It writes faster than other series.
 *   There is no page erase command.
 * 
 * - Flash can be written in 64/128-byte units.
 *   Page erase is only available for Flash memory.
 * 
 * - Erase/write command combinations are also available for flash memory.
 *   So normally, page erasure is not necessary.
 * 
 * - USERROW is an EEPROM type memory.
 */

namespace NVM::V0 {

  constexpr auto NVM_CTRL   = 0x1000;
  constexpr auto NVM_STATUS = 0x1002;
  constexpr auto NVM_DATA   = 0x1006;
  constexpr auto PROD_SIG   = 0x1100;

  uint8_t nvm_wait (void) {
    do { UPDI::recv_byte(NVM_STATUS); } while (RXDATA & 3);
    return RXDATA;
  }

  bool write_fuse (uint16_t _wAddr, size_t _wLength) {
    struct fuse_packet_t {
      uint16_t data;  /* NVMCTRL_REG_DATA */
      uint16_t addr;  /* NVMCTRL_REG_ADDR */
    } fuses;
    for (size_t _i = 0; _i < _wLength; _i++) {
      fuses.data = packet.out.memData[_i];
      fuses.addr = _wAddr + _i;
      D2PRINTF(" NVM_V0_WFU=%04X<%02X\r\n", fuses.addr, fuses.data);
      nvm_wait();
      if (!(UPDI::send_bytes_data(NVM_DATA, (uint8_t*)&fuses, 4)
        && UPDI::nvm_ctrl(0x07)   /* NVM_CMD_WFU */
        && (nvm_wait() & 7) == 0)) return false;
    }
    return true;
  }

  bool write_flash (uint16_t _wAddr, size_t _wLength) {
    D2PRINTF(" NVM_V0_ERWP=%04X\r\n", _wAddr);
    if (SYS::is_boundary_flash_page(_wAddr)) {
      nvm_wait();
      UPDI::nvm_ctrl(0x04);       /* NVM_CMD_PBC */
    }
    nvm_wait();
    return (
      UPDI::send_bytes_block(_wAddr, _wLength)
      && UPDI::nvm_ctrl(0x03)     /* NVM_CMD_ERWP */
      && (nvm_wait() & 7) == 0
    );
  }

  bool write_eeprom (uint16_t _wAddr, size_t _wLength) {
    D2PRINTF(" NVM_V0_ERWP=%04X\r\n", _wAddr);
    nvm_wait();
    return (
      UPDI::send_bytes_block(_wAddr, _wLength)
      && UPDI::nvm_ctrl(0x03)     /* NVM_CMD_ERWP */
      && (nvm_wait() & 7) == 0
    );
  }

  size_t prog_init (void) {
    nvm_wait();
    UPDI::nvm_ctrl(0x04);         /* NVM_CMD_PBC */
    nvm_wait();
    return UPDI::nvm_ctrl(0x00);
  }

  size_t read_memory (void) {
    uint8_t  m_type = packet.out.bMType;
    uint16_t _wAddr = packet.out.dwAddr;
    size_t _wLength = packet.out.dwLength;
    if (m_type == 0xB4) {
      /* MTYPE_SIGN_JTAG */
      _wAddr = PROD_SIG + ((uint8_t)_wAddr & 0x7F);
    }
    else if (m_type == 0xB0) {
      /* MTYPE_FLASH_PAGE (PROGMEM) */
      _wAddr += Device_Descriptor.UPDI.prog_base;
    }
    if (bit_is_set(PGCONF, PGCONF_PROG_bp)
     && UPDI::recv_bytes_block(_wAddr, _wLength))
      return _wLength + 1;
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
    uint8_t  m_type = packet.out.bMType;
    uint16_t _wAddr = packet.out.dwAddr;
    size_t _wLength = packet.out.dwLength;
    if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) return UPDI::write_userrow();
    if (m_type == 0xB2 || m_type == 0xB3) {
      /* MTYPE_FUSE_BITS */
      /* MTYPE_LOCK_BITS */
      return write_fuse(_wAddr, _wLength);
    }
    else if (m_type == 0x22 || m_type == 0xC4 || m_type == 0xC5) {
      /* MTYPE_EEPROM */
      /* MTYPE_EEPROM_XMEGA */
      /* MTYPE_USERSIG (USERROW) */
      return write_eeprom(_wAddr, _wLength);
    }
    else if (m_type == 0xB0 || m_type == 0xC0) {
      /* MTYPE_FLASH_PAGE (PROGMEM) */
      /* MTYPE_FLASH (alias) */
      _wAddr += Device_Descriptor.UPDI.prog_base;
      return write_flash(_wAddr, _wLength);
    }
    else {
      /* MTYPE_SRAM */
      return UPDI::send_bytes_block(_wAddr, _wLength);
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

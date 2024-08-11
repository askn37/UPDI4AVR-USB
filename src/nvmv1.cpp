/**
 * @file nvmv1.cpp
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
 * An implementation of UPDI NVM Version 1 does not exist yet.
 * This file is a template for the future.
 */

namespace NVM::V1 {

  size_t prog_init (void) { return 0; }

  size_t erase_memory (void) {
    /* The addressing of the chip erase is meaningless. */
    // if      (e_type == 0x00)   /* XMEGA_ERASE_CHIP */
    /* The following require addressing: */
    // else if (e_type == 0x03)   /* XMEGA_ERASE_EEPROM */
    // else if (e_type == 0x04)   /* XMEGA_ERASE_APP_PAGE (PROGMEM) */
    // else if (e_type == 0x06)   /* XMEGA_ERASE_EEPROM_PAGE */
    // else if (e_type == 0x07)   /* XMEGA_ERASE_USERSIG */
    /* BOOTROW should be cleared with the same command as USERROW. */
    /* The PROGMEM area must set the MSB of the 24-bit address. */
    return 0;
  }

  size_t read_memory (void) {
    /* The PROGMEM area must set the MSB of the 24-bit address. */
    // if (m_type == 0xB0)        /* MTYPE_FLASH_PAGE (PROGMEM) */
    // else                       /* Other */
    return 0;
  }

  size_t write_memory (void) {
    /* The PROGMEM area must set the MSB of the 24-bit address. */
    // if      (m_type == 0x20)   /* MTYPE_SRAM */
    // else if (m_type == 0x22)   /* MTYPE_EEPROM (EEPROM) */
    // else if (m_type == 0xB0)   /* MTYPE_FLASH_PAGE (PROGMEM) */
    // else if (m_type == 0xB2)   /* MTYPE_FUSE_BITS (FUSE) */
    // else if (m_type == 0xB3)   /* MTYPE_LOCK_BITS (FUSE) */
    // else if (m_type == 0xB4)   /* MTYPE_SIGN_JTAG (PRODSIG) */
    // else if (m_type == 0xC0)   /* MTYPE_FLASH (BOOTROW) */
    // else if (m_type == 0xC4)   /* MTYPE_EEPROM_XMEGA (EEPROM) */
    // else if (m_type == 0xC5)   /* MTYPE_USERSIG (USERROW, BOOTROW) */
    // else if (m_type == 0xD3)   /* MTYPE_SIB */ /* This is in its own memory space. */
    return 0;
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

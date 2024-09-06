/**
 * @file updi.cpp
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
#include "api/capsule.h"    /* _CAPS macro */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * UPDI command payloads use the JTAGICE3 standard.
 * Multibyte fields are in little endian format.
 *
 * The UPDI communications protocol uses the same access layer command set as PDI.
 * The hardware layer also uses the same 12-bit frames UART as PDI/TPI.
 * It is a single-wire bidirectional communication like RS485.
 *
 * The difference is that all commands are preceded by a sync character,
 * allowing auto-synchronization of the baudrate without the need for an XCLK.
 */

namespace UPDI {

  /* This fixed data is stored in SRAM for speed. */

  const static uint8_t nvmprog_key[10] =    /* "NVMProg " */
    {0x55, 0xE0, 0x20, 0x67, 0x6F, 0x72, 0x50, 0x4D, 0x56, 0x4E};
  const static uint8_t erase_key[10] =      /* "NVMErase" */
    {0x55, 0xE0, 0x65, 0x73, 0x61, 0x72, 0x45, 0x4D, 0x56, 0x4E};
  const static uint8_t urowwrite_key[10] =  /* "NVMUs&te" */
    {0x55, 0xE0, 0x65, 0x74, 0x26, 0x73, 0x55, 0x4D, 0x56, 0x4E};

  static uint8_t _set_ptr24[] = {
    0x55, 0x6A, 0, 0, 0, 0  /* ST(0x60) PTR(0x08) ADDR3(0x02) */
  };

  static uint8_t _set_repeat[] = {
    0x55, 0xA0, 0x00, /* repeat */
    0x55, 0x04        /* LD,ST PTR++ DATA1,2 */
  };

  bool send_break (void) {
    D2PRINTF("<BRK>");
    USART0_BAUD = USART0_BAUD + (USART0_BAUD >> 2);
    send(0x00);
    USART0_BAUD = USART::calk_baud_khz(_xclk);
    USART::drain();
    return false;
  }

  void long_break (void) {
    D2PRINTF("<BREAK>");
    USART0_BAUD = USART::calk_baud_khz(_xclk >> 2);
    send(0x00);
    USART0_BAUD = USART::calk_baud_khz(_xclk);
  }

  bool recv (void) {
    loop_until_bit_is_set(USART0_STATUS, USART_RXCIF_bp);
    RXSTAT = USART0_RXDATAH ^ 0x80;
    RXDATA = USART0_RXDATAL;
    return RXSTAT == 0 || send_break();
  }

  bool recv_bytes (uint8_t* _data, size_t _len) {
    do {
      if (!recv()) return false;
      *_data++ = RXDATA;
    } while (--_len);
    return true;
  }

  bool is_ack (void) {
    return recv() && 0x40 == RXDATA;
  }

  bool send (const uint8_t _data) {
    loop_until_bit_is_set(USART0_STATUS, USART_DREIF_bp);
    USART0_TXDATAL = _data;
    return recv() && _data == RXDATA;
  }

  bool send_bytes (const uint8_t* _data, size_t _len) {
    do {
      if (!send(*_data++)) return false;
    } while (--_len);
    return true;
  }

  bool send_bytes_fill (size_t _len) {
    do {
      if (!send(0xFF)) return false;
    } while (--_len);
    return true;
  }

  /* Returns whether there is an error or not. */
  /* The acquired data is stored in RXDATA.    */
  bool recv_byte (uint32_t _dwAddr) {
    static uint8_t _set_ptr[] = {
      0x55, 0x08, 0, 0, 0, 0  /* LDS ADDR3 DATA1 */
    };
    _CAPS32(_set_ptr[2])->dword = _dwAddr;
    return send_bytes(_set_ptr, 5) && recv();
  }

  bool send_byte (uint32_t _dwAddr, uint8_t _data) {
    static uint8_t _set_ptr[] = {
      0x55, 0x48, 0, 0, 0, 0  /* STS ADDR3 DATA1 */
    };
    _CAPS32(_set_ptr[2])->dword = _dwAddr;
    return send_bytes(_set_ptr, 5)
      && is_ack()
      && send(_data)
      && is_ack();
  }

  bool sys_reset (bool _leave = true) {
    const static uint8_t _reset[] = {
      0x55, 0xC8, 0x59, /* SYSRST */
      0x55, 0xC8, 0x00, /* SYSRUN */
      0x55, 0xC3, 0x04  /* UPDIDIS */
    };
    return send_bytes(_reset, _leave ? 9 : 6);
  }

  bool set_rsd (void) {
    const static uint8_t _set_rsd[] = {0x55, 0xC2, 0x0D};
    return send_bytes(_set_rsd, 3);
  }

  bool clear_rsd (void) {
    const static uint8_t _clear_rsd[] = {0x55, 0xC2, 0x05};
    return send_bytes(_clear_rsd, 3);
  }

  bool recv_bytes_block (uint32_t _dwAddr, size_t _wLength) {
    _CAPS32(_set_ptr24[2])->dword = _dwAddr;
    _set_repeat[2] = _wLength - 1;
    _set_repeat[4] = 0x24;  /* LD PTR++ DATA1 */
    return send_bytes(_set_ptr24, 5)
      && is_ack()
      && send_bytes(_set_repeat, sizeof(_set_repeat))
      && recv_bytes(&packet.in.data[0], _wLength);
  }

  bool recv_words_block (uint32_t _dwAddr, size_t _wLength) {
    /* This function works in word units up to 256 words, */
    /* and will round down any fractional words.          */
    _CAPS32(_set_ptr24[2])->dword = _dwAddr;
    _set_repeat[2] = (_wLength >> 1) - 1;
    _set_repeat[4] = 0x25;  /* LD PTR++ DATA2 */
    return send_bytes(_set_ptr24, 5)
      && is_ack()
      && send_bytes(_set_repeat, sizeof(_set_repeat))
      && recv_bytes(&packet.in.data[0], _wLength & ~1);
  }

  bool send_bytes_block (uint32_t _dwAddr, size_t _wLength) {
    _CAPS32(_set_ptr24[2])->dword = _dwAddr;
    _set_repeat[2] = _wLength - 1;
    _set_repeat[4] = 0x64;  /* ST PTR++ DATA1 */
    return send_bytes(_set_ptr24, 5)
      && is_ack()
      && set_rsd()
      && send_bytes(_set_repeat, sizeof(_set_repeat))
      && send_bytes(&packet.out.memData[0], _wLength)
      && clear_rsd();
  }

  bool send_bytes_block_fill (uint32_t _dwAddr, size_t _wLength) {
    _CAPS32(_set_ptr24[2])->dword = _dwAddr;
    _set_repeat[2] = _wLength - 1;
    _set_repeat[4] = 0x64;  /* ST PTR++ DATA1 */
    return send_bytes(_set_ptr24, 5)
      && is_ack()
      && set_rsd()
      && send_bytes(_set_repeat, sizeof(_set_repeat))
      && send_bytes_fill(_wLength)
      && clear_rsd();
  }

  bool send_words_block (uint32_t _dwAddr, size_t _wLength) {
    /* This function works in word units up to 256 words, */
    /* and will round down any fractional words.          */
    _CAPS32(_set_ptr24[2])->dword = _dwAddr;
    _set_repeat[2] = (_wLength >> 1) - 1;
    _set_repeat[4] = 0x65;  /* ST PTR++ DATA2 */
    return send_bytes(_set_ptr24, 5)
      && is_ack()
      && set_rsd()
      && send_bytes(_set_repeat, sizeof(_set_repeat))
      && send_bytes(&packet.out.memData[0], _wLength & ~1)
      && clear_rsd();
  }

  bool send_bytes_data (uint32_t _dwAddr, uint8_t* _data, size_t _wLength) {
    for (size_t _i = 0; _i < _wLength; _i++) {
      if (!send_byte(_dwAddr++, *_data++)) return false;
    }
    return true;
  }

  bool send_bytes_block_slow (uint32_t _dwAddr, size_t _wLength) {
    /* This slow process is due to USERROW and BOOTROW. */
    return send_bytes_data(_dwAddr, &packet.out.memData[0], _wLength);
  }

  bool nvm_ctrl (uint8_t _nvmcmd) {
    return send_byte(0x1000, _nvmcmd);  /* NVMCTRL_CTRLA */
  }

  bool key_status (void) {
    const static uint8_t _key_stat[] = {0x55, 0x87};
    return send_bytes(_key_stat, 2) && recv();
  }

  bool sys_status (void) {
    const static uint8_t _sys_stat[] = {0x55, 0x8B};
    return send_bytes(_sys_stat, 2) && recv();
  }

  bool set_nvmprog_key (bool _reset = true) {
    D1PRINTF(" PROG_KEY\r\n");
    if (!send_bytes(nvmprog_key, sizeof(nvmprog_key))) return false;
    do { key_status(); } while(bit_is_clear(RXDATA, 4));  /* wait set NVMPROG */
    D1PRINTF(" KEY=%02X\r\n", RXDATA);
    return _reset ? sys_reset(false) : true;
  }

  bool set_erase_key (void) {
    if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) set_nvmprog_key(false);
    D1PRINTF(" ERASE_KEY\r\n");
    if (!send_bytes(erase_key, sizeof(erase_key))) return false;
    do { key_status(); } while(bit_is_clear(RXDATA, 3));  /* wait set CHIPERASE */
    D1PRINTF(" KEY=%02X\r\n", RXDATA);
    return sys_reset(false);
  }

  bool set_urowwrite_key (void) {
    D1PRINTF(" UROW_KEY\r\n");
    if (!send_bytes(urowwrite_key, sizeof(urowwrite_key))) return false;
    do { key_status(); } while(bit_is_clear(RXDATA, 5));  /* wait set UROWWRITE */
    D1PRINTF(" KEY=%02X\r\n", RXDATA);
    return sys_reset(false);
  }

  bool chip_erase (void) {
    USART::drain();
    if (!set_erase_key()) return false;
    delay_millis(200);
    USART::drain();
    do { sys_status(); } while(bit_is_set(RXDATA, 5));    /* wait clear RSTSYS */
    do { sys_status(); } while(bit_is_set(RXDATA, 0));    /* wait clear LOCKSTATUS */
    D1PRINTF(" <SYS:%02X>\r\n", RXDATA);
    do { key_status(); } while(bit_is_set(RXDATA, 3));    /* wait clear CHIPERASE */
    sys_status();
    if (bit_is_clear(RXDATA, 3)) {
      if (!set_nvmprog_key()) return false;
      do { sys_status(); } while(bit_is_clear(RXDATA, 3));  /* wait set PROGSTART */
    }
    D1PRINTF(" PROGSTART=%02X\r\n", RXDATA);
    bit_set(PGCONF, PGCONF_ERSE_bp);
    bit_set(PGCONF, PGCONF_PROG_bp);
    return (*Command_Table.prog_init)();
  }

  /*
   * Use the UPDI ACC to write to the USERROW of the locked chip.
   * The write start address should match the USERROW address.
   * The write length should match the USERROW field.
   */
  bool write_userrow (void) {
    const static uint8_t _urowdone[] = { 0x55, 0xCA, 0x03 };  /* ASI_SYS_CTRLA <= UROWDONE|CLKREQ */
    const static uint8_t _urowstop[] = { 0x55, 0xC7, 0x20 };  /* ASI_KEY_STATUS <= UROWWR */
    uint8_t   m_type = packet.out.bMType;
    uint32_t _dwAddr = packet.out.dwAddr;
    size_t  _wLength = packet.out.dwLength;
    if (bit_is_clear(PGCONF, PGCONF_UPDI_bp)
     || m_type != 0xC5
     || _wLength != Device_Descriptor.UPDI.user_sig_bytes
     || (uint16_t)_dwAddr != Device_Descriptor.UPDI.user_sig_base) return false;
    USART::drain();
    D1PRINTF(" ENTER_UROW=%04lX:%04X\r\n", _dwAddr, _wLength);
    if (!set_urowwrite_key()) return false;
    do { sys_status(); } while(bit_is_clear(RXDATA, 2));    /* wait set UROWPROG */
    send_words_block(_dwAddr, _wLength);
    send_bytes(_urowdone, 3);
    do { sys_status(); } while(bit_is_set(RXDATA, 2));      /* wait clear UROWPROG */
    send_bytes(_urowstop, 3);
    if (bit_is_set(PGCONF, PGCONF_PROG_bp)) {
      set_nvmprog_key();
      do { sys_status(); } while(bit_is_clear(RXDATA, 3));  /* wait set PROGSTART */
      D1PRINTF(" RE_PROGSTART=%02X\r\n", RXDATA);
      return true;
    }
    else return sys_reset(false);
  }

  /*
   * Read dummy memory if necessary.
   */
  size_t read_dummy (void) {
    uint8_t  m_type = packet.out.bMType;
    size_t _wLength = packet.out.dwLength;
    if (m_type == 0xB4 && _sib[0]) {
      /* The SIGNATURE of a locked device cannot be read.            */
      /* But, the SIB can be read, so a Dummy SIGNATURE is returned. */
      packet.in.data[0] = 0x1E;
      packet.in.data[1] = _sib[0] == ' ' ? 'A' : _sib[0];
      packet.in.data[2] = _sib[10];
    }
    else memset(&packet.in.data[0], 0xFF, _wLength);
    return _wLength + 1;
  }

  /*
   * For UPDI communication, first set the following:
   * - Keep forced reset for wakeup
   * - Ignore communication collisions
   * - Make guard time short enough
   */
  size_t connect (void) {
    const static uint8_t _init[] = {
      0x55, 0xC8, 0x59, /* SYSRST */
      0x55, 0xC3, 0x08, /* CCDETDIS */
      0x55, 0xC2, 0x05, /* GTVAL[4] */
    };
    const static uint8_t _sib256[] = {
      0x55, 0xE6        /* SIB[256] */
    };
    PGCONF = PGCONF_FAIL_bm;
    _sib[0] = 0;
    _before_page = -1L;
    NVM::V1::setup();   /* default is dummy callback */
    openDrainWriteMacro(PIN_PGM_TRST, LOW);
    nop();

    /* External Reset */
    if (packet.out.bMType) {
      D1PRINTF("<PWRST>\r\n");
      SYS::power_reset();
  #ifdef CONFIG_HVC_ENABLE
      /* High-Voltage control */
  #endif
    }

    openDrainWriteMacro(PIN_PGM_TRST, HIGH);
    USART::drain();
    long_break();
    if (send_bytes(_init, sizeof(_init))) {
      do { sys_status(); } while(bit_is_set(RXDATA, 4));  /* wait clear INSLEEP */
      D1PRINTF("<STAT:%02X>", RXDATA);
      if (send_bytes(_sib256, sizeof(_sib256))
       && recv_bytes(_sib, 32)) {
        size_t _result = 0;
        D1PRINTF(" SIB=%s\r\n", _sib);
        D1PRINTF(" <NVM:%02X>\r\n", _sib[10]);
        /* Depending on the SIB, different low-level methods are executed. */
        if      (_sib[10] == '5') _result = NVM::V5::setup();
        else if (_sib[10] == '4') _result = NVM::V4::setup();
        else if (_sib[10] == '3') _result = NVM::V3::setup();
        else if (_sib[10] == '2') _result = NVM::V2::setup();
        else if (_sib[10] == '0') _result = NVM::V0::setup();
        if (_result) {
          /* If the SIB is obtained, the first 4-characters are returned.       */
          /* If the 1st character is blank, the next 4-characters are returned. */
          memcpy(&packet.in.data[0], _sib[0] == ' ' ? &_sib[4] : &_sib[0], 4);
          bit_set(PGCONF, PGCONF_UPDI_bp);
          return 5;
        }
      }
    }
    return 0;
  }

  size_t disconnect (void) {
    USART::drain();
    bool _result = sys_reset(true);
    PGCONF = 0;
    D1PRINTF(" <RST:%d>\r\n", _result);
    openDrainWriteMacro(PIN_PGM_TRST, LOW);
    nop();
    openDrainWriteMacro(PIN_PGM_TRST, HIGH);
    return _result;
  }

  size_t enter_progmode (void) {
    if (bit_is_set(PGCONF, PGCONF_PROG_bp)) return 1;
    if (!set_nvmprog_key()) return 0;
    uint8_t _count = 0;
    do {
      /* Do not wait for the global timeout to ensure that LOCKSTAT cannot */
      /* be released. Aborting the ACC instruction set here will adversely */
      /* affect subsequent USERROW writes or chip erases. */
      if (0 == ++_count) return 0;
      delay_micros(50);
      sys_status();
    } while (bit_is_clear(RXDATA, 3));  /* wait set PROGSTART */
    D1PRINTF(" PROGSTART=%02X\r\n", RXDATA);
    bit_set(PGCONF, PGCONF_PROG_bp);
    return (*Command_Table.prog_init)();
  }

  /* ARCH=UPDI scope Provides functionality. */
  size_t jtag_scope_updi (void) {
    size_t _rspsize = 0;
    uint8_t _cmd = packet.out.cmd;
    if (_cmd == 0x10) {             /* CMD3_SIGN_ON */
      D1PRINTF(" UPDI_SIGN_ON=EXT:%02X\r\n", packet.out.bMType);
      USART::setup();
      USART::change_updi();
      _rspsize = Timeout::command(&connect);
      /* If it fails here, it is expected to try again, giving it a chance at HV control. */
      packet.in.res = _rspsize ? 0x84 : 0xA0; /* RSP3_DATA : RSP3_FAILED */
      return _rspsize;
    }
    else if (_cmd == 0x11) {        /* CMD3_SIGN_OFF */
      D1PRINTF(" UPDI_SIGN_OFF\r\n");
      /* If UPDI control has failed, RSP3_OK is always returned. */
      _rspsize = bit_is_set(PGCONF, PGCONF_UPDI_bp) ? Timeout::command(&disconnect) : 1;
      USART::setup();
      USART::change_vcp();
    }
    else if (_cmd == 0x15) {        /* CMD3_ENTER_PROGMODE */
      D1PRINTF(" UPDI_ENTER_PROG\r\n");
      /* On failure, RSP3_OK is returned if a UPDI connection is available. */
      /* Locked devices are given the opportunity to write to USERROW and erase the chip. */
      _rspsize = Timeout::command(&enter_progmode) || bit_is_set(PGCONF, PGCONF_UPDI_bp);
    }
    else if (_cmd == 0x16) {        /* CMD3_LEAVE_PROGMODE */
      D1PRINTF(" UPDI_LEAVE_PROG\r\n");
      /* There is nothing to do. */
      /* The actual termination process is delayed until CMD3_SIGN_OFF. */
      _rspsize = 1;
    }
    else if (_cmd == 0x20) {        /* CMD3_ERASE_MEMORY */
      D1PRINTF(" UPDI_ERASE=%02X:%06lX\r\n",
        packet.out.bEType, packet.out.dwPageAddr);
      _rspsize = Timeout::command(Command_Table.erase_memory);
    }
    else if (bit_is_clear(PGCONF, PGCONF_UPDI_bp)) { /* empty */ }
    else if (_cmd == 0x21) {        /* CMD3_READ_MEMORY */
      D1PRINTF(" UPDI_READ=%02X:%06lX:%04X\r\n", packet.out.bMType,
        packet.out.dwAddr, (size_t)packet.out.dwLength);
      uint8_t m_type = packet.out.bMType;
      size_t _wLength = packet.out.dwLength;
      if (m_type == 0xD3) {         /* MTYPE_SIB */
        /* The SIB request occurs before ENTER_PROGMODE. */
        memcpy(&packet.in.data[(uint8_t)packet.out.dwAddr & 31], &_sib, ((_wLength - 1) & 31) + 1);
        _rspsize = _wLength + 1;
      }
      else if (bit_is_set(PGCONF, PGCONF_PROG_bp)) {
        _rspsize = Timeout::command(Command_Table.read_memory);
      }
      /* If not in PROGMODE, respond with a dummy. */
      /* A dummy SIG will be returned for locked devices. */
      /* This will prevent AVRDUDE from displaying annoying errors. */
      else _rspsize = read_dummy();
      packet.in.res = _rspsize ? 0x184 : 0xA0;
      return _rspsize;
    }
    else if (_cmd == 0x23) {        /* CMD3_WRITE_MEMORY */
      D1PRINTF(" UPDI_WRITE=%02X:%06lX:%04X\r\n", packet.out.bMType,
        packet.out.dwAddr, (size_t)packet.out.dwLength);
      _rspsize = Timeout::command(Command_Table.write_memory);
    }
    packet.in.res = _rspsize ? 0x80 : 0xA0;   /* RSP3_OK : RSP3_FAILED */
    return _rspsize;
  }

};

// end of code

/**
 * @file updi.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.33.46+
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
  #define UPDI_GETVAL 0x06
  #define UPDI_CTRLAV (0x10 + UPDI_GETVAL)

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

  // MARK: UPDI Low level

  bool send_break (void) {
    D2PRINTF("<BRK>");
    DFLUSH();
    USART0_BAUD = USART0_BAUD + (USART0_BAUD << 1);
    send(0x00);
    USART0_BAUD = USART::calk_baud_khz(_xclk);
    return false;
  }

  void long_break (void) {
    D2PRINTF("<BREAK>");
    DFLUSH();
    USART0_BAUD = USART::calk_baud_khz(_xclk >> 2);
    send(0x00);
    USART0_BAUD = USART::calk_baud_khz(_xclk);
  }

  bool recv (void) {
    loop_until_bit_is_set(USART0_STATUS, USART_RXCIF_bp);
    RXSTAT = USART0_RXDATAH ^ 0x80;
    RXDATA = USART0_RXDATAL;
    return RXSTAT == 0;
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
    D1PRINTF("<RST>");
    DFLUSH();
    return send_bytes(_reset, _leave ? 9 : 6);
  }

  bool set_rsd (void) {
    const static uint8_t _set_rsd[] = {0x55, 0xC2, 0x08 + UPDI_CTRLAV};
    return send_bytes(_set_rsd, 3);
  }

  bool clear_rsd (void) {
    const static uint8_t _clear_rsd[] = {0x55, 0xC2, UPDI_CTRLAV};
    return send_bytes(_clear_rsd, 3);
  }

  // MARK: UPDI API

  bool recv_bytes_block (uint32_t _dwAddr, size_t _wLength) {
    if (_wLength == 1) {
      if (recv_byte(_dwAddr)) {
        packet.in.data[0] = RXDATA;
        return true;
      }
      return false;
    }
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
    if (_wLength == 1) return send_byte(_dwAddr, packet.out.memData[0]);
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

  bool key_wait_set (uint8_t _bit) {
    do {
      SYS::delay_100us();
      key_status();
    } while (bit_is_clear(RXDATA, _bit));
    return true;
  }

  bool key_wait_clear (uint8_t _bit) {
    do {
      SYS::delay_100us();
      key_status();
    } while (bit_is_set(RXDATA, _bit));
    return true;
  }

  bool sys_wait_set (uint8_t _bit) {
    do {
      SYS::delay_100us();
      sys_status();
    } while (bit_is_clear(RXDATA, _bit));
    return true;
  }

  bool sys_wait_clear (uint8_t _bit) {
    do {
      SYS::delay_100us();
      sys_status();
    } while (bit_is_set(RXDATA, _bit));
    return true;
  }

  bool set_nvmprog_key (bool _reset = true) {
    D1PRINTF(" PROG_KEY=");   /* wait set NVMPROG */
    if (!send_bytes(nvmprog_key, sizeof(nvmprog_key)) || !key_wait_set(4)) return false;
    return _reset ? sys_reset(false) : true;
  }

  bool set_erase_key (void) {
    if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) set_nvmprog_key(false);
    D1PRINTF(" ERASE_KEY=");  /* wait set CHIPERASE */
    if (!send_bytes(erase_key, sizeof(erase_key)) || !key_wait_set(3)) return false;
    return sys_reset(false);
  }

  bool set_urowwrite_key (void) {
    D1PRINTF(" UROW_KEY=");   /* wait set UROWWRITE */
    if (!send_bytes(urowwrite_key, sizeof(urowwrite_key)) || !key_wait_set(5)) return false;
    return sys_reset(false);
  }

  bool chip_erase (void) {
    D1PRINTF(" CHIP_ERASE");
    USART::drain();
    if (!set_erase_key()) return false;
    D1PRINTF(" WAIT");
    SYS::delay_125ms();
    SYS::delay_125ms();
    USART::drain();
    sys_wait_clear(5);      /* wait clear RSTSYS */
    sys_wait_clear(0);      /* wait clear LOCKSTATUS */
    D1PRINTF(" <SYS:%02X>\r\n", RXDATA);
    key_wait_clear(3);      /* wait clear CHIPERASE */
    if (sys_status() && bit_is_clear(RXDATA, 3)) {
      if (!set_nvmprog_key(true)) return false;
      sys_wait_set(3);      /* wait set PROGSTART */
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
    DFLUSH();
    if (!set_urowwrite_key()) return false;
    sys_wait_set(2);      /* wait set UROWPROG */
    D1PRINTF("<SYS:%02X:%d>\r\n", RXDATA, bit_is_clear(RXDATA, 2));
    DFLUSH();
    send_words_block(_dwAddr, _wLength);
    SYS::delay_100us();
    send_bytes(_urowdone, 3);
    sys_wait_clear(2);    /* wait clear UROWPROG */
    D1PRINTF("<SYS:%02X:%d>\r\n", RXDATA, bit_is_clear(RXDATA, 2));
    DFLUSH();
    send_bytes(_urowstop, 3);
    if (bit_is_set(PGCONF, PGCONF_PROG_bp) && set_nvmprog_key()) {
      sys_wait_set(3);    /* wait set PROGSTART */
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

  // MARK: UPDI Session

  size_t timeout_fallback (void) {
    /* If a timeout occurs, the communication speed will be reduced. */
    if (_xclk == 40) return 0;
    _xclk -= 25;
    if (_xclk < 40) _xclk = 40;
    USART::change_updi();
    send_break();
    return clear_rsd();
  }

  /*
   * For UPDI communication, first set the following:
   * - Keep forced reset for wakeup
   * - Ignore communication collisions
   * - Make guard time short enough
   */
  size_t updi_activate (void) {
    const static uint8_t _init[] = {
      0x55, 0xC8, 0x59, /* SYSRST */
      0x55, 0xC3, 0x08, /* CCDETDIS */
      0x55, 0xC2, UPDI_CTRLAV,
      0x55, 0x89        /* ASI_CTRLA */
    };

    /* When operating at 40kHz or above, a BREAK character length of 2.5ms is sufficient. */
    openDrainWriteMacro(PIN_PGM_TDAT, LOW);
    delay_micros(2500);
    openDrainWriteMacro(PIN_PGM_TDAT, HIGH);

    nop();
    while (!digitalReadMacro(PIN_PGM_TDAT));

    USART::change_updi();
    SYS::delay_100us();

    /* It is considered a failure if the ASI_CTRA register does not return 0x03. */
    return ((send_bytes(_init, sizeof(_init)) && recv() && 0x03 == RXDATA));
  }

  size_t read_sib (void) {
    const static uint8_t _sib256[] = { 0x55, 0xE6 };
    if (send_bytes(_sib256, sizeof(_sib256)) && recv_bytes(_sib, 32)) {
      size_t _result = 0;
      D1PRINTF(" SIB=\"%s\"\r\n", _sib);
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
    return 0;
  }

  size_t connect (void) {
    PGCONF = 0;
    _sib[0] = 0;
    _before_page = -1L;
    NVM::V1::setup();   /* default is dummy callback */
    USART::setup();

    openDrainWriteMacro(PIN_PGM_TRST, LOW);
    SYS::power_reset();
    openDrainWriteMacro(PIN_PGM_TRST, HIGH);

    /* External Reset */
    if (_packet_length >= 7 && packet.out.bMType) {
  #ifdef CONFIG_HVC_ENABLE
      /* High-Voltage control */
      uint8_t _v = Device_Descriptor.UPDI.hvupdi_variant;
      if (_jtag_hvctrl && _v != 1) {
        SYS::hvc_enable();
        if (_v == 0) {
          D1PRINTF("<HVC:V0>");
          digitalWriteMacro(PIN_HVC_SELECT1, HIGH);
        }
        else if (_v == 2) {
          D1PRINTF("<HVC:V2+>");
          digitalWriteMacro(PIN_HVC_SELECT3, HIGH);
        }
        /* Most early silicon requires a pulse of 700us or more. */
        SYS::delay_800us();
        SYS::hvc_leave();
        digitalWriteMacro(PIN_HVC_SELECT1, LOW);
        digitalWriteMacro(PIN_HVC_SELECT3, LOW);
        bit_set(PGCONF, PGCONF_HVEN_bp);
        /* From this point onwards, UPDI activation must be successful within 64ms. */
      }
  #endif
    }

    /* Fallback Retry */
    /* It must complete within a maximum of 64 milliseconds, */
    /* so a maximum of three attempts will be made. */
    uint16_t _bak = _xclk;
    for (uint8_t _r = 0; _r < 4; _r++) {
      wdt_reset();
      if (Timeout::command(&updi_activate, nullptr, 20)) {
        size_t _result = Timeout::command(&read_sib, nullptr, 20);
        if (_result) return _result;
      }
      /* If UPDI activation fails, the communication speed will be reduced. */
      _xclk -= 25;
      if (_xclk < 40) _xclk = 40;
      USART::setup();
    }

    /* If HV mode is enabled, the specified speed will be restored on error exit. */
    if (_jtag_hvctrl) _xclk = _bak;
    return 0;
  }

  inline size_t disconnect (void) {
    return sys_reset(true);
  }

  size_t enter_progmode (void) {
    if (bit_is_set(PGCONF, PGCONF_PROG_bp)) return 1;

    /* Enter NVMPROGKEY. */
    if (!set_nvmprog_key()) return 0;

    /* Verify that the NVMPROG bit is cleared. */
    /* A locked device does not have LOCKSTATUS cleared. */
    if (key_wait_clear(4) && sys_status() && bit_is_clear(RXDATA, 0)) {
      bit_set(PGCONF, PGCONF_PROG_bp);
      (*Command_Table.prog_init)();
    }
    D1PRINTF("<ST:%02X>\r\n", RXDATA);

    /* In either case it returns success. */
    return 1;
  }

  // MARK: JTAG SCOPE

  /* ARCH=UPDI scope Provides functionality. */
  size_t jtag_scope_updi (void) {
    size_t _rspsize = 0;
    uint8_t _cmd = packet.out.cmd;
    if (_cmd == 0x10) {             /* CMD3_SIGN_ON */
      D1PRINTF(" UPDI_SIGN_ON=EXT:%02X\r\n", packet.out.bMType);
      _rspsize = connect();
      /* If it fails here, it is expected to try again, giving it a chance at HV control. */
      packet.in.res = _rspsize ? 0x84 : 0xA0; /* RSP3_DATA : RSP3_FAILED */
      return _rspsize;
    }
    else if (_cmd == 0x11) {        /* CMD3_SIGN_OFF */
      D1PRINTF(" UPDI_SIGN_OFF\r\n");
      /* If UPDI control has failed, RSP3_OK is always returned. */
      _rspsize = bit_is_set(PGCONF, PGCONF_UPDI_bp) ? Timeout::command(&disconnect) : 1;
      SYS::delay_100us();
      USART::setup();
      openDrainWriteMacro(PIN_PGM_TRST, LOW);
      SYS::power_reset();
      openDrainWriteMacro(PIN_PGM_TRST, HIGH);
      PGCONF = 0;
      USART::change_vcp();
    }
    else if (_cmd == 0x15) {        /* CMD3_ENTER_PROGMODE */
      D1PRINTF(" UPDI_ENTER_PROG\r\n");
      /* On failure, RSP3_OK is returned if a UPDI connection is available. */
      /* Locked devices are given the opportunity to write to USERROW and erase the chip. */
      _rspsize = Timeout::command(&enter_progmode, &timeout_fallback) || bit_is_set(PGCONF, PGCONF_UPDI_bp);
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
      _rspsize = Timeout::command(Command_Table.erase_memory, &timeout_fallback, 800);
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
        _rspsize = Timeout::command(Command_Table.read_memory, &timeout_fallback);
      }
      /* If not in PROGMODE, respond with a dummy. */
      /* A dummy SIG will be returned for locked devices. */
      /* This will prevent AVRDUDE from displaying annoying errors. */
      else _rspsize = read_dummy();
      packet.in.res = _rspsize ? 0x184 : 0xA0;  /* RSP3_DATA : RSP3_FAILED */
      return _rspsize;
    }
    else if (_cmd == 0x23) {        /* CMD3_WRITE_MEMORY */
      D1PRINTF(" UPDI_WRITE=%02X:%06lX:%04X\r\n", packet.out.bMType,
        packet.out.dwAddr, (size_t)packet.out.dwLength);
      _rspsize = Timeout::command(Command_Table.write_memory, &timeout_fallback);
    }
    packet.in.res = _rspsize ? 0x80 : 0xA0;     /* RSP3_OK : RSP3_FAILED */
    return _rspsize;
  }

};

// end of code

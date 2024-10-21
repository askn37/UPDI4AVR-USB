/**
 * @file pdi.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.33.46+
 * @date 2024-10-08
 * @copyright Copyright (c) 2024 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include "api/macro_api.h"  /* ATOMIC_BLOCK */
#include "api/capsule.h"    /* _CAPS macro */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * PDI command payloads use the JTAGICE3 standard.
 * 
 * This mode is only used by the ATxmega series and is for 3.3V operation only.
 * There is no HV control mode.
 *
 * The PDI communications protocol uses the same access layer command set as UPDI.
 * The hardware layer also uses the same 12-bit frames USART as UPDI/TPI.
 * It is a single-wire bidirectional communication like RS485,
 * synchronized by the XCLK just like TPI.
 *
 * ACC handles 32-bit addresses:
 *   If the high word is in the 0x0100 area, it points to DATA space.
 *   If the high word is in the 0x0080 area, it points to NVM space.
 * 
 * PDI_DATA is a special signal line. The device pulls it down with 22kohm
 * by default. To activate PDI, from the device's point of view, TxD mode must
 * be Push-Pull and RxD mode must be Hi-Z. Therefore, it is recommended to add
 * an external translator buffer with direction control.
 */

#define PCLK_IN portRegister(PIN_PGM_PCLK).IN
#define PCLK_bp pinPosition(PIN_PGM_PCLK)
#define PDI_GVAL 0x05

#define pinLogicPush(PIN) openDrainWriteMacro(PIN, LOW)
#define pinLogicOpen(PIN) openDrainWriteMacro(PIN, HIGH)

namespace PDI {

  /* This fixed data is stored in SRAM for speed. */

  /* The NVM key for PDI is the same as that for TPI. */
  const static uint8_t nvmprog_key[] = {
    0xE0, 0xFF, 0x88, 0xD8, 0xCD, 0x45, 0xAB, 0x89, 0x12
  };

  static uint8_t _set_ptr32[] = {
    0x6B, 0, 0, 0, 0  /* ST(0x60) PTR(0x08) ADDR4(0x03) */
  };

  /* The repeat instruction allows a 16-bit count mode, */
  /* but this has been removed in UPDI. */
  static uint8_t _set_repeat[] = {
    0xA1, 0x00, 0x00, /* repeat2 */
    0x00              /* LD/ST */
  };

  // MARK: PDI Low level

  bool idle_clock (const size_t clock) {
    for (size_t i = 0; i < clock; i++) {
      loop_until_bit_is_set(PCLK_IN, PCLK_bp);
      loop_until_bit_is_clear(PCLK_IN, PCLK_bp);
    }
    return true;
  }

  /* To run push-pull TxD in single-wire mode with loopback, */
  /* controlling the USART becomes a bit more complicated.   */
  bool start_txd (void) {
    if (bit_is_clear(PGCONF, PGCONF_XDIR_bp)) {
      idle_clock(1);
      digitalWriteMacro(PIN_PGM_PDAT, HIGH);
      pinLogicPush(PIN_PGM_PDAT);
  #ifdef PIN_PGM_XDIR
      digitalWriteMacro(PIN_PGM_XDIR, HIGH);
  #endif
      USART0_CTRLB = USART_RXEN_bm | USART_TXEN_bm;
      bit_set(PGCONF, PGCONF_XDIR_bp);
    }
    return true;
  }

  bool stop_txd (void) {
    if (bit_is_set(PGCONF, PGCONF_XDIR_bp)) {
      idle_clock(1);
      pinLogicOpen(PIN_PGM_PDAT);
  #ifdef PIN_PGM_XDIR
      digitalWriteMacro(PIN_PGM_XDIR, LOW);
  #endif
      USART0_CTRLB = USART_RXEN_bm | USART_ODME_bm;
      bit_clear(PGCONF, PGCONF_XDIR_bp);
    }
    return true;
  }

  bool send_break (void) {
    stop_txd();
    digitalWriteMacro(PIN_PGM_PDAT, LOW);
    pinLogicPush(PIN_PGM_PDAT);
    idle_clock(16);
    digitalWriteMacro(PIN_PGM_PDAT, HIGH);
    idle_clock(2);
    digitalWriteMacro(PIN_PGM_PDAT, LOW);
    idle_clock(16);
    return start_txd();
  }

  bool recv (void) {
    do { RXSTAT = USART0_RXDATAH; } while (!RXSTAT);
    RXDATA = USART0_RXDATAL;
    RXSTAT ^= 0x80;
    // D2PRINTF("(%02X:%02X)", RXSTAT, RXDATA);
    return RXSTAT == 0;
  }

  bool recv_bytes (uint8_t* _data, size_t _len) {
    stop_txd();
    do {
      if (!recv()) return false;
      *_data++ = RXDATA;
    } while (--_len);
    return true;
  }

  bool send (const uint8_t _data) {
    loop_until_bit_is_set(USART0_STATUS, USART_DREIF_bp);
    // D2PRINTF("[%02X]", _data);
    USART0_TXDATAL = _data;
    return recv() && _data == RXDATA;
  }

  bool send_bytes (const uint8_t* _data, size_t _len) {
    start_txd();
    // D2PRINTF("\r\n");
    do {
      if (!send(*_data++)) return false;
    } while (--_len);
    return true;
  }

  bool recv_byte (uint32_t _dwAddr) {
    static uint8_t _set_ptr[] = {
      0x0C, 0, 0, 0, 0    /* LDS ADDR4 DATA1 */
    };
    _CAPS32(_set_ptr[1])->dword = _dwAddr;
    return send_bytes(_set_ptr, sizeof(_set_ptr)) && stop_txd() && recv();
  }

  bool send_byte (uint32_t _dwAddr, uint8_t _data) {
    static uint8_t _set_ptr[] = {
      0x4C, 0, 0, 0, 0    /* STS ADDR4 DATA1 */
    };
    _CAPS32(_set_ptr[1])->dword = _dwAddr;
    return send_bytes(_set_ptr, sizeof(_set_ptr)) && send(_data);
  }

  bool pdibus_wait (void) {
    do {
      /* ACC STATUS check */
      idle_clock(2);
      if (start_txd() && send(0x80)) {
        if (stop_txd() && recv() && bit_is_set(RXDATA, 1)) return true;
      }
      else break;
    } while (true);
    return false;
  }

  // MARK: PDI NVM API

  uint32_t get_mtype_offset (uint8_t m_type) {
    if      (m_type == 0x22) return Device_Descriptor.Xmega.nvm_eeprom_offset;    /* MTYPE_EEPROM (EEPROM) */
    else if (m_type == 0xB0) return Device_Descriptor.Xmega.nvm_boot_offset;      /* MTYPE_FLASH_PAGE (BOOTCODE) */
    else if (m_type == 0xB1) return Device_Descriptor.Xmega.nvm_eeprom_offset;    /* MTYPE_EEPROM_PAGE (EEPROM) */
    else if (m_type == 0xB2) return Device_Descriptor.Xmega.nvm_fuse_offset;      /* MTYPE_FUSE_BITS (FUSE) */
    else if (m_type == 0xB3) return Device_Descriptor.Xmega.nvm_lock_offset;      /* MTYPE_LOCK_BITS (LOCK) */
    else if (m_type == 0xB4) return Device_Descriptor.Xmega.nvm_data_offset
                                  + Device_Descriptor.Xmega.mcu_base_addr;        /* MTYPE_SIGN_JTAG */
    else if (m_type == 0xC0) return Device_Descriptor.Xmega.nvm_app_offset;       /* MTYPE_FLASH (APPCODE) */
    else if (m_type == 0xC1) return Device_Descriptor.Xmega.nvm_boot_offset;      /* MTYPE_BOOT_FLASH (BOOTCODE) */
    else if (m_type == 0xC4) return Device_Descriptor.Xmega.nvm_eeprom_offset;    /* MTYPE_EEPROM_XMEGA (EEPROM) */
    else if (m_type == 0xC5) return Device_Descriptor.Xmega.nvm_user_sig_offset;  /* MTYPE_USERSIG (USERROW) */
    else if (m_type == 0xC6) return Device_Descriptor.Xmega.nvm_prod_sig_offset;  /* MTYPE_PRODSIG (PRODSIG) */
    else                     return Device_Descriptor.Xmega.nvm_data_offset;
  }

  bool nvm_cmd (uint8_t _nvmcmd) {
    return send_byte(0x010001CAUL, _nvmcmd);  /* 0x01CA: NVMCTRL_CMD */
  }

  bool nvm_cmdex (void) {
    return send_byte(0x010001CBUL, 1);        /* 0x01CA: NVMCTRL_CTRLA */
  }

  bool nvm_wait (void) {
    do { recv_byte(0x010001CFUL); } while (RXDATA & 0xC0); /* 0x01CF: NVMCTRL_STATUS */
    return true;
  }

  size_t erase_memory (void) {
    uint8_t   e_type = packet.out.bEType;
    uint32_t _dwAddr = packet.out.dwAddr;
    _CAPS32(_set_ptr32[1])->dword = _dwAddr;
    nvm_wait();
    if (e_type == 0x00) {
      /* XMEGA_ERASE_CHIP */
      if (!(nvm_cmd(0x40) && nvm_cmdex() && pdibus_wait() && nvm_wait())) return 0;
    }
    /* Section erasure is only used with USERSIG. */
    /* Unlike READ/WRITE, the address is given as an absolute value. */
    else if (e_type == 0x07) {
      /* XMEGA_ERASE_USERSIG */
      if (!(nvm_cmd(0x18)
         && send_bytes(_set_ptr32, sizeof(_set_ptr32))
         && send(0x64)
         && send(0xFF)
         && nvm_wait())) return 0;
    }
    /* Any other sections specified will be ignored, */
    /* but the command will return successful completion. */
    return 1;
  }

  size_t read_memory (void) {
    uint8_t   m_type = packet.out.bMType;
    uint32_t _dwAddr = packet.out.dwAddr + get_mtype_offset(m_type);
    size_t  _wLength = packet.out.dwLength;
    size_t  _rspsize = 0;
    D1PRINTF(" L=%04X,A=%08lX,", _wLength, _dwAddr);
    if (!nvm_cmd(0x43)) return 0;
    if (_wLength == 1) {
      if (recv_byte(_dwAddr)) {
        packet.in.data[0] = RXDATA;
        _rspsize = 1 + _wLength;
      }
    }
    else {
      _CAPS32(_set_ptr32[1])->dword = _dwAddr;
      _CAPS16(_set_repeat[1])->word = _wLength - 1;
      _set_repeat[3] = 0x24;  /* LD PTR++ DATA1 */
      if (send_bytes(_set_ptr32, sizeof(_set_ptr32))
       && send_bytes(_set_repeat, sizeof(_set_repeat))
       && recv_bytes(&packet.in.data[0], _wLength)) {
        _rspsize = 1 + _wLength;
        D2PRINTHEX(&packet.in.data[0], _wLength);
      }
    }
    return _rspsize;
  }

  size_t write_memory (void) {
    uint8_t _nvm_cmd = 0, _nvm_pclr = 0, _nvm_pset = 0;
    uint8_t   m_type = packet.out.bMType;
    uint32_t _dwAddr = packet.out.dwAddr + get_mtype_offset(m_type);
    size_t  _wLength = packet.out.dwLength;
    _CAPS32(_set_ptr32[1])->dword = _dwAddr;
    _CAPS16(_set_repeat[1])->word = _wLength - 1;
    _set_repeat[3] = 0x64;  /* ST PTR++ DATA1 */
    D1PRINTF(" L=%04X,A=%08lX,", _wLength, _dwAddr);
    nvm_wait();
    if (m_type == 0xC0) {                                           /* APPCODE or FLASH */
      _nvm_cmd = 0x25; _nvm_pclr = 0x26; _nvm_pset = 0x23;
    }
    else if (m_type == 0xB0 || m_type == 0xC1) {                    /* BOOTCODE */
      _nvm_cmd = 0x2D; _nvm_pclr = 0x26; _nvm_pset = 0x23;
    }
    else if (m_type == 0x22 || m_type == 0xB1 || m_type == 0xC4) {  /* EEPROM */
      _nvm_cmd = 0x35; _nvm_pclr = 0x36; _nvm_pset = 0x33;
    }
    else if (m_type == 0xC5) {                                      /* USERSIG */
      _nvm_cmd = 0x1A; _nvm_pclr = 0x26; _nvm_pset = 0x23;
    }
    else {
      if      (m_type == 0xB2) _nvm_cmd = 0x4C;   /* FUSE */
      else if (m_type == 0xB3) _nvm_cmd = 0x08;   /* LOCK */
      else return 0;
      return (nvm_cmd(_nvm_cmd)
           && send_bytes(_set_ptr32, sizeof(_set_ptr32))
           && send_bytes(_set_repeat, sizeof(_set_repeat))
           && send_bytes(&packet.out.memData[0], _wLength)
           && nvm_wait());
    }

    /* FLASH, BOOTCODE, APPCODE or EEPROM */
    return (nvm_cmd(_nvm_pclr)
     && nvm_cmdex()
     && nvm_wait()
     && nvm_cmd(_nvm_pset)
     && send_bytes(_set_ptr32, sizeof(_set_ptr32))
     && send_bytes(_set_repeat, sizeof(_set_repeat))
     && send_bytes(&packet.out.memData[0], _wLength)
     && nvm_cmd(_nvm_cmd)
     && send_bytes(_set_ptr32, sizeof(_set_ptr32))
     && send(0x64)  /* ST */
     && send(0xFF)  /* dummy byte */
     && nvm_wait());
  }

  // MARK: PDI Session

  size_t timeout_fallback (void) {
    /* If a timeout occurs, the communication speed will be reduced. */
    if (_xclk == 50) return 0;
    _xclk -= 50;
    if (_xclk < 50) _xclk = 50;
    USART::change_pdi();
    return send_break();
  }

  size_t connect (void) {
    const static uint8_t _init[] = {
      0xC2, PDI_GVAL,   /* Set GUADTIME in ASI_CTRL */
      0xC1, 0x59,       /* SYSRST */
      0x82              /* Read back ASI_CTRL */
    };

    USART::setup();

    digitalWriteMacro(PIN_PGM_PDAT, LOW);
    pinLogicPush(PIN_PGM_PDAT);
    pinLogicPush(PIN_PGM_PCLK);
    SYS::power_reset();
    digitalWriteMacro(PIN_PGM_PDAT, HIGH);
    SYS::delay_55us();

    /* Send at least a certain number of IDLE bits to PCLK within 100us after PoR. */
    USART::change_pdi();
    idle_clock(16);

    if (send_bytes(_init, sizeof(_init)) && stop_txd() && recv() && RXDATA == PDI_GVAL) {
      start_txd();
      D1PRINTF(" PDION;GVAL=%02X\r\n", RXDATA);
      bit_set(PGCONF, PGCONF_UPDI_bp);
      return 1;
    }

    return 0;
  }

  size_t disconnect (void) {
    const static uint8_t _leave[] = {
      0xC0, 0x02,       /* NVM disable */
      0xC1, 0x00        /* leave SYSRST */
    };
    if (bit_is_set(PGCONF, PGCONF_UPDI_bp)) return 1;
    return send_bytes(_leave, sizeof(_leave));
  }

  size_t enter_progmode (void) {
    if (bit_is_set(PGCONF, PGCONF_PROG_bp)) return 1;

    /* Enter NVMPROGKEY */
    if (send_bytes(nvmprog_key, sizeof(nvmprog_key)) && pdibus_wait()) {
      bit_set(PGCONF, PGCONF_PROG_bp);
      D1PRINTF("<ST:%02X>\r\n", RXDATA);
      return 1;
    }

    return 0;
  }

  size_t jtag_scope_xmega (void) {
    size_t _rspsize = 0;
    uint8_t _cmd = packet.out.cmd;
    if (_cmd == 0x10) {             /* CMD3_SIGN_ON */
      D1PRINTF(" PDI_SIGN_ON=EXT:%02X\r\n", packet.out.bMType);
      _rspsize = Timeout::command(&connect);
      packet.in.res = _rspsize ? 0x84 : 0xA0; /* RSP3_DATA : RSP3_FAILED */
      return _rspsize;
    }
    else if (_cmd == 0x11) {        /* CMD3_SIGN_OFF */
      D1PRINTF(" PDI_SIGN_OFF\r\n");
      /* If UPDI control has failed, RSP3_OK is always returned. */
      _rspsize = Timeout::command(&disconnect);
      SYS::delay_100us();
      USART::setup();
      pinLogicOpen(PIN_PGM_PDAT);
      pinControlRegister(PIN_PGM_PCLK) = 0;
      digitalWriteMacro(PIN_PGM_PCLK, LOW);
      pinLogicPush(PIN_PGM_PCLK);
      if (bit_is_set(PGCONF, PGCONF_PROG_bp)) {
        SYS::power_reset();
        SYS::delay_2500us();
      }
      pinLogicOpen(PIN_PGM_PCLK);
      PGCONF = 0;
      USART::change_vcp();
    }
    else if (_cmd == 0x15) {        /* CMD3_ENTER_PROGMODE */
      D1PRINTF(" PDI_ENTER_PROG\r\n");
      /* On failure, RSP3_OK is returned if a PDI connection is available. */
      _rspsize = Timeout::command(&enter_progmode, &timeout_fallback) || bit_is_set(PGCONF, PGCONF_UPDI_bp);
    }
    else if (_cmd == 0x16) {        /* CMD3_LEAVE_PROGMODE */
      D1PRINTF(" PDI_LEAVE_PROG\r\n");
      /* There is nothing to do. */
      /* The actual termination process is delayed until CMD3_SIGN_OFF. */
      _rspsize = 1;
    }
    else if (bit_is_clear(PGCONF, PGCONF_PROG_bp)) { /* empty */ }
    else if (_cmd == 0x20) {        /* CMD3_ERASE_MEMORY */
      D1PRINTF(" PDI_ERASE=%02X:%08lX\r\n",
        packet.out.bEType, packet.out.dwPageAddr);
      _rspsize = Timeout::command(&erase_memory, &timeout_fallback);
    }
    else if (_cmd == 0x21) {        /* CMD3_READ_MEMORY */
      D1PRINTF(" PDI_READ=%02X:%08lX:%04X\r\n", packet.out.bMType,
        packet.out.dwAddr, (size_t)packet.out.dwLength);
      _rspsize = Timeout::command(&read_memory, &timeout_fallback);
      packet.in.res = _rspsize ? 0x184 : 0xA0;  /* RSP3_DATA : RSP3_FAILED */
      return _rspsize;
    }
    else if (_cmd == 0x23) {        /* CMD3_WRITE_MEMORY */
      D1PRINTF(" PDI_WRITE=%02X:%08lX:%04X\r\n", packet.out.bMType,
        packet.out.dwAddr, (size_t)packet.out.dwLength);
      _rspsize = Timeout::command(&write_memory, &timeout_fallback);
    }
    packet.in.res = _rspsize ? 0x80 : 0xA0;     /* RSP3_OK : RSP3_FAILED */
    return _rspsize;
  }

};

// end of code

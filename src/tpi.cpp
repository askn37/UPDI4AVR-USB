/**
 * @file tpi.cpp
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
#include "api/macro_api.h"  /* ATOMIC_BLOCK */
#include "api/capsule.h"    /* _CAPS macro */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * The TPI command payload uses the STK600's XPRG standard,
 * with multi-byte fields in big endian format.
 *
 * The TPI communications protocol is minimally proprietary,
 * but at the hardware layer it uses the same 12-bit frames USART as PDI/UPDI.
 * It is a single-wire bidirectional communication like RS485,
 * and like PDI it is synchronized by an XCLK.
 */

#define TCLK_IN portRegister(PIN_VCP_TXD).IN
#define TCLK_bp pinPosition(PIN_VCP_TXD)

namespace TPI {
  const static uint8_t nvmprog_key[] = {
    0xE0, 0xFF, 0x88, 0xD8, 0xCD, 0x45, 0xAB, 0x89, 0x12
  };

  void idle_clock (const size_t clock) {
    for (size_t i = 0; i < clock; i++) {
      loop_until_bit_is_set(TCLK_IN, TCLK_bp);
      loop_until_bit_is_clear(TCLK_IN, TCLK_bp);
    }
  }

  bool recv (void) {
    loop_until_bit_is_set(USART0_STATUS, USART_RXCIF_bp);
    RXSTAT = USART0_RXDATAH ^ 0x80;
    RXDATA = USART0_RXDATAL;
    return RXSTAT == 0;
  }

  bool send (const uint8_t _data) {
    loop_until_bit_is_set(USART0_STATUS, USART_DREIF_bp);
    USART0_TXDATAL = _data;
    return (recv() && _data == RXDATA);
  }

  /*** TPI control and CSS area command ***/

  bool get_sldcs (const uint8_t _addr) {
    return (send(0x80 | _addr) && recv());
  }

  bool set_sstcs (const uint8_t _addr, const uint8_t _data) {
    return (send(0xC0 | _addr) && send(_data));
  }

  bool set_sout (const uint8_t _addr, const uint8_t _data) {
    return (send(0x90 | _addr) && send(_data));
  }

  bool get_sin (const uint8_t _addr) {
    return (send(0x10 | _addr) && recv());
  }

  bool set_sstpr (const uint16_t _addr) {
    return (send(0x68) && send(_addr & 0xFF) && send(0x69) && send(_addr >> 8));
  }

  bool get_sld (void) {
    return (send(0x24) && recv());
  }

  bool set_sst (const uint8_t _data) {
    return (send(0x64) && send(_data));
  }

  /*** NVM control ****/

  bool nvm_wait (void) {
    while (get_sin(0x62) && RXDATA);  /* NVMCSR_REG: IO=0x32 */
    return true;
  }

  bool nvm_ctrl (const uint8_t _nvmcmd) {
    return set_sout(0x63, _nvmcmd);   /* NVMCMD_REG: IO=0x33 */
  }

  size_t erase_memory (void) {
    uint8_t   m_type = packet.out.tpi.read.bMType;
    uint16_t _dwAddr = bswap16(_CAPS32(packet.out.tpi.read.dwAddr)->words[1]);
    if (m_type == 0x01) {
      /* XPRG_ERASE_CHIP */
      D1PRINTF(" CHIP_ERASE=%04X\r\n", _dwAddr);
      if (nvm_wait()
        && set_sstpr(_dwAddr)
        && nvm_ctrl(0x10)
        && set_sst(0xFF)
        && nvm_wait()
        && nvm_ctrl(0x00)) {
        return 1;
      }
    }
    else {
      /* 0x02: XPRG_ERASE_APP */
      /* 0x09: XPRG_ERASE_CONFIG */
      /* Currently not called on AVRDUDE<=8.0. */
      /* It may be called from terminal mode.  */
      D1PRINTF(" SECTION_ERASE=%04X\r\n", _dwAddr);
      if (nvm_wait()
        && set_sstpr(_dwAddr)
        && nvm_ctrl(0x14)
        && set_sst(0xFF)
        && nvm_wait()
        && nvm_ctrl(0x00)) {
          return 1;
        }
    }
    return 0;
  }

  size_t read_memory (void) {
    /* With reduceAVR you don't need to look at the memory type. */
    uint16_t _dwAddr = bswap16(_CAPS32(packet.out.tpi.read.dwAddr)->words[1]);
    size_t  _wLength = bswap16(packet.out.tpi.read.wLength);
    uint8_t *_q = &packet.in.data[0];
    size_t _cnt = 0;
    D2PRINTF(" READ=%08X:%04X\r\n", _dwAddr, _wLength);
    set_sstpr(_dwAddr);
    while (_cnt < _wLength) {
      if (!get_sld()) return 0;
      *_q++ = RXDATA;
      ++_cnt;
    }
    return _wLength + 1;
  }

  size_t write_memory (void) {
    uint8_t   m_type = packet.out.tpi.read.bMType;
    uint16_t _dwAddr = bswap16(_CAPS32(packet.out.tpi.write.dwAddr)->words[1]);
    size_t  _wLength = bswap16(packet.out.tpi.write.wLength);
    uint8_t *_p = &packet.out.tpi.write.memData[0];
    bool _result = true;

    /* To accommodate older host programs, */
    /* the address must be aligned to the top of the page. */
    while (_dwAddr & (_tpi_chunks - 1)) {
      _dwAddr--;
      _wLength++;
      *--_p = 0xFF;   /* NAND masked dummy bytes */
    }
    while (_wLength & (_tpi_chunks - 1)) {
      *((uint8_t*)(_dwAddr + _wLength++)) = 0xFF;
    }
    D2PRINTF(" FIXED_WRITE=%08X:%04X\r\n", _dwAddr, _wLength);

    /* For the flash code area, the page erase can be */
    /* omitted if the chip has already been erased.   */
    /* 0x01: XPRG_MEM_TYPE_APPL */
    if (m_type != 0x01) {
      /* SECTION_ERASE */
      D2PRINTF(" SECTION_ERASE=%04X>%04X\r\n", _dwAddr | 1, _CAPS16(_before_page)->word);
      _result &= nvm_wait()
        && set_sstpr(_dwAddr | 1)
        && nvm_ctrl(0x14)
        && set_sst(0xFF)
        && nvm_wait()
        && nvm_ctrl(0x00);
      if (!_result) return 0;
    }
    _result &= nvm_wait() && set_sstpr(_dwAddr);
    /* WRITE_PAGE */
    for (size_t _i = 0; _i < _wLength; _i += _tpi_chunks) {
      D2PRINTF(" CODE_WRITE=%08X:%04X\r\n", _dwAddr, _tpi_chunks);
      _result &= nvm_ctrl(0x1D)
              && set_sst(*_p++)
              && set_sst(*_p++);
      if (_tpi_chunks == 4) {
        idle_clock(12);
        _result &= set_sst(*_p++) && set_sst(*_p++);
      }
      if (_tpi_chunks == 8) {
        idle_clock(12);
        _result &= set_sst(*_p++) && set_sst(*_p++);
        idle_clock(12);
        _result &= set_sst(*_p++) && set_sst(*_p++);
      }
      _result &= nvm_wait();
      if (!_result) return 0;
      _dwAddr += _tpi_chunks;
    }
    return nvm_ctrl(0x00);
  }

  /*** Application ****/

  size_t connect (void) {
    PGCONF = PGCONF_FAIL_bm;
    USART::disable_vcp();
    openDrainWriteMacro(PIN_PGM_TDAT, LOW);
  #if (PIN_PGM_TCLK != PIN_VCP_TXD)
    openDrainWriteMacro(PIN_PGM_TCLK, LOW);
  #endif

    if (_packet_length > 6 && packet.out.tpi.bType) {
      /* External Reset : Activation High-Voltage mode */
      D1PRINTF("<HVEN>\r\n");
      SYS::power_reset();
      SYS::hvc_enter_tpi();
      bit_set(PGCONF, PGCONF_HVEN_bp);
    }
    else {
      /*** Enter RESET (normal programing) ***/
      /* TRST remains LOW until program mode is exited. */
      openDrainWriteMacro(PIN_PGM_TRST, LOW);
      SYS::power_reset();
    }

    /*** Activate clock ***/
    /* If a device is pushing or pulling a control pin at OUTPUT,     */
    /* the potential must be lowered for a sufficient period of time. */
    /* During the wait, TCLK will emit a sufficient number of pulses. */
    delay_millis(10);
    USART::change_tpi();
    idle_clock(36);

    /*** Set TPIPCR Guard Time : 4 clock ****/
    if (!set_sstcs(0x02, 0x05)) return 0;
    D1PRINTF(" TPIPCR<05\r\n");

    /*** Check TPIIR code : Fixed 0x80 ***/
    while (!(get_sldcs(0x0F) && (RXDATA == 0x80)));
    D1PRINTF(" TPIIR>%02X\r\n", RXDATA);
    bit_set(PGCONF, PGCONF_UPDI_bp);

    /*** Activate NVMPROG mode ***/

    while (!(get_sldcs(0x00) && RXDATA == 0x02)) {  /* get TPISR */
      D1PRINTF(" SKEY\r\n");
      for (uint8_t i = 0; i < (uint8_t)sizeof(nvmprog_key); i++) {
        if (!send(nvmprog_key[i])) {
          return 0;
        }
        idle_clock(4);
      }
    };
    D1PRINTF(" TPISR>%02X\r\n", RXDATA);

    /*
     * Get the device signature.
     * Currently AVRDUDE <= 8.0 does not report the device descriptor for reduceAVR cores.
     * So you'll need to find out for yourself.
     *
     * The ATtiny20 is written in 2 word chunks,
     * The ATtiny40 is written in 4 word chunks,
     * Other is written in 1 word chunks.
     *
     * The original PICKit4 probably does the same thing,
     * since the JTAG3 protocol does not include these notifications.
     */
    uint16_t _signature = 0;
    if (set_sstpr(0x3FC1) && get_sld()) {
      _CAPS16(_signature)->bytes[1] = RXDATA;
      if (get_sld()) _CAPS16(_signature)->bytes[0] = RXDATA;
    }
    _tpi_chunks = _signature == 0x920E ? 8  /* ATtiny40 */
                : _signature == 0x910F ? 4  /* ATtiny20 */
                : 2;                        /* Othres   */
    D1PRINTF(" SIG>%04X:%02X\r\n", _signature, _tpi_chunks);
    bit_set(PGCONF, PGCONF_PROG_bp);
    return 1;
  }

  size_t disconnect (void) {
    /*** leave RESET (normal programing) ***/
    set_sstcs(0x00, 0x00);
    D1PRINTF(" TPISR<00\r\n");
    /* Send the NVM exit command, wait a short while and release RESET. */
    SYS::delay_100us();
    if (bit_is_set(PGCONF, PGCONF_HVEN_bp)) {
      SYS::hvc_leave();
      digitalWriteMacro(PIN_HVC_SELECT2, LOW);
      D1PRINTF("<HVLV>\r\n");
    }

    openDrainWriteMacro(PIN_VCP_TXD, HIGH);
    openDrainWriteMacro(PIN_PGM_TRST, HIGH);
    SYS::power_reset();
    PGCONF = 0;
    return 1;
  }

  /*** The TPI scope provides access to the reduceAVR chip. ***/
  /*
   * Packets in this scope are a subset of STK600's XPRG.
   * Therefore, addresses and data lengths are big endian.
   *
   * When this scope is used, CMD3_SIGN_ON will not be called,
   * It doesn't seem to be implemented in the "mEDBG".
   * which means it will start immediately with XPRG_CMD_ENTER_PROGMODE.
   */
  size_t jtag_scope_tpi (void) {
    size_t _rspsize = 0;  /* Make final adjustments. */
    uint8_t _cmd    = packet.out.cmd;
    if (_cmd == 0x01) {             /* XPRG_CMD_ENTER_PROGMODE */
      D1PRINTF(" TPI_ENTER_PROGMODE\r\n");
      USART::setup();
      _rspsize = Timeout::command(&connect);
    }
    else if (_cmd == 0x02) {        /* XPRG_CMD_LEAVE_PROGMODE */
      D1PRINTF(" TPI_LEAVE_PROGMODE\r\n");
      _rspsize = disconnect();
      _tpi_setmode = 0;
      USART::setup();
      USART::change_vcp();
    }
    else if (bit_is_clear(PGCONF, PGCONF_UPDI_bp)) { /* empty */ }
    else if (_cmd == 0x03) {        /* XPRG_CMD_ERASE */
      D1PRINTF(" TPI_ERASE=%02X:%08lX\r\n",
        packet.out.tpi.read.bMType,
        bswap32(packet.out.tpi.read.dwAddr)
      );
      _rspsize = Timeout::command(&erase_memory);
    }
    else if (_cmd == 0x04) {        /* XPRG_CMD_WRITE_MEM */
      D1PRINTF(" TPI_WRITE=%02X:%08lX:%04X\r\n",
        packet.out.tpi.write.bMType,
        bswap32(packet.out.tpi.write.dwAddr),
        bswap16(packet.out.tpi.write.wLength)
      );
      _rspsize = Timeout::command(&write_memory);
    }
    else if (_cmd == 0x05) {        /* XPRG_CMD_READ_MEM */
      D1PRINTF(" TPI_READ=%02X:%08lX:%04X\r\n",
        packet.out.tpi.read.bMType,
        bswap32(packet.out.tpi.read.dwAddr),
        bswap16(packet.out.tpi.read.wLength)
      );
      _rspsize = Timeout::command(&read_memory);
    }
    else if (_cmd == 0x06) {        /* XPRG_CMD_CRC */
      D1PRINTF(" TPI_CRC\r\n");     /* not used (Fail) */
    }
    else if (_cmd == 0x07) {        /* XPRG_CMD_SET_PARAM */
  #ifdef _Use_hardcoded_code_instead_
      /* I don't trust it as the values ​​passed seem to vary depending on the driver. */
      /* If necessary, use a hard-coded one based on the signatures you collect. */
      D1PRINTF(" TPI_SET_PARAM=%02X:%02X\r\n", _bType, _bValue);
      uint8_t _bType  = packet.out.tpi.bType;
      uint8_t _bValue = packet.out.tpi.bValue;
      if (_bType == 0x03) {         /* XPRG_PARAM_NVMCMD_ADDR */
        _bValue = ((_bValue & 0xF0) << 1) + (_bValue & 0x0F);
        _tpi_cmd_addr = _bValue;    /* XPRG_TPI_NVMCMD_ADDRESS */
      }
      else if (_bType == 0x04) {    /* XPRG_PARAM_NVMCSR_ADDR */
        _bValue = ((_bValue & 0xF0) << 1) + (_bValue & 0x0F);
        _tpi_csr_addr = _bValue;    /* XPRG_TPI_NVMCSR_ADDRESS */
      }
  #endif
      _rspsize = 1;
    }
    packet.in.tpi_res = _rspsize ? 0x00 : 0x01; /* XPRG_ERR_OK : XPRG_ERR_FAILED */
    D1PRINTF(" <RES:%02X>\r\n", _rspsize);

    /* Adds padding to XPRG responses to adjust the length of the payload. */
    return ++_rspsize;
  }

};

// end of code

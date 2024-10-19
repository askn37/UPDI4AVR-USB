/**
 * @file usb.cpp
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
#include <avr/eeprom.h>     /* EEMEM */
#include <avr/pgmspace.h>   /* PROGMEM memcpy_P */
#include <string.h>         /* memcpy */
#include "api/macro_api.h"  /* ATOMIC_BLOCK */
#include "api/capsule.h"    /* _CAPS macro */
#include "api/btools.h"     /* crc32 */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * USB VID:PID pair value
 *
 *   The default VID:PID is 0x04DB:0x0B15 (Microchip:CDC-ACM).
 *   AVRDUDE won't recognize it out of the box.
 *   Change the first 8 bytes of the EEPROM to your desired value.
 *
 * USB Vendor and Manufacturer Strings
 *
 *   If using V-USB shared VID:PID pairs,
 *   the format of the vendor string must comply with regulations.
 *
 *   The "CMSIS-DAP" substring in the manufacture string is required
 *   for AVRDUDE to work and cannot be removed.
 */

namespace USB {

  // MARK: Descroptor

  const wchar_t PROGMEM vstring[] = L"MultiX.jp OSSW/OSHW Prod.";
  const wchar_t PROGMEM mstring[] = L"UPDI4AVR-USB:AVR-DU:EDBG/CMSIS-DAP";
  const wchar_t PROGMEM istring[] = L"CDC-ACM/VCP";

  const uint8_t PROGMEM device_descriptor[] = {
    /* This device descriptor contains the VID:PID, default is MCHP:CDC-ACM. */
    0x12, 0x01, 0x00, 0x02, 0xEF, 0x02, 0x01, 0x40,
    0xDB, 0x04, 0x15, 0x0B, 0x00, 0x01, 0x01, 0x02, 0x03, 0x01
  };
  const uint8_t PROGMEM qualifier_descriptor[] = {
    /* This descriptor selects Full-Speed (USB 2.0) ​​for USB 3.0. */
    0x0A, 0x06, 0x00, 0x02, 0xEF, 0x02, 0x01, 0x40, 0x00, 0x00
  };
  const uint8_t PROGMEM current_descriptor[] = {
    /* This descriptor is almost identical to the Xplained Mini series. */
    /* It does not allow for an dWire gateway. */
    0x09, 0x02, 0x6B, 0x00, 0x03, 0x01, 0x00, 0x80, 0x32, /* Information Set#3 */
    0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, /* Interface #0 HID  */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x23, 0x00, /*   HID using       */
    0x07, 0x05, 0x02, 0x03, 0x40, 0x00, 0x01,             /*   EP_DPO_OUT 0x02 */
    0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01,             /*   EP_DPI_IN  0x81 */
    0x08, 0x0B, 0x01, 0x02, 0x02, 0x02, 0x01, 0x04,       /* CDC IAD    Str#4  */
    0x09, 0x04, 0x01, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00, /* Interface #1 CDC  */
    0x05, 0x24, 0x00, 0x10, 0x01,                         /* Functional Header */
    0x04, 0x24, 0x02, 0x06,                               /* Functional ACM Capabilities  */
    0x05, 0x24, 0x06, 0x01, 0x02,                         /* Functional UNION     #1 + #2 */
    0x05, 0x24, 0x01, 0x03, 0x02,                         /* Functional CallManagement #2 */
    0x07, 0x05, 0x82, 0x03, 0x10, 0x00, USB_CCI_INTERVAL, /*   EP_CCI_IN  0x82 */
    0x09, 0x04, 0x02, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00, /* Interface #2 Bulk */
    0x07, 0x05, 0x03, 0x02, 0x40, 0x00, 0x00,             /*   EP_CDO_OUT 0x03 */
    0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00,             /*   EP_CDI_IN  0x83 */
  #ifdef _I_want_to_use_a_second_CDC_ACM_additional_
    /*
     * Examples for increasing the number of CDC-ACM interfaces on Windows and macos.
     * Besides this, you need to extend the required endpoint implementation.
     * You can add up to 6 sets.
     * Linux? Basically no, probably only the first one will work.
     *
     * Please update the Information Descriptor as shown in the example.
     *             ____  ____  ____
     * 0x09, 0x02, 0xAD, 0x00, 0x05, 0x01, 0x00, 0x80, 0x32, // Information Set#5 (Dual CDC)
    */
    0x08, 0x0B, 0x03, 0x02, 0x02, 0x02, 0x01, 0x05,       /* CDC IAD    Str#5  */
    0x09, 0x04, 0x03, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00, /* Interface #3 CDC  */
    0x05, 0x24, 0x00, 0x10, 0x01,                         /* Functional Header */
    0x04, 0x24, 0x02, 0x06,                               /* Functional ACM Capabilities  */
    0x05, 0x24, 0x06, 0x03, 0x04,                         /* Functional UNION     #3 + #4 */
    0x05, 0x24, 0x01, 0x03, 0x04,                         /* Functional CallManagement #4 */
    0x07, 0x05, 0x84, 0x03, 0x10, 0x00, 0x05,             /*   EP_CCI_IN  0x84 */
    0x09, 0x04, 0x04, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00, /* Interface #4 Bulk */
    0x07, 0x05, 0x05, 0x02, 0x40, 0x00, 0x00,             /*   EP_CDO_OUT 0x05 */
    0x07, 0x05, 0x85, 0x02, 0x40, 0x00, 0x00,             /*   EP_CDI_IN  0x85 */
  #endif
  };
  const uint8_t PROGMEM report_descriptor[] = {
    /* This descriptor defines a HID report. */
    /* The maximum buffer size allowed in Full-Speed (USB 2.0) mode is 64 bytes. */
    0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x15,
    0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x96, 0x40,
    0x00, 0x09, 0x01, 0x81, 0x02, 0x96, 0x40, 0x00,
    0x09, 0x01, 0x91, 0x02, 0x95, 0x04, 0x09, 0x01,
    0xB1, 0x02, 0xC0
  };

  const EP_TABLE_t PROGMEM ep_init = {
    { /* FIFO */ },
    { /* EP */
      { /* EP_REQ */
        { 0,
          USB_TYPE_CONTROL_gc                                 | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.req_data, 0 },
        /* EP_RES */
        { 0,
          USB_TYPE_CONTROL_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.res_data, 0 },
      },
      { /* EP_DPI */
        { /* not used */ },
        { 0,
          USB_TYPE_BULKINT_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          64, (uint16_t)&EP_MEM.dap_data, 0 },
      },
      { /* EP_DPO */
        { 0,
          USB_TYPE_BULKINT_gc                                 | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.dap_data, 64 },
        /* EP_CCI */
        { 0,
          USB_TYPE_BULKINT_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF16_gc,
          0, (uint16_t)&EP_MEM.cci_data, 0 },
      },
      { /* EP_CDO */
        { 0,
          USB_TYPE_BULKINT_gc                                 | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.cdo_data, 64 },
        /* EP_CDI */
        { USB_BUSNAK_bm,
          USB_TYPE_BULKINT_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.cdi_data, 0 },
      },
    },
    { /* FRAMENUM */ }
  };

  size_t get_descriptor (uint8_t* _buffer, uint16_t _index) {
    uint8_t* _pgmem = 0;
    size_t   _size = 0;
    uint8_t  _type = _index >> 8;
    if (_type == 0x01) {          /* DEVICE */
      _pgmem = (uint8_t*)&device_descriptor;
      _size = sizeof(device_descriptor);
      memcpy_P(_buffer, _pgmem, _size);
      uint32_t _vidpid = *((uint32_t*)EEPROM_START);
      if (_vidpid + 1) _CAPS32(_buffer[8])->dword = _vidpid;
      D1PRINTF(" VID:PID=%04X:%04X\r\n", _CAPS16(_buffer[8])->word, _CAPS16(_buffer[10])->word);
      return _size;
    }
    else if (_type == 0x02) {     /* CONFIGURATION */
      _pgmem = (uint8_t*)&current_descriptor;
      _size = sizeof(current_descriptor);
    }
    else if (_type == 0x06) {     /* QUALIFIER */
      _pgmem = (uint8_t*)&qualifier_descriptor;
      _size = sizeof(qualifier_descriptor);
    }
    else if (_type == 0x21) {     /* HID */
      _pgmem = (uint8_t*)&current_descriptor + 18;
      _size = 9;
    }
    else if (_type == 0x22) {     /* REPORT */
      _pgmem = (uint8_t*)&report_descriptor;
      _size = sizeof(report_descriptor);
    }
    else if (_index == 0x0300) {  /* LANGUAGE */
      _size = 4;
      *_buffer++ = 4;
      *_buffer++ = 3;
      *_buffer++ = 0x09;
      *_buffer++ = 0x04;
      return _size;
    }
    else {
      switch (_index) {
        case 0x0301: _pgmem = (uint8_t*)&vstring; _size = sizeof(vstring); break;
        case 0x0302: _pgmem = (uint8_t*)&mstring; _size = sizeof(mstring); break;
        case 0x0304: _pgmem = (uint8_t*)&istring; _size = sizeof(istring); break;
        // case 0x0303: _pgmem = (uint8_t*)&string3; _size = sizeof(string3); break;
        case 0x0303: {
          /*
           * NOTE: Serial Number String Generation
           *
           * If the 4 bytes from offset 4 of the EEPROM are anything other
           * than 0xFFFF:FFFF, use them, otherwise generate a 32-bit random
           * serial number using CRC32 from the factory information.
           */
          uint32_t _sn = ((User_EEP_t*)EEPROM_START)->dwSerialNumber;
          if (!(_sn + 1)) _sn = crc32((uint8_t*)SIGNATURES_START, 32);
          uint8_t* _p = (uint8_t*)&_sn;
          *_buffer++ = 22;  *_buffer++ = 3;
          *_buffer++ = 'M'; *_buffer++ = 0;
          *_buffer++ = 'X'; *_buffer++ = 0;
          for (uint8_t _i = 0; _i < 4; _i++) {
            uint8_t _c = *_p++;
            *_buffer++ = btoh(_c >> 4); *_buffer++ = 0;
            *_buffer++ = btoh(_c     ); *_buffer++ = 0;
          }
          return 22;
        }
      }
      *_buffer++ = (uint8_t)_size;
      *_buffer++ = 3;
      if (_size) memcpy_P(_buffer, _pgmem, _size - 2);
      return _size;
    }
    if (_size) memcpy_P(_buffer, _pgmem, _size);
    return _size;
  }

  void set_cci_data (uint16_t _state) {
    _set_serial_state = _state;
    EP_MEM.cci_header.bmRequestType = 0xA1; /* REQTYPE_DIRECTION | REQTYPE_CLASS | RECIPIENT_INTERFACE */
    EP_MEM.cci_header.bRequest      = 0x20; /* CDC_REQ_SerialState */
    EP_MEM.cci_header.wValue        = 0;
    EP_MEM.cci_header.wIndex        = 1;    /* Interface#1 */
    EP_MEM.cci_header.wLength       = 2;
    EP_MEM.cci_wValue               = _state;
  }

  void setup_device (bool _force) {
    if (_led_mode != 3) SYS::LED_Flash();
    USB0_ADDR = 0;
    if (USB0_CTRLA || _force) {
      USB0_CTRLA = 0;
      USB0_FIFOWP = 0;
      USB0_EPPTR = (uint16_t)&EP_TABLE.EP;
      USB0_CTRLB = USB_ATTACH_bm;
      GPCONF = 0;
      PGCONF = 0;
      RXSTAT = 0;
      _send_break = 0;
      _send_count = 0;
      _recv_count = 0;
      _set_config = 0;
      _sof_count = 0;
      memcpy_P(&EP_TABLE, &ep_init, sizeof(EP_TABLE_t));
      set_cci_data(0x00);
      USB0_CTRLA = USB_ENABLE_bm | (USB_ENDPOINTS_MAX - 1);
    }
  }

  // MARK: Endpoint

  bool is_ep_setup (void) { return bit_is_set(EP_REQ.STATUS, USB_EPSETUP_bp); }
  bool is_not_dap (void) { return bit_is_clear(EP_DPO.STATUS, USB_BUSNAK_bp); }
  void ep_req_pending (void) { loop_until_bit_is_set(EP_REQ.STATUS, USB_BUSNAK_bp); }
  void ep_res_pending (void) { loop_until_bit_is_set(EP_RES.STATUS, USB_BUSNAK_bp); }
  void ep_dpi_pending (void) { loop_until_bit_is_set(EP_DPI.STATUS, USB_BUSNAK_bp); }
  void ep_dpo_pending (void) { loop_until_bit_is_set(EP_DPO.STATUS, USB_BUSNAK_bp); }
  void ep_cci_pending (void) { loop_until_bit_is_set(EP_CCI.STATUS, USB_BUSNAK_bp); }
  void ep_cdo_pending (void) { loop_until_bit_is_set(EP_CDO.STATUS, USB_BUSNAK_bp); }
  void ep_cdi_pending (void) { loop_until_bit_is_set(EP_CDI.STATUS, USB_BUSNAK_bp); }

  void ep_req_listen (void) {
    EP_REQ.CNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_REQ) = ~USB_TOGGLE_bm;
  }

  void ep_res_listen (void) {
    EP_RES.MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_RES) = ~USB_TOGGLE_bm;
  }

  void ep_dpi_listen (void) {
    EP_DPI.CNT = 64;
    EP_DPI.MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_DPI) = ~USB_TOGGLE_bm;
  }

  void ep_dpo_listen (void) {
    EP_DPO.CNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_DPO) = ~USB_TOGGLE_bm;
  }

  void ep_cci_listen (void) {
    if ((_send_break + 1) > 1 && _send_break > USB_CCI_INTERVAL) {
      _send_break -= USB_CCI_INTERVAL;
    }
    EP_CCI.CNT = 10;
    EP_CCI.MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_CCI) = ~USB_TOGGLE_bm;
  }

  void ep_cdi_listen (void) {
    /* Send the VCP-RxD buffer to the host. */
    /* If our math is correct, then if each side of the double */
    /* buffer can complete the transmission of 64 characters   */
    /* in 1 ms, then it can support 640 kbps. */
    if (bit_is_clear(GPCONF, GPCONF_OPN_bp)
     || bit_is_clear(EP_CDI.STATUS, USB_BUSNAK_bp)) {
      /* No sending allowed while port is closed.  */
      /* If the buffer overflows, it is discarded. */
      if (_send_count == 64) _send_count = 0;
      return;
    }
    D2PRINTF(" VI=%02X:", _send_count);
    D2PRINTHEX(bit_is_set(GPCONF, GPCONF_DBL_bp)
      ? &EP_MEM.cdi_data[64]
      : &EP_MEM.cdi_data[0], _send_count);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      EP_CDI.DATAPTR = bit_is_set(GPCONF, GPCONF_DBL_bp)
        ? (register16_t)&EP_MEM.cdi_data[64]
        : (register16_t)&EP_MEM.cdi_data[0];
      EP_CDI.CNT = _send_count;
      EP_CDI.MCNT = 0;
      _send_count = 0;
      GPCONF ^= GPCONF_DBL_bm;
    }
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_CDI) = ~USB_TOGGLE_bm;
  }

  void ep_cdo_listen (void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      _recv_count = 0;
      EP_CDO.CNT  = 0;
    }
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_CDO) = ~USB_TOGGLE_bm;
  }

  void ep0_stalled (void) {
    D1PRINTF("[STALLED]\r\n");
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_SET(USB_EP_RES) = USB_STALLED_bm;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_SET(USB_EP_REQ) = USB_STALLED_bm;
  }

  void complete_dap_out (void) {
    ep_dpi_listen();
    ep_dpo_listen();  /* continue transaction */
  }

  void break_on (void) {
    if (bit_is_set(GPCONF, GPCONF_VCP_bp)
     && bit_is_clear(GPCONF, GPCONF_BRK_bp)) {
      /* SET_SEND_BREAK is called when the port is closed successfully. */
      bit_clear(GPCONF, GPCONF_OPN_bp);
      _sof_count = 0;
      USART::disable_vcp();
  #ifdef CONFIG_VCP_TXD_ODM
      /* During Break, VCP-TxD is pulled LOW. */
      pinModeMacro(PIN_VCP_TXD, OUTPUT);
  #endif
    }
    bit_set(GPCONF, GPCONF_BRK_bp);
  }

  void break_off (void) {
    if (bit_is_set(GPCONF, GPCONF_VCP_bp)
     && bit_is_set(GPCONF, GPCONF_BRK_bp)) {
      USART::change_vcp();
      bit_set(GPCONF, GPCONF_OPN_bp);
    }
    bit_clear(GPCONF, GPCONF_BRK_bp);
  }

  void cci_break_count (void) {
    /* If the break value is between 1 and 65534, it will count down. */
    if ((_send_break + 1) > 1) {
      if (_send_break > USB_CCI_INTERVAL) {
        if (bit_is_set(EP_CCI.STATUS, USB_BUSNAK_bp)) ep_cci_listen();
      }
      else {
        _send_break = 0;
        break_off();
      }
      D2PRINTF(" SB=%04X\r\n", _send_break);
    }
  }

  void cci_interrupt (void) {
  #if defined(CONFIG_VCP_INTERRUPT_SUPPRT)
    SerialState_t _value = {};
    #if defined(CONFIG_VCP_RS232C_ENABLE)
    uint8_t _c = VPORTD_IN;
    _value.bValue = (_c ^ 0x0B) & 0x0B;
    #endif
    if (bit_is_set(RXSTAT, USART_FERR_bp))   _value.bFraming = true;
    if (bit_is_set(RXSTAT, USART_PERR_bp))   _value.bParity  = true;
    if (bit_is_set(RXSTAT, USART_BUFOVF_bp)) _value.bOverRun = true;
    RXSTAT = 0;
    if (bit_is_set(GPCONF, GPCONF_OPN_bp) && _set_serial_state != _value.bValue) {
      set_cci_data(_value.bValue);
      D1PRINTF(" CCI=");
      D1PRINTHEX(&EP_MEM.cci_data, 10);
      if (bit_is_set(EP_CCI.STATUS, USB_BUSNAK_bp)) ep_cci_listen();
    }
  #endif
  }

  // MARK: VCP

  void write_byte (const uint8_t _c) {
    /* The double buffer consists of two blocks. */
    uint8_t* _buf = bit_is_set(GPCONF, GPCONF_DBL_bp)
      ? &EP_MEM.cdi_data[64]
      : &EP_MEM.cdi_data[0];
    _buf[_send_count++] = _c;
    if (_send_count < 64) _sof_count = 30;
    else ep_cdi_listen();
  }

  uint8_t read_byte (void) {
    uint8_t _c = 0;
    if (EP_CDO.CNT != _recv_count) {
  #if defined(DEBUG) && (DEBUG >= 2)
      if (_recv_count == 0) {
        D2PRINTF(" VO=%02X:", EP_CDO.CNT);
        D2PRINTHEX(&EP_MEM.cdo_data, EP_CDO.CNT);
      }
  #endif
      _c = EP_MEM.cdo_data[_recv_count++];
    }
    if (EP_CDO.CNT == _recv_count) ep_cdo_listen();
    return _c;
  }

  bool read_available (void) {
    uint8_t _s = 0;
    if (bit_is_set(EP_CDO.STATUS, USB_BUSNAK_bp) && bit_is_set(GPCONF, GPCONF_VCP_bp)) {
      _s = EP_CDO.CNT - _recv_count;
      if (_s == 0) ep_cdo_listen();
    }
    return _s != 0;
  }

  void read_drop (void) {
    if (bit_is_set(EP_CDO.STATUS, USB_BUSNAK_bp)) ep_cdo_listen();
  }

  void vcp_receiver (void) {
    uint8_t _d = USART0_RXDATAH;
    uint8_t _c = USART0_RXDATAL;
    if (!(_d & (USART_BUFOVF_bm | USART_FERR_bm | USART_PERR_bm))) {
      write_byte(_c);
    }
    RXSTAT |= _d;
  #if defined(CONFIG_VCP_INTERRUPT_SUPPRT)
    if (bit_is_set(EP_CCI.STATUS, USB_BUSNAK_bp)) cci_interrupt();
  #endif
  }

  void vcp_receiver_9bit (void) {
  #if defined(CONFIG_VCP_9BIT_SUPPORT)
    uint8_t _c = USART0_RXDATAL;
    uint8_t _d = USART0_RXDATAH;
    if (!(_d & (USART_BUFOVF_bm | USART_FERR_bm | USART_PERR_bm))) {
      write_byte(_c);
      write_byte(_d & 1);
    }
    RXSTAT |= _d;
    #if defined(CONFIG_VCP_INTERRUPT_SUPPRT)
    if (bit_is_set(EP_CCI.STATUS, USB_BUSNAK_bp)) cci_interrupt();
    #endif
  #endif
  }

  void vcp_transceiver (void) {
    if (bit_is_clear(GPCONF, GPCONF_BRK_bp)
  #if defined(CONFIG_VCP_CTS_ENABLE)
     && !digitalReadMacro(PIN_VCP_CTS)
  #endif
     && bit_is_set(USART0_STATUS, USART_DREIF_bp)
     && read_available()) {
      USART0_TXDATAL = read_byte();
    }
  }

  void vcp_transceiver_9bit (void) {
  #if defined(CONFIG_VCP_9BIT_SUPPORT)
    if (bit_is_clear(GPCONF, GPCONF_BRK_bp)
    #if defined(CONFIG_VCP_CTS_ENABLE)
     && !digitalReadMacro(PIN_VCP_CTS)
    #endif
     && bit_is_set(USART0_STATUS, USART_DREIF_bp)
     && read_available()) {
      uint8_t _c = read_byte();
      uint8_t _d = read_byte();
      USART0_TXDATAH = _d & 1;
      USART0_TXDATAL = _c;
    }
  #endif
  }

  // MARK: USB Session

  /*** USB Standard Request Enumeration. ***/
  bool request_standard (void) {
    bool _listen = true;
    uint8_t bRequest = EP_MEM.req_data.bRequest;
    if (bRequest == 0x00) {       /* GET_STATUS */
      EP_MEM.res_data[0] = 0;
      EP_MEM.res_data[1] = 0;
      EP_RES.CNT = 2;
    }
    else if (bRequest == 0x01) {  /* CLEAR_FEATURE */
      D1PRINTF(" CF=%02X:%02X\r\n", EP_MEM.req_data.wValue, EP_MEM.req_data.wIndex);
      if (0 == (uint8_t)EP_MEM.req_data.wValue) {
        /* Expects an endpoint number to be passed in. Swaps the high and low */
        /* nibbles to make it a representation of the USB controller. */
        uint8_t _EP = USB_EP_ID_SWAP(EP_MEM.req_data.wIndex);
        loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
        USB_EP_STATUS_CLR(_EP) = USB_STALLED_bm | USB_BUSNAK_bm | USB_TOGGLE_bm;
      }
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x04) {  /* SET_FEATURE */
      /* If used, it will be ignored. */
      D1PRINTF(" SF=%02X:%02X\r\n", EP_MEM.req_data.wValue, EP_MEM.req_data.wIndex);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x05) {  /* SET_ADDRESS */
      uint8_t _addr = EP_MEM.req_data.wValue & 0x7F;
      ep_res_listen();
      ep_res_pending();
      USB0_ADDR = _addr;
      D1PRINTF(" USB0_ADDR=%d\r\n", _addr);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x06) {  /* GET_DESCRIPTOR */
      size_t _length = EP_MEM.req_data.wLength;
      size_t _size = get_descriptor((uint8_t*)&EP_MEM.res_data, EP_MEM.req_data.wValue);
      EP_RES.CNT = (_size > _length) ? _length : _size;
      _listen = !!_size;
    }
    else if (bRequest == 0x08) {  /* GET_CONFIGURATION */
      EP_MEM.res_data[0] = _set_config;
      D1PRINTF("<GC:%02X>\r\n", _set_config);
      EP_RES.CNT = 1;
    }
    else if (bRequest == 0x09) {  /* SET_CONFIGURATION */
      /* Once the USB connection is fully initiated, it will go through here. */
      _set_config = (uint8_t)EP_MEM.req_data.wValue;
      bit_set(GPCONF, GPCONF_USB_bp);
      SYS::LED_HeartBeat();
      D1PRINTF("<SC:%02X>\r\n", _set_config);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x0A) {  /* GET_INTREFACE */
      /* It seems not to be used. */
      D1PRINTF("<SI:0>\r\n");
      EP_MEM.res_data[0] = 0;
      EP_RES.CNT = 1;
    }
    else if (bRequest == 0x0B) {  /* SET_INTREFACE */
      /* It seems not to be used. */
      D1PRINTF("<GI:%02X>\r\n", EP_MEM.req_data.wValue);
      EP_RES.CNT = 0;
    }
    else {
      _listen = false;
    }
    return _listen;
  }

  /*** CDC-ACM request processing. ***/
  bool request_class (void) {
    bool _listen = true;
    uint8_t bRequest = EP_MEM.req_data.bRequest;
    if (bRequest == 0x0A) {       /* SET_IDLE */
      /* This is a HID Compliance Class Request. */
      /* It is called but not used. */
      D1PRINTF(" IDL=%02X\r\n", (uint8_t)EP_MEM.req_data.wValue);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x20) {  /* SET_LINE_ENCODING */
      /* SET_LINE_ENCODING is called whenever a port is opened. */
      /* On Windows, it runs immediately after SET_INTREFACE.   */
      /* On macOS, it runs when the host app opens the port.    */
      /* In any case, this isn't a one-off thing. */
      /* If the same parameter settings persist,  */
      /* it's probably best to do nothing.        */
      ep_req_pending();
      USART::set_line_encoding(&EP_MEM.res_encoding);
      D1PRINTF(" SLE=");
      D1PRINTHEX(&_set_line_encoding, sizeof(LineEncoding_t));
      bit_set(GPCONF, GPCONF_OPN_bp);
      _send_count = 0;
      _recv_count = 0;
      _sof_count = 0;
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x21) {  /* GET_LINE_ENCODING */
      memcpy(&EP_MEM.res_encoding, &_set_line_encoding, sizeof(LineEncoding_t));
      if (EP_MEM.res_encoding.dwDTERate == 0) {
        /* Parameters that, if incorrectly accepted,           */
        /* would result in division by zero must be corrected. */
        EP_MEM.res_encoding.dwDTERate = 9600UL;
        EP_MEM.res_encoding.bDataBits = 8;
      }
      D1PRINTF(" GLE=");
      D1PRINTHEX(&EP_MEM.res_encoding, sizeof(LineEncoding_t));
      EP_RES.CNT = sizeof(LineEncoding_t);
    }
    else if (bRequest == 0x22) {  /* SET_LINE_STATE */
      /* This includes the DTR and RTS settings. */
      D1PRINTF(" SLS=%02X\r\n", (uint8_t)EP_MEM.req_data.wValue);
      USART::set_line_state((uint8_t)EP_MEM.req_data.wValue);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x23) {  /* SET_SEND_BREAK */
      /* When the host application closes the port, it may send a BREAK=0. */
      /* Nothing else is used unless programmed by the application. */
      D1PRINTF(" SB=%04X\r\n", EP_MEM.req_data.wValue);
      _send_break = EP_MEM.req_data.wValue;
      if (_send_break) break_on();
      else break_off();
      EP_RES.CNT = 0;
    }
    else {
      _listen = false;
    }
    return _listen;
  }

  /*** Accept the EP0 setup packet. ***/
  /* This process is equivalent to a endpoint interrupt. */
  /* The reason for using polling is to prioritize VCP performance. */
  void handling_control_transactions (void) {
    bool _listen = false;
    uint8_t bmRequestType = EP_MEM.req_data.bmRequestType;
    D1PRINTF("RQ=%02X:%04X:%02X:%02X:%04X:%04X:%04X\r\n",
      EP_REQ.STATUS, EP_REQ.CNT, EP_MEM.req_data.bmRequestType, EP_MEM.req_data.bRequest,
      EP_MEM.req_data.wValue, EP_MEM.req_data.wIndex, EP_MEM.req_data.wLength);
    /* Accepts subsequent EP0_DATA packets as needed. */
    if (bit_is_clear(bmRequestType, 7)) ep_req_listen();
    bmRequestType &= (3 << 5);
    if (bmRequestType == (0 << 5)) {
      _listen = request_standard();
    }
    else if (bmRequestType == (1 << 5)) {
      _listen = request_class();
    }
  #ifdef _Not_being_used_STUB_
    else if (bmRequestType == (2 << 5)) {
      _listen = request_vendor();
    }
  #endif
    if (_listen) {
      ep_res_listen();
      ep_req_listen();
    }
    else {
      ep0_stalled();
    }
    USB0_INTFLAGSB |= USB_EPSETUP_bp;
  }

  /*** This process is equivalent to a bus interrupt. ***/
  /* The reason for using polling is to prioritize VCP performance. */
  /* The trade-off is that power standby is not available. */
  void handling_bus_events (void) {
    uint8_t busstate = USB0_INTFLAGSA;
    USB0_INTFLAGSA = busstate;
  #ifdef PIN_SYS_VDETECT
    /* This section is still experimental. */
    if (digitalReadMacro(PIN_SYS_VDETECT)) {
      if (!USB0_CTRLA) {
        setup_device(true);
        return;
      }
    }
    else if (USB0_CTRLA) {
      /* System reboot */
      SYS::reboot();
    }
  #endif
  #if defined(DEBUG) && (DEBUG >= 2)
    if (bit_is_set(busstate, USB_RESET_bp)) {
      D1PRINTF("<RESET:%04X>\r\n", USB0_ADDR);
    }
    if (bit_is_set(busstate, USB_SUSPEND_bp)) {
      D1PRINTF("<SUSPEND:%04X>\r\n", USB0_ADDR);
    }
    if (bit_is_set(busstate, USB_RESUME_bp)) {
      D1PRINTF("<RESUME:%04X>\r\n", USB0_ADDR);
    }
  #endif
    if (bit_is_set(busstate, USB_SOF_bp)) {
      /* If there is deferred data for a block transfer, it is sent here. */
      if (_sof_count > 0 && 0 == (--_sof_count)) {
        if (bit_is_set(EP_CDI.STATUS, USB_BUSNAK_bp) && _send_count > 0) {
          ep_cdi_listen();
        }
      }
    }
    if (bit_is_set(busstate, USB_SUSPEND_bp)
     || bit_is_set(busstate, USB_RESUME_bp)) {
      /* This implementation does not transition to power saving mode. */
      /* This is only passed when the USB cable is unplugged. */
      if (bit_is_set(GPCONF, GPCONF_USB_bp)) {
        /* System reboot */
        SYS::reboot();
      }
      bit_set(busstate, USB_RESET_bp);
    }
    if (bit_is_set(busstate, USB_RESET_bp)) {
      setup_device(false);
    }
  }

};

// end of code

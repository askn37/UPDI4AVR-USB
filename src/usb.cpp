/**
 * @file usb.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.35.50+
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include <avr/eeprom.h>     /* EEMEM */
#include <avr/pgmspace.h>   /* PROGMEM memcpy_P */
#include <string.h>         /* memcpy */
#include <api/macro_api.h>  /* ATOMIC_BLOCK */
#include <api/capsule.h>    /* _CAPS macro */
#include <api/btools.h>     /* crc32 */
#include <peripheral.h>     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"
#include "USB_Utils.h"
#include "USB/Descriptor.h"

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

  /* Filescope variables */
  NOINIT uint16_t _send_break;
  NOINIT volatile uint8_t _sof_count;
  NOINIT volatile uint8_t _set_serial_state;
  NOINIT volatile uint8_t _send_count;
  NOINIT uint8_t _recv_count;
  NOINIT uint8_t _set_config;
  NOINIT uint8_t _vcp_ctrl;

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
          0, (uint16_t)&EP_MEM.cdo_data, VCP_RXD_BUFFSIZE },
        /* EP_CDI */
        { USB_BUSNAK_bm,
          USB_TYPE_BULKINT_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.cdi_data, 0 },
      },
    },
    { /* FRAMENUM */ }
  };

  size_t get_descriptor (uint8_t* _buffer, uint16_t _index, size_t _length) {
    if (_index == 0x03FF) {
      return get_auto_generate_serial_number_string(_buffer, true);
    }
  #if (__AVR_ARCH__ == 103) || (USB_DESC_MEM >= 1)
    size_t _size = get_descriptor_copy(_buffer, _index, get_descriptor_ptr());
  #else
    size_t _size = get_descriptor_copy_P(_buffer, _index, get_descriptor_ptr());
  #endif
    if (_length >= 12 && _index == bDescriptorType_DEVICE << 8) {
      uint32_t _vidpid = *((uint32_t*)EEPROM_START);
      if (_vidpid + 1) _CAPS32(_buffer[8])->dword = _vidpid;
      D1PRINTF(" VID:PID=%04X:%04X\r\n", _CAPS16(_buffer[8])->word, _CAPS16(_buffer[10])->word);
    }
    return _size;
  }

  void set_cci_data (uint8_t _state) {
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
      GPCONF &= GPCONF_SW0_gm;
      PGCONF = 0;
      RXSTAT = 0;
      _send_break = 0;
      _send_count = 0;
      _recv_count = 0;
      _set_config = 0;
      _sof_count = 0;
      _set_serial_state = 0;
      memcpy_P(&EP_TABLE, &ep_init, sizeof(EP_TABLE_t));
      set_cci_data(0x00);
      USB0_CTRLA = USB_ENABLE_bm | (USB_ENDPOINTS_MAX - 1);
    }
  }

  void usb_detach (void) {
    bit_clear(USB0_CTRLB, USB_ATTACH_bp);
    bit_clear(GPCONF, GPCONF_USB_bp);
    wdt_reset();
    SYS::delay_125ms();
    USB0_CTRLA = 0;
  }

  // MARK: Endpoint

  bool is_ep_setup (void) { return bit_is_set(EP_REQ.STATUS, USB_EPSETUP_bp); }
  bool is_ep_cci (void) { return bit_is_set(EP_CCI.STATUS, USB_BUSNAK_bp); }
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

  void ep_setup_stalled (void) {
    bit_set(EP_RES.CTRL, USB_DOSTALL_bp);
  }

  void ep_dpi_listen (void) {
    EP_DPI.CNT = VCP_TXD_BUFFSIZE;
    EP_DPI.MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_DPI) = ~USB_TOGGLE_bm;
  }

  void ep_dpo_listen (void) {
    EP_DPO.CNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_DPO) = ~USB_TOGGLE_bm;
  }

  bool progress_break (void) {
    if ((_send_break + 1) > 1) {
      _send_break -= _send_break > USB_CCI_INTERVAL
        ? USB_CCI_INTERVAL 
        : _send_break;
      return !_send_break;
    }
    else return false;
  }

  void ep_cci_listen (void) {
    if (is_ep_cci()) {
      D1PRINTF("CCI=");
      D1PRINTHEX(&EP_MEM.cci_data, 10, ':');
      EP_CCI.CNT = 10;
      EP_CCI.MCNT = 0;
      loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
      USB_EP_STATUS_CLR(USB_EP_CCI) = ~USB_TOGGLE_bm;
    }
    #if defined(CONFIG_VCP_INTERRUPT_SUPPRT)
    else {
      _sof_count = SERIALSTATE_DELAY;
    }
    #endif
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
      if (_send_count == VCP_TXD_BUFFSIZE) _send_count = 0;
      return;
    }
    D2PRINTF(" VI=%02X\r\n    ", _send_count);
    D2PRINTHEX(bit_is_set(GPCONF, GPCONF_DBL_bp)
      ? &EP_MEM.cdi_data[VCP_TXD_BUFFSIZE]
      : &EP_MEM.cdi_data[0], _send_count, ':', 16, "\r\n    ");
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      EP_CDI.DATAPTR = bit_is_set(GPCONF, GPCONF_DBL_bp)
        ? (register16_t)&EP_MEM.cdi_data[VCP_TXD_BUFFSIZE]
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

  void complete_dap_out (void) {
    ep_dpi_listen();
    ep_dpo_listen();  /* continue transaction */
  }

  void break_on (void) {
    if (bit_is_set(GPCONF, GPCONF_VCP_bp)
     && bit_is_clear(GPCONF, GPCONF_BRK_bp)) {
      /* SET_SEND_BREAK is called when the port is closed successfully. */
      bit_clear(GPCONF, GPCONF_OPN_bp);
      _send_count = 0;
      _vcp_ctrl = USART0_CTRLB;
      USART0_CTRLB &= ~(USART_RXEN_bm | USART_TXEN_bm);
      bit_set(GPCONF, GPCONF_VCP_bp);
      /* During Break, VCP-TxD is pulled LOW. */
      pinLogicPull(PIN_VCP_TXD);
    }
    bit_set(GPCONF, GPCONF_BRK_bp);
  }

  void break_off (void) {
    if (bit_is_set(GPCONF, GPCONF_VCP_bp)
     && bit_is_set(GPCONF, GPCONF_BRK_bp)) {
      pinLogicOpen(PIN_VCP_TXD);
      _send_count = 0;
      _send_break = 0;
      USART0_CTRLB = _vcp_ctrl;
      bit_set(GPCONF, GPCONF_OPN_bp);
    }
    bit_clear(GPCONF, GPCONF_BRK_bp);
  }

  void cci_break_count (void) {
    /* If the break value is between 1 and 65534, it will count down. */
    if (progress_break()) break_off();
  }

  void cci_interrupt (void) {
  #if defined(CONFIG_VCP_INTERRUPT_SUPPRT)
    SerialState_t _value = {};
    #if defined(CONFIG_VCP_RS232C_ENABLE)
    /* This paragraph is implementation-dependent. */
    /* Please set the states of appropriately,     */
    /*    bRxCarrier, bTxCarrier, and bRingSignal. */
    uint8_t _c = VPORTD_IN;
    _value.bValue = (_c ^ 0x03) & 0x03;
    #else
    if (bit_is_set(GPCONF, GPCONF_VCP_bp)
     && bit_is_set(GPCONF, GPCONF_OPN_bp)
     && bit_is_clear(GPCONF, GPCONF_BRK_bp)) {
      _value.bValue = 0x03; /* bRxCarrier(DCD) and bTxCarrier(DSR) */
    }
    else {
      _value.bValue = 0x04; /* bBreak(BREAK) */
    }
    #endif
    if (bit_is_set(RXSTAT, USART_FERR_bp))   /* 0x04 */ _value.bFraming = true; /* 0x10 */
    if (bit_is_set(RXSTAT, USART_PERR_bp))   /* 0x02 */ _value.bParity  = true; /* 0x20 */
    if (bit_is_set(RXSTAT, USART_BUFOVF_bp)) /* 0x40 */ _value.bOverRun = true; /* 0x40 */
    RXSTAT = 0;
    if (_set_serial_state != _value.bValue) {
      /* If it doesn't meet your expectations, you should reconfirm it after the delay. */
      if (_value.bValue ^ 0x03) _sof_count = SERIALSTATE_DELAY;
      D2PRINTF(" SS=%02X:%02X:%02X\r\n", _set_serial_state, _value.bValue, _sof_count);
      set_cci_data(_value.bValue);
      ep_cci_listen();
    }
  #endif
  }

  // MARK: VCP

  void write_byte (const uint8_t _c) {
    /* The double buffer consists of two blocks. */
    uint8_t* _buf = bit_is_set(GPCONF, GPCONF_DBL_bp)
      ? &EP_MEM.cdi_data[VCP_TXD_BUFFSIZE]
      : &EP_MEM.cdi_data[0];
    _buf[_send_count++] = _c;
    if (_send_count < VCP_TXD_BUFFSIZE) _sof_count = VCP_BULK_DELAY;
    else ep_cdi_listen();
  }

  uint8_t read_byte (void) {
    uint8_t _c = 0;
    if (EP_CDO.CNT != _recv_count) {
  #if defined(DEBUG) && (DEBUG >= 2)
      if (_recv_count == 0) {
        D2PRINTF(" VO=%02X\r\n    ", EP_CDO.CNT);
        D2PRINTHEX(&EP_MEM.cdo_data, EP_CDO.CNT, ':', 16, "\r\n    ");
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
      size_t _size = get_descriptor((uint8_t*)&EP_MEM.res_data, EP_MEM.req_data.wValue, _length);
      EP_RES.CNT = constrain(_size, (size_t)0, _length);
    #if defined(DEBUG)
      size_t _count = EP_RES.CNT;
      size_t _step = _count;
      uint16_t _req = EP_MEM.req_data.wValue;
      uint8_t *_buff = EP_MEM.res_data;
      while (_count > 0) {
        if (_req == 0x200) {
          _step = _buff[0];
          if (_count < _step) _step = _count;
        }
        D1PRINTF(" ");
        D1PRINTHEX(_buff, _step, ':', 16, "\r\n ");
        _buff += _step;
        _count -= _step;
      }
    #endif
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
      D1PRINTHEX(&_set_line_encoding, sizeof(LineEncoding_t), ':');
      bit_set(GPCONF, GPCONF_OPN_bp);
      _send_count = 0;
      _recv_count = 0;
    #if defined(CONFIG_VCP_INTERRUPT_SUPPRT)
      _sof_count = SERIALSTATE_DELAY;
    #endif
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
      D1PRINTHEX(&EP_MEM.res_encoding, sizeof(LineEncoding_t), ':');
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
    #if defined(CONFIG_VCP_INTERRUPT_SUPPRT)
      _sof_count = SERIALSTATE_DELAY;
    #endif
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
    EVSYS_SWEVENTA = EVSYS_SWEVENTA_CH2_gc;
  #if defined(DEBUG)
    if (EP_MEM.req_data.wRequestType == 0x0680 && EP_MEM.req_data.wValue == 0x0600) ep_setup_stalled();
  #endif
    D1PRINTF("RQ=%04X:%02X:%02X:%04X:%04X:%04X\r\n",
      EP_REQ.CNT, EP_MEM.req_data.bmRequestType, EP_MEM.req_data.bRequest,
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
    if (!_listen) ep_setup_stalled();
    ep_res_listen();
    ep_req_listen();
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
      usb_detach();
      SYS::reboot();
    }
  #endif
  #if defined(DEBUG) && (DEBUG >= 2)
    if (bit_is_set(busstate, USB_RESET_bp)) {
      D1PRINTF("<RESET>\r\n");
    }
    if (bit_is_set(busstate, USB_SUSPEND_bp)) {
      D1PRINTF("<SUSPEND>\r\n");
    }
    if (bit_is_set(busstate, USB_RESUME_bp)) {
      D1PRINTF("<RESUME>\r\n");
    }
  #endif
    if (bit_is_set(busstate, USB_STALLED_bp)) {
      bit_clear(EP_RES.CTRL, USB_DOSTALL_bp);
      D1PRINTF("<STALLED>");
    }
    if (bit_is_set(busstate, USB_SOF_bp)) {
      /* If there is deferred data for a block transfer, it is sent here. */
      if (_sof_count > 0 && 0 == (--_sof_count)) {
        if (bit_is_set(EP_CDI.STATUS, USB_BUSNAK_bp) && _send_count > 0) {
          ep_cdi_listen();
        }
        cci_interrupt();
      }
    }
    if (bit_is_set(busstate, USB_SUSPEND_bp)
     || bit_is_set(busstate, USB_RESUME_bp)) {
      /* This implementation does not transition to power saving mode. */
      /* This is only passed when the USB cable is unplugged. */
      if (bit_is_set(GPCONF, GPCONF_USB_bp)) {
        /* System reboot */
        usb_detach();
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

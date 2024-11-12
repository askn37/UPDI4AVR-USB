/**
 * @file main.cpp
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
#include <stddef.h>
#ifndef F_CPU
  #define F_CPU 20000000L
#endif
#include "api/macro_api.h"  /* interrupts, initVariant */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

namespace /* NAMELESS */ {

  /* SYS */
  NOINIT jmp_buf TIMEOUT_CONTEXT;
  uint8_t _led_mode = 0;

  /* USB */
  alignas(2) NOINIT EP_TABLE_t EP_TABLE;
  alignas(2) NOINIT EP_DATA_t EP_MEM;
  NOINIT Device_Desc_t Device_Descriptor;

  /* Vertual Communication Port */
#if defined(CONFIG_VCP_9BIT_SUPPORT)
  NOINIT void (*usart_receiver)(void);
  NOINIT void (*usart_transmitter)(void);
#endif
  LineEncoding_t _set_line_encoding;
  LineState_t    _set_line_state;
  NOINIT uint16_t _send_break;
  NOINIT volatile uint8_t _send_count;
  NOINIT uint8_t _recv_count;
  NOINIT uint8_t _set_config;
  NOINIT volatile uint8_t _sof_count;
  NOINIT uint8_t _set_serial_state;

  /* JTAG packet payload */
  NOINIT JTAG_Packet_t packet;
  NOINIT size_t  _packet_length;
  NOINIT uint8_t _packet_fragment;
  NOINIT uint8_t _packet_chunks;
  NOINIT uint8_t _packet_endfrag;

  /* JTAG parameter */
  NOINIT uint32_t _before_page;
  NOINIT uint16_t _vtarget;           /* LSB=1/1000V <- SYS::get_vdd() */
  NOINIT uint16_t _xclk, _xclk_bak;   /* LSB=1KHz */
  NOINIT uint8_t _jtag_vpow;
  NOINIT uint8_t _jtag_hvctrl;
  NOINIT uint8_t _jtag_unlock;
  NOINIT uint8_t _jtag_arch;
  NOINIT uint8_t _jtag_sess;
  NOINIT uint8_t _jtag_conn;

  /* UPDI parameter */
  NOINIT Command_Table_t Command_Table;
  NOINIT uint8_t _sib[32];

  /* TPI parameter */
  NOINIT uint8_t _tpi_cmd_addr;
  NOINIT uint8_t _tpi_csr_addr;
  NOINIT uint8_t _tpi_chunks;

} /* NAMELESS */;

__attribute__((used, naked, section(".init3")))
void setup_mcu (void) { initVariant(); }

int main (void) {

  SYS::setup();
  Timeout::setup();

#if defined(DEBUG)
  Serial.begin(CONSOLE_BAUD);
  delay_millis(600);
  D1PRINTF("\n<startup>\r\n");
  D1PRINTF("F_CPU = %ld\r\n", F_CPU);
  D1PRINTF("_AVR_IOXXX_H_ = " _AVR_IOXXX_H_ "\r\n");
  D1PRINTF("__AVR_ARCH__ = %d\r\n", __AVR_ARCH__);
  DFLUSH();
#endif

  USART::setup();

  loop_until_bit_is_clear(WDT_STATUS, WDT_SYNCBUSY_bp);
  _PROTECTED_WRITE(WDT_CTRLA, WDT_PERIOD_1KCLK_gc);

  #if defined(PIN_SYS_SW0)
  /* Clear the dirty flag before enabling interrupts. */
  vportRegister(PIN_SYS_SW0).INTFLAGS = ~0;
  CCL_INTFLAGS = ~0;
  #endif
  interrupts();

  #if !defined(PIN_SYS_VDETECT)
  /* If you do not use VBD, insert the shortest possible delay instead. */
  SYS::delay_125ms();
  SYS::delay_125ms();
  USB::setup_device(true);
  #else
  SYS::LED_Flash();
  #endif

  /* From here on, it's an endless loop. */
  D1PRINTF("<WAITING>\r\n");
  bool _wdt = true;
  while (true) {
    if (_wdt) wdt_reset();

    /*** USB control handling ***/
    USB::handling_bus_events();
    if (USB::is_ep_setup()) USB::handling_control_transactions();

    /* If SW0 was used, work here. */
    if (bit_is_clear(PGCONF, PGCONF_UPDI_bp)) {
      if      (bit_is_set(GPCONF, GPCONF_FAL_bp)) SYS::reset_enter();
      else if (bit_is_set(GPCONF, GPCONF_RIS_bp)) SYS::reset_leave();
    }

    /* If the USB port is not open, go back to the loop beginning. */
    if (bit_is_clear(GPCONF, GPCONF_USB_bp)) continue;

    /*** CMSIS-DAP VCP transceiver ***/
    /* The AVR series requires at least 100 clocks to service   */
    /* an interrupt. At the maximum speed of the VCP-RxD, one   */
    /* character arrives every 400 clocks on a 20MHz reference. */
    /* So we avoid using interrupts here and use polling to gain speed. */
  #if defined(CONFIG_VCP_9BIT_SUPPORT)
    if (bit_is_set(GPCONF, GPCONF_VCP_bp)) usart_transmitter();
  #else
    if (bit_is_set(GPCONF, GPCONF_VCP_bp)) USB::vcp_transceiver();
  #endif
    else USB::read_drop();

    /*** If the break value is between 1 and 65534, it will count down. ***/
    if (bit_is_set(GPCONF, GPCONF_BRK_bp)) USB::cci_break_count();

    /*** If CMSIS-DAP is not received, return to the top. ***/
    if (USB::is_not_dap()) {
      /* To force exit from a non-responsive terminal mode, press SW0. */
      if (bit_is_set(PGCONF, PGCONF_PROG_bp)) {
        if (bit_is_set(GPCONF, GPCONF_RIS_bp)) _wdt = false;
        bit_clear(GPCONF, GPCONF_RIS_bp);
        /* If no response is received for more than 1 second, a WDT reset will fire. */
      }
      continue;
    }
    _wdt = true;

    /*** CMSIS-DAP and JTAG3 packet receiver ***/
    if (JTAG::dap_command_check()) JTAG::jtag_scope_branch();
  }

}

// end of code

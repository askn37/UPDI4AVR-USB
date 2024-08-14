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
  NOINIT uint16_t _vtarget;         /* LSB=1V/1000 <- SYS::get_vdd() */
  uint16_t _xclk = UPDI_CLK / 1000; /* LSB=1KHz using USART_CMODE_SYNCHRONOUS_gc */
  uint8_t _jtag_vpow = 1;
  NOINIT uint8_t _jtag_hvctrl;
  NOINIT uint8_t _jtag_unlock;
  NOINIT uint8_t _jtag_arch;
  NOINIT uint8_t _jtag_sess;
  NOINIT uint8_t _jtag_conn;

  /* UPDI parameter */
  NOINIT Command_Table_t Command_Table;
  NOINIT uint8_t _sib[32];

  /* TPI parameter */
  NOINIT uint8_t _tpi_setmode;
  NOINIT uint8_t _tpi_cmd_addr;
  NOINIT uint8_t _tpi_csr_addr;
  NOINIT uint8_t _tpi_chunks;

} /* NAMELESS */;

__attribute__((used, naked, section(".init3")))
void setup_mcu (void) { initVariant(); }

int main (void) {

#if defined(DEBUG)
  Serial.begin(CONSOLE_BAUD).println(F("\n<startup>"));
  Serial.print(F("F_CPU = ")).println(F_CPU, DEC);
  Serial.print(F("_AVR_IOXXX_H_ = ")).println(F(_AVR_IOXXX_H_));
  Serial.print(F("__AVR_ARCH__ = ")).println(__AVR_ARCH__, DEC);
#endif

  SYS::setup();
  Timeout::setup();
  USART::setup();

  loop_until_bit_is_clear(WDT_STATUS, WDT_SYNCBUSY_bp);
  _PROTECTED_WRITE(WDT_CTRLA, WDT_PERIOD_1KCLK_gc);

  #if defined(PIN_SYS_SW0)
  /* Clear the dirty flag before enabling interrupts. */
  vportRegister(PIN_SYS_SW0).INTFLAGS = ~0;
  CCL_INTFLAGS = ~0;
  #endif
  interrupts();

  #if !defined(PIN_USB_VDETECT)
  /* If you do not use VBD, insert the shortest possible delay instead. */
  delay_millis(250);
  USB::setup_device(true);
  #else
  SYS::LED_Flash();
  #endif

  /* From here on, it's an endless loop. */
  D1PRINTF("<WAITING>\r\n");
  while (true) {
    wdt_reset();

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
    if (USB::is_not_dap()) continue;

    /*** CMSIS-DAP and JTAG3 packet receiver ***/
    if (JTAG::dap_command_check()) JTAG::jtag_scope_branch();
  }

}

// end of code

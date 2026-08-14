/**
 * @file sys.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.34.49+
 * @date 2026-08-09
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include <peripheral.h>     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

namespace SYS {

  const uint8_t _updi_bitmap_reset[] = {  /* LSB First */
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, /* BREAK IDLE */
    0x55, 0x7E, 0xC8, 0x7F, 0x59, 0xFE, 0xFF  /* SYSRST */
  };
  const uint8_t _updi_bitmap_leave[] = {  /* LSB First */
    0x7F, 0x55, 0x7E, 0xC8, 0x7F, 0x00, 0xFE, /* SYSRST */
    0x7F, 0x55, 0x7E, 0xC3, 0x7E, 0x04, 0xFF  /* UPDIDIS */
  };

  /*
   * Target Reset
   */

  WEAK void power_reset (bool _off, bool _on) {
    if (_off) {
  #ifdef CONFIG_PGM_VPOWER_ENABLE
      digitalWriteMacro(PIN_PGM_VPOWER, HIGH);  /* VTG off */
      /* Temporarily disable the pullup to stop current leakage when VTG=OFF. */
      /* It would be easier to just set the pin output LOW,                   */
      /* but we do it this way because of possible conflicts.                 */
      pinControlRegister(PIN_PGM_TRST) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TDAT) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_VCP_TXD)  &= ~PORT_PULLUPEN_bm;  /* internal shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  &= ~PORT_PULLUPEN_bm;
  #endif
    }
    if (_on) {
  #ifdef CONFIG_PGM_VPOWER_ENABLE
      if (_off) delay_125ms();  /* discharge duration */
      digitalWriteMacro(PIN_PGM_VPOWER, LOW);   /* VTG on */
      pinControlRegister(PIN_VCP_TXD)  |= PORT_PULLUPEN_bm;   /* internal shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TDAT) |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TRST) |= PORT_PULLUPEN_bm;
  #endif
    }
  }

  /*** Low level TDAT stream manipulation ***/
  /* UPDI commands are sent from TDAT using only TCAB
     and bit manipulation, without switching USART. */
  void send_bitmap (const uint8_t _bitmap[], const size_t _length) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TCB0_CNT      = 0;
      TCB0_INTCTRL  = 0;
      TCB0_INTFLAGS = ~0;
      TCB0_CCMP     = F_CPU / 125000L;
      TCB0_CTRLA    = TCB_ENABLE_bm | TCB_CLKSEL_DIV1_gc;
    }
    for (uint8_t i = 0; i < _length; i++) {
      uint8_t _d = (_bitmap[i >> 3]) >> (i & 7);
      loop_until_bit_is_set(TCB0_INTFLAGS, TCB_CAPT_bp);
      if (bit_is_set(_d, 0))
        pinLogicOpen(PIN_PGM_TDAT);
      else
        pinLogicPull(PIN_PGM_TDAT);
      bit_set(TCB0_INTFLAGS, TCB_CAPT_bp);
      wdt_reset();
    }
    TCB0_CTRLA = 0;
  }

  /*
   * Executed when SW0 is detected as being pressed.
   * May be called multiple times due to chattering.
   */
  void reset_enter (void) {
    if (bit_is_clear(GPCONF, GPCONF_HLD_bp)) {
      LED_Blink();
      pinLogicPull(PIN_PGM_TRST);
      /*
      * Puts a tinyAVR-0 which does not have a reset pad into reset state.
      * This applies to all chips which have an enabled UPDI pad.
      * Does not affect chips with an active reset pad or TPI/PDI type chips.
      */
      send_bitmap(_updi_bitmap_reset, sizeof(_updi_bitmap_reset) * 8);
      D1PRINTF("<RST:IN>\r\n");
      DFLUSH();
      bit_set(GPCONF, GPCONF_HLD_bp);
    }
    bit_clear(GPCONF, GPCONF_FAL_bp);
  }

  /*
   * This will be executed when SW0 is released.
   * If the VCP is operating, it will return to normal operation,
   * but if the USB is stopped, it will reboot at the end.
   */
  void reset_leave (void) {
    if (bit_is_set(GPCONF, GPCONF_HLD_bp)) {
      send_bitmap(_updi_bitmap_leave, sizeof(_updi_bitmap_leave) * 8);
      pinLogicOpen(PIN_PGM_TRST);
  #ifdef CONFIG_VCP_DTR_RESET
      /* A delay of 64ms or more between when the bootloader starts and when RxD opens. */
      delay_125ms();
  #endif
      D1PRINTF("<RST:OUT>\r\n");
      DFLUSH();
      if (bit_is_set(GPCONF, GPCONF_USB_bp))
        LED_HeartBeat();  /* The USB is ready. */
      else reboot();      /* USB disconnected, System reboot. */
    }
    GPCONF &= ~(GPCONF_HLD_bm | GPCONF_RIS_bm | GPCONF_FAL_bm);
  }

  /*
   * System reboot
   *
   * Always run it after the USB has stopped.
   */
  void reboot (void) {
    D1PRINTF("<REBOOT>\r\n");
    DFLUSH();
    _PROTECTED_WRITE(WDT_CTRLA, WDT_PERIOD_8CLK_gc);
    for (;;);
  }

  /*
   * Flash memory boundary check
   *
   * True when a page area address different from the previous time is detected.
   * Indicates the need to erase a page in FLASH/USERROW/BOOTROW of the AVR-Dx series.
   */
  bool is_boundary_flash_page (uint32_t _dwAddr) {
    uint32_t _mask = ~(((uint16_t)Device_Descriptor.UPDI.flash_page_size_msb << 8)
                                + Device_Descriptor.UPDI.flash_page_size - 1);
    uint32_t _after_page = _dwAddr & _mask;
    bool _result = _before_page != _after_page;
    _before_page = _after_page;
    return _result;
  }

  /*
   * Measure self operating voltage.
   *
   * Vdd/10 goes into MUXPOS and is divided by the internal reference voltage of 1.024V.
   * A delay of 1250us is required for the voltage to stabilize.
   * The result is 10-bit, so multiply by 10.0 to convert to 1V * 0.0001.
   * The ADC0 peripheral is operational only during voltage measurements.
   */
  uint16_t get_vdd (void) {
    CLKCTRL_MCLKTIMEBASE = F_CPU / 1000000.0;
    ADC0_INTFLAGS = ~0;
    ADC0_SAMPLE = 0;
    ADC0_CTRLA = ADC_ENABLE_bm;
    ADC0_CTRLB = ADC_PRESC_DIV8_gc;
    ADC0_CTRLC = ADC_REFSEL_1V024_gc;
    ADC0_CTRLE = 250; /* (SAMPDUR + 0.5) * fCLK_ADC sample duration */
    ADC0_MUXPOS = ADC_MUXPOS_VDDDIV10_gc; /* ADC channel VDD * 0.1 */
    loop_until_bit_is_clear(ADC0_STATUS, ADC_ADCBUSY_bp);
    ADC0_COMMAND = ADC_MODE_SINGLE_10BIT_gc | ADC_START_IMMEDIATE_gc;
    loop_until_bit_is_set(ADC0_INTFLAGS, ADC_SAMPRDY_bp);
    uint16_t _adc_reading = ADC0_SAMPLE;
    _adc_reading += (_adc_reading << 3) + _adc_reading;
    ADC0_CTRLA = 0;
    return _adc_reading;
  }

  void hvc_enable (void) {
  #ifdef CONFIG_HVC_ENABLE
    TCA0_SPLIT_CTRLA = 0;
    TCA0_SPLIT_CTRLB = TCA_SPLIT_HCMP2EN_bm | TCA_SPLIT_HCMP1EN_bm;
    TCA0_SPLIT_HCMP1 = F_CPU / HVC_CLK / 2;
    TCA0_SPLIT_HCMP2 = F_CPU / HVC_CLK / 2;
    TCA0_SPLIT_HPER = (F_CPU / HVC_CLK) - 1;
    TCA0_SPLIT_HCNT = 0;
    TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1_gc;
    delay_100us();
  #endif
  }

  void hvc_leave (void) {
  #ifdef CONFIG_HVC_ENABLE
    TCA0_SPLIT_CTRLB = 0;
    TCA0_SPLIT_CTRLA = 0;
  #endif
  }

  void delay_55us (void) {
    delay_micros(55);
  }

  void delay_100us (void) {
    delay_micros(100);
  }

  void delay_800us (void) {
    delay_micros(800);
  }

  void delay_2500us (void) {
    delay_micros(2500);
  }

  void delay_125ms (void) {
    delay_millis(125);
  }

};  /* SYS */

#if defined(PIN_SYS_SW0)
/* If the level is not maintained for a sufficient period of time it will not function properly. */
ISR(portIntrruptVector(PIN_SYS_SW0)) {
  /* SW0 Raising Interrupt */
  vportRegister(PIN_SYS_SW0).INTFLAGS = ~0;
  bit_set(GPCONF, GPCONF_RIS_bp);
  D2PRINTF("{R}");
}

ISR(CCL_CCL_vect) {
  /* SW0 Falling Intrrupt from CCLn */
  CCL_INTFLAGS = ~0;
  bit_set(GPCONF, GPCONF_FAL_bp);
  D2PRINTF("{F}");
}
#endif

// end of code

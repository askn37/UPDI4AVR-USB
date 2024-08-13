/**
 * @file sys.cpp
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
#include <math.h>           /* sqrt() */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*** LED Timer configuration ***/
#define HBEAT_HZ   (0.5)  /* Periodic 0.5Hz */
#define TCA0_STEP  ((uint8_t)(sqrt((F_CPU / 1024.0) * (1.0 / HBEAT_HZ)) - 0.5))
#define TCA0_128K  (F_CPU / 128000L)
#define TCA0_225K  (F_CPU / 225000L)
#define TCB1_HBEAT (((TCA0_STEP /  2) << 8) + (TCA0_STEP - 1))
#define TCB1_STEP  (170)
#define TCB1_BLINK (((TCB1_STEP /  2) << 8) + (TCB1_STEP - 1))
#define TCB1_FLASH (((TCB1_STEP / 34) << 8) + (TCB1_STEP - 1))
#define TCB1_FAST  (((TCB1_STEP / 10) << 8) + (TCB1_STEP / 5))

namespace SYS {

  /* Since this array will have rewritten parameters, place it in the SRAM area. */
  uint8_t _updi_bitmap[] = { /* LSB First */
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, /* BREAK IDLE */
    0x55, 0x7E, 0xC8, 0x7F, 0x59, 0xFE, /* SYSRST */
    0x55, 0x7E, 0xC3, 0x7E, 0x04, 0xFF  /* UPDIDIS */
  };

  void setup (void) {

    /*
     * GPIO - VCP
     */

  #if defined(PIN_VCP_DTR)
    pinModeMacro(PIN_VCP_DTR, OUTPUT);
  #endif
  #if defined(PIN_VCP_RTS)
    pinModeMacro(PIN_VCP_RTS, OUTPUT);
  #endif
  #if defined(PIN_VCP_CTS) && defined(CONFIG_VCP_CTS_ENABLE)
    pinControlRegister(PIN_VCP_CTS) = PORT_PULLUPEN_bm;
  #endif
  #if defined(PIN_VCP_DCD)
    pinControlRegister(PIN_VCP_DCD) = PORT_PULLUPEN_bm;
  #endif
  #if defined(PIN_VCP_DSR)
    pinControlRegister(PIN_VCP_DSR) = PORT_PULLUPEN_bm;
  #endif
  #if defined(PIN_VCP_RI)
    pinControlRegister(PIN_VCP_RI) = PORT_PULLUPEN_bm;
  #endif

    /*
     * GPIO - HVCTRL
     */

  #if defined(PIN_HV_SELECT)
    /* HV control selection: Output 0=TRST, 1=TDAT */
    pinModeMacro(PIN_HV_SELECT, OUTPUT);
  #endif
  #if defined(PIN_HV_CHGPUMP)
    /* Charge pump pulse: TCA0_WO4 output */
    pinModeMacro(PIN_HV_CHGPUMP, OUTPUT);
  #endif
  #if defined(PIN_HV_FEEDBACK)
    /* Charge pump feedback: AC0 Input positive logic. */
    /* No pull-up. Compared with DAC setting voltage.  */
    pinControlRegister(PIN_HV_FEEDBACK) = 0;
  #endif
  #if defined(PIN_HV_SWITCH)
    /* Charge pump voltage control; AC0 Output negative logic */
    pinControlRegister(PIN_HV_SWITCH) = PORT_INVEN_bm;
    pinModeMacro(PIN_HV_SWITCH, OUTPUT);
  #endif
  #if defined(PIN_HV_POWER)
    /* Device power control: output positive logic */
    pinModeMacro(PIN_HV_POWER, OUTPUT);
    digitalWriteMacro(PIN_HV_POWER, HIGH);
  #endif

    /*
     * GPIO - USB
     */

  #if defined(PIN_USB_VDETECT)
    pinControlRegister(PIN_USB_VDETECT) = PORT_PULLUPEN_bm;
  #endif

    /*
     * GPIO - SYS
     */

  #if defined(PIN_SYS_SW0)
    /* SW0 detection: Input negative logic. */
    pinControlRegister(PIN_SYS_SW0) = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
  #endif

    /*
     * GPIO - LED
     */

  #undef LED_CCL
  #if (CONFIG_HAL_TYPE == HAL_BAREMETAL_14P)
    /* PORTMUX LUT0OUT -> PIN_PC3 */
    /* TRUTH0: 001 010 is ON */
    CCL_TRUTH0    = CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_TCB1_gc;
    CCL_LUT0CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm; /* PIN_PC3 */
  #elif (CONFIG_HAL_TYPE == HAL_CNANO)
    /* PORTMUX LUT0OUT -> EVSYSOUTF -> PIN_PF2 Invert */
    pinControlRegister(PIN_PF2) = PORT_INVEN_bm;
    EVSYS_CHANNEL0 = EVSYS_CHANNEL_CCL_LUT0_gc;
    EVSYS_USEREVSYSEVOUTF = EVSYS_USER_CHANNEL0_gc;
    /* TRUTH0: 000 001 010 011 is ON */
    CCL_TRUTH0    = CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_TCB1_gc;
    CCL_LUT0CTRLA = CCL_ENABLE_bm;
  #else
    /* PORTMUX LUT1OUT -> PIN_PA6 */
    PORTMUX_CCLROUTEA = PORTMUX_LUT0_ALT1_gc;
    /* TRUTH1: 001 010 is ON */
    CCL_TRUTH1    = CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT1CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_TCB1_gc;
    CCL_LUT1CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm; /* PIN_PA6 */
  #endif

    /*** CCL enable ***/
    /* One of the CCL's is the LED output control. */
    CCL_CTRLA = CCL_RUNSTDBY_bm | CCL_ENABLE_bm;

    /*** TCA0 ***/
    /* TCA0 is split into two 8-bit timers. */
    /* The lower timer controls the blinking rate of the LED. */
    /* The top timer is used as a period timer */
    /* and as the output for the charge pump.  */
    TCA0_SPLIT_CTRLD = TCA_SPLIT_SPLITM_bm;
    TCA0_SPLIT_LPER  = TCA0_STEP - 2;
    TCA0_SPLIT_LCMP0 = TCA0_STEP / 2;

    /*** TCB0 ***/
    /* The TCB0 timer is configured in the <timeout.cpp> module. */

    /*** TCB1 ***/
    /* TCB1 is used to control the LED blinking rate. */
    TCB1_CTRLB = TCB_ASYNC_bm | TCB_CNTMODE_PWM8_gc;
    TCB1_CCMP  = TCB1_FLASH;
    TCB1_CTRLA = TCB_RUNSTDBY_bm | TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc;

  }

  /*
  * LED operation switching
  */

  /* Heartbeat (waiting) */
  void LED_HeartBeat (void) {
    if (_led_mode != 1) {
      TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1024_gc;
      TCB1_CNTL = 0;
      TCB1_CCMP = TCB1_HBEAT;
      TCB1_CTRLA = TCB_RUNSTDBY_bm | TCB_ENABLE_bm | TCB_CLKSEL_TCA0_gc;
      _led_mode = 1;
    }
  }

  void LED_TCB1 (uint8_t _mode, uint16_t _ccmp) {
    if (_led_mode != _mode) {
      TCA0_SPLIT_CTRLA = 0;
      TCB1_CNTL = 0;
      TCB1_CCMP = _ccmp;
      TCB1_CTRLA = TCB_RUNSTDBY_bm | TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc;
      _led_mode = _mode;
    }
  }

  /* USB uplink is disabled indicator. */
  void LED_Flash (void) {
    /*
     * AVR-DU Errata?
     * Restarting TCBn in PWM8 mode may cause the duty
     * cycle of TCBn_WO to invert. Solution unknown.
     * This function is affected.
     */
    LED_TCB1(2, TCB1_FLASH);
  }

  /* SW0 is pressed indicator. */
  void LED_Blink (void) {
    LED_TCB1(3, TCB1_BLINK);
  }

  /* Programming in progress indicator. */
  void LED_Fast (void) {
    LED_TCB1(4, TCB1_FAST);
  }

  /*
   * Target Reset
   */

  /*** Low level TDAT stream manipulation ***/
  /* UPDI commands are sent from TDAT using only TCA0 and bit manipulation, without switching USART. */
  /* 128kbps is the lowest limit that can be achieved with an 8-bit timer at 32MHz or less. */
  void send_bitmap (const uint8_t _bitmap[], const size_t _length) {
    TCA0_SPLIT_HPER  = TCA0_225K;
    TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1_gc;
    for (uint8_t i = 0; i < _length; i++) {
      uint8_t _d = (_bitmap[i >> 3]) >> (i & 7);
      loop_until_bit_is_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_HUNF_bp);
      if (bit_is_set(_d, 0))
        openDrainWriteMacro(PIN_PG_TDAT, HIGH);
      else
        openDrainWriteMacro(PIN_PG_TDAT, LOW);
      bit_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_HUNF_bp);
    }
    TCA0_SPLIT_CTRLA = 0;
  }

  /*
   * Executed when SW0 is detected as being pressed.
   * May be called multiple times due to chattering.
   */
  void reset_enter (void) {
    if (bit_is_set(PGCONF, PGCONF_UPDI_bp)) return;
    LED_Blink();
    if (_jtag_arch == 5) {
      /* 
       * Puts a tinyAVR-0 which does not have a reset pad into reset state.
       * This applies to all chips which have an enabled UPDI pad.
       * Does not affect TPI/PDI types.
       */
      _updi_bitmap[10] = 0x59; /* SYSRST */
      send_bitmap(_updi_bitmap, sizeof(_updi_bitmap) * 8);
    }
    openDrainWriteMacro(PIN_PG_TRST, LOW);
    D1PRINTF("<RST:IN>\r\n");
    bit_clear(GPCONF, GPCONF_FAL_bp);
  }

  /*
   * This will be executed when SW0 is released.
   * If the VCP is operating, it will return to normal operation,
   * but if the USB is stopped, it will reboot at the end.
   */
  void reset_leave (void) {
    if (bit_is_set(PGCONF, PGCONF_UPDI_bp)) return;
    if (_jtag_arch == 5) {
      _updi_bitmap[10] = 0x00; /* SYSRUN */
      send_bitmap(_updi_bitmap, sizeof(_updi_bitmap) * 8);
    }
    openDrainWriteMacro(PIN_PG_TRST, HIGH);
  #ifdef CONFIG_VCP_DTR_RESET
    /* A delay of 64ms or more between when the bootloader starts and when RxD opens. */
    delay_millis(100);
  #endif
    D1PRINTF("<RST:OUT>\r\n");
    if (bit_is_set(GPCONF, GPCONF_USB_bp))
      LED_HeartBeat();  /* The USB is ready. */
    else if (!USB0_ADDR)
      reboot();         /* USB disconnected, System reboot. */
    else
      LED_Flash();      /* USB is not yet enabled. */
    bit_clear(GPCONF, GPCONF_FAL_bp);
    bit_clear(GPCONF, GPCONF_RIS_bp);
  }

  /*
   * System reboot
   *
   * Always run it after the USB has stopped.
   */
  void reboot (void) {
  #if defined(DEBUG)
    D0PRINTF("<REBOOT>\r\n");
    Serial.flush();
  #endif
    _PROTECTED_WRITE(RSTCTRL_SWRR, 1);
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
    ADC0_INTFLAGS = ~0;
    ADC0_SAMPLE = 0;
    ADC0_CTRLA = ADC_ENABLE_bm;
    ADC0_CTRLB = ADC_PRESC_DIV4_gc;
    ADC0_CTRLC = ADC_REFSEL_1V024_gc;
    ADC0_CTRLE = 250; /* (SAMPDUR + 0.5) * fCLK_ADC sample duration */
    ADC0_MUXPOS = ADC_MUXPOS_VDDDIV10_gc; /* ADC channel VDD * 0.1 */
    ADC0_COMMAND = ADC_MODE_SINGLE_10BIT_gc | ADC_START_IMMEDIATE_gc;
    loop_until_bit_is_set(ADC0_INTFLAGS, ADC_SAMPRDY_bp);
    uint16_t _adc_reading = ADC0_SAMPLE;
    _adc_reading += (_adc_reading << 3) + _adc_reading;
    ADC0_CTRLA = 0;
    return _adc_reading;
  }

};

#if defined(PIN_SYS_SW0)
ISR(portIntrruptVector(PIN_SYS_SW0)) {
  vportRegister(PIN_SYS_SW0).INTFLAGS = ~0;
  /* If the level is not maintained for a sufficient period of time it will not function properly. */
  if (bit_is_clear(PGCONF, PGCONF_UPDI_bp)) {
    if (digitalReadMacro(PIN_SYS_SW0))
      bit_set(GPCONF, GPCONF_RIS_bp);
    else
      bit_set(GPCONF, GPCONF_FAL_bp);
  }
}
#endif

// end of code

/**
 * @file AVRDU_20P.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.35.49+
 * @date 2026-08-09
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include <variant.h>
#include "../configuration.h"
#ifdef HAL_AVRDU_20P
#include "../prototype.h"

namespace SYS {

  void setup (void) {

    /*
     * Before reaching this point,
     * `PORT<ALL>.PINCONFIG = PORT_ISC_INPUT_DISABLE_gc`
     * is already executed.
     *
     * VCP control: Initial values ​​are all open-drain.
     *
     * PGM control: Initial values ​​are all open-drain.
     * TCLK is changed to push-pull when in use.
     *
     * SW0 detection: Input negative logic.
     * Use CCL in conjunction to separate the falling edge and rising edge interrupts.
     * For the falling edge, use the CCL filter function to remove chattering noise.
     */

    /* HV-control is not available in this package. */

    /* Output GPIO */
    VPORTA_DIR = 0b10100000;    /* 7:HVSL2 5;HVSL1 */
    VPORTD_DIR = 0b10110000;    /* 7:HVSL3 5:HVCP2 4:HVCP1 */

    /* Pull-Up GPIO */
    /* Grouping consecutive lines that assign the same value results in shorter code. */
    pinControlRegister(PIN_VCP_TXD)      = PORT_PULLUPEN_bm;
    pinControlRegister(PIN_VCP_RXD)      = PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TDAT)     = PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TRST)     = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc;
  #ifdef CONFIG_PGM_PDI_ENABLE
    pinControlRegister(PIN_PGM_PDAT)     = 0;
    pinControlRegister(PIN_PGM_PCLK)     = PORT_ISC_INPUT_DISABLE_gc;
  #endif
    pinControlRegister(PIN_SYS_SW0)      = PORT_PULLUPEN_bm | PORT_ISC_RISING_gc;
    pinControlRegister(PIN_HVC_CHGPUMP1) = PORT_INVEN_bm    | PORT_ISC_INPUT_DISABLE_gc;
    /* PCLK disable/output is shared internal connection with TRST */

    /* PORTx event generator */
  #if ((PIN_SYS_SW0 & 0xE0) == (PIN_VCP_RXD & 0xE0))
    portRegister(PIN_SYS_SW0).EVGENCTRLA = pinPosition(PIN_SYS_SW0)       /* EVG0 */
                                         | pinPosition(PIN_VCP_RXD) << 4; /* EVG1 */
  #else
    portRegister(PIN_SYS_SW0).EVGENCTRLA = pinPosition(PIN_SYS_SW0);      /* EVG0 */
    portRegister(PIN_VCP_RXD).EVGENCTRLA = pinPosition(PIN_VCP_RXD) << 4; /* EVG1 */
  #endif

    /*** Port Multiplexer ***/
    PORTMUX_CCLROUTEA     = PORTMUX_LUT2_ALT1_gc;           /* CCL2_OUT_ALT1 -> PIN_PD6 */
    PORTMUX_TCAROUTEA     = PORTMUX_TCA0_PORTD_gc;          /* TCA0_WOn_ALT3 -> PORTD */

    /*** Event System ***/
    EVSYS_CHANNEL0        = EVSYS_CHANNEL_RTC_EVGEN0_gc;    /* 1024Hz periodic.  */
    EVSYS_CHANNEL1        = EVSYS_CHANNEL_RTC_EVGEN1_gc;    /* 256Hz periodic.   */
    EVSYS_CHANNEL2        = EVSYS_CHANNEL_CCL_LUT1_gc;      /* <- LED1 */
    EVSYS_CHANNEL4        = EVSYS_CHANNEL_PORTX_EVGEN1(PIN_VCP_RXD);  /* <- VRxD */
    EVSYS_CHANNEL5        = EVSYS_CHANNEL_PORTX_EVGEN0(PIN_SYS_SW0);  /* <- SW0  */

    EVSYS_USERCCLLUT0A    = EVSYS_USER_CHANNEL5_gc;         /* <- SW0  */
    EVSYS_USERCCLLUT3A    = EVSYS_USER_CHANNEL4_gc;         /* <- VRxD */

    EVSYS_USERTCB1CAPT    = EVSYS_USER_CHANNEL2_gc;         /* TCB1_CAPT <- Strobe */
    EVSYS_USERTCB0COUNT   = EVSYS_USER_CHANNEL0_gc;         /* TCB0_CLK = 1024Hz */

    /*** CCL0 : SW0 FALLING Interrupt generator ***/
    /* The rising edge of SW0 triggers a PORTx interrupt,
       while the falling edge triggers a CCL interrupt. */
    CCL_TRUTH0    = CCL_TRUTH_1_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_EVENTA_gc;                   /* <- EVS_CH5 */
    CCL_LUT0CTRLA = CCL_ENABLE_bm | CCL_FILTSEL_FILTER_gc | CCL_CLKSRC_OSC32K_gc;
    CCL_INTCTRL0  = CCL_INTMODE0_FALLING_gc;

    /*** CCL3 : VCP Indicator ***/
    CCL_TRUTH3    = CCL_TRUTH_0_bm | CCL_TRUTH_1_bm;
    CCL_LUT3CTRLB = CCL_INSEL0_USART0_gc                    /* <- UART_TX */
                  | CCL_INSEL1_EVENTA_gc;                   /* <- EVS_CH4 : VRxD */
    CCL_LUT3CTRLA = CCL_ENABLE_bm;

    /*** CCL1 : LED1 (PC3:ORANGE) generator ***/
    /* It flashes at 256 Hz during VCP communication. */
    CCL_TRUTH1    = CCL_TRUTH_2_bm;
    CCL_LUT1CTRLB = CCL_INSEL1_TCB1_gc;                     /* <- TCB1_WO */
    CCL_LUT1CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm;           /* -> PIN_PC3 */

    /*** CCL2 : LED0 (PD6:GREEN) Heart-Beat generator ***/
    CCL_TRUTH2    = CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT2CTRLC = CCL_INSEL2_TCB1_gc;
    CCL_LUT2CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_EVENTA_gc;
    CCL_LUT2CTRLA = CCL_ENABLE_bm      | CCL_OUTEN_bm;      /* -> PIN_PD6 */

    /*** CCL enable ***/
    /* One of the CCL's is the LED output control. */
    CCL_CTRLA = CCL_ENABLE_bm;

    /*** TCA0 ***/
    /* TCA0 is split into two 8-bit timers. */
    /* The lower timer controls the blinking rate of the LED. */
    /* The top timer is used as a period timer */
    /* and as the output for the charge pump.  */

    /*** TCB0 ***/
    /* The TCB0 timer is configured in the sys and timeout module. */

    /*** TCB1 ***/
    /* TCB1 is used to control the LED blinking rate. */
    TCB1_CCMP   = TM_VCPBL;
    TCB1_EVCTRL = TCB_CAPTEI_bm;
    TCB1_CTRLB  = TCB_CNTMODE_SINGLE_gc;
    TCB1_CTRLA  = TCB_ENABLE_bm | TCB_CLKSEL_TCA0_gc;

    /*** PIT and RTC ***/
    /* EVG0 <- 1024Hz (32768/32) : EVG1 <- 128Hz (32768/256) */
    RTC_PITEVGENCTRLA = RTC_EVGEN0SEL_DIV32_gc | RTC_EVGEN1SEL_DIV128_gc;
    RTC_PITCTRLA = RTC_PITEN_bm;

    /*** VUSB Bus-Powerd ***/
    /* If you are supplying 3V3 to the VUSB pad from an
       external power source, you can comment this out.*/
    SYSCFG_VUSBCTRL = SYSCFG_USBVREG_bm;

    /* Voltage measurements may initially return erroneous values. */
    _vtarget = get_vdd();
  }

  /*
  * LED operation switching
  */

  /* Heartbeat (waiting) */
  void LED_HeartBeat (void) {
    if (_led_mode != 1) {
      TCA0_SPLIT_CTRLA = 0;
      D1PRINTF(" LED:HBEAT\r\n");
      TCA0_SPLIT_CTRLD = TCA_SPLIT_SPLITM_bm;       /* SINGLESLOPE PWM */
      TCA0_SPLIT_LPER  = TM_HBEAT;                  /* TOP WO[210] */
      TCA0_SPLIT_LCMP0 = TM_HBEAT >> 1;             /* CMP WO0 */
      TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1024_gc;
      EVSYS_USERCCLLUT2A = EVSYS_USER_CHANNEL1_gc;  /* <- for RTC 128Hz */
      _led_mode = 1;
    }
  }

  void LED_TM (uint8_t _mode, uint16_t _ccmp) {
    if (_led_mode != _mode) {
      EVSYS_USERCCLLUT2A = 0;   /* <- sognal in stop for TCA0_WO0 */
      TCA0_SINGLE_CTRLA = 0;
      D1PRINTF(" LED:MD=%d\r\n", _mode);
      TCA0_SINGLE_CTRLD = 0;
      TCA0_SINGLE_PER   = _ccmp;
      TCA0_SINGLE_CMP0  = _ccmp >> 1;
      TCA0_SINGLE_CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
      TCA0_SINGLE_CTRLA = TCA_SINGLE_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1024_gc;
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
    LED_TM(2, TM_FLASH);
  }

  /* SW0 is pressed indicator. */
  void LED_Blink (void) {
    LED_TM(3, TM_BLINK);
  }

  /* Programming in progress indicator. */
  void LED_Fast (void) {
    LED_TM(4, TM_FAST);
  }

  /* Response to holding down SW0 before
     USB communication is established. */
  void LED_Turn (void) {
    #ifdef PIN_SYS_SW0
    if (!digitalReadMacro(PIN_SYS_SW0)) {
      EVSYS_SWEVENTA = EVSYS_SWEVENTA_CH2_gc;
      GPCONF &= ~(GPCONF_HLD_bm | GPCONF_RIS_bm | GPCONF_FAL_bm);
    }
    #endif
  }

};  /* SYS */

#endif

// end of code


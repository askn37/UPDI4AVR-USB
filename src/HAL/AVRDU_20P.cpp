/**
 * @file AVRDU_20P.cpp
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

#include "../configuration.h"
#ifdef HAL_AVRDU_20P
#include "../prototype.h"

namespace SYS {

  WEAK void setup (void) {

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
    /* PDAT in/output is shared outside connection with TDAT */
    /* PCLK disable/output is shared internal connection with TRST */

    /* PORTx event generator */
    portRegister(PIN_SYS_SW0).EVGENCTRLA = pinPosition(PIN_SYS_SW0);
    portRegister(PIN_VCP_RXD).EVGENCTRLA = pinPosition(PIN_VCP_RXD) << 4;

    /*** Multiplexer ***/
    PORTMUX_CCLROUTEA     = PORTMUX_LUT2_ALT1_gc;           /* CCL2_OUT_ALT1 -> PIN_PD6 */
    PORTMUX_TCAROUTEA     = PORTMUX_TCA0_PORTD_gc;          /* TCA0_WOn_ALT3 -> PORTD */
    EVSYS_CHANNEL4        = EVSYS_CHANNEL_PORTA_EVGEN1_gc;  /* <- VRxD */
    EVSYS_CHANNEL5        = EVSYS_CHANNEL_PORTF_EVGEN0_gc;  /* <- SW0  */
    EVSYS_USERCCLLUT1A    = EVSYS_USER_CHANNEL4_gc;         /* <- VRxD */
    EVSYS_USERCCLLUT0A    = EVSYS_USER_CHANNEL5_gc;         /* <- SW0  */

    /*** SW0 FALLING Interrupt generator ***/
    CCL_TRUTH0    = CCL_TRUTH_1_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_EVENTA_gc;                   /* <- EVS_CH5 */
    CCL_LUT0CTRLA = CCL_ENABLE_bm | CCL_FILTSEL_FILTER_gc;
    CCL_INTCTRL0  = CCL_INTMODE0_FALLING_gc;

    /*** LED1 generator ***/
    CCL_TRUTH1    = CCL_TRUTH_0_bm       | CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT1CTRLB = CCL_INSEL0_USART0_gc | CCL_INSEL1_EVENTA_gc;  /* <- CH4 */
    CCL_LUT1CTRLA = CCL_ENABLE_bm        | CCL_OUTEN_bm;          /* -> PIN_PC3 */

    /*** LED0 Heart-Beat generator ***/
    CCL_TRUTH2    = CCL_TRUTH_1_bm     | CCL_TRUTH_2_bm;
    CCL_LUT2CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_TCB1_gc;
    CCL_LUT2CTRLA = CCL_ENABLE_bm      | CCL_OUTEN_bm;            /* -> PIN_PD6 */

    /*** CCL enable ***/
    /* One of the CCL's is the LED output control. */
    CCL_CTRLA = CCL_ENABLE_bm;

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
    TCB1_CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc;

    /* Voltage measurements may initially return erroneous values. */
    _vtarget = get_vdd();
  }

};  /* SYS */

#endif

// end of code


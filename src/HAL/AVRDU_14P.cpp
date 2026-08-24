/**
 * @file AVRDU_14P.cpp
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
#include <variant.h>
#include "../configuration.h"
#ifdef HAL_AVRDU_14P
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

    /* HV-control and PDI support is not available in this package. */

    /* Enables automatic adjustment of the OSCHF synchronized to the USB SOF. */
    uint8_t _t = CLKCTRL_OSCHFCTRLA | CLKCTRL_ALGSEL_bm | CLKCTRL_AUTOTUNE_SOF_gc;
    _PROTECTED_WRITE(CLKCTRL_OSCHFCTRLA, _t);

    /* Output GPIO */

    /* Pull-Up GPIO */
    /* Grouping consecutive lines that assign the same value results in shorter code. */
    pinControlRegister(PIN_VCP_TXD)      = PORT_PULLUPEN_bm;
    pinControlRegister(PIN_VCP_RXD)      = PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TDAT)     = PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TRST)     = PORT_PULLUPEN_bm;
    pinControlRegister(PIN_SYS_SW0)      = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
    /* TCLK disable/output is shared outside connection with VTxD */

    /* PORTx event generator */
    portRegister(PIN_VCP_RXD).EVGENCTRLA = pinPosition(PIN_VCP_RXD)         /* EVG0 */
                                         | pinPosition(PIN_VCP_TXD) << 4;   /* EVG1 */

    /*** Port Multiplexer ***/
    PORTMUX_EVSYSROUTEA   = PORTMUX_EVOUTD_ALT1_gc;         /* EVOUTD_ALT1 -> PIN_PD7 -> LED0 */

    /*** Event System ***/
    EVSYS_CHANNEL1        = EVSYS_CHANNEL_RTC_EVGEN1_gc;    /* 128Hz periodic. */
    EVSYS_CHANNEL2        = EVSYS_CHANNEL_CCL_LUT3_gc;      /* <- Indicator */
    EVSYS_CHANNEL4        = EVSYS_CHANNEL_PORTX_EVGEN0(PIN_VCP_RXD);  /* <- VRxD */
    EVSYS_CHANNEL5        = EVSYS_CHANNEL_PORTX_EVGEN1(PIN_VCP_TXD);  /* <- VTxD */

    EVSYS_USEREVSYSEVOUTD = EVSYS_USER_CHANNEL3_gc;         /* -> EVOUTD:LED1 */

    EVSYS_USERCCLLUT3A    = EVSYS_USER_CHANNEL4_gc;         /* <- VRxD */
    EVSYS_USERCCLLUT1A    = EVSYS_USER_CHANNEL4_gc;         /* <- VRxD */
    EVSYS_USERCCLLUT1B    = EVSYS_USER_CHANNEL5_gc;         /* <- VTxD */

    EVSYS_USERTCB1CAPT    = EVSYS_USER_CHANNEL2_gc;         /* TCB1_CAPT <- Strobe */
    EVSYS_USERTCB0COUNT   = EVSYS_USER_CHANNEL1_gc;         /* <- 128Hz */

    /*** CCL3 : VCP Indicator ***/
    CCL_TRUTH3    = CCL_TRUTH_0_bm | CCL_TRUTH_1_bm;
    CCL_LUT3CTRLB = CCL_INSEL0_USART0_gc                    /* <- UART_TX */
                  | CCL_INSEL1_EVENTA_gc;                   /* <- EVS_CH4 : VRxD */
    CCL_LUT3CTRLA = CCL_ENABLE_bm;

    /*** CCL1 : LED1 (PC3) generator ***/
    /* Do not turn on; 0, 3 */
    CCL_TRUTH1    = ~(CCL_TRUTH_0_bm | CCL_TRUTH_3_bm);
    CCL_LUT1CTRLC = CCL_INSEL2_TCB1_gc;                     /* <- TCB1_WO */
    CCL_LUT1CTRLB = CCL_INSEL1_EVENTB_gc                    /* <- EVS_CH5 : VTxD */
                  | CCL_INSEL0_EVENTA_gc;                   /* <- EVS_CH4 : VRxD */
    CCL_LUT1CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm;           /* -> PIN_PC3 */

    /*** CCL2 : LED0 (PD7) Heart-Beat generator ***/
    CCL_TRUTH2    = CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT2CTRLC = CCL_INSEL2_TCB1_gc;                     /* <- TCB1_WO */
    CCL_LUT2CTRLB = CCL_INSEL1_TCA0_gc                      /* <- TCA0_WO1 */
                  | CCL_INSEL0_TCB0_gc;                     /* <- TCB0_WO */
    CCL_LUT2CTRLA = CCL_ENABLE_bm | CCL_OUTEN_bm;           /* -> PIN_PD3 */

    /*** CCL enable ***/
    /* One of the CCL's is the LED output control. */
    CCL_CTRLA = CCL_ENABLE_bm;

    /*** TCA0 ***/
    /*
     * TCA0 is divided into 8-bit timers totaling 6-channels.
     *
     * WO0 : ISP Bit-banging
     * WO1 : LED0 Heart-beat (in combination with TCB0)
     * WO2 : UPDI Bit-banging
     * WO3 : reserved
     * WO4 : Charge-pump output-1 (negative)
     * WO5 : Charge-pump output-2 (positive)
     */
    TCA0_SPLIT_CTRLD = TCA_SPLIT_SPLITM_bm;
    TCA0_SPLIT_LCMP2 = F_CPU / 125000  / 2; /* TCA0_WO2 */
    TCA0_SPLIT_HCMP1 = F_CPU / HVC_CLK / 2; /* TCA0_WO4 */
    TCA0_SPLIT_HCMP2 = F_CPU / HVC_CLK / 2; /* TCA0_WO5 */
    TCA0_SPLIT_HPER = (F_CPU / HVC_CLK) - 1;

    /*** TCB0 ***/
    TCB0_CTRLB = TCB_CNTMODE_PWM8_gc;

    /*** TCB1 ***/
    /* TCB1 is used to control the LED1 blinking rate. */
    TCB1_CCMP   = TM_VCPBL;
    TCB1_EVCTRL = TCB_CAPTEI_bm;
    TCB1_CTRLB  = TCB_CNTMODE_SINGLE_gc;
    TCB1_CTRLA  = TCB_ENABLE_bm | TCB_CLKSEL_TCA0_gc;

    /*** PIT and RTC ***/
    /* EVG0 <- 1024Hz (32768/32) : EVG1 <- 128Hz (32768/256) */
    RTC_PITEVGENCTRLA = RTC_EVGEN0SEL_DIV32_gc | RTC_EVGEN1SEL_DIV256_gc;
    RTC_PITCTRLA = RTC_PITEN_bm;

    /* Drive a 1024Hz counter for RTC_CNT */
    RTC_CTRLA = RTC_RTCEN_bm | RTC_PRESCALER_DIV32_gc;

    /*** VUSB Bus-Powerd ***/
    /* If you are supplying 3V3 to the VUSB pad from an
       external power source, you can comment this out.*/
    SYSCFG_VUSBCTRL = SYSCFG_USBVREG_bm;

    /* Voltage measurements may initially return erroneous values. */
    setup_adc();
  }

};  /* SYS */

#endif

// end of code

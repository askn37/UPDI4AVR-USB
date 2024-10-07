/**
 * @file sys.cpp
 * @author askn (K.Sato) multix.jp
 * @brief UPDI4AVR-USB is a program writer for the AVR series, which are UPDI/TPI
 *        type devices that connect via USB 2.0 Full-Speed. It also has VCP-UART
 *        transfer function. It only works when installed on the AVR-DU series.
 *        Recognized by standard drivers for Windows/macos/Linux and AVRDUDE>=7.2.
 * @version 1.33.46+
 * @date 2024-08-26
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
#define HBEAT_HZ   (0.5)    /* Periodic 0.5Hz */
#define TCA0_STEP  ((uint8_t)(sqrt((F_CPU / 1024.0) * (1.0 / HBEAT_HZ)) - 0.5))
#define TCB1_HBEAT (((TCA0_STEP /  2) << 8) + (TCA0_STEP - 1))
#define TCB1_STEP  (170)    /* Periodic 0.67Hz */
#define TCB1_BLINK (((TCB1_STEP /  2) << 8) + (TCB1_STEP - 1))
#define TCB1_FLASH (((TCB1_STEP / 34) << 8) + (TCB1_STEP - 1))
#define TCB1_FAST  (((TCB1_STEP / 10) << 8) + (TCB1_STEP / 5))
#define HVC_CLK    5000000

namespace SYS {

  const uint8_t _updi_bitmap_reset[] = {  /* LSB First */
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, /* BREAK IDLE */
    0x55, 0x7E, 0xC8, 0x7F, 0x59, 0xFE, 0xFF  /* SYSRST */
  };
  const uint8_t _updi_bitmap_leave[] = {  /* LSB First */
    0x7F, 0x55, 0x7E, 0xC8, 0x7F, 0x00, 0xFE, /* SYSRST */
    0x7F, 0x55, 0x7E, 0xC3, 0x7E, 0x04, 0xFF  /* UPDIDIS */
  };

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
     *
     * V-Target power control: output negative logic.
     */

  #if (CONFIG_HAL_TYPE == HAL_BAREMETAL_14P)
    /* HV-control and PDI support is not available in this package. */

    /* Output GPIO */
    VPORTD_DIR = 0b10000000;    /* PIN_SYS_LED0 */

    /* Pull-Up GPIO */
    pinControlRegister(PIN_VCP_TXD)  = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_VCP_RXD)  = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TDAT) = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TRST) = PORT_ISC_INPUT_DISABLE_gc | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_SYS_SW0)  = PORT_ISC_RISING_gc        | PORT_PULLUPEN_bm;
    /* TCLK disable/output is shared outside connection with VTxD */

    /* PORTx event generator */
    portRegister(PIN_SYS_SW0).EVGENCTRLA = pinPosition(PIN_SYS_SW0);
    portRegister(PIN_VCP_RXD).EVGENCTRLA = pinPosition(PIN_VCP_RXD) << 4;

    /*** Multiplexer ***/
    PORTMUX_EVSYSROUTEA   = PORTMUX_EVOUTD_ALT1_gc;         /* EVOUTD_ALT1 -> PIN_PD7 */
    EVSYS_CHANNEL3        = EVSYS_CHANNEL_CCL_LUT2_gc;      /* <- LED0 */
    EVSYS_CHANNEL4        = EVSYS_CHANNEL_PORTA_EVGEN1_gc;  /* <- VRxD */
    EVSYS_CHANNEL5        = EVSYS_CHANNEL_PORTF_EVGEN0_gc;  /* <- SW0  */
    EVSYS_USEREVSYSEVOUTD = EVSYS_USER_CHANNEL3_gc;         /* LUT2_OUT -> EVOUTD */
    EVSYS_USERCCLLUT1A    = EVSYS_USER_CHANNEL4_gc;         /* <- VRxD */
    EVSYS_USERCCLLUT0A    = EVSYS_USER_CHANNEL5_gc;         /* <- SW0 */

    /*** SW0 FALLING Interrupt generator ***/
    CCL_TRUTH0    = CCL_TRUTH_1_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_EVENTA_gc;                         /* <- CH5 */
    CCL_LUT0CTRLA = CCL_ENABLE_bm | CCL_FILTSEL_FILTER_gc;
    CCL_INTCTRL0  = CCL_INTMODE0_FALLING_gc;

    /*** LED1 generator ***/
    CCL_TRUTH1    = CCL_TRUTH_0_bm       | CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT1CTRLB = CCL_INSEL0_USART0_gc | CCL_INSEL1_EVENTA_gc;  /* <- CH4 */
    CCL_LUT1CTRLA = CCL_ENABLE_bm        | CCL_OUTEN_bm;          /* -> PIN_PC3 */

    /*** LED0 Heart-Beat generator ***/
    CCL_TRUTH2    = CCL_TRUTH_1_bm     | CCL_TRUTH_2_bm;
    CCL_LUT2CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_TCB1_gc;
    CCL_LUT2CTRLA = CCL_ENABLE_bm;  /* -> CH3 */

    /*** VUSB Bus-Powerd ***/
    SYSCFG_VUSBCTRL = SYSCFG_USBVREG_bm;

  #elif (CONFIG_HAL_TYPE == HAL_BAREMETAL_20P)

    /* Output GPIO */
    VPORTA_DIR = 0b10100010;    /* VPW HVSL1 HVSL2 */
    VPORTD_DIR = 0b10110000;    /* HVCP1 HVCP2 HVSL2 */

    /* Pull-Up GPIO */
    pinControlRegister(PIN_VCP_TXD)      = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_VCP_RXD)      = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TDAT)     = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TRST)     = PORT_ISC_INPUT_DISABLE_gc | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_PDAT)     = PORT_ISC_INTDISABLE_gc;
    pinControlRegister(PIN_SYS_SW0)      = PORT_ISC_RISING_gc        | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_HVC_CHGPUMP1) = PORT_ISC_INPUT_DISABLE_gc | PORT_INVEN_bm;
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
    EVSYS_USERCCLLUT0A    = EVSYS_USER_CHANNEL5_gc;         /* <- SW0 */

    /*** SW0 FALLING Interrupt generator ***/
    CCL_TRUTH0    = CCL_TRUTH_1_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_EVENTA_gc;                         /* <- CH5 */
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

  #elif (CONFIG_HAL_TYPE == HAL_CNANO)

    /* Output GPIO */
    VPORTD_DIR = 0b00110111;    /* HVSL1 HVSL2 HVSL3 HVCP1 HVCP2 */
    VPORTF_DIR = 0b00010100;    /* LED0 VPW */

    /* Pull-Up GPIO */
    pinControlRegister(PIN_VCP_TXD)      = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_VCP_RXD)      = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TDAT)     = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TRST)     = PORT_ISC_INPUT_DISABLE_gc | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_PDAT)     = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_PCLK)     = PORT_ISC_INPUT_DISABLE_gc | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_SYS_SW0)      = PORT_ISC_RISING_gc        | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_SYS_LED0)     = PORT_ISC_INPUT_DISABLE_gc | PORT_INVEN_bm;
    pinControlRegister(PIN_HVC_CHGPUMP1) = PORT_ISC_INPUT_DISABLE_gc | PORT_INVEN_bm;

    /* PORTx event generator */
    portRegister(PIN_SYS_SW0).EVGENCTRLA = pinPosition(PIN_SYS_SW0);
    portRegister(PIN_VCP_RXD).EVGENCTRLA = pinPosition(PIN_VCP_RXD) << 4;

    /*** Multiplexer ***/
    PORTMUX_TCAROUTEA     = PORTMUX_TCA0_PORTD_gc;          /* TCA0_WOn_ALT3 -> PORTD */
    EVSYS_CHANNEL3        = EVSYS_CHANNEL_CCL_LUT2_gc;      /* <- LED0 */
    EVSYS_CHANNEL4        = EVSYS_CHANNEL_PORTA_EVGEN1_gc;  /* <- VRxD */
    EVSYS_CHANNEL5        = EVSYS_CHANNEL_PORTF_EVGEN0_gc;  /* <- SW0  */
    EVSYS_USEREVSYSEVOUTF = EVSYS_USER_CHANNEL3_gc;         /* LED0 -> EVOUTF:PIN_PF2 */
    EVSYS_USERCCLLUT3A    = EVSYS_USER_CHANNEL4_gc;         /* <- VRxD */
    EVSYS_USERCCLLUT0A    = EVSYS_USER_CHANNEL5_gc;         /* <- SW0 */

    /*** SW0 FALLING Interrupt generator ***/
    CCL_TRUTH0    = CCL_TRUTH_1_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_EVENTA_gc;                         /* <- CH5 */
    CCL_LUT0CTRLA = CCL_ENABLE_bm | CCL_FILTSEL_FILTER_gc;
    CCL_INTCTRL0  = CCL_INTMODE0_FALLING_gc;

    /*** LED1 generator ***/
    CCL_TRUTH3    = CCL_TRUTH_0_bm       | CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT3CTRLB = CCL_INSEL0_USART0_gc | CCL_INSEL1_EVENTA_gc;  /* <- CH4 */
    CCL_LUT3CTRLA = CCL_ENABLE_bm        | CCL_OUTEN_bm;          /* -> PIN_PF3 */

    /*** LED0 Heart-Beat generator ***/
    CCL_TRUTH2    = CCL_TRUTH_1_bm     | CCL_TRUTH_2_bm;
    CCL_LUT2CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_TCB1_gc;
    CCL_LUT2CTRLA = CCL_ENABLE_bm;                                /* -> CH3 */

  #else /* (CONFIG_HAL_TYPE == HAL_BAREMETAL_28P) || (CONFIG_HAL_TYPE == HAL_BAREMETAL_32P) */

    /* Output GPIO */
    VPORTA_DIR = 0b00000010;    /* VPW */
    VPORTD_DIR = 0b00110111;    /* HVSL1 HVSL2 HVSL3 HVCP1 HVCP2 */

    /* Pull-Up GPIO */
    pinControlRegister(PIN_VCP_TXD)      = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_VCP_RXD)      = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TDAT)     = PORT_ISC_INTDISABLE_gc    | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_TRST)     = PORT_ISC_INPUT_DISABLE_gc | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_PGM_PDAT)     = PORT_ISC_INTDISABLE_gc;
    pinControlRegister(PIN_SYS_SW0)      = PORT_ISC_RISING_gc        | PORT_PULLUPEN_bm;
    pinControlRegister(PIN_HVC_CHGPUMP1) = PORT_ISC_INPUT_DISABLE_gc | PORT_INVEN_bm;
    /* PDAT in/output is shared outside connection with TDAT */
    /* PCLK disable/output is shared internal connection with TRST */

    /* PORTx event generator */
    portRegister(PIN_SYS_SW0).EVGENCTRLA = pinPosition(PIN_SYS_SW0)
                                         | pinPosition(PIN_VCP_RXD) << 4;

    /*** Multiplexer ***/
    PORTMUX_TCAROUTEA     = PORTMUX_TCA0_PORTD_gc;          /* TCA0_WOn_ALT3 -> PORTD */
    EVSYS_CHANNEL4        = EVSYS_CHANNEL_PORTA_EVGEN1_gc;  /* <- VRxD */
    EVSYS_CHANNEL5        = EVSYS_CHANNEL_PORTA_EVGEN0_gc;  /* <- SW0  */
    EVSYS_USERCCLLUT1A    = EVSYS_USER_CHANNEL4_gc;         /* <- VRxD */
    EVSYS_USERCCLLUT0A    = EVSYS_USER_CHANNEL5_gc;         /* <- SW0 */

    /*** SW0 FALLING Interrupt generator ***/
    CCL_TRUTH0    = CCL_TRUTH_1_bm;
    CCL_LUT0CTRLB = CCL_INSEL0_EVENTA_gc;                         /* <- CH5 */
    CCL_LUT0CTRLA = CCL_ENABLE_bm | CCL_FILTSEL_FILTER_gc;
    CCL_INTCTRL0  = CCL_INTMODE0_FALLING_gc;

    /*** LED1 generator ***/
    CCL_TRUTH1    = CCL_TRUTH_0_bm       | CCL_TRUTH_1_bm | CCL_TRUTH_2_bm;
    CCL_LUT1CTRLB = CCL_INSEL0_USART0_gc | CCL_INSEL1_EVENTA_gc;  /* <- CH4 */
    CCL_LUT1CTRLA = CCL_ENABLE_bm        | CCL_OUTEN_bm;          /* -> PIN_PC3 */

    /*** LED0 Heart-Beat generator ***/
    CCL_TRUTH2    = CCL_TRUTH_1_bm     | CCL_TRUTH_2_bm;
    CCL_LUT2CTRLB = CCL_INSEL0_TCA0_gc | CCL_INSEL1_TCB1_gc;
    CCL_LUT2CTRLA = CCL_ENABLE_bm      | CCL_OUTEN_bm;            /* -> PIN_PD3 */

  #endif

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
      TCB1_CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_TCA0_gc;
      _led_mode = 1;
    }
  }

  void LED_TCB1 (uint8_t _mode, uint16_t _ccmp) {
    if (_led_mode != _mode) {
      TCA0_SPLIT_CTRLA = 0;
      TCB1_CNTL = 0;
      TCB1_CCMP = _ccmp;
      TCB1_CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc;
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

  void power_reset (bool _off, bool _on) {
    if (_off) {
  #ifdef PIN_PGM_VPOWER
      digitalWriteMacro(PIN_PGM_VPOWER, HIGH);  /* VTG off */
      /* Temporarily disable the pullup to stop current leakage when VTG=OFF. */
      /* It would be easier to just set the pin output LOW,                   */
      /* but we do it this way because of possible conflicts.                 */
    #if CONFIG_PGM_TYPE == 1
      pinControlRegister(PIN_PGM_TRST) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TDAT) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_VCP_TXD)  &= ~PORT_PULLUPEN_bm;  /* outside shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  &= ~PORT_PULLUPEN_bm;
    #elif CONFIG_PGM_TYPE == 2
      pinControlRegister(PIN_PGM_TRST) &= ~PORT_PULLUPEN_bm;  /* internal shared PCLK */
      pinControlRegister(PIN_PGM_TDAT) &= ~PORT_PULLUPEN_bm;  /* outside shared PDAT */
      pinControlRegister(PIN_VCP_TXD)  &= ~PORT_PULLUPEN_bm;  /* internal shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  &= ~PORT_PULLUPEN_bm;
    #else
      pinControlRegister(PIN_PGM_TRST) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_PCLK) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TDAT) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_PDAT) &= ~PORT_PULLUPEN_bm;
      pinControlRegister(PIN_VCP_TXD)  &= ~PORT_PULLUPEN_bm;  /* internal shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  &= ~PORT_PULLUPEN_bm;
    #endif
  #endif
    }
    if (_on) {
  #ifdef PIN_PGM_VPOWER
      delay_125ms();  /* discharge duration */
      digitalWriteMacro(PIN_PGM_VPOWER, LOW);   /* VTG on */
    #if CONFIG_PGM_TYPE == 1
      pinControlRegister(PIN_VCP_TXD)  |= PORT_PULLUPEN_bm;   /* outside shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TDAT) |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TRST) |= PORT_PULLUPEN_bm;
    #elif CONFIG_PGM_TYPE == 2
      pinControlRegister(PIN_VCP_TXD)  |= PORT_PULLUPEN_bm;   /* internal shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TDAT) |= PORT_PULLUPEN_bm;   /* outside shared PDAT */
      pinControlRegister(PIN_PGM_TRST) |= PORT_PULLUPEN_bm;   /* internal shared PCLK */
    #else
      pinControlRegister(PIN_VCP_TXD)  |= PORT_PULLUPEN_bm;   /* internal shared TCLK */
      pinControlRegister(PIN_VCP_RXD)  |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TDAT) |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_PDAT) |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_PCLK) |= PORT_PULLUPEN_bm;
      pinControlRegister(PIN_PGM_TRST) |= PORT_PULLUPEN_bm;
    #endif
      delay_800us();
  #endif
    }
  }

  /*** Low level TDAT stream manipulation ***/
  /* UPDI commands are sent from TDAT using only TCA0 and bit manipulation, without switching USART. */
  /* 128kbps is the lowest limit that can be achieved with an 8-bit timer at 32MHz or less. */
  void send_bitmap (const uint8_t _bitmap[], const size_t _length) {
    TCA0_SPLIT_HPER  = F_CPU / 125000L;
    TCA0_SPLIT_CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV1_gc;
    for (uint8_t i = 0; i < _length; i++) {
      uint8_t _d = (_bitmap[i >> 3]) >> (i & 7);
      loop_until_bit_is_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_HUNF_bp);
      if (bit_is_set(_d, 0))
        openDrainWriteMacro(PIN_PGM_TDAT, HIGH);
      else
        openDrainWriteMacro(PIN_PGM_TDAT, LOW);
      bit_set(TCA0_SPLIT_INTFLAGS, TCA_SPLIT_HUNF_bp);
    }
    TCA0_SPLIT_CTRLA = 0;
  }

  /*
   * Executed when SW0 is detected as being pressed.
   * May be called multiple times due to chattering.
   */
  void reset_enter (void) {
    LED_Blink();
    openDrainWriteMacro(PIN_PGM_TRST, LOW);
    /*
     * Puts a tinyAVR-0 which does not have a reset pad into reset state.
     * This applies to all chips which have an enabled UPDI pad.
     * Does not affect chips with an active reset pad or TPI/PDI type chips.
     */
    send_bitmap(_updi_bitmap_reset, sizeof(_updi_bitmap_reset) * 8);
    D1PRINTF("<RST:IN>\r\n");
    bit_clear(GPCONF, GPCONF_FAL_bp);
  }

  /*
   * This will be executed when SW0 is released.
   * If the VCP is operating, it will return to normal operation,
   * but if the USB is stopped, it will reboot at the end.
   */
  void reset_leave (void) {
    send_bitmap(_updi_bitmap_leave, sizeof(_updi_bitmap_leave) * 8);
    openDrainWriteMacro(PIN_PGM_TRST, HIGH);
  #ifdef CONFIG_VCP_DTR_RESET
    /* A delay of 64ms or more between when the bootloader starts and when RxD opens. */
    delay_125ms();
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
    _PROTECTED_WRITE(WDT_CTRLA, WDT_PERIOD_8CLK_gc);
    for (;;);
    // _PROTECTED_WRITE(RSTCTRL_SWRR, 1);
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
    ADC0_CTRLB = ADC_PRESC_DIV4_gc;
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

  void delay_100us (void) {
    delay_micros(100);
  }

  void delay_800us (void) {
    delay_micros(800);
  }

  void delay_125ms (void) {
    delay_millis(125);
  }

};

#if defined(PIN_SYS_SW0)
/* If the level is not maintained for a sufficient period of time it will not function properly. */
ISR(portIntrruptVector(PIN_SYS_SW0)) {
  /* SW0 Raising Interrupt */
  vportRegister(PIN_SYS_SW0).INTFLAGS = ~0;
  bit_set(GPCONF, GPCONF_RIS_bp);
}

ISR(CCL_CCL_vect) {
  /* SW0 Falling Intrrupt from CCL2 */
  CCL_INTFLAGS = ~0;
  bit_set(GPCONF, GPCONF_FAL_bp);
}
#endif

// end of code

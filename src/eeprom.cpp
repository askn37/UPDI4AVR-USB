/**
 * @file eeprom.cpp
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
#include "api/memspace.h"   /* EEPROM */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * The first 8 bytes of the EEPROM are CONFIG_USB_VIDPID.
 * The next 8 bytes are CONFIG_USB_SERIALNUMBER.
 *
 * The defined contents will be written to the <vidpid.eep> Hex-format file.
 * You can change the default by writing this to the device.
 *
 * Ex) Specifying values ​​directly in the CLI
 * avrdude -P usb -c pkobn_updi -p avr64du32 -DU eeprom:w:0xEB,0x03,0x77,0x21:m
 *
 * Ex) Replace with configured <vidpid.eep>
 * avrdude -P usb -c pkobn_updi -p avr64du32 -DU eeprom:w:VIDPID_PICK4.eep:i *
 */

#ifndef CONFIG_USB_VIDPID
  #define CONFIG_USB_VIDPID 0xFFFF,0xFFFF
#endif
#ifndef CONFIG_USB_SERIALNUMBER
  #define CONFIG_USB_SERIALNUMBER 0xFFFFFFFFUL
#endif

/* User-defined EEPROM <vidpid.eep> */
User_EEP_t _EEP EEPROM = { CONFIG_USB_VIDPID, CONFIG_USB_SERIALNUMBER };

// end of code

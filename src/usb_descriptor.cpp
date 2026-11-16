/**
 * @file Descriptor_CDC_EDBG.cpp
 * @author askn (K.Sato) multix.jp
 * @brief
 * @version 0.1
 * @date 2026-11-10
 * @copyright Copyright (c) 2026 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include "configuration.h"
#include "prototype.h"
#include "USB_Utils.h"
#include "USB/Descriptor.h"

namespace USB {

  /*** WARNING ***
   * This USB device information applies only when using Microchip Technology Inc. products.
   * It must be modified when porting to other companies' products.
   */
  #define USB_VENDOR_ID   0x04D8  /* 0x04D8=MCHP VID. */
  #define USB_PRODUCT_ID  0x0B15  /* PID 0x0010-0x002F reserved for testing/non-public demos. */

  /* All strings are concatenated and written in ASCIIZ format. */
  /* It is dynamically converted to UTF16-LE format. */
  #define USB_STR_1 "MULTIX.JP OSSW/OSHW"
  #define USB_STR_2 "UPDI4AVR:AVR-DU:EDBG:CMSIS-DAP"
  #define USB_STR_4 "CDC-ACM-VCP"
  #define USB_DESC_STRS USB_STR_1 USB_STR_2 USB_STR_4

  /* We decide which NVM to place the generated results in. */
  // #define USB_DESC_MEM 0

  #ifndef USB_DESC_MEM
    #if (__AVR_ARCH__ == 103)
      #define USB_DESC_MEM 1
    #else
      #define USB_DESC_MEM 0
    #endif
  #endif

  #if (USB_DESC_MEM == 1)   /* 1:DATA */
    #define USB_DESC_ATTR
    #define USB_DESC_ADDR 0
  #elif (USB_DESC_MEM == 2) /* 2:EEPROM */
    #define USB_DESC_ATTR __attribute__((__eeprom__))
    #define USB_DESC_ADDR EEPROM_START
  #elif (USB_DESC_MEM == 3) /* 3:USERROW */
    #define USB_DESC_ATTR __attribute__((section(".user_signatures")))
    #define USB_DESC_ADDR USER_SIGNATURES_START
  #elif (USB_DESC_MEM == 4) /* 4:BOOTROW */
    #define USB_DESC_ATTR __attribute__((section(".boot_signatures")))
    #define USB_DESC_ADDR BOOTROW_START
  #else
    #define USB_DESC_ATTR __attribute__((__progmem__))
    #define USB_DESC_ADDR 0
  #endif

  /**********************
   HID Descriptor Section
  **********************/

  /* HID_REPORT is defined as a macro to determine the total length. */

  /* CMSIS-DAP/EDBG (Full-Speed 64 byte payload) */
  #define HID_Report_Descriptor_0 { \
    HR_wUsage_Page(0xFF00), \
    HR_bUsage(1), \
    HR_bCollection(1), \
      HR_bLogical_Minimum(0), \
      HR_wLogical_Maximum(255), \
      HR_bReport_Size(8), \
      HR_bReport_Count(64), \
      HR_bUsage(1), \
      HR_bInput(2), \
      HR_bUsage(1), \
      HR_bOutput(2), \
      HR_bReport_Count(4), \
      HR_bUsage(1), \
      HR_bFeature(2), \
    HR_EndCollection \
  }
  #define HID_Report_Descriptor_0_temp (uint8_t[])HID_Report_Descriptor_0

  /**********************
   USB Descriptor Section
  **********************/

  const USB_Device_Descriptor_t USB_Device_Descriptor USB_DESC_ATTR = {
      .bLength              = sizeof(USB_Device_Descriptor_t)
    , .bDescriptorType      = bDescriptorType_DEVICE
    , .bcdUSB               = bcdUSB_2
    , .bDeviceClass         = bClassCode_Miscellaneous
    , .bDeviceSubClass      = bSubClassCode_Common
    , .bDeviceProtocol      = bProtocolCode_Interface_Association
    , .bMaxPacketSize0      = 64
    , .idVendor             = USB_VENDOR_ID
    , .idProduct            = USB_PRODUCT_ID
    , .bcdDevice            = 0x0135
    , .iManufacturer        = 1         // sManufacturer
    , .iProduct             = 2         // sProduct
    , .iSerialNumber        = 0xFF      // automatic generation
    , .bNumConfigurations   = 1
  };

  /* A structure template is required to determine */
  /* the total length of the General Configuration Set. */

  /* Configuration Set #0 */
  typedef struct {
    USB_Configuration_Descriptor_t                USB_Configuration_Descriptor_0;
    USB_Interface_Descriptor_t                    USB_Interface_Descriptor_0;
      USB_HID_Class_Descriptor_t                  USB_HID_Class_Descriptor_0;
        USB_HID_Item_Descriptors_List_t           USB_HID_Item_Descriptor_0[1];
      USB_Endpoint_Descriptor_t                   USB_Endpoint_Descriptor_81;
      USB_Endpoint_Descriptor_t                   USB_Endpoint_Descriptor_02;
    USB_Interface_Association_Descriptor_t        USB_Interface_Association_Descriptor_1;
    USB_Interface_Descriptor_t                    USB_Interface_Descriptor_1;
      USB_Header_Functional_Descriptor_t          USB_Header_Functional_Descriptor_1;
      USB_ACM_Functional_Descriptor_t             USB_ACM_Functional_Descriptor_1;
      USB_Union_Functional_Descriptor_t           USB_Union_Functional_Descriptor_1;
                                          uint8_t bSubordinateInterface_1[1];
      USB_Call_Management_Functional_Descriptor_t USB_Call_Management_Functional_Descriptor_1;
      USB_Endpoint_Descriptor_t                   USB_Endpoint_Descriptor_82;
    USB_Interface_Descriptor_t                    USB_Interface_Descriptor_2;
      USB_Endpoint_Descriptor_t                   USB_Endpoint_Descriptor_83;
      USB_Endpoint_Descriptor_t                   USB_Endpoint_Descriptor_03;
  } USB_Genelal_Configuration_Descriptor_0_t;

  const USB_Genelal_Configuration_Descriptor_0_t USB_Genelal_Configuration_Descriptor_0 USB_DESC_ATTR = {
    .USB_Configuration_Descriptor_0 = {
        .bLength                = sizeof(USB_Configuration_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_CONFIGURATION
      , .wTotalLength           = sizeof(USB_Genelal_Configuration_Descriptor_0_t)
      , .bNumInterfaces         = 3     // Interface x 3
      , .bConfigurationValue    = 1     // Configuration#1
      , .iConfiguration         = 0     // iStrings
      , .bmAttributes           = 0x80
      , .bMaxPower              = 100 / 2
    },
    .USB_Interface_Descriptor_0 = {
        .bLength                = sizeof(USB_Interface_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_INTERFACE
      , .bInterfaceNumber       = 0     // Interface#0
      , .bAlternateSetting      = 0     // not used
      , .bNumEndpoints          = 2     // 0x81 + 0x02
      , .bInterfaceClass        = bClassCode_Human_Interface_Device
      , .bInterfaceSubClass     = bSubClassCode_None
      , .bInterfaceProtocol     = bProtocolCode_None
      , .iInterface             = 0     // iStrings
    },
    .USB_HID_Class_Descriptor_0 = {
        .bLength                = sizeof(USB_HID_Class_Descriptor_t) + sizeof(USB_HID_Item_Descriptors_List_t) * 1
      , .bDescriptorType        = bDescriptorType_HID_CLASS
      , .bcdHID                 = 0x111
      , .bCountryCode           = 0
      , .bNumDescriptors        = 1
    },
    .USB_HID_Item_Descriptor_0 = {
      { bDescriptorType_HID_REPORT, sizeof(HID_Report_Descriptor_0_temp) }
    },
    .USB_Endpoint_Descriptor_81 = {
        .bLength                = sizeof(USB_Endpoint_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_ENDPOINT
      , .bEndpointAddress       = 0x81
      , .bmAttributes           = bmEP_Type_Interrupt
      , .wMaxPacketSize         = 64
      , .bInterval              = 1
    },
    .USB_Endpoint_Descriptor_02 = {
        .bLength                = sizeof(USB_Endpoint_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_ENDPOINT
      , .bEndpointAddress       = 0x02
      , .bmAttributes           = bmEP_Type_Interrupt
      , .wMaxPacketSize         = 64
      , .bInterval              = 1
    },
    .USB_Interface_Association_Descriptor_1 = {
        .bLength                = sizeof(USB_Interface_Association_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_INTERFACE_ASSOCIATION
      , .bFirstInterface        = 1     // Interface#1
      , .bInterfaceCount        = 2     // Interface x 2
      , .bFunctionClass         = bClassCode_Communication_Device
      , .bFunctionSubClass      = bSubClassCode_Abstract_Model
      , .bFunctionProtocol      = bProtocolCode_Composite
      , .iFunction              = 4     // iStrings
    },
    .USB_Interface_Descriptor_1 = {
        .bLength                = sizeof(USB_Interface_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_INTERFACE
      , .bInterfaceNumber       = 1     // Interface#1
      , .bAlternateSetting      = 0     // not used
      , .bNumEndpoints          = 1     // to EP 0x82 Interrupt
      , .bInterfaceClass        = bClassCode_Communication_Device
      , .bInterfaceSubClass     = bSubClassCode_Abstract_Model
      , .bInterfaceProtocol     = bProtocolCode_Composite
      , .iInterface             = 0     // iStrings
    },
    .USB_Header_Functional_Descriptor_1 = {
        .bLength                = sizeof(USB_Header_Functional_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_CS_INTERFACE
      , .bDescriptorSubtype     = bDescriptorSubtype_Header
      , .bcdCDC                 = 0x110
    },
    .USB_ACM_Functional_Descriptor_1 = {
        .bLength                = sizeof(USB_ACM_Functional_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_CS_INTERFACE
      , .bDescriptorSubtype     = bDescriptorSubtype_Abstract_Control_Management
      , .bmCapabilities         = bmACM_SEND_BREAK | bmACM_SERIAL_STATE
    },
    .USB_Union_Functional_Descriptor_1 = {
        .bLength                = sizeof(USB_Union_Functional_Descriptor_t) + 1
      , .bDescriptorType        = bDescriptorType_CS_INTERFACE
      , .bDescriptorSubtype     = bDescriptorSubtype_Union
      , .bControlInterface      = 1     // length
    },
    .bSubordinateInterface_1 = {2},     // USB_Interface_Descriptor_2
    .USB_Call_Management_Functional_Descriptor_1 = {
        .bLength                = sizeof(USB_Call_Management_Functional_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_CS_INTERFACE
      , .bDescriptorSubtype     = bDescriptorSubtype_Call_Management
      , .bmCapabilities         = 3     // or 3
      , .bDataInterface         = 2     // to USB_Interface_Descriptor_2
    },
    .USB_Endpoint_Descriptor_82 = {
        .bLength                = sizeof(USB_Endpoint_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_ENDPOINT
      , .bEndpointAddress       = 0x82
      , .bmAttributes           = bmEP_Type_Interrupt
      , .wMaxPacketSize         = VCP_INT_BUFFSIZE
      , .bInterval              = USB_CCI_INTERVAL
    },
    .USB_Interface_Descriptor_2 = {
        .bLength                = sizeof(USB_Interface_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_INTERFACE
      , .bInterfaceNumber       = 2     // Interface#2
      , .bAlternateSetting      = 0     // not used
      , .bNumEndpoints          = 2     // 0x83 + 0x03
      , .bInterfaceClass        = bClassCode_Communications_Data
      , .bInterfaceSubClass     = bSubClassCode_Composite
      , .bInterfaceProtocol     = bProtocolCode_Composite
      , .iInterface             = 0     // iStrings
    },
    .USB_Endpoint_Descriptor_83 = {
        .bLength                = sizeof(USB_Endpoint_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_ENDPOINT
      , .bEndpointAddress       = 0x83
      , .bmAttributes           = bmEP_Type_Bulk
      , .wMaxPacketSize         = VCP_TXD_BUFFSIZE
      , .bInterval              = 0
    },
    .USB_Endpoint_Descriptor_03 = {
        .bLength                = sizeof(USB_Endpoint_Descriptor_t)
      , .bDescriptorType        = bDescriptorType_ENDPOINT
      , .bEndpointAddress       = 0x03
      , .bmAttributes           = bmEP_Type_Bulk
      , .wMaxPacketSize         = VCP_RXD_BUFFSIZE
      , .bInterval              = 0
    }
  };

  /**************************
   List-up Descriptor Section
  **************************/

  typedef struct {
    uint8_t head_0[3];    uint8_t body_0[sizeof(USB_DESC_STRS)];  // end null is head_1[1]
  /*uint8_t head_1[1];*/  USB_Device_Descriptor_t body_1;
    uint8_t head_2[1];    USB_Genelal_Configuration_Descriptor_0_t body_2;
    uint8_t head_3[3];    uint8_t body_3[sizeof(HID_Report_Descriptor_0_temp)];
    uint8_t string_1[4];
    uint8_t string_2[4];
    uint8_t string_3[4];
    uint8_t string_4[4];
    uint8_t string_0[3];  uint16_t wLAGID[1];
  } USB_Descriptor_t;

  /* The elements defined so far are then assembled into a single table. */
  /* It's impossible to achieve this automatically using only C/C++ macros. */
  /* Please refer to the relevant documentation for details. */
  const USB_Descriptor_t USB_Descriptor USB_DESC_ATTR = {
    /* Strings store */
      {0,  1 + sizeof(USB_DESC_STRS), bDescriptorType_NONCE}
    ,      USB_DESC_STRS
    /* Device */
    ,      USB_Device_Descriptor
    /* Configuration */
    , {0}, USB_Genelal_Configuration_Descriptor_0
    , {0,  2 + sizeof(HID_Report_Descriptor_0_temp), bDescriptorType_HID_REPORT}
    ,      HID_Report_Descriptor_0
    /* Strings */
    , {1,  sizeof(USB_STR_1)-1, bDescriptorType_STRING, 3}
    , {2,  sizeof(USB_STR_2)-1, bDescriptorType_STRING, 3 + sizeof(USB_STR_1)-1}
    , {3,                    9, bDescriptorType_STRING, 3 + sizeof(USB_STR_1)-1 + sizeof(USB_STR_2)-1 - 9}
    , {4,  sizeof(USB_STR_4)-1, bDescriptorType_STRING, 3 + sizeof(USB_STR_1)-1 + sizeof(USB_STR_2)-1}
    /* Strings LANGID's : Table Terminator */
    , {0, 4, bDescriptorType_STRING}, {wLANG_EN_US}
  };

  uint8_t* get_descriptor_ptr (void) { return (uint8_t*)&USB_Descriptor + USB_DESC_ADDR; }

} /* USB */

// end of code
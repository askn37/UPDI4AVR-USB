/**
 * @file prototype.h
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

#pragma once
#include <avr/io.h>
#include <stddef.h>
#include <setjmp.h>
#include <api/memspace.h>
#include "configuration.h"

// #undef DEBUG
// #define DEBUG 3

#ifndef F_CPU
  #define F_CPU 20000000L
#endif
#ifndef CONSOLE_BAUD
  #define CONSOLE_BAUD 500000L
#endif

#undef Serial
#define D0PRINTF(FMT, ...)
#define D1PRINTF(FMT, ...)
#define D2PRINTF(FMT, ...)
#define D3PRINTF(FMT, ...)
#define D0PRINTHEX(P,L)
#define D1PRINTHEX(P,L)
#define D2PRINTHEX(P,L)
#define D3PRINTHEX(P,L)
#if defined(DEBUG)
  #include "peripheral.h" /* from Micro_API : import Serial (Debug) */
  #define Serial Serial1C /* PIN_PD6:TxD, PIN_PD7:RxD */
  #undef  D0PRINTF
  #define D0PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
  #undef  D0PRINTHEX
  #define D0PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #if (DEBUG >= 1)
    #undef D1PRINTF
    #define D1PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
    #undef  D1PRINTHEX
    #define D1PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #endif
  #if (DEBUG >= 2)
    #undef  D2PRINTF
    #define D2PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
    #undef  D2PRINTHEX
    #define D2PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #endif
  #if (DEBUG >= 3)
    #undef  D3PRINTF
    #define D3PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
    #undef  D3PRINTHEX
    #define D3PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #endif
#endif

#define PACKED __attribute__((packed))
#define WEAK   __attribute__((weak))
#define RODATA __attribute__((__progmem__))
#define NOINIT __attribute__((section(".noinit")))

#define USB_EP_SIZE_gc(x)  ((x <= 8 ) ? USB_BUFSIZE_DEFAULT_BUF8_gc :\
                            (x <= 16) ? USB_BUFSIZE_DEFAULT_BUF16_gc:\
                            (x <= 32) ? USB_BUFSIZE_DEFAULT_BUF32_gc:\
                                        USB_BUFSIZE_DEFAULT_BUF64_gc)
#define USB_EP_ID_SWAP(x) __builtin_avr_swap(x)
#define USB_EP(EPFIFO) (*(USB_EP_t *)(((uint16_t)&EP_TABLE.EP) + (EPFIFO)))
#define USB_EP_STATUS_CLR(EPFIFO) _SFR_MEM8(&USB0_STATUS0_OUTCLR + ((EPFIFO) >> 2))
#define USB_EP_STATUS_SET(EPFIFO) _SFR_MEM8(&USB0_STATUS0_OUTSET + ((EPFIFO) >> 2))

#define USB_ENDPOINTS_MAX 4
#define USB_CCI_INTERVAL  4

/* In the internal representation of an endpoint number, */
/* the high and low nibbles are reversed from the representation on the USB device. */
#define USB_EP_REQ  (0x00)
#define USB_EP_RES  (0x08)
#define USB_EP_DPI  (0x18)  /* #0 DAP IN  */
#define USB_EP_DPO  (0x20)  /* #0 DAP OUT */
#define USB_EP_CCI  (0x28)  /* #1 CCI Communications-Control IN */
#define USB_EP_CDO  (0x30)  /* #2 CDO Communications-Data OUT */
#define USB_EP_CDI  (0x38)  /* #2 CDI Communications-Data IN */

#define EP_REQ  USB_EP(USB_EP_REQ)
#define EP_RES  USB_EP(USB_EP_RES)
#define EP_DPI  USB_EP(USB_EP_DPI)
#define EP_DPO  USB_EP(USB_EP_DPO)
#define EP_CCI  USB_EP(USB_EP_CCI)
#define EP_CDI  USB_EP(USB_EP_CDI)
#define EP_CDO  USB_EP(USB_EP_CDO)

#define GPCONF GPR_GPR0
  #define GPCONF_USB_bp   0         /* USB interface is active */
  #define GPCONF_USB_bm   (1 << 0)
  #define GPCONF_VCP_bp   1         /* VCP enabled */
  #define GPCONF_VCP_bm   (1 << 1)
  #define GPCONF_DBL_bp   2         /* VCP-RxD double buffer selection */
  #define GPCONF_DBL_bm   (1 << 2)
  #define GPCONF_BRK_bp   3         /* VCP-TxD BREAK transmission */
  #define GPCONF_BRK_bm   (1 << 3)
  #define GPCONF_OPN_bp   4         /* VCP-RxD open */
  #define GPCONF_OPN_bm   (1 << 4)
  #define GPCONF_RIS_bp   6         /* SW0 release event */
  #define GPCONF_RIS_bm   (1 << 6)
  #define GPCONF_FAL_bp   7         /* SW0 push event */
  #define GPCONF_FAL_bm   (1 << 7)

#define PGCONF GPR_GPR1
  #define PGCONF_UPDI_bp  0         /* UPDI active (SIB read successful) or TPI active */
  #define PGCONF_UPDI_bm  (1 << 0)
  #define PGCONF_PROG_bp  1         /* Programmable (memory access unlocked) */
  #define PGCONF_PROG_bm  (1 << 1)
  #define PGCONF_ERSE_bp  2         /* Chip erase completed */
  #define PGCONF_ERSE_bm  (1 << 2)
  #define PGCONF_FAIL_bp  7         /* Initialization failed (timeout) */
  #define PGCONF_FAIL_bm  (1 << 7)

/* The last received data and state of UPDI are stored in the GP Register. */
#define RXSTAT GPR_GPR2
#define RXDATA GPR_GPR3

/*
 * Global struct
 */

typedef struct {
  uint32_t dwDTERate;
  uint8_t  bCharFormat;       /* 0,2 */
  uint8_t  bParityType;       /* 0,1,2 */
  uint8_t  bDataBits;         /* 5,6,7,8,16(9) */
} PACKED LineEncoding_t;

typedef struct {
  union {
    uint8_t bValue;
    struct {
      bool  bStateDTR : 1;    /* DTR -> DSR or CD */
      bool  bStateRTS : 1;    /* RTS -> CTS       */
    };
  };
} PACKED LineState_t;

typedef struct {              /* DTE <-> DCE      */
  union {
    uint8_t bValue;
    struct {
      bool bRxCarrier  : 1;   /* DCD <- CD        */
      bool bTxCarrier  : 1;   /* DSR <- DTR       */
      bool bBreak      : 1;   /* not used         */
      bool bRingSignal : 1;   /*  RI <- RI        */
      bool bFraming    : 1;   /* Frame Error   */
      bool bParity     : 1;   /* Parity Error  */
      bool bOverRun    : 1;   /* Overrun Error */
      bool reserve     : 1;
    };
  };
  uint8_t  reserved;
} PACKED SerialState_t;

typedef struct {
  union {
    uint8_t rawData[540];
    struct {
      uint8_t  token;             /* offset 0 */
      uint8_t  reserve1;
      uint16_t sequence;
      uint8_t  scope;
      uint8_t  cmd;
      union {
        uint8_t data[534];
        struct {  /* CMD=21,23:CMD3_READ,WRITE_MEMORY */
          uint8_t  reserve2;
          uint8_t  bMType;
          uint32_t dwAddr;
          uint32_t dwLength;
          uint8_t  reserve3;
          uint8_t  memData[513];  /* WRITE_MEMORY */
        };
        struct {  /* CMD=1,2:CMD3_GET,SET_PARAMETER */
          uint8_t  reserve4;
          uint8_t  section;
          uint8_t  index;
          uint8_t  length;
          union {
            uint16_t wValue;
            uint8_t  setData[255]; /* SET_PARAMETER */
          };
        };
        struct {  /* CMD3_ERASE_MEMORY */
          uint8_t  reserve5;
          uint8_t  bEType;
          uint32_t dwPageAddr;
        };
        union {
          struct {  /* XPRG_SET_PARAM */
            uint8_t  bType;
            uint8_t  bValue;
          };
          struct {  /* XPRG_CMD_READ_MEM */
            uint8_t  bMType;
            uint32_t dwAddr;
            uint16_t wLength;
          } read;
          struct {  /* XPRG_CMD_WRITE_MEM */
            uint8_t  bMType;
            uint8_t  bPmode;
            uint32_t dwAddr;
            uint16_t wLength;
            uint8_t  memData[64 + 8]; /* ATTiny40=64, Other=16 */
          } write;
        } tpi;
      };
    } out;
    struct {
      uint8_t  reserve6;          /* offset -1 */
      uint8_t  token;             /* offset 0  */
      uint16_t sequence;
      uint8_t  scope;
      union {
        uint16_t res;
        struct {
          uint8_t tpi_cmd;
          uint8_t tpi_res;
        };
      };
      union {
        uint8_t  data[513];       /* READ_MEMORY */
        uint8_t  bStatus;
        uint16_t wValue;
        uint32_t dwValue;
      };
    } in;
  };
} PACKED JTAG_Packet_t;

typedef struct {
  union {
    uint16_t  wRequestType;
    struct {
      uint8_t bmRequestType;
      uint8_t bRequest;
    };
  };
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} PACKED Setup_Packet_t;

typedef struct {
  Setup_Packet_t req_data;
  union {
    uint8_t res_data[256 + 16];
    LineEncoding_t res_encoding;
    struct {
      union {
        uint8_t cci_data[16];
        struct {
          Setup_Packet_t cci_header;
          uint16_t cci_wValue;
        };
      };
      uint8_t dap_data[64];   /* DAP IN/OUT */
      uint8_t cdo_data[64];
      uint8_t cdi_data[128];  /* 64x2 Double buffer */
    };
  };
} PACKED EP_DATA_t;

typedef struct {
  register8_t   FIFO[USB_ENDPOINTS_MAX * 2];  /* FIFO Index Table */
  USB_EP_PAIR_t EP[USB_ENDPOINTS_MAX];        /* USB Device Controller EP */
  _WORDREGISTER(FRAMENUM);                    /* FRAMENUM count */
} PACKED EP_TABLE_t;

/* Mega device descriptor */
typedef struct {
  uint16_t flash_page_size;     // in bytes
  uint32_t flash_size;          // in bytes
  uint32_t dummy1;              // always 0
  uint32_t boot_address;        // maximal (BOOTSZ = 3) bootloader address, in 16-bit words
  uint16_t sram_offset;         // pointing behind IO registers
  uint16_t eeprom_size;
  uint8_t  eeprom_page_size;
  uint8_t  ocd_revision;        // see XML; basically: all other megaAVR devices:  3
  uint8_t  always_one;          // always = 1
  uint8_t  allow_full_page_bitstream; // old AVRs, see XML
  uint16_t dummy2;              // always 0
  uint8_t  idr_address;         // IDR, aka. OCDR
  uint8_t  eearh_address;       // EEPROM access
  uint8_t  eearl_address;
  uint8_t  eecr_address;
  uint8_t  eedr_address;
  uint8_t  spmcr_address;
  uint8_t  osccal_address;
} PACKED Mega_Device_Desc_t;

/* Xmega device descriptor */
typedef struct {
  uint32_t nvm_app_offset;      // NVM offset for application flash
  uint32_t nvm_boot_offset;     // NVM offset for boot flash
  uint32_t nvm_eeprom_offset;   // NVM offset for EEPROM
  uint32_t nvm_fuse_offset;     // NVM offset for fuses
  uint32_t nvm_lock_offset;     // NVM offset for lock bits
  uint32_t nvm_user_sig_offset; // NVM offset for user signature row
  uint32_t nvm_prod_sig_offset; // NVM offset for production sign. row
  uint32_t nvm_data_offset;     // NVM offset for data memory (SRAM + IO)
  uint32_t app_size;            // size of application flash
  uint16_t boot_size;           // size of boot flash
  uint16_t flash_page_size;     // flash page size
  uint16_t eeprom_size;         // size of EEPROM
  uint8_t  eeprom_page_size;    // EEPROM page size
  uint16_t nvm_base_addr;       // IO space base address of NVM controller
  uint16_t mcu_base_addr;       // IO space base address of MCU control
} PACKED Xmega_Device_Desc_t;

/* UPDI device descriptor */
typedef struct {
  uint16_t prog_base;
  uint8_t  flash_page_size;
  uint8_t  eeprom_page_size;
  uint16_t nvm_base_addr;
  uint16_t ocd_base_addr;
  // Configuration below, except for "Extended memory support", is only used by kits with
  // embedded debuggers (XPlained, Curiosity, ...).
  uint16_t default_min_div1_voltage;  // Default minimum voltage for 32M => 4.5V -> 4500
  uint16_t default_min_div2_voltage;  // Default minimum voltage for 16M => 2.7V -> 2700
  uint16_t default_min_div4_voltage;  // Default minimum voltage for 8M  => 2.2V -> 2200
  uint16_t default_min_div8_voltage;  // Default minimum voltage for 4M  => 1.5V -> 1500
  uint16_t pdi_pad_fmax;              // 750
  uint32_t flash_bytes;               // Flash size in bytes
  uint16_t eeprom_bytes;              // EEPROM size in bytes
  uint16_t user_sig_bytes;            // UserSignture size in bytes
  uint8_t  fuses_bytes;               // Fuses size in bytes
  uint8_t  syscfg_offset;             // Offset of SYSCFG0 within FUSE space
  uint8_t  syscfg_write_mask_and;     // AND mask to apply to SYSCFG0 when writing
  uint8_t  syscfg_write_mask_or;      // OR mask to apply to SYSCFG0 when writing
  uint8_t  syscfg_erase_mask_and;     // AND mask to apply to SYSCFG0 after erase
  uint8_t  syscfg_erase_mask_or;      // OR mask to apply to SYSCFG0 after erase
  uint16_t eeprom_base;               // Base address for EEPROM memory
  uint16_t user_sig_base;             // Base address for UserSignature memory
  uint16_t signature_base;            // Base address for Signature memory
  uint16_t fuses_base;                // Base address for Fuses memory
  uint16_t lockbits_base;             // Base address for Lockbits memory
  uint16_t device_id;                 // Two last bytes of the device ID
  // Extended memory support. Needed for flash >= 64kb
  uint8_t  prog_base_msb;             // Extends prog_base, used in 24-bit mode
  uint8_t  flash_page_size_msb;       // Extends flash_page_size, used in 24-bit mode
  uint8_t  address_mode;              // 0x00 = 16-bit mode, 0x01 = 24-bit mode
  uint8_t  hvupdi_variant;            // Indicates the target UPDI HV implementation
} PACKED UPDI_Device_Desc_t;

typedef struct {
  union {
    Mega_Device_Desc_t  Mega;
    Xmega_Device_Desc_t Xmega;
    UPDI_Device_Desc_t  UPDI;
  };
} PACKED Device_Desc_t;

typedef struct {
  size_t (*prog_init)(void);
  size_t (*read_memory)(void);
  size_t (*erase_memory)(void);
  size_t (*write_memory)(void);
} PACKED Command_Table_t;

typedef struct {
  uint16_t wVidPid[2];
  uint32_t dwSerialNumber;
} PACKED User_EEP_t;

/*
 * Global workspace
 */

extern "C" {
  namespace /* NAMELESS */ {

    /* SYSTEM */
    extern jmp_buf TIMEOUT_CONTEXT;
    extern uint8_t _led_mode;

    /* USB */
    extern EP_TABLE_t EP_TABLE;
    extern EP_DATA_t EP_MEM;
    extern Device_Desc_t Device_Descriptor;

    /* Vertual Communication Port */
    #if defined(CONFIG_VCP_9BIT_SUPPORT)
    extern void (*usart_receiver)(void);
    extern void (*usart_transmitter)(void);
    #endif
    extern LineEncoding_t _set_line_encoding;
    extern LineState_t _set_line_state;
    extern uint16_t _send_break;
    extern volatile uint8_t _send_count;
    extern uint8_t _recv_count;
    extern uint8_t _set_config;
    extern volatile uint8_t _sof_count;
    extern uint8_t _set_serial_state;

    /* JTAG packet payload */
    extern JTAG_Packet_t packet;
    extern size_t  _packet_length;
    extern uint8_t _packet_fragment;
    extern uint8_t _packet_chunks;
    extern uint8_t _packet_endfrag;

    /* JTAG parameter */
    extern uint32_t _before_page; /* before flash page section */
    extern uint16_t _vtarget;     /* LSB = 1V / 1000 */
    extern uint16_t _xclk;        /* LSB = 1KHz */
    extern uint8_t _jtag_vpow;    /* 1:VPOW_ON */
    extern uint8_t _jtag_hvctrl;  /* 1:ENABLE */
    extern uint8_t _jtag_unlock;  /* 1:ENABLE */
    extern uint8_t _jtag_arch;    /* 5:ARCH */
    extern uint8_t _jtag_sess;    /* ?:SESSION */
    extern uint8_t _jtag_conn;    /* 8:CONN_UPDI */

    /* UPDI parameter */
    extern Command_Table_t Command_Table;
    extern uint8_t _sib[32];

    /* TPI parameter */
    extern uint8_t _tpi_setmode;
    extern uint8_t _tpi_cmd_addr;
    extern uint8_t _tpi_csr_addr;
    extern uint8_t _tpi_chunks;

  } /* NAMELESS */;
};

namespace JTAG {
  bool dap_command_check (void);
  void jtag_scope_branch (void);
};

namespace NVM::V0 { bool setup (void); };
namespace NVM::V1 { bool setup (void); };
namespace NVM::V2 { bool setup (void); };
namespace NVM::V3 { bool setup (void); };
namespace NVM::V4 { bool setup (void); };
namespace NVM::V5 { bool setup (void); };

namespace PDI {
  /* STUB */
}

namespace SYS {
  void setup (void);
  void LED_HeartBeat (void);
  void LED_Flash (void);
  void LED_Blink (void);
  void LED_Fast (void);
  void reset_enter (void);
  void reset_leave (void);
  void reboot (void);
  bool is_boundary_flash_page (uint32_t _dwAddr);
  uint16_t get_vdd (void);
};

namespace Timeout {
  void setup (void);
  void start (uint16_t _ms);
  void stop (void) __attribute__((used, naked, noinline));
  void extend (uint16_t _ms);
  size_t command (size_t (*func_p)(void), uint16_t _ms = 800);
};

namespace TPI {
  size_t connect (void);
  size_t disconnect (void);
  size_t erase_memory (void);
  size_t read_memory (void);
  size_t write_memory (void);
  size_t jtag_scope_tpi (void);
};

namespace UPDI {
  bool send_break (void);
  bool recv_bytes (uint8_t* _data, size_t _len);
  bool recv (void);
  bool send_bytes (const uint8_t* _data, size_t _len);
  bool send (const uint8_t _data);
  bool recv_byte (uint32_t _dwAddr);
  bool send_byte (uint32_t _dwAddr, uint8_t _data);
  bool is_ack (void);
  bool recv_bytes_block (uint32_t _dwAddr, size_t _wLength);
  bool recv_words_block (uint32_t _dwAddr, size_t _wLength);
  bool send_bytes_block (uint32_t _dwAddr, size_t _wLength);
  bool send_words_block (uint32_t _dwAddr, size_t _wLength);
  bool send_bytes_data (uint32_t _dwAddr, uint8_t* _data, size_t _wLength);
  bool send_bytes_block_slow (uint32_t _dwAddr, size_t _wLength);
  bool nvm_ctrl (uint8_t _nvmcmd);
  bool chip_erase (void);
  bool write_userrow (void);
  size_t read_dummy (void);
  size_t connect (void);
  size_t disconnect (void);
  size_t enter_progmode (void);
  size_t jtag_scope_updi (void);
};

namespace USART {
  void setup (void);
  uint16_t calk_baud_khz (uint16_t _khz);
  void drain (size_t _delay = 0);
  void disable_vcp (void);
  void change_vcp (void);
  void change_updi (void);
  void change_tpi (void);
  void set_line_encoding (LineEncoding_t* _buff);
  void set_line_state (uint8_t _line_state);
  LineEncoding_t& get_line_encoding (void);
  LineState_t get_line_state (void);
};

namespace USB {
  bool is_ep_setup (void);
  bool is_not_dap (void);
  void ep_dpi_pending (void);
  void ep_cdo_pending (void);
  void complete_dap_out (void);
  void cci_break_count (void);
  void read_drop (void);
  void vcp_receiver (void);
  void vcp_receiver_9bit (void);
  void vcp_transceiver (void);
  void vcp_transceiver_9bit (void);
  void setup_device (bool _force = false);
  void handling_bus_events (void);
  void handling_control_transactions (void);
};

// end of header

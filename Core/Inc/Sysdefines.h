#ifndef Sysdefines_H
#define Sysdefines_H


// includes
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx.h"
#include "mdi.h"

// System constants definition

/*************************
Configure Pins of STM32
**************************/
//#pragma anon_unions


// MSDA 
#define MSDA_PORT GPIOB
#define MSDA_PIN LL_GPIO_PIN_13
#define MSDA_PIN_number 13

// MSCL - PIOA_15
#define MSCL_PORT GPIOB
#define MSCL_PIN LL_GPIO_PIN_14
#define MSCL_PIN_number 14

// BAT / Power
#define BAT_PORT GPIOB
#define BAT_PIN LL_GPIO_PIN_12
#define BAT_PIN_number 12



// Configs for HW
#define USE_CRC32_HW	1		// Use HW CRC32 Calculation Unit of STM32F1


/* Configs from github PCF project */
#define BOARD_OSC_STARTUP_US    	15625UL

#define MDI_SND_BITS         			0x08
#define MDI_RCV_BITS         			0x08
#define DEV_ACK              			0x55
#define TIMEOUT_MSCL_LOW     			((73 + 62 + 1) * 8)  // Tdhdl + Tdldx + Tdxcl (us)
#define TIMEOUT_MSCL_HIGH    			200 + 480					// Twup + Twlch
#define TIMEOUT_DEV_ACK      			200000             // 200ms
#define TIMEOUT_OPS_DONE     			1000000            // 1000ms
#define TIMEOUT_SCL_RISE_WD_DIS		400									// 400�s		timeout after c_trace when PCF must respond with confirmation pulse (WD disable)
#define TIMEOUT_SCL_FALL_WD_DIS		50									// 50�s			max length of PCF c_trace WD_disable confirmation pulse
#define T_DLY_SDA_HIGH_AFT_PON		500            // 500us (225...725us possible). Time after BATT_ON when MSDA is set high. Needed for PCF7945C05 MDI init


#define EROM_SIZE            8192
#define EROM_PAGE_SIZE       32
#define EROM_PAGE_SIZE64		 64
#define EEROM_SIZE           1024
#define EEPROM_PAGE_SIZE     4
#define BUF_SIZE             8224

#define TIMES_PROTECT        (IFLASH1_ADDR + IFLASH1_SIZE - IFLASH1_PAGE_SIZE)
#define MAX_TIMES            0x32


/*************************
Definition of Enums 
***************************/

enum MDI_DIR_E {
	RECV = 0x10,
	SEND = 0x20
};

enum MDI_STATUS_E {
	INIT 						= 0x09,
	IDLE 						= 0x0A,
	BUSY 						= 0x0B,
	WAIT_LAST_PULSE = 0x0C,
	DONE 						= 0x0D
};

/*
// Attention: Defined also in mdi.h
enum MDI_DEVICETYPE_E {
	F26A0700 						= 0x00,
	PCF7945 						= 0x0A
};
*/

enum MDI_COMMAND_E {
	C_GO = 0x01,
	C_TRACE = 0x02,
	C_GETDAT = 0x03,
	C_SETDAT = 0x04,
	C_SETPC = 0x05,
	C_RESET = 0x06,
	C_SETBRK = 0x07,
	C_ER_EROM = 0x08,
	C_WR_EROM = 0x09,
	C_WR_EEPROM = 0x0A,
	C_WR_EROM_B = 0x0B,
	C_SIG_ROM = 0x0C,
	C_SIG_EROM = 0x0D,
	C_EE_DUMP = 0x0E,
	C_ER_DUMP = 0x0F,
	C_SIG_EE = 0x11,
	C_PROTECT = 0x12,
	C_PROG_CONFIG = 0x14,
	C_WR_EROM64 = 0x18	
};

struct chip_data_s {
	unsigned short erom_start;	  // erom start address
	unsigned short eeprom_start;  // eeprom start address
	unsigned short erom_len;	  // eeprom data length
	unsigned short eeprom_len;	  // eeprom data length
	unsigned char erom[8224];     // data 8K + 32Bdummy
	unsigned char eeprom[1056];   // data 1K + 32Bdummy
	unsigned long erom_crc32;     // crc32 of erom
	unsigned long eeprom_crc32;	  // crc32 of eeprom
};

struct uart_ops_s {
	unsigned char ops;			  // operations
	union {
		unsigned char addresses[2];	  // address
		unsigned short address;
	};
	unsigned char *data;	      // buffer
	union {						  // data length
		unsigned char lens[2];	 
		unsigned short len;
	};
	union {
		unsigned char crc32s[4];		  // crc32
		unsigned long crc32;
	};
	unsigned char status;		  // status
};

struct mdi_data_s {
	enum MDI_DEVICETYPE_E type;		// type of mdi device
	enum MDI_DIR_E dir;			  // mdi transmission direction
	enum MDI_STATUS_E status;		  // mdi status ToDo: Change to enum type
	union {
		struct {
			enum MDI_COMMAND_E command;	      // mdi command
			unsigned char para;		      // mdi parameter
			unsigned char buf[BUF_SIZE];  //  mdi send data	 max page size is 32bytes + 32Bdummy
		};
		unsigned char data[BUF_SIZE + 2];
	};
	unsigned long transfer;		  // data transfered
	unsigned char *data_ptr;	// pointer to the data to be sent
};

extern struct mdi_data_s mdi;



enum Logic_Level {
	LOW = 0,
	HIGH = 1
};


enum status_code {
	STATUS_OK               =  0, //!< Success
	STATUS_ERR_BUSY         =  0x19,
	STATUS_ERR_DENIED       =  0x1C,
	STATUS_ERR_TIMEOUT      =  0x12,
	ERR_IO_ERROR            =  -1, //!< I/O error
	ERR_FLUSHED             =  -2, //!< Request flushed from queue
	ERR_TIMEOUT             =  -3, //!< Operation timed out
	ERR_BAD_DATA            =  -4, //!< Data integrity check failed
	ERR_PROTOCOL            =  -5, //!< Protocol error
	ERR_UNSUPPORTED_DEV     =  -6, //!< Unsupported device
	ERR_NO_MEMORY           =  -7, //!< Insufficient memory
	ERR_INVALID_ARG         =  -8, //!< Invalid argument
	ERR_BAD_ADDRESS         =  -9, //!< Bad address
	ERR_BUSY                =  -10, //!< Resource is busy
	ERR_BAD_FORMAT          =  -11, //!< Data format not recognized
	ERR_NO_TIMER            =  -12, //!< No timer available
	ERR_TIMER_ALREADY_RUNNING   =  -13, //!< Timer already running
	ERR_TIMER_NOT_RUNNING   =  -14, //!< Timer not running
	ERR_ABORTED             =  -15, //!< Operation aborted by user
	/**
	 * \brief Operation in progress
	 *
	 * This status code is for driver-internal use when an operation
	 * is currently being performed.
	 *
	 * \note Drivers should never return this status code to any
	 * callers. It is strictly for internal use.
	 */
	OPERATION_IN_PROGRESS	= -128,
};

typedef enum status_code status_code_t;


#endif

#ifndef Sysdefines_H
#define Sysdefines_H


// includes
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx.h"



/*************************
Global Constants
**************************/
#define PCF_TOOL_SW_VERSION  {1,1,1} // Version x.y.z

extern __attribute__((section(".shared_mem.VERSION_NUMBER"))) uint8_t PCF_Tool_SW_Version[3];


/*************************
Configure Pins of STM32
**************************/
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


/*************************
Specific configurations for STM32 HW
**************************/
// Configs for HW
#define USE_CRC32_HW	1		// Use HW CRC32 Calculation Unit of STM32F1


/*************************
Config for MDI protocol
**************************/
#define MDI_SND_BITS         			0x08
#define MDI_RCV_BITS         			0x08
#define DEV_ACK              			0x55
#define TIMEOUT_MSCL_LOW     			((73 + 62 + 1) * 8)  // Tdhdl + Tdldx + Tdxcl (us)
#define TIMEOUT_MSCL_HIGH    			200 + 480			// Twup + Twlch
#define TIMEOUT_DEV_ACK      			200000             	// 200ms
#define TIMEOUT_OPS_DONE     			1000000           	// 1000ms
#define TIMEOUT_SCL_RISE_WD_DIS			400					// 400�s		timeout after c_trace when PCF must respond with confirmation pulse (WD disable)
#define TIMEOUT_SCL_FALL_WD_DIS			50					// 50�s			max length of PCF c_trace WD_disable confirmation pulse
#define T_DLY_SDA_HIGH_AFT_PON			500            		// 500us (225...725us possible). Time after BATT_ON when MSDA is set high. Needed for PCF7945C05 MDI init
#define T_DLY_SDA_LOW_AFT_SDA_HIGH  	200


/*************************
Memory configuration (PCF memory / PCF_Tool memory)
**************************/
#define EROM_SIZE            		8192
#define EROM_PAGE_SIZE       		32
#define EROM_PAGE_SIZE64	 		64
#define EEROM_SIZE           		1024
#define EEPROM_PAGE_SIZE     		4
#define BUF_SIZE             		8224


/*************************
Definition of common used Enums
***************************/
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

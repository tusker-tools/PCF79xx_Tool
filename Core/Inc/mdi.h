#ifndef MDI_H
#define MDI_H


#include "stdlib.h"


enum MDI_RET_STATUS_E{
	OK = 0,
	TOO_LESS_DATA = 1,
	ERR_STATE_RCV = 2,		// Error-status received
	RCV_TOUT = 4,			// No clock edges before timeout time
	PROG_SPCL_PG_FAIL = 8,	// No confirmation received after special page 127 was programmed
	ERR = 16				// General Error
};

// Attention: Defined also in Sysdefines
enum MDI_DEVICETYPE_E {
	F26A0700,
	PCF7945
};

enum MDI_DIR_E {
	RECV = 0x10,
	SEND = 0x20
};

enum MDI_STATUS_E {
	INIT 				= 0x09,
	IDLE 				= 0x0A,
	BUSY 				= 0x0B,
	WAIT_LAST_PULSE 	= 0x0C,
	DONE 				= 0x0D
};

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
	C_SIG_EEROM = 0x11,
	C_SIG_EROM_NORM = 0x1D,
	C_SIG_XROM_or_EEROM_NORM = 0x1E,
	C_EE_DUMP = 0x0E,
	C_ER_DUMP = 0x0F,
	C_SIG_EE = 0x11,
	C_PROTECT = 0x12,
	C_PROG_CONFIG = 0x14,
	C_WR_EROM64 = 0x18
};

struct mdi_data_s {
	enum MDI_DEVICETYPE_E type;		// type of mdi device
	enum MDI_DIR_E dir;			  // mdi transmission direction
	enum MDI_STATUS_E status;		  // mdi status
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

/* Magic string needed for deleting protected PCF or read EROM_NORM operation */
extern const unsigned char magic[16];

/* Function declarations */
void exti_handler(void);
int recv_data(unsigned long len);
int send_data(unsigned char *data_ptr, unsigned long len);
int send_mdi_cmd(unsigned char byte);
int enter_monitor_mode(void);

#endif

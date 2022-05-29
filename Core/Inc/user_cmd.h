#ifndef USER_CMD_H
#define USER_CMD_H

#include "stdlib.h"

enum USER_OPS_E {
	CONNECT = 					0x09,
	ERASE = 					0x0A,
	PROTECT = 					0x1A,
	SWITCH_BTLD_MODE =			0x2A,
	PCF_RUN_PROGRAM =			0x3A,
	PCF_RESET =					0x4A,
	PCF_PWR_ON =				0x5A,
	WRITE_ER_BUF = 				0x2B,
	PROGRAM_ER64 = 				0x0B,
	PROGRAM_ER = 				0x4B,
	WRITE_EE_BUF = 				0x3B,
	PROGRAM_EE = 				0x1B,
	PROGRAM_EE_MANUAL = 		0x5B,
	PROGRAM_EE_WO_SPCL_PAGE = 	0x7B,
	PROGRAM_SPECIAL_BYTES = 	0x6B,
	WRITE_PCF_REG =				0x8B,
	VERIFY_ER = 				0x0C,
	VERIFY_ER_BUF = 			0x2C,
	VERIFY_EE = 				0x1C,
	VERIFY_EE_BUF = 			0x3C,
	READ_ER = 					0x0D,
	READ_EE = 					0x1D,
	READ_ER_BUF = 				0x2D,
	READ_EE_BUF = 				0x3D,
	READ_ER_BUF_CKS = 			0x4D,
	READ_PCF_MEM_CKS = 			0x5D,
	READ_TOOL_SW_VERSION = 		0x6D
};

enum USER_CMD_STATUS_E {
	SUCCESSFULL = 0x06,
	CONNECT_ERR = 0x90,
	CONNECT_ERR_PROT_MODE = 0x91,
	RECV_TOUT = 0x92,
	UNEXPECTED_RESPONSE = 0x93,
	ERASE_ERR = 0xA0,
	PROTECT_ERR = 0xA1,
	BOOTKEY_NOT_WRITTEN = 0xA2,
	WRITE_ER_BUF_ERR = 0xB2,
	PROGRAM_ER_ERR = 0xB0,
	PROGRAM_ER64_ERR = 0xB4,
	PROGRAM_SPECIAL_BYTES_ERR = 0xB6,
	WRITE_EE_BUF_ERR = 0xB3,
	PROGRAM_EE_ERR = 0xB1,
	VERIFY_ER_BUF_ERR = 0xC2,
	VERIFY_ER_ERR = 0xC0,
	VERIFY_EE_BUF_ERR = 0xC3,
	VERIFY_EE_ERR = 0xC1,
	READ_ER_BUF_ERR = 0xD2,
	READ_ER_ERR = 0xD0,
	READ_EE_BUF_ERR = 0xD3,
	READ_EE_ERR = 0xD1,
	COMMAND_ERR = 0x66
};


struct user_cmd_s {
	unsigned char ops;			  		// operations
	union {
		unsigned char addresses[2];		// address
		unsigned short address;
	};
	unsigned char *data;	      		// buffer
	union {						  		// data length
		unsigned char lens[2];
		unsigned short len;
	};
	union {
		unsigned char crc32s[4];		// crc32
		unsigned long crc32;
	};
	unsigned char status;		  		// status
};


extern struct chip_data_s chip_data;
extern struct user_cmd_s user_op;
extern enum MDI_DEVICETYPE_E mdi_type;


int ui_cmd_recv(void);
int ui_cmd_handler(void);

#endif

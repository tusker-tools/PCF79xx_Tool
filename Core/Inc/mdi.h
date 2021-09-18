#ifndef MDI_H
#define MDI_H


#include "stdlib.h"


enum MDI_RET_STATUS_E{
	OK = 0,
	TOO_LESS_DATA = 1,
	ERR_STATE_RCV = 2,		// Error-status received
	RCV_TOUT = 4,			// No clock edges before timeout time
};

// Attention: Defined also in Sysdefines
enum MDI_DEVICETYPE_E {
	F26A0700 						= 0x00,
	PCF7945 						= 0x0A
};

void data_handler(void);
int recv_data(unsigned long len);
int send_data(unsigned char *data_ptr, unsigned long len);
int send_mdi_cmd(unsigned char byte);
int enter_monitor_mode(enum MDI_DEVICETYPE_E mditype);

#endif

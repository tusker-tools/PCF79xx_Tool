#ifndef MDI_H
#define MDI_H


#include "stdlib.h"


enum MDI_RET_STATUS_E{
	OK = 0,
	TOO_LESS_DATA = 1,
	ERR_STATE_RCV = 2,		// Error-status received
	RCV_TOUT = 4,			// No clock edges before timeout time
	PROG_SPCL_PG_FAIL = 8	// No confirmation received after special page 127 was programmed
};

// Attention: Defined also in Sysdefines
enum MDI_DEVICETYPE_E {
	F26A0700,
	PCF7945
};

void data_handler(void);
int recv_data(unsigned long len);
int send_data(unsigned char *data_ptr, unsigned long len);
int send_mdi_cmd(unsigned char byte);
int enter_monitor_mode(void);

#endif

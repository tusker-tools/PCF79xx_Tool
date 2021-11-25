#include <stdio.h>
#include <string.h>
#include "user_cmd.h"
#include "rom.h"
#include "Stubs.h"
#include "Sysdefines.h"
#include "Utility.h"
#include "mdi.h"
#include "dwt_stm32_delay.h"
#include "crc.h"


// Function prototype
int pcf_erase(void);


/*
 * Initialize PCF's monitor- and download interface (MDI)
 * 1. Send init-pattern (by setting electrical levels of MSCL and MSCL lines accordingly)
 * 2a)Start PCF erase procedure if requested by user command or
 * 2b)Send c_trace command to disable watchdog and halt. Check confirmation of PCF.
 * return -1:error 0:success
 */
int pcf_init_mdi(void)
{
	int status = 0;
	uint16_t t_tout_confirm_disable_wd = 0;

	status = enter_monitor_mode();


	if(status != 0){
		return status;
	}

	active_MSCL_rising_edge_IT(0);
	LL_GPIO_TogglePin(GPIOA,LL_GPIO_PIN_3);
	delay_us(5);
	
	/* Check if argument for activating connect + erase sequence received */
	if (user_op.address == 1){		
		pcf_erase();
	}
	else{
		status |= send_mdi_cmd(C_TRACE);
			
		/* Detection of PCF Watchdog Disable confirmation (response to C_TRACE command) */
		while(MSCL() == 0)
		{
			/* wait unitl MSCL is set signalizing that watchdog is disabled */
			delay_us(1);	// ToDo: Check if delay is necessary here
			if(++t_tout_confirm_disable_wd > TIMEOUT_SCL_RISE_WD_DIS)
			{
				LL_GPIO_TogglePin(GPIOA,LL_GPIO_PIN_3);
				return status = CONNECT_ERR_PROT_MODE;		// if timout occured, device is in protected mode
			}
		}
		t_tout_confirm_disable_wd = 0;
		while(MSCL() == 1) // ToDo: Improve accuracy of delay. Currently ~2x as long as speficied by timeout const
		{
			delay_us(1);		
			if(++t_tout_confirm_disable_wd > TIMEOUT_SCL_FALL_WD_DIS)
			{
				LL_GPIO_TogglePin(GPIOA,LL_GPIO_PIN_3);
				return status = CONNECT_ERR_PROT_MODE;		// if timout occured, device is in protected mode
			}
		}
			
	
		status |= recv_data(0x02);
		
		active_MSCL_rising_edge_IT(0);
	}
	return status;
	
}

/*
 * Erase EROM of PCF
 * return -1:error 0:success
 */
int pcf_erase(void)
{																						
	int status = 0;
	
	LL_GPIO_TogglePin(GPIOA,LL_GPIO_PIN_3);
	
	/* send command */	
	status = send_mdi_cmd(C_ER_EROM);
	
	LL_GPIO_TogglePin(GPIOA,LL_GPIO_PIN_3);
	
	/* check 0x88 */
	status |= recv_data(0x01);
	
	
	if (mdi.data[0] != 0x88)
		return -1;
	
	/* send 16byte magic pattern for unlocking erase functionality */
	memcpy(&mdi.data[0], magic, sizeof(magic));
	status |= send_data(mdi.data, sizeof(magic));
	
	delay_ms(100);
	
	/* check eecon status register of PCF */
	status |= recv_data(0x01);
	if ((mdi.data[0] & 0xC0) != 0x00)
		return -1;
		
	return status;
}

/*
 * Writes EROM buffer sent by user to PCF_Tool internal memory
 * return -1:error 0:success
 */
int write_erom_buf(void)
{
	unsigned long crc32 = 0;
	
	if (user_op.data == NULL)
		return -1;
	
	if (user_op.address != 0)
		return -1;
	
	if (user_op.len > EROM_SIZE)
		return -1;
		
	memset(chip_data.erom, 0x00, EROM_SIZE); 
	chip_data.erom_len = user_op.len;
	chip_data.erom_start = user_op.address;
	chip_data.erom_crc32 = user_op.crc32;
	memcpy(chip_data.erom, user_op.data, user_op.len);

    if (chip_data.erom_crc32 == 0x00000000)
		return 0;
	   
	crc32 = crc32_caculate(chip_data.erom, chip_data.erom_len);
	if (crc32 != chip_data.erom_crc32)
		return -1;
	
   return 0;
}
 
/*
 * Program EROM of PCF in pages sized 32 bytes
 * Important Hint: Only the memory regioin 0x0000...0x1000 - can be programmed with this command). For example PCF7945 can
 * not fully be programmed. For full memory, use write_erom64 function
 * return -1:error 0:success
 */
int program_erom(void)
{
	unsigned char start_page = chip_data.erom_start / EEPROM_PAGE_SIZE;
	unsigned short pages = chip_data.erom_len / EROM_PAGE_SIZE;
	int status = 0;	
		
	if ((chip_data.erom_start +  chip_data.erom_len) > EROM_SIZE)
		return -1;
	
	pages = (pages > 0) ? pages : 1;		
	for (unsigned int i = 0; i < pages; i++) {
		/* send command */
		memcpy(&mdi.buf[0], &chip_data.erom[(start_page + i) * EROM_PAGE_SIZE], EROM_PAGE_SIZE);
		status |= send_mdi_cmd(C_WR_EROM);
		status |= send_mdi_cmd(start_page + i);
		status |= send_data(mdi.buf, EROM_PAGE_SIZE);

		delay_ms(5);
	
		/* check eecon */	
		status |= recv_data(0x01);
		if (status < 0)
			break;
		if ((mdi.data[0] & 0xC0) != 0x00)
			return -1;
	}
	
	return status;	
}

/*
 * Program EROM of PCF in pages sized 64byte
 * Important Hint: Only this command is able to program complete memory - 0x0000...0x2000 of PCF7945
 * return -1:error 0:success
 */
int program_erom64(void)
{
	unsigned char start_page = chip_data.erom_start / EEPROM_PAGE_SIZE;
	unsigned short pages = chip_data.erom_len / EROM_PAGE_SIZE64;
	int status = 0;	
		
	if ((chip_data.erom_start +  chip_data.erom_len) > EROM_SIZE)
		return -1;
	
	pages = (pages > 0) ? pages : 1;		
	for (unsigned int i = 0; i < pages; i++) {
		/* send command */
		memcpy(&mdi.buf[0], &chip_data.erom[(start_page + i) * EROM_PAGE_SIZE64], EROM_PAGE_SIZE64);
		status |= send_mdi_cmd(C_WR_EROM64);
		status |= send_mdi_cmd(start_page + i);
		status |= send_data(mdi.buf, EROM_PAGE_SIZE64);
	
		/* 4.8ms enough ? */	
		delay_ms(5);
	
		/* check eecon */	
		status |= recv_data(0x01);
		if (status < 0)
			break;
		if ((mdi.data[0] & 0xC0) != 0x00)
			return -1;
	}
	
	return status;	
}


/*
 * Writes EEROM buffer sent by user to PCF_Tool's internal memory
 * return -1:error 0:success
 */
int write_eerom_buf(void)
{
	unsigned long crc32 = 0;
	
	if (user_op.address != 0)
		return -1;
		
	if (user_op.len > EEROM_SIZE)
		return -1;
		 
	memset(chip_data.eeprom, 0x00, EEROM_SIZE); 
	chip_data.eeprom_len = user_op.len;
	chip_data.eeprom_start = user_op.address;
	chip_data.eeprom_crc32 = user_op.crc32;
	memcpy(chip_data.eeprom, user_op.data, user_op.len);
   
    if (chip_data.eeprom_crc32 == 0x00000000)
		return 0;
	
	crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
	if (crc32 != chip_data.eeprom_crc32)
		return -1;
	
   return 0;
}

/*
 * Program special pages of PCF.
 * EEROM Page 127 contains partly write only data. Thus programming byte 2 and 3 requires special command
 */
int ee_prog_conf(unsigned char page)
{
	int status = 0;
	unsigned char byte_2_3[2];	// definition of array for byte 2 and 3 of spacial page 127
	
	if (page != 127) 
		return -1;

	if(user_op.ops != PROGRAM_SPECIAL_BYTES)
	{
		/* command issued by write_eerom command. Take take data argument from eerom chip data array */
		byte_2_3[0] = chip_data.eeprom[127 * 4 + 2];
		byte_2_3[1] = chip_data.eeprom[127 * 4 + 3];
	}
	else
	{
		/* command issued direcly by ui command "PROGRAM_SPECIAL_BYTES". Take Length as argument */
		byte_2_3[0] = user_op.addresses[0];
		byte_2_3[1] = user_op.addresses[1];
	}

	/* send cmd to PCF */
	status |= send_mdi_cmd(C_PROG_CONFIG);

	/* send page 127 byte 3 */
	status |= send_data(byte_2_3, 2);
	
	/* Receive status feedback */
	status |= recv_data(0x01);

	if( status != 0){
		status |= PROG_SPCL_PG_FAIL;
		return status;
	}
	else{
		/* check eecon */
		if ((mdi.data[0] & 0xC0) != 0x00){
			return ERR_STATE_RCV;
		}
		else{
			return OK;
		}
	}
}


/*
 * Writes the EEROM of PCF
 * return -1:error 0:success
 */
int program_eerom(void)
{
	volatile unsigned char start_page;
	volatile unsigned short pages;
	int status = 0;
	
  if(user_op.address == 0){
		/* take address from write_ee_buf cmd if it was not specified in write_ee cmd */
		start_page = chip_data.eeprom_start / EEPROM_PAGE_SIZE;
	}
	else{
		/* take address from write_ee cmd if it is not left empty */
		start_page = user_op.address / EEPROM_PAGE_SIZE;
	}
	
  if(user_op.len == 0){
		/* take length from write_ee_buf cmd if it was not specified in write_ee cmd */
		pages = chip_data.eeprom_len / EEPROM_PAGE_SIZE;
	}
	else{
		/* take length from write_ee cmd if it is not left empty */
		pages = user_op.len / EEPROM_PAGE_SIZE;
	}
	
	
	if ((chip_data.eeprom_start + chip_data.eeprom_len) > EEROM_SIZE)
		return -1;
	
	pages = (pages > 0) ? pages : 1;				// at least one page must be written. ToDo: Error Message to user
	for (unsigned int i = 0; i < pages; i++)
	{
		/*  Check if special pages shall be written. Page 125 is read only. For page 127, only byte 2 and 3 is writable  */
		if (((start_page + i) == 0) || (((start_page + i) >= 125) && ((start_page + i) <= 127)))
		{
			if ((start_page + i) == 127) 
			{
				status |= ee_prog_conf(start_page + i);

				/* If special pages programming failed, PCF might fall into an error state. Thus, MDI must be reinitialized */
				if(status & PROG_SPCL_PG_FAIL)
				{
					status = PROG_SPCL_PG_FAIL;		// reset err bits other then PROG_SPCL_PG_FAIL (redundand info not needed), as this would lead to unwanted error detection later
					pcf_init_mdi();		// ToDo: Evaluate return value and create corresponding reaction to it
				}

			}
			continue;		// exit current for-loop iteration and continue with next iteration
		}
		if (i != 0)
			memcpy(&mdi.buf[0], &mdi.buf[(start_page + i) * EEPROM_PAGE_SIZE], EEPROM_PAGE_SIZE); // ToDo: Abolish copying to mdi.buf. Read directly from chipdata instead
				
		/* send write eerom command */
		status |= send_mdi_cmd(C_WR_EEPROM);

		/* send eerom address (page nr) */
		status |= send_mdi_cmd(start_page + i);

		/* send data */
		status |= send_data(mdi.buf, EEPROM_PAGE_SIZE);
		
		delay_ms(5);
	
		/* check eecon */	
		status |= recv_data(0x01);
		if (status != OK && status != PROG_SPCL_PG_FAIL)
			return status;
			
		if ((mdi.data[0] & 0xC0) != 0x00)
			status |= ERR_STATE_RCV;
	}
	
	return status;	
}

/*
 * !!! Function is under development!!!
 * Writes EEROM of PCF with respect to the start page and lenth specified by user command.
 *
 * return -1:error 0:success
 */
int program_eerom_wo_spcl_page(void)
{
	volatile unsigned char start_page;
	volatile unsigned short pages;
	int status = 0;

  if(user_op.address == 0){
		// take address from write_ee_buf cmd if it was not specified in write_ee cmd
		start_page = chip_data.eeprom_start / EEPROM_PAGE_SIZE;
	}
	else{
		// take address from write_ee cmd if it is not left empty
		start_page = user_op.address / EEPROM_PAGE_SIZE;
	}

  if(user_op.len == 0){
		// take length from write_ee_buf cmd if it was not specified in write_ee cmd
		pages = chip_data.eeprom_len / EEPROM_PAGE_SIZE;
	}
	else{
		// take length from write_ee cmd if it is not left empty
		pages = user_op.len / EEPROM_PAGE_SIZE;
	}


	if ((chip_data.eeprom_start + chip_data.eeprom_len) > EEROM_SIZE)
		return -1;

	pages = (pages > 0) ? pages : 1;				// at least one page must be written. ToDo: Error Message to user
	for (unsigned int i = 0; i < pages; i++) {

		/*  Treatment for writing special pages */
		if (((start_page + i) == 0) || (((start_page + i) >= 125) && ((start_page + i) <= 127)))
		{

			continue;		// exit current for-loop iteration and continue with next iteration
		}
		if (i != 0)
			memcpy(&mdi.buf[0], &mdi.buf[(start_page + i) * EEPROM_PAGE_SIZE], EEPROM_PAGE_SIZE); // ToDo: Abolish copying to mdi.buf. Read directly from chipdata instead

		/* send write eerom command */
		status |= send_mdi_cmd(C_WR_EEPROM);

		/* send eerom address (page nr) */
		status |= send_mdi_cmd(start_page + i);

		/* send data */
		status |= send_data(mdi.buf, EEPROM_PAGE_SIZE);

		/* 4.8ms enough ? */
		delay_ms(5);

		/* check eecon */
		status |= recv_data(0x01);
		if (status != OK && status != PROG_SPCL_PG_FAIL)
			return status;

		if ((mdi.data[0] & 0xC0) != 0x00)
			status |= ERR_STATE_RCV;
	}

	return status;
}

/*
 * Writes EEROM of PCF without handling of special (read-only) pages except Pg0 (contains IDE)
 * User must ensure that the location to write data to is writable.
 *
 * return -1:error 0:success
 */
int program_eerom_manual(void)
{

	unsigned char start_page = chip_data.eeprom_start / EEPROM_PAGE_SIZE;

	unsigned short pages = chip_data.eeprom_len / EEPROM_PAGE_SIZE;
	int status = 0;

	if ((chip_data.eeprom_start + chip_data.eeprom_len) > EEROM_SIZE)
		return -1;
		
	for (unsigned int i = 0; i < pages; i++) {
		if ((start_page + i) == 0 ) {
			continue;		// Page 0 (byte 0...3) can not be written as it contains the IDE
		}
			
		if (i != 0)
			memcpy(&mdi.buf[0], &mdi.buf[(start_page + i) * EEPROM_PAGE_SIZE], EEPROM_PAGE_SIZE);	// copy specific page (4 byte) to buffer bottom
				
		/* send command */
		status |= send_mdi_cmd(C_WR_EEPROM);
		status |= send_mdi_cmd(start_page + i);
		status |= send_data(mdi.buf, EEPROM_PAGE_SIZE);
		
		/* 4.8ms enough ? */	
		delay_ms(5);
	
		/* check eecon */	
		status |= recv_data(0x01);
		if (status < 0)
			return -1;
			
		if ((mdi.data[0] & 0xC0) != 0x00)
			return -1;
	}
	
	return status;	
}


/*
 * Reads the EROM content stored in the tool's buffer and outputs it to the user interface.
 * If related CRC32 value is also stored in buffer, the CRC32 of the data is calculated and compared
 * against the stored value.
 *
 * return -1:Stored CRC32 does not match with actual CRC32  0:Stored CRC32 matches with actual CRC32
 */
int read_erom_buf(void)
{
	unsigned long crc32 = 0;
	
	if (chip_data.eeprom_crc32 != 0x00000000) {
		crc32 = crc32_caculate(chip_data.erom, chip_data.erom_len);
		if (crc32 != chip_data.erom_crc32)
			return -1;
	}

	SendBytesUsb(chip_data.erom, EROM_SIZE, UINT32_MAX);

   return 0;
}

/*
 * Reads the whole EROM of PCF and outputs it to the user interface.
 *
 * return -1:error 0:success
 */
int read_erom(void)
{
	int status = 0;

	/* send command */
	send_mdi_cmd(C_ER_DUMP);
	
	/* check eecon */	
	status = recv_data(EROM_SIZE);
	
	if (status == 0)

	SendBytesUsb(mdi.data, EROM_SIZE, UINT32_MAX);
	return status;	
}

/*
 * Reads the EEROM content stored in the tool's buffer and outputs it to the user interface.
 * If related CRC32 value is also stored in buffer, the CRC32 of the data is calculated and compared
 * against the stored value.
 *
 * return -1:Stored CRC32 does not match with actual CRC32  0:Stored CRC32 matches with actual CRC32
 */
int read_eerom_buf(void)
{
	unsigned long crc32 = 0;

	if (chip_data.eeprom_crc32 != 0x00000000) {	
		crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
		if (crc32 != chip_data.eeprom_crc32)
			return -1;
	}

   SendBytesUsb(chip_data.eeprom, EEROM_SIZE, UINT32_MAX);
   return 0;
}

/*
 * Reads the whole EEROM of PCF and outputs it to the user interface.
 * If size specified by user cmd is zero, then standard size is assumed
 *
 * return -1:error 0:success
 */
int read_eerom(void)
{
	int status = 0;
	
	/* send command */
	send_mdi_cmd(C_EE_DUMP);
	
	if(user_op.len == 0)
	{
		status = recv_data(EEROM_SIZE);
		
		if (status == 0) {
			revert(mdi.data, EEROM_SIZE);	
			SendBytesUsb(mdi.data, EEROM_SIZE, UINT32_MAX);
		}
		else if(status == TOO_LESS_DATA){
			revert(mdi.data, EEROM_SIZE);	
			SendBytesUsb(mdi.data, mdi.transfer, UINT32_MAX);
		}
	}
	else{
		status = recv_data(user_op.len);
		
		if (status == 0) {
			revert(mdi.data, user_op.len);	
			SendBytesUsb(mdi.data, user_op.len, UINT32_MAX);
		}
		else if(status == TOO_LESS_DATA){
			revert(mdi.data, user_op.len);	
			SendBytesUsb(mdi.data, mdi.transfer, UINT32_MAX);
		}
	}
	
	return status;	
}

/*
 * Checks, if the actual CRC32 signature of the EROM data in tool's buffer matches the CRC32 send
 * by user command. This implies that the user has to send 4 bytes CRC32 to compare in the data field.
 *
 * return -1:CRC32 not matching 0:CRC32 matching
 */
int verify_erom_buf(void)
{
	unsigned long crc32 = 0;
	unsigned crc32_expect = 0;
	
	if (user_op.data == NULL)
		return -1;
	
	crc32_expect = user_op.data[0] | (user_op.data[1] << 8) | (user_op.data[2] << 16) | (user_op.data[3] << 24);
	crc32 = crc32_caculate(chip_data.erom, chip_data.erom_len);
	if (crc32 != crc32_expect)
		return -1;
	
   return 0;
}

/*
 * Reads EROM from PCF and compares it to the EROM data stored in tool's buffer.
 *
 * return -1:data is different 0:data is the same
 */
int verify_erom(void)
{
	int status = 0;

	if (user_op.len > EROM_SIZE)
		return -1;
	 
	/* send command */
	status = send_mdi_cmd(C_ER_DUMP);
	
	/* check eecon */	
	status |= recv_data(EROM_SIZE);
	if (status < 0)
		return -1;
			
	/* do not compare the last byte */
	for (unsigned short i = 0; i < user_op.len - 1; i++) {
		if (chip_data.erom[i] != mdi.data[i])
			return -1;	
	}
	
	return status;	
}

/*
 *	Calculates the checksum of the erom data stored in tool's buffer and compares it to the target checksum
 *	sent by the client. If there was no target checksum provided by the client (checksum field 0) then no
 *	check is performed an "checksum ok" will be returned.
 *
 * return -1:checksum not matching 0:checksum ok
 */
int check_erom_buf(void)
{
	unsigned long crc32 = 0;

	if (chip_data.erom_crc32 == 0x00000000)
		return 0;

	crc32 = crc32_caculate(chip_data.erom, chip_data.erom_len);
	if (crc32 != chip_data.erom_crc32)
		return -1;

	return 0;
}

/*
 *	Calculates the checksum of the EEROM data stored in tool's buffer and compares it to the target checksum
 *	sent by the client. If there was no target checksum provided by the client (checksum field 0) then no
 *	check is performed an "checksum ok" will be returned.
 *
 * return -1:checksum not matching 0:checksum ok
 */
int check_eerom_buf(void)
{
	unsigned long crc32 = 0;

	if (chip_data.eeprom_crc32 == 0x00000000)
		return 0;

	crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
	if (crc32 != chip_data.eeprom_crc32)
		return -1;

	return 0;
}


/*
 * Checks, if the actual CRC32 signature of the eerom data in tool's buffer matches the CRC32 send
 * by user command. This implies that the user has to send 4 bytes CRC32 to compare in the data field.
 *
 * return -1:CRC32 not matching 0:CRC32 matching
 */
int verify_eerom_buf(void)
{
	unsigned long crc32 = 0;
	unsigned crc32_expected = 0;
	
	if (user_op.data == NULL)
		return -1;
	
	crc32_expected = user_op.data[0] | (user_op.data[1] << 8) | (user_op.data[2] << 16) | (user_op.data[3] << 24);
	crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
	if (crc32 != crc32_expected)
		return -1;
	
   return 0;
}

/*
 * Compare eerom of PCF against eerom buffer on tool's buffer
 * ToDo: Test the function! Usage of chip_data.erom instead of chip_data.eerom could be an error.
 * return -1:error 0:success
 */
int verify_eerom(void)
{
	int status = 0;

	if (user_op.len > EEROM_SIZE)
		return -1;
		
	/* send command */
	status = send_mdi_cmd(C_EE_DUMP);
	
	/* check eecon */	
	status |= recv_data(EEROM_SIZE);
  	if (status < 0)
  		return -1;
	
	for (unsigned short i = 0; i < user_op.len; i++) {
		if ((i < 4) || ((i >= (125 * 4)) && (i < (128* 4))))
			continue;

		if (chip_data.erom[i] != mdi.data[i])
			return -1;
	}
		
	return status;	
}

/*
 * Calculate and read CRC32 checksum of erom content stored in tool's buffer
 */
int read_erom_buf_cks(void)
{
	union cks {
		unsigned char crc32_array[4];		  // crc32
		unsigned long crc32_long;
	}temp;

	temp.crc32_long = crc32_caculate(user_op.data, user_op.len);
		
	SendBytesUsb(temp.crc32_array, 4, UINT32_MAX);
	
	if(temp.crc32_long == user_op.crc32){
		return 0;
	}
	else{
		return -1;
	}
}


/*
 * Read signature of the EROM
 */
int read_pcf_mem_cks(PCF_MEM_CKS_E pcf_mem_cks_type){
	
	int status = 0;
	
	/* send command read EROM signature */
	switch (pcf_mem_cks_type){
		case EROM_NORM:
			status = send_mdi_cmd(C_SIG_EROM_NORM);
			status |= recv_data(1);
			if(mdi.data[0] != 0x88 || status != OK)
				return ERR;
			status |= send_data((unsigned char*)magic,16);
			status |= recv_data(3);
			break;
		case EROM:
			status = send_mdi_cmd(C_SIG_EROM);
			status |= recv_data(3);
			break;
		case EEROM:
			status = send_mdi_cmd(C_SIG_EEROM);
			status |= recv_data(3);
			break;
		case ROM:
			status = send_mdi_cmd(C_SIG_EEROM);
			status |= recv_data(3);
			break;
	}

	if(status == OK){
		SendBytesUsb(mdi.data, 3, UINT32_MAX);
		return 0;
	}
	else{
		return status;
	}
	
}

/*
 * Set PCF to protected mode
 */
int pcf_protect(void){
	int status = 0;

	/* send command for setting device to protected mode */
	status = send_mdi_cmd(C_PROTECT);
	
	status |= recv_data(1);

	/* chip answers with EECON (status byte) */
	SendBytesUsb(mdi.data, 1, UINT32_MAX);
	
	return status;
}


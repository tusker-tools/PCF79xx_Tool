#include <stdio.h>
#include <string.h>
#include "rom.h"
#include "uart_ops.h"
#include "Stubs.h"
#include "Sysdefines.h"
#include "Utility.h"
#include "mdi.h"
#include "dwt_stm32_delay.h"

// Function prototype
int pcf_erase(void);

extern UART_HandleTypeDef huart1;

/**
 * init_chip
 *
 * return -1:error 0:success
 * enter mdi mode and send 0x02 if not protected
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
	
	// Check if argument for activating connect + erase sequence received
	if (uart_ops.address == 1){		
		pcf_erase();
	}
	else{
		status |= send_mdi_cmd(C_TRACE);
			
		// Detection of PCF Watchdog Disable confirmation (response to C_TRACE command)
		LL_GPIO_TogglePin(GPIOA,LL_GPIO_PIN_3);
		while(MSCL() == 0){		// wait unitl MSCL set to signalize watchdog disable
			delay_us(1);
			if(++t_tout_confirm_disable_wd > TIMEOUT_SCL_RISE_WD_DIS)
			{
				LL_GPIO_TogglePin(GPIOA,LL_GPIO_PIN_3);
				return status = CONNECT_ERR_PROT_MODE;		// if timout occured, device is in protected mode
			}
		}
		t_tout_confirm_disable_wd = 0;
		while(MSCL() == 1){		// ToDo: Improve accuracy of delay. Currently ~2x as long as speficied by timeout const
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

/**
 * erase_erom
 *
 * return -1:error 0:success
 * erase erom
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
	
	/* send byte0 - byte15 */
	memcpy(&mdi.data[0], magic, sizeof(magic));
	status |= send_data(mdi.data, sizeof(magic));
	
	/* 4.8ms? x20? */
	delay_ms(100);
	
	/* check eecon */
	status |= recv_data(0x01);
	if ((mdi.data[0] & 0xC0) != 0x00)
		return -1;
		
	return status;
}
/**
 * write_erom_buf
 *
 * return -1:error 0:success
 * program erom
 */
int write_erom_buf(void)
{
	unsigned long crc32 = 0;
	
	if (uart_ops.data == NULL)
		return -1;
	
	if (uart_ops.address != 0)
		return -1;
	
	if (uart_ops.len > EROM_SIZE)
		return -1;
		
	memset(chip_data.erom, 0x00, EROM_SIZE); 
	chip_data.erom_len = uart_ops.len;
	chip_data.erom_start = uart_ops.address;
	chip_data.erom_crc32 = uart_ops.crc32;
	memcpy(chip_data.erom, uart_ops.data, uart_ops.len);

    if (chip_data.erom_crc32 == 0x00000000)
		return 0;
	   
	crc32 = crc32_caculate(chip_data.erom, chip_data.erom_len);
	if (crc32 != chip_data.erom_crc32)
		return -1;
	
   return 0;
}
 
/**
* write_erom with page size 32 byte (Important: Only half of the memory - 0x0000 to 0x1000 -  could be programmed with this command)
* for writing complete memory, use write_erom64 function
 *
 * return -1:error 0:success
 * program erom
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

/**
* write_erom with page size 64byte (Important: Only with this command complete memory - 0x0000 to 0x2000 -  could be programmed)
* for writing complete memory, use write_erom64 function
 *
 * return -1:error 0:success
 * program erom
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


/**
 * write_eerom_buf
 *
 * return -1:error 0:success
 * program eerom buffer
 */
int write_eerom_buf(void)
{
	unsigned long crc32 = 0;
	
	/*if (uart_ops.data == NULL)
		return -1;  For what is this? If data buffer starts with 0, function returns always an error*/

	if (uart_ops.address != 0)
		return -1;
		
	if (uart_ops.len > EEROM_SIZE)
		return -1;
		 
	memset(chip_data.eeprom, 0x00, EEROM_SIZE); 
	chip_data.eeprom_len = uart_ops.len;
	chip_data.eeprom_start = uart_ops.address;
	chip_data.eeprom_crc32 = uart_ops.crc32;
	memcpy(chip_data.eeprom, uart_ops.data, uart_ops.len);
   
    if (chip_data.eeprom_crc32 == 0x00000000)
		return 0;
	
	crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
	if (crc32 != chip_data.eeprom_crc32)
		return -1;
	
   return 0;
}

/**
 * ee_prog_conf
 *
 * return -1:error 0:success
 * prog byte2 and byte3 of page127
 */
int ee_prog_conf(unsigned char page)
{
	int status = 0;
	unsigned char byte_2_3[2];	// definition of array for byte 2 and 3 of spacial page 127
	
	if (page != 127) 
		return -1;

	if(uart_ops.ops != PROGRAM_SPECIAL_BYTES)
	{
		/* command issued by write_eerom command. Take take data argument from eerom chip data array */
		byte_2_3[0] = chip_data.eeprom[127 * 4 + 2];
		byte_2_3[1] = chip_data.eeprom[127 * 4 + 3];
	}
	else
	{
		/* command issued direcly by uart_op "PROGRAM_SPECIAL_BYTES". Take Length as argument */
		byte_2_3[0] = uart_ops.addresses[0];
		byte_2_3[1] = uart_ops.addresses[1];
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

/**
 * write_eerom
 *
 * return -1:error 0:success
 * program eerom
 */
int program_eerom(void)
{
	volatile unsigned char start_page;
	volatile unsigned short pages;
	int status = 0;
	
  if(uart_ops.address == 0){
		// take address from write_ee_buf cmd if it was not specified in write_ee cmd
		start_page = chip_data.eeprom_start / EEPROM_PAGE_SIZE;
	}
	else{
		// take address from write_ee cmd if it is not left empty
		start_page = uart_ops.address / EEPROM_PAGE_SIZE;
	}
	
  if(uart_ops.len == 0){
		// take length from write_ee_buf cmd if it was not specified in write_ee cmd
		pages = chip_data.eeprom_len / EEPROM_PAGE_SIZE;
	}
	else{
		// take length from write_ee cmd if it is not left empty
		pages = uart_ops.len / EEPROM_PAGE_SIZE;
	}
	
	
	if ((chip_data.eeprom_start + chip_data.eeprom_len) > EEROM_SIZE)
		return -1;
	
	pages = (pages > 0) ? pages : 1;				// at least one page must be written. ToDo: Error Message to user
	for (unsigned int i = 0; i < pages; i++) {

		/*  Treatment for writing special pages */
		if (((start_page + i) == 0) || (((start_page + i) >= 125) && ((start_page + i) <= 127)))
		{
			if ((start_page + i) == 127) 
			{
				status |= ee_prog_conf(start_page + i);

				/* If special pages programming failed, PCF might fall into an error state. Thus, MDI must be reinitialized */
				if(status & PROG_SPCL_PG_FAIL){
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


int program_eerom_wo_spcl_page(void)
{
	volatile unsigned char start_page;
	volatile unsigned short pages;
	int status = 0;

  if(uart_ops.address == 0){
		// take address from write_ee_buf cmd if it was not specified in write_ee cmd
		start_page = chip_data.eeprom_start / EEPROM_PAGE_SIZE;
	}
	else{
		// take address from write_ee cmd if it is not left empty
		start_page = uart_ops.address / EEPROM_PAGE_SIZE;
	}

  if(uart_ops.len == 0){
		// take length from write_ee_buf cmd if it was not specified in write_ee cmd
		pages = chip_data.eeprom_len / EEPROM_PAGE_SIZE;
	}
	else{
		// take length from write_ee cmd if it is not left empty
		pages = uart_ops.len / EEPROM_PAGE_SIZE;
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

/**
 * write_eerom_manual
 *
 * return -1:error 0:success
 * Writes eerom of PCF without handling special (read-only) pages. User must ensure that the location to write data to is writable.
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


/**
 * read_erom_buf
 *
 * return -1:error 0:success
 * read erom buf
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

/**
 * read_erom
 *
 * return -1:error 0:success
 * read erom
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

/**
 * read_eerom_buf
 *
 * return -1:error 0:success
 * read eeprom buf
 */
int read_eerom_buf(void)
{
	unsigned long crc32 = 0;

	if (chip_data.eeprom_crc32 != 0x00000000) {	
		crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
		if (crc32 != chip_data.eeprom_crc32)
			return -1;
	}
	//usart_serial_write_packet((Usart *)UART, chip_data.eeprom, EEROM_SIZE);
	//	HAL_UART_Transmit(&huart1,chip_data.eeprom,EEROM_SIZE,UINT32_MAX);
	SendBytesUsb(chip_data.eeprom, EEROM_SIZE, UINT32_MAX);
   return 0;
}

/**
 * read_eerom
 *
 * return -1:error 0:success
 * read eeprom
 */
int read_eerom(void)
{
	int status = 0;
	
	/* send command */
	send_mdi_cmd(C_EE_DUMP);
	
	if(uart_ops.len == 0)
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
		status = recv_data(uart_ops.len);
		
		if (status == 0) {
			revert(mdi.data, uart_ops.len);	
			SendBytesUsb(mdi.data, uart_ops.len, UINT32_MAX);
		}
		else if(status == TOO_LESS_DATA){
			revert(mdi.data, uart_ops.len);	
			SendBytesUsb(mdi.data, mdi.transfer, UINT32_MAX);
		}
	}
	
	return status;	
}

/**
 * verify_erom_buf
 *
 * return -1:error 0:success
 * check erom buffer
 */
int verify_erom_buf(void)
{
	unsigned long crc32 = 0;
	unsigned crc32_expect = 0;
	
	if (uart_ops.data == NULL)
		return -1;
	
	crc32_expect = uart_ops.data[0] | (uart_ops.data[1] << 8) | (uart_ops.data[2] << 16) | (uart_ops.data[3] << 24);
	crc32 = crc32_caculate(chip_data.erom, chip_data.erom_len);
	if (crc32 != crc32_expect)
		return -1;
	
   return 0;
}

/**
 * verify_erom
 *
 * return -1:error 0:success
 * check erom
 */
int verify_erom(void)
{
	int status = 0;

	if (uart_ops.len > EROM_SIZE)
		return -1;
	 
	/* send command */
	status = send_mdi_cmd(C_ER_DUMP);
	
	/* check eecon */	
	status |= recv_data(EROM_SIZE);
	if (status < 0)
		return -1;
			
	/* do not compare the last byte */
	for (unsigned short i = 0; i < uart_ops.len - 1; i++) {
		if (chip_data.erom[i] != mdi.data[i])
			return -1;	
	}
	
	return status;	
}

/**
 * verify_eerom_buf
 *
 * return -1:error 0:success
 * check eeprom buf
 */
int verify_eerom_buf(void)
{
	unsigned long crc32 = 0;
	unsigned crc32_expect = 0;
	
	if (uart_ops.data == NULL)
		return -1;
	
	crc32_expect = uart_ops.data[0] | (uart_ops.data[1] << 8) | (uart_ops.data[2] << 16) | (uart_ops.data[3] << 24);
	crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
	if (crc32 != crc32_expect)
		return -1;
	
   return 0;
}

/**
 * verify_eerom
 *
 * return -1:error 0:success
 * compare erom	with exisxting data
 */
int verify_eerom(void)
{
	int status = 0;

	if (uart_ops.len > EEROM_SIZE)
		return -1;
		
	/* send command */
	status = send_mdi_cmd(C_EE_DUMP);
	
	/* check eecon */	
	status |= recv_data(EEROM_SIZE);
  	if (status < 0)
  		return -1;
	
	for (unsigned short i = 0; i < uart_ops.len; i++) {
		if ((i < 4) || ((i >= (125 * 4)) && (i < (128* 4))))
			continue;

		if (chip_data.erom[i] != mdi.data[i])
			return -1;
	}
		
	return status;	
}

int read_erom_buf_cks(void)
{
	union cks {
		unsigned char crc32_array[4];		  // crc32
		unsigned long crc32_long;
	}temp;

	temp.crc32_long = crc32_caculate(uart_ops.data, uart_ops.len);
		
	SendBytesUsb(temp.crc32_array, 4, UINT32_MAX);
	
	if(temp.crc32_long == uart_ops.crc32){
		return 0;
	}
	else{
		return -1;
	}
}

// Read signature of the EROM
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

// Set device to protected mode
int pcf_protect(void){
	int status = 0;

	/* send command for setting device to protected mode */
	status = send_mdi_cmd(C_PROTECT);
	
	status |= recv_data(1);

	/* chip answers with EECON (status byte) */
	SendBytesUsb(mdi.data, 1, UINT32_MAX);
	
	return status;
}


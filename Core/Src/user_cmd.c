#include <string.h>
#include "uart_ops.h"
#include "Sysdefines.h"
#include "mdi.h"
#include "rom.h"
#include "stdlib.h"
#include "Stubs.h"
#include "stm32f1xx_hal_uart.h"
#include "Utility.h"
#include "Usb.h"
#include "crc.h"


struct chip_data_s chip_data;
struct uart_ops_s uart_ops;
enum MDI_DEVICETYPE_E mdi_type;

extern UART_HandleTypeDef huart1;


/*******************
 *	Revert bits in byte x
*********************/
static unsigned char byte_revert(unsigned char x)
{
	x = (((x & 0xaa) >> 1) | ((x & 0x55) << 1));
	x = (((x & 0xcc) >> 2) | ((x & 0x33) << 2));
	
	return((x >> 4) | (x << 4));
}

/*************************
 *	Revert all bits in bytes starting at *data
 **************************/
int revert(unsigned char *data, unsigned long len)
{
	volatile unsigned char reverted = 0;
	
	if (data == NULL)
		return -1;
		
	for (unsigned int i = 0; i < len; i++) {
		  reverted = byte_revert(data[i]);
		  data[i] = reverted;
	}
	
	return 0;
}

/**
 *	check_erom_buf
 *
 * return -1:error 0:success
 * check erom buf
 */
static int check_erom_buf(void)
{
	unsigned long crc32 = 0;
	
	if (chip_data.erom_crc32 == 0x00000000)
		return 0;
	
	crc32 = crc32_caculate(chip_data.erom, chip_data.erom_len);
	if (crc32 != chip_data.erom_crc32)
		return -1;
		
	return 0;
}
/**
 *	int check_eerom_buf
 *
 * return -1:error 0:success
 * check eerom buf
 */
static int check_eerom_buf(void)
{
	unsigned long crc32 = 0;

	if (chip_data.eeprom_crc32 == 0x00000000)
		return 0;
		
	crc32 = crc32_caculate(chip_data.eeprom, chip_data.eeprom_len);
	if (crc32 != chip_data.eeprom_crc32)
		return -1;
	
	return 0;
}

/**
 * uart_ops_recv
 *
 * return none
 * read ops data from uart
 */

int uart_ops_recv(void)
{
	volatile status_code_t status = STATUS_OK;
	unsigned int crc32 = 0;
	
	/* store data to mdi buf */
	uart_ops.data = mdi.buf;
	
	
	status = RcvBytesUSB(&uart_ops.ops,1,UINT32_MAX);

	status = RcvBytesUSB(uart_ops.addresses,2,100);
	
	if (status == ERR_TIMEOUT)
		return -1;

	status = RcvBytesUSB(uart_ops.lens,2, 100);
	if (status == ERR_TIMEOUT)
		return -1;
	
	if (uart_ops.len > 0) {
		if (uart_ops.len > BUF_SIZE)
			return -1;
		
		if (uart_ops.ops == READ_ER_BUF_CKS || uart_ops.ops == CONNECT || uart_ops.ops == READ_PCF_MEM_CKS )
			return 0;
		
		/*if ((uart_ops.ops != WRITE_ER_BUF) && (uart_ops.ops != WRITE_EE_BUF) && 
			(uart_ops.ops != VERIFY_ER_BUF) && (uart_ops.ops != VERIFY_EE_BUF) && (uart_ops.ops != READ_EE))
			return -1;
		*/
		
		// Receive Data + CKS
		if ((uart_ops.ops == WRITE_ER_BUF) || (uart_ops.ops == WRITE_EE_BUF))
		{
			status = RcvBytesUSB(uart_ops.data,uart_ops.len,1000);
			
			if (status == ERR_TIMEOUT)
				return -1;
			
			status = RcvBytesUSB(uart_ops.crc32s,4,100);
			
			if (status == ERR_TIMEOUT)
				return -1;		
			
			if (uart_ops.crc32 == 0x00000000)
				return 0;
			
			crc32 = crc32_caculate(uart_ops.data, uart_ops.len);
			if (crc32 != uart_ops.crc32)
				return -1;
		}
	}
	
	return 0;
}

/**
 * uart_ops_handler
 *
 * return -1:error 0:success
 * handle uart ops
 */

int uart_ops_handler(void)
{
	volatile unsigned char ops = uart_ops.ops;
	volatile enum UART_STATUS_E status = SUCCESSFULL;
	int ret = 0;
	
	switch(ops)	{
	case CONNECT:
		if(uart_ops.len == 1){
			mdi_type = PCF7945;
		}
		else{
			mdi_type = F26A0700;
		}

		ret = pcf_init_mdi();

		if(ret==0){
			status = SUCCESSFULL;
		}
		else{
			status = ret;
		}
		break;
		
	case ERASE:
		ret = pcf_erase();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
	
	case WRITE_ER_BUF:
		ret = write_erom_buf();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
			
	case PROGRAM_ER:
		ret = check_erom_buf();
		if (ret == 0) 
			ret = program_erom();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
		
	case PROGRAM_ER64:
		ret = check_erom_buf();
		if (ret == 0) 
			ret = program_erom64();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
		

	case WRITE_EE_BUF:
		ret = write_eerom_buf();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
	
	case PROGRAM_EE:
		ret = check_eerom_buf();
		if (ret == 0) {
			memcpy(mdi.buf, chip_data.eeprom, EEROM_SIZE);
			ret = revert(mdi.buf, EEROM_SIZE);
			ret |= program_eerom();
		}
		if (ret != OK){
			UsbCharOut(PROGRAM_EE_ERR);
			status = ret;
		}
		else{
			status = SUCCESSFULL;
		}
		break;

	case PROGRAM_EE_MANUAL:
		ret = check_eerom_buf();
		if (ret == 0) {
			memcpy(mdi.buf, chip_data.eeprom, EEROM_SIZE);
			ret = revert(mdi.buf, EEROM_SIZE);
			ret |= program_eerom_manual();
		}
		status = ret > 0 ? ret : SUCCESSFULL;
		break;

	case PROGRAM_EE_WO_SPCL_PAGE:
		ret = check_eerom_buf();
		if (ret == 0) {
			memcpy(mdi.buf, chip_data.eeprom, EEROM_SIZE);
			ret = revert(mdi.buf, EEROM_SIZE);
			ret |= program_eerom_wo_spcl_page();
		}
		if (ret != OK){
			UsbCharOut(PROGRAM_EE_ERR);
			status = ret;
		}
		else{
			status = SUCCESSFULL;
		}
		break;

	case VERIFY_ER_BUF:
		ret = verify_erom_buf();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
				
	case VERIFY_ER:
		ret = verify_erom();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;

	case VERIFY_EE_BUF:
		ret = verify_eerom_buf();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
		
	case VERIFY_EE:
		ret = verify_eerom_buf();
		if (ret == 0) {
			memcpy(mdi.buf, chip_data.eeprom, EEROM_SIZE);
			ret = revert(mdi.buf, EEROM_SIZE);
			ret |= verify_eerom();
		}
		status = ret > 0 ? ret : SUCCESSFULL;
		break;	

	case READ_ER_BUF:
		ret = read_erom_buf();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
		
	case READ_ER:
		ret = read_erom();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;

	case READ_EE_BUF:
		ret = read_eerom_buf();
		status = ret > 0 ? READ_EE_BUF_ERR : SUCCESSFULL;
		break;
		
	case READ_EE:
		revert(uart_ops.data, uart_ops.len);
		ret = read_eerom();
		if(ret == 0){
			status = SUCCESSFULL;
		}
		else{
			status = READ_EE_ERR;
			if(ret== TOO_LESS_DATA){
				UsbCharOut(READ_EE_ERR);
				status = (unsigned char)TOO_LESS_DATA;
			}
		}
		break;
	
	case READ_ER_BUF_CKS:
		ret = read_erom_buf_cks();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
	
	case READ_PCF_MEM_CKS:
		switch(uart_ops.len){
		case 0:
			ret = read_pcf_mem_cks(EROM_NORM);
			break;
		case 1:
			ret = read_pcf_mem_cks(EROM);
			break;
		case 2:
			ret = read_pcf_mem_cks(EEROM);
			break;
		case 3:
			ret = read_pcf_mem_cks(ROM);
			break;
		}
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
	
	case PROTECT:
		ret = pcf_protect();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;

	case PROGRAM_SPECIAL_BYTES:
		ret =  ee_prog_conf(127);
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
			
	default:
		status = COMMAND_ERR;
		break;	
	}
	

  UsbCharOut(status);
	return status;
}

#include <string.h>
#include "user_cmd.h"
#include "Sysdefines.h"
#include "mdi.h"
#include "rom.h"
#include "stdlib.h"
#include "Utility.h"
#include "Usb.h"
#include "crc.h"
#include "dwt_stm32_delay.h"


struct chip_data_s chip_data;
struct user_cmd_s user_op;
enum MDI_DEVICETYPE_E mdi_type;
extern volatile uint32_t BOOTKEY;




/**
 * uart_ops_recv
 *
 * return none
 * read ops data from uart
 */

int ui_cmd_recv(void)
{
	volatile status_code_t status = STATUS_OK;
	unsigned int crc32 = 0;
	
	/* store data to mdi buf */
	user_op.data = mdi.buf;
	
	
	status = RcvBytesUSB(&user_op.ops,1,UINT32_MAX);

	status = RcvBytesUSB(user_op.addresses,2,100);
	
	if (status == ERR_TIMEOUT)
		return -1;

	status = RcvBytesUSB(user_op.lens,2, 100);
	if (status == ERR_TIMEOUT)
		return -1;
	
	if (user_op.len > 0) {
		if (user_op.len > BUF_SIZE)
			return -1;
		
		if (user_op.ops == READ_ER_BUF_CKS || user_op.ops == CONNECT || user_op.ops == READ_PCF_MEM_CKS )
			return 0;
		
		/*if ((uart_ops.ops != WRITE_ER_BUF) && (uart_ops.ops != WRITE_EE_BUF) && 
			(uart_ops.ops != VERIFY_ER_BUF) && (uart_ops.ops != VERIFY_EE_BUF) && (uart_ops.ops != READ_EE))
			return -1;
		*/
		
		// Receive Data + CKS
		if ((user_op.ops == WRITE_ER_BUF) || (user_op.ops == WRITE_EE_BUF))
		{
			status = RcvBytesUSB(user_op.data,user_op.len,1000);
			
			if (status == ERR_TIMEOUT)
				return -1;
			
			status = RcvBytesUSB(user_op.crc32s,4,100);
			
			if (status == ERR_TIMEOUT)
				return -1;		
			
			if (user_op.crc32 == 0x00000000)
				return 0;
			
			crc32 = crc32_calculate(user_op.data, user_op.len);
			if (crc32 != user_op.crc32)
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

int ui_cmd_handler(void)
{
	volatile unsigned char ops = user_op.ops;
	volatile enum USER_CMD_STATUS_E status = SUCCESSFULL;
	int ret = 0;
	
	switch(ops)	{
	case CONNECT:
		if(user_op.len == 1){
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
		revert(user_op.data, user_op.len);
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
		switch(user_op.len){
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

	case READ_TOOL_SW_VERSION:
		ret = output_PCF_Tool_SW_Version();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;
	
	case PROTECT:
		ret = pcf_protect();
		status = ret > 0 ? ret : SUCCESSFULL;
		break;

	case SWITCH_BTLD_MODE:
		BOOTKEY = 0x12345678;
		if(BOOTKEY == 0x12345678)
		{
			/* Stop USB Device to enable new enumeration in bootloader mode */
		    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_12, LL_GPIO_MODE_OUTPUT);
		    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_12, LL_GPIO_SPEED_FREQ_LOW);
		    LL_GPIO_SetPinOutputType(GPIOA,LL_GPIO_PIN_12, LL_GPIO_OUTPUT_PUSHPULL);
		    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_12);

		    delay_ms(1000);

		    /* Force soft reset to restart in bootloader mode */
		    NVIC_SystemReset();
		}
		else
		{
			status = BOOTKEY_NOT_WRITTEN;
		}
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

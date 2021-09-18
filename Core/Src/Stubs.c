/*
This file conains the stubs temperorary needed in order to make the project compile
*/

#include "stm32f1xx_ll_usart.h"
#include "Utility.h"
#include "stdint.h"
#include "stm32f1xx_hal.h"
#include "dwt_stm32_delay.h"


extern uint8_t test;

void usart_serial_putchar(uint8_t test){
	
	LL_USART_TransmitData8(USART1,test);
	
}


// Set MSDA to high state
void pio_set(uint8_t PIO, uint8_t PIN){
	if(PIO == 1 && PIN == 26){
			set_MSDA(1);
	}
}

// Set MSDA to low state
void pio_clear(uint8_t PIO, uint8_t PIN){
	if(PIO == 1 && PIN == 26){
			set_MSDA(0);
	}
}

void pio_set_output(uint8_t PIO, uint8_t PIN, uint8_t logic_level, uint8_t open_drain, uint8_t pullup){
	if(PIO == 1 && PIN ==26){
		set_MSDA(logic_level);
	}
}

uint8_t pio_get(uint8_t PIO,uint8_t stub,uint8_t PIN){
	return MSCL();
}



void delay_us(uint32_t us){
	DWT_Delay_us(us);
}
	
void delay_ms(uint32_t ms){	
	DWT_Delay_us(ms*1000);
}

/*
status_code_t convertStatus_statusCode_t_2_HAL_StatusTypeDef(HAL_StatusTypeDef Hal_Status){
	switch(Hal_Status){
		case HAL_OK:
			return STATUS_OK;
		case HAL_ERROR:
			return ERR_INVALID_ARG;
		case HAL_BUSY:
			return ERR_BUSY;
		case HAL_TIMEOUT:
			return ERR_TIMEOUT;
		default:
			return ERR_ABORTED;
	}
}
*/
			
	

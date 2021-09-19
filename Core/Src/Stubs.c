/*
This file conains the stubs temperorary needed in order to make the project compile
*/

#include "stm32f1xx_ll_usart.h"
#include "stdint.h"
#include "stm32f1xx_hal.h"
#include "dwt_stm32_delay.h"


extern uint8_t test;

void usart_serial_putchar(uint8_t test)
{
	LL_USART_TransmitData8(USART1,test);
}

void delay_us(uint32_t us)
{
	DWT_Delay_us(us);
}
	
void delay_ms(uint32_t ms)
{
	DWT_Delay_us(ms*1000);
}
	


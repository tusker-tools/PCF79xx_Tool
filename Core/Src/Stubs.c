/*
This file conains the stubs temperorary needed in order to make the project compile
*/

#include "stdint.h"
#include "dwt_stm32_delay.h"


void delay_us(uint32_t us)
{
	DWT_Delay_us(us);
}
	
void delay_ms(uint32_t ms)
{
	DWT_Delay_us(ms*1000);
}
	

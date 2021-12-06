#include "stm32f1xx_hal.h"
#include "dwt_stm32_delay.h"

/*
 * Initializes DWT_Clock_Cycle_Count for DWT_Delay_us function
 */
uint32_t DWT_Delay_Init(void)
{
  /* Disable TRC */
  CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk; // ~0x01000000;
  /* Enable TRC */
  CoreDebug->DEMCR |=  CoreDebug_DEMCR_TRCENA_Msk; // 0x01000000;

  /* Disable clock cycle counter */
  DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk; //~0x00000001;
  /* Enable  clock cycle counter */
  DWT->CTRL |=  DWT_CTRL_CYCCNTENA_Msk; //0x00000001;

  /* Reset the clock cycle counter value */
  DWT->CYCCNT = 0;

  /* 3 NO OPERATION instructions */
  __ASM volatile ("NOP");
  __ASM volatile ("NOP");
  __ASM volatile ("NOP");

  /* Check if clock cycle counter has started */
 if(DWT->CYCCNT)
 {
   return 0; /*clock cycle counter started*/
 }
 else
 {
   return 1; /*clock cycle counter not started*/
 }
}

/*
 * Waits a time specified by parameter µs
 * Input parameter: us = wait time in microseconds
 */
void delay_us(uint32_t us)
{
	DWT_Delay_us(us);
}


/*
 * Waits a time specified by parameter ms
 * Input parameter: ms = wait time in milliseconds
 */
void delay_ms(uint32_t ms)
{
	DWT_Delay_us(ms*1000);
}


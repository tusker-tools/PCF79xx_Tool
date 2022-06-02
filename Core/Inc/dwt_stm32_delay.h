#ifndef DWT_STM32_DELAY_H
#define DWT_STM32_DELAY_H

#include "stdint.h"
#include "stm32f103xb.h"
#include "core_cm3.h"


/*
 * Initializes DWT_Cycle_Count for DWT_Delay_us function
 */
uint32_t DWT_Delay_Init(void);

/*
 * Waits a time specified by parameter µs
 * Input parameter: us = wait time in microseconds
 */
void delay_us(uint32_t us);

/*
 * Waits a time specified by parameter ms
 * Input parameter: ms = wait time in milliseconds
 */
void delay_ms(uint32_t ms);



/*
 * Provid a delay (in microseconds) using STM32 DWT unit
 * Hint: Function needs to be defined here in order to "inline" it in other translation units
 */
static inline void DWT_Delay_us(volatile uint32_t microseconds)
{
  uint32_t clk_cycle_start = DWT->CYCCNT;

  /* Go to number of cycles for system */
  microseconds *= (SystemCoreClock / 1000000);

  /* Delay till end */
  while ((DWT->CYCCNT - clk_cycle_start) < microseconds);
}

#endif

/*
This file conains the stubs temperorary needed in order to make the project compile
*/
#ifndef Stubs_H
#define Stubs_H
#include "Sysdefines.h"

void usart_serial_putchar(uint8_t data);
void pio_set(uint8_t PIO, uint8_t PIN);
void pio_clear(uint8_t PIO, uint8_t PIN);
void pio_set_output(uint8_t PIO, uint8_t PIN, uint8_t logic_level, uint8_t open_drain, uint8_t pullup);	
uint8_t pio_get(uint8_t PIO,uint8_t stub,uint8_t PIN);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);


extern unsigned int bit;

	
#define PIOA 0
#define PIO_PA15 15
#define ID_PIOA 115

#define PIOB  1
#define PIO_PB26  26

#define PIO_TYPE_PIO_INPUT 0
#define UART USART1


//status_code_t convertStatus_statusCode_t_2_HAL_StatusTypeDef(HAL_StatusTypeDef Hal_Status);

#endif


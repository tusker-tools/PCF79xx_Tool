// This files contains all the small helper functions for configuring the HW or configuring a set of variables


// Includes
#include "Sysdefines.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_exti.h"
#include "usb.h"

/* Variables definition */
uint32_t raw_adc = 0;
uint8_t adc_offset_hex = 85;
uint8_t converted_adc_str[] = { 0 , ',' , 0 , 13}; // Voltage format example: "2" , "," , "7" , [newline]

/* Enum definitions */
enum edges{
	rising = 1,
	falling = 0
};


// Function prototypes
void set_IRQ_and_EXTI_Line_Cmd(uint8_t enable, uint8_t edge);

/*
 *  Activate / deactivate external interrupt on rising edge of MSCL pin level.
 *  Other edge interrupts are automatically deactivated.
 */
void active_MSCL_rising_edge_IT(uint8_t active)
{
	set_IRQ_and_EXTI_Line_Cmd(active,rising);
}

/*
 *  Activate / deactivate external interrupt on falling edge of MSCL pin level.
 *  Other edge interrupts are automatically deactivated.
 */
void active_MSCL_falling_edge_IT(uint8_t active)
{
	set_IRQ_and_EXTI_Line_Cmd(active,falling);
}


/*
 * Configure external interrupt edge level and activateion status
 */
void set_IRQ_and_EXTI_Line_Cmd(uint8_t enable, uint8_t edge)
{
	/* Set Edge on which an external interrupt is issued */
	if(edge==rising){
	  /* Rising edge */
	  LL_EXTI_DisableFallingTrig_0_31(LL_EXTI_LINE_14);
	  LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_14);
	}
	else{
	  //* Falling edge */
	  LL_EXTI_DisableRisingTrig_0_31(LL_EXTI_LINE_14);
	  LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_14);
	}
	
	
	/* Set activation status */
	if(enable){
		/* Activate Interrupt */
		LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_14);	// Clear flag of eventually pending interrupt
		(void) EXTI->PR;							// dummy read access in order to assure EXTI->PR is reset when interrupt is activated
		LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_14);
		NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
		NVIC_EnableIRQ(EXTI15_10_IRQn);
	}
	else{
		/* Deactivate Interrupt */
		LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_14);
		NVIC_DisableIRQ(EXTI15_10_IRQn);
	}

}

/*
 * Set signal level of MSDA output port
 */
void set_MSDA(uint8_t lv_active)
{

	LL_GPIO_SetPinMode(MSDA_PORT, MSDA_PIN, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinOutputType(MSDA_PORT, MSDA_PIN, LL_GPIO_OUTPUT_PUSHPULL);
	
	if(lv_active){
		//GPIO_InitStruct.Pull = LL_GPIO_PULL_UP ;
		LL_GPIO_SetOutputPin(MSDA_PORT,MSDA_PIN);
	}
	else{
		//GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;	
		LL_GPIO_ResetOutputPin(MSDA_PORT, MSDA_PIN);
	}
	
}

/*
 * Set signal level of BAT output port. This controls the power supply of the PCF.
 */
void set_BAT(uint8_t lv_active)
{
	LL_GPIO_SetPinMode(BAT_PORT, BAT_PIN, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinOutputType(BAT_PORT, BAT_PIN, LL_GPIO_OUTPUT_PUSHPULL);
	
	if(lv_active){
		LL_GPIO_SetOutputPin(BAT_PORT,BAT_PIN);
	}
	else{
		LL_GPIO_ResetOutputPin(BAT_PORT,BAT_PIN);
	}
	
}

/*
 * Set signal level of MSCL output port.
 */
void set_MSCL(uint8_t lv_active)
{
	if(lv_active){
		LL_GPIO_SetOutputPin(MSCL_PORT, MSCL_PIN);
	}
	else{
		LL_GPIO_ResetOutputPin(MSCL_PORT, MSCL_PIN);
	}
}


/*
 * Read signal level of MSCL input pin
 */
uint8_t MSCL(void)
{
	return (uint8_t)((LL_GPIO_ReadInputPort(MSCL_PORT)>>MSCL_PIN_number)&1);
}

/*
 * Read signal level of MSDA input pin
 */
uint8_t MSDA(void)
{
	return (uint8_t)((LL_GPIO_ReadInputPort(MSDA_PORT)>>MSDA_PIN_number)&1);
}


/*
 * Set electrical configuration of MSDA pin to "input with pullup"
 */
void set_MSDA_input_pullup(void)
{
	LL_GPIO_SetPinMode(MSDA_PORT, MSDA_PIN, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinPull(MSDA_PORT, MSDA_PIN, LL_GPIO_PULL_UP);
}

/*
 * Set electrical configuration of MSDA pin to "input floating"
 */
void set_MSDA_input_floating(void)
{
	LL_GPIO_SetPinMode(MSDA_PORT, MSDA_PIN, LL_GPIO_MODE_FLOATING);
}

/*
 * Set electrical configuration of MSDA pin to "output" with push-pull driver connected
 */
void set_MSDA_output_pushpull(void)
{
	LL_GPIO_SetPinMode(MSDA_PORT, MSDA_PIN, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinOutputType(MSDA_PORT, MSDA_PIN, LL_GPIO_OUTPUT_PUSHPULL);
}

void set_MSCL_input_floating(void){
	LL_GPIO_SetPinMode(MSCL_PORT, MSCL_PIN, LL_GPIO_MODE_FLOATING);
}

/*
 * Set electrical config of MSCL pin to "input with pulldown"
 */
void set_MSCL_input_pulldown(void){
	LL_GPIO_SetPinMode(MSCL_PORT, MSCL_PIN, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinPull(MSCL_PORT, MSCL_PIN, LL_GPIO_PULL_DOWN);
}

/*
 * Set electrical config of MSCL pin to "input with pullup"
 */
void set_MSCL_input_pullup(void){
	LL_GPIO_SetPinMode(MSCL_PORT, MSCL_PIN, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinPull(MSCL_PORT, MSCL_PIN, LL_GPIO_PULL_UP);
}

/*
 * Subtracts two u32 numbers and limits result's lower limit to 0
 */
uint32_t u32_sat_sub_u32_u32(uint32_t sub1, uint32_t sub2){
		uint32_t res;
		res = sub1 - sub2;
		
		if(res > sub1){
			res = 0;
		}
		
		return res;
}


/*
 * Revert bits in byte x
 */
static unsigned char byte_revert(unsigned char x)
{
	x = (((x & 0xaa) >> 1) | ((x & 0x55) << 1));
	x = (((x & 0xcc) >> 2) | ((x & 0x33) << 2));

	return((x >> 4) | (x << 4));
}

/*
 *	Revert all bits in bytes starting at *data
 */
int revert(unsigned char *data, unsigned long len)
{
	volatile uint32_t reverted = 0;

	if (data == NULL)
		return -1;

	for (unsigned int i = 0; i < len; i++)
	{
		#if ((defined (__ARM_ARCH_7M__      ) && (__ARM_ARCH_7M__      == 1)) || \
	     	 (defined (__ARM_ARCH_7EM__     ) && (__ARM_ARCH_7EM__     == 1)) || \
			 (defined (__ARM_ARCH_8M_MAIN__ ) && (__ARM_ARCH_8M_MAIN__ == 1))    )

			/* use reverse bits hardware functionality */
			reverted = __RBIT((uint32_t)data[i]);
			data[i] = (unsigned char)(reverted>>24);
		#else
			/* else use reverse bits by discrete function */
			data[i] = byte_revert(data[i]);
		#endif
	}
	return 0;
}

/*
 * Revert byte order within an amount of bytes
 */
void revert_bytes(uint8_t *Data, uint8_t length)
{
	uint8_t copy[length];

	// save original data
	for(uint32_t i=0; i<length; i++)
	{
			copy[i] = Data[i];
	}

	for(uint32_t i=0; i<length; i++)
	{
		Data[length-i-1] = copy[i];
	}
}





/*
 *  Receive a number of bytes from USB buffer and writes it to Array "Data". If data is not available within tout_ms milliseconds,
 *  function returns corresponding status code. If tout_ms = UINT32_MAX, then the function will never time out.
 */
status_code_t RcvBytesUSB(uint8_t Data[], uint16_t bytes, uint32_t tout_ms){
	
	// All times given as CPU clock cycles

	uint32_t t_act;// Current time
	uint32_t t_old = DWT->CYCCNT;
	
	uint16_t bytes_received = 0;
	
	if(tout_ms == UINT32_MAX)
	{
		// Endlessloop w/o timeout
		while( !(bytes_received >= bytes))
		{	
			if( UsbRxAvail() )
			{												
				Data[bytes_received] = UsbGetChar();
				bytes_received++;
			}
		}	
	}
	else{
		// Loop with timeout
		uint32_t t_resi = tout_ms * (HAL_RCC_GetHCLKFreq() / 1000);					// Residual time of tout

		while( (t_resi > 0) && (bytes_received < bytes) )
		{	
			if( UsbRxAvail() )
			{												
				Data[bytes_received] = UsbGetChar();
				bytes_received++;
			}
			t_act = DWT->CYCCNT;
			t_resi = u32_sat_sub_u32_u32(t_resi,(t_act - t_old));
			
			t_old = t_act;
		}

		if (t_resi == 0){
			return ERR_TIMEOUT;
		}
	}
	
	// Return resulting state
	if (bytes_received == bytes){
		return STATUS_OK;
	}

	return ERR_ABORTED;
}

/*
 * Sends an array of bytes
 * Data: Adress of the first bite
 * bytes: Number of bytes
 * tout: Timeout clock cycles
 */
void SendBytesUsb(uint8_t Data[], uint16_t bytes, uint32_t tout)
{
	uint16_t bytes_sent = 0;
	
	uint32_t t_act;
	uint32_t t_old = DWT->CYCCNT;

	
	// No Timeout
	if(tout == UINT32_MAX)
	{
		while((bytes_sent < bytes))
		{
			if (UsbCharOut(Data[bytes_sent]))
			{
				bytes_sent++;
			}
		}
	}
	else
	{
		uint32_t t_resi = tout * (HAL_RCC_GetHCLKFreq() / 1000000);					// Residual time of tout

		while((bytes_sent < bytes) && (t_resi > 0))
		{
			if (UsbCharOut(Data[bytes_sent]))
			{
				bytes_sent++;
			}
		
		t_act = DWT->CYCCNT;
		u32_sat_sub_u32_u32(t_resi,(t_act-t_old));
		t_old = t_act;
		}
	}
}

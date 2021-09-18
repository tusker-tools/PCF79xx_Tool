// This files contains all the small helper functions for configuring the HW or configurating a set of variables


// Includes
#include "Sysdefines.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_exti.h"
#include "Stubs.h"
#include "usb.h"

// Variables definition
	uint32_t raw_adc = 0;
	uint8_t adc_offset_hex = 85;
	uint8_t converted_adc_str[] = { 0 , ',' , 0 , 13}; // Voltage format example: "2" , "," , "7" , [newline]

// Externs
extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;

// Function prototypes
void set_IRQ_and_EXTI_Line_Cmd(uint8_t enable, uint8_t edge);

// this function activates or deactivates a rising edge pin interrupt
void active_MSCL_rising_edge_IT(uint8_t active){
	
  set_IRQ_and_EXTI_Line_Cmd(active,1);

}

void active_MSCL_falling_edge_IT(uint8_t active){
	
	set_IRQ_and_EXTI_Line_Cmd(active,0);
	
}


void set_IRQ_and_EXTI_Line_Cmd(uint8_t enable, uint8_t edge){
	/* Configure Interrupt	*/
	
	// Set Edge of EXTI
	// Rising edge
  if(edge){
		LL_EXTI_DisableFallingTrig_0_31(LL_EXTI_LINE_14);
		LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_14);
	}
	// Falling edge
	else{
		LL_EXTI_DisableRisingTrig_0_31(LL_EXTI_LINE_14);
		LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_14);
	}
	
	
	// Activate or deactivate line
  if(enable){
		
		LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_14);
		(void) EXTI->PR;							// dummy read access in order to assure EXTI->PR is reset when interrupt is activated
		LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_14);
		NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
		NVIC_EnableIRQ(EXTI15_10_IRQn);
	}
	else{
	
		LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_14);
		NVIC_DisableIRQ(EXTI15_10_IRQn);
	}

}

// This function sets signal level of MSDA port
void set_MSDA(uint8_t lv_active){

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
	
// This function sets signal level of MSDA port
void set_BAT(uint8_t lv_active){
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	if(lv_active){
		GPIO_InitStruct.Pull = LL_GPIO_PULL_UP ;
	}
	else{
		GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;	
	}
	
	GPIO_InitStruct.Pin = BAT_PIN;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;

	
  LL_GPIO_Init(BAT_PORT, &GPIO_InitStruct);
	
}

uint8_t MSCL(void){
	return (uint8_t)((LL_GPIO_ReadInputPort(MSCL_PORT)>>MSCL_PIN_number)&1);
}

uint8_t MSDA(void){
	return (uint8_t)((LL_GPIO_ReadInputPort(MSDA_PORT)>>MSDA_PIN_number)&1);
}

void set_MSCL(uint8_t lv_active){
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	if(lv_active){
		GPIO_InitStruct.Pull = LL_GPIO_PULL_UP ;
	}
	else{
		GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;	
	}
	
	GPIO_InitStruct.Pin = MSCL_PIN;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	
	LL_GPIO_Init(MSCL_PORT, &GPIO_InitStruct);
}

void set_MSDA_input_pullup(void){
	//LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	//GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
  //GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  //GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
	LL_GPIO_SetPinMode(MSDA_PORT, MSDA_PIN, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinPull(MSDA_PORT, MSDA_PIN, LL_GPIO_PULL_UP);
 // LL_GPIO_Init(MSDA_PORT, &GPIO_InitStruct);
}

void set_MSDA_input_floating(void){
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_FLOATING;
	
  LL_GPIO_Init(MSDA_PORT, &GPIO_InitStruct);
}

void set_MSDA_output_pushpull(void){
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	
  LL_GPIO_Init(MSDA_PORT, &GPIO_InitStruct);
}

void set_MSCL_input_floating(void){
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	GPIO_InitStruct.Pin = MSCL_PIN;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_FLOATING;
	
  LL_GPIO_Init(MSCL_PORT, &GPIO_InitStruct);
}

void set_MSCL_input_pulldown(void){
	LL_GPIO_SetPinMode(MSCL_PORT, MSCL_PIN, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinPull(MSCL_PORT, MSCL_PIN, LL_GPIO_PULL_DOWN);
}

void set_MSCL_input_pullup(void){
	LL_GPIO_SetPinMode(MSCL_PORT, MSCL_PIN, LL_GPIO_MODE_INPUT);
	LL_GPIO_SetPinPull(MSCL_PORT, MSCL_PIN, LL_GPIO_PULL_UP);
}


// Subtracts two u32 numbers and limits (saturatest) result to 0
uint32_t u32_sat_sub_u32_u32(uint32_t sub1, uint32_t sub2){
		uint32_t res;
		res = sub1 - sub2;
		
		if(res > sub1){
			res = 0;
		}
		
		return res;
}


/**********************
// This function receives a number of PRM_bytes from USB buffer and writes it to PRM_*pdata. If data is not available in PRM_tout, it returns
***********************/
status_code_t RcvBytesUSB(uint8_t Data[], uint16_t bytes, uint32_t tout_ms){
	
	// All times given as CPU clock cycles
	
	uint32_t t_act = DWT->CYCCNT;																											// Current time
	uint32_t t_old = t_act;	// Last While recurrence begin time
	uint32_t t_resi = tout_ms * (HAL_RCC_GetHCLKFreq() / 1000);					// Residual time of tout
	
	
	uint16_t bytes_received = 0;																								// Residual bytest to fetch
	
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
	}
	
	// Return resulting state
	if (bytes_received == bytes){
		return STATUS_OK;
	}
	if (t_resi == 0){
		return ERR_TIMEOUT;
	}
	return ERR_ABORTED;
}


void SendBytesUsb(uint8_t Data[], uint16_t bytes, uint32_t tout)
{
	uint16_t bytes_sent = 0;
	
	uint32_t t_act = DWT->CYCCNT;																				// Start time
	uint32_t t_old = t_act + (HAL_RCC_GetHCLKFreq() / 1000000) * tout;	// Last While recurrence begin time
	uint32_t t_resi = tout * (HAL_RCC_GetHCLKFreq() / 1000000);					// Residual time of tout
	
	while((bytes_sent < bytes) && (t_resi > 0)) {
		if (UsbCharOut(Data[bytes_sent])){
			bytes_sent++;
		}
		
		t_act = DWT->CYCCNT;
		u32_sat_sub_u32_u32(t_resi,(t_act-t_old));
		
		t_old = t_act;
		
	}
}

// This function reverts the order of single bytes 
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



/************BEGIN Section for Test Instrumentation ********************/

// For testing
void GetAdcValue_convert_and_print_serial(void){
		raw_adc = HAL_ADC_GetValue(&hadc1);
		converted_adc_str[0] = (raw_adc + adc_offset_hex) / 1241 + '0';		// 3,3 / 4095 = 1241 
		converted_adc_str[2] = (((raw_adc + adc_offset_hex) % 1241) / 124) + '0' ;
		//HAL_UART_Transmit(&huart1,(uint8_t*)&converted_adc_str,sizeof(converted_adc_str),UINT32_MAX);
}
	
/**************** END Section Test Instrumentation ********************/

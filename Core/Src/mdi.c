/*
 * mdi.c
 *
 * Created: 2018/8/15 16:28:17
 *  Author: wangj
 */ 
#include "Utility.h"
#include "Stubs.h"
#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_it.h"
#include "main.h"
#include "mdi.h"
#include "dwt_stm32_delay.h"

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;

//static unsigned int bit = 0; Uncommented for debug reasons. Must be static in final code
unsigned int bit = 0;

uint8_t debugflag = 0;

static unsigned char data = 0;
volatile uint8_t test = 255;
volatile unsigned long nr_aufruf = 0;
volatile unsigned long c_nr_aufruf = UINT32_MAX;

struct mdi_data_s mdi = {.dir = RECV, .status = IDLE};

/**
 * wait_ops_done
 *
 * return -1:hight 0:low
 * check mscl become low
 */
__inline static int wait_ops_done(void)
{
	unsigned long timeout = 0;
	while(mdi.status != DONE) {
		if (++timeout >= TIMEOUT_OPS_DONE)
		return -1;
		delay_us(1);
	};
	
	return 0;
}

/**
 * wait_mscl_high
 *
 * return -1:hight 0:low
 * wait until mscl gets high the first time when entering mdi mode
 */
int wait_mscl_high(void)			// function used for first mscl-high pulse at pcf wakeup
{
	unsigned long timeout = 0;
	
	while(MSCL() == 0) {
		delay_us(1);
		if (++timeout >= TIMEOUT_MSCL_HIGH)
			return -1;
	};
	
	return 0;
}



/**
 * wait_mscl_low
 *
 * return -1:hight 0:low
 * check mscl become low
 */
__inline static int wait_mscl_low(void)
{
	unsigned long timeout = 0;
	
	while(pio_get(PIOA, PIO_TYPE_PIO_INPUT, PIO_PA15) == 1) {
		delay_us(1);
		if (++timeout >= TIMEOUT_MSCL_LOW)
			return -1;
		};
	
	return 0;
}

/**
 * enter_monitor_mode
 *
 * return -1:error 0:success
 * Set device to MDI mode. There are different init sequences necessary for different chips. Sequence to use is specified by user via parameter 2 (former known as "Length")
 * Parameter 2	For Chip		Sequence		
 * 				0			26A0700			BATT_ON and MSDA_OUTPUT_0, wait until SCL high, wait until SCL low, MSDA_OUTPUT_1 (MDI internal mode), wait ~25us, 
 * 				1			PCF7945C05	BATT_ON and MSDA_OUTPUT_0, wait 500us (225...725us), MSDA_OUTPUT_1, wait until SCL low, TBC
 */
int enter_monitor_mode(enum MDI_DEVICETYPE_E mditype) // // ToDo: Catch error case: Device is not connected at all
{
	unsigned long timeout = 0;
	volatile uint8_t debug_ctr = 0xFF;
	uint8_t lv_scl_old = 0;

	
	/* Disable PIO controller IRQs. */
	//NVIC_DisableIRQ(PIOA_IRQn);
	set_IRQ_and_EXTI_Line_Cmd(0,0);	//DISABLE MSCL interrupt, (falling edge)
	
	/* power down */
	set_BAT(0); set_MSDA(0);
	delay_ms(100);
		
	/* electrical config for SCL pin */
	// PCF7945C05
	if(mditype == 1){
		set_MSCL_input_floating();
	}
	// 26A0700
	else{
		set_MSCL_input_pulldown();
	}	
		
	/* power up */
	set_BAT(1);
	LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_3);		// TestPoint
	
	// PCF7945C05
	if(mditype == 1){
		delay_us(T_DLY_SDA_HIGH_AFT_PON);
		set_MSDA(1);
	}
	// 26A0700
	else{
		if (wait_mscl_high() < 0){
			return -1;
		}
	}	
				
	delay_us(200);

	
	if (wait_mscl_low() < 0)		// Wait PCF setting mscl low, right after master must select internal or external mdi mode
		return -1;

	
	// PCF7945C05
	if(mditype == 1){
		set_MSDA(0);
		delay_us(8);
		set_MSDA(1);
	}
	// 26A0700
	else{
		delay_us(2);
		set_MSDA(1);
	}	
	
	
	
	//own test
	delay_us(100);	

	set_MSDA(0);
	
	/* Enable PIO controller IRQs. */
	mdi.status = INIT;
	mdi.dir = RECV;
	//NVIC_EnableIRQ(PIOA_IRQn);

	//set_MSDA_input_floating();
	active_MSCL_rising_edge_IT(1);	// rising edge to activate mscl pullup
	//active_MSCL_falling_edge_IT(1);
	LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_3);
	delay_us(10);

	
	timeout = 0;
	while(mdi.status != DONE) {
		if (++timeout >= TIMEOUT_DEV_ACK)
			return -1;
		delay_us(1);
	};
	
	if (mdi.data[0] != DEV_ACK)
		return -1;
	
	return 0;
}

/**
 * recv_byte
 *
 * return received data
 * receive data called by external interrupt handler
 */

static unsigned char recv_byte(void)
{
	// First call of function (at first rising edge of MSCL signal)?
	if (mdi.status == INIT)	{
		mdi.status = IDLE;
		bit = 0;
		return data;
	}

	// First bit (= falling edge of MSCL)? -> Change mdi status and reset data
	if (bit == 0){
		mdi.status = BUSY;
		data = 0;
	} 
	
	data |= MSDA() << (bit);

	/* check if one byte received */
	if (++bit >= MDI_RCV_BITS) {
		if(mdi.status == WAIT_LAST_PULSE){		
			if (wait_mscl_low() < 0)
				return -1;
		
			set_MSDA(1);
			active_MSCL_falling_edge_IT(0);
			delay_us(5);
			mdi.status = DONE;
		}
		else{
			mdi.status = WAIT_LAST_PULSE;
		}
	}
	
	return data;	
}

/**
 * send_byte
 *
 * return 0: success -1:error
 * send data called by external interrupt handler
 */
static int send_byte(unsigned char *data)
{
	static unsigned int bit = 0;
	
	/* check if bits is error */
	if (bit >= MDI_SND_BITS)
		bit = 0;
	
	if (mdi.status == IDLE)	{
		mdi.status = BUSY;
		bit = 0;
	}
	
	/* send the first bit */
	if (bit == 0) {
		if ((*data & 0x01) != 0)
			pio_set(PIOB, PIO_PB26);
		else
			pio_clear(PIOB, PIO_PB26);
	}
	
	
	if (wait_mscl_low() < 0)
		return -1;

	/* send the other bits */
	if (bit < (MDI_SND_BITS - 1)) {
		if ((*data >> (bit + 1) & 0x01) != 0)
			pio_set(PIOB, PIO_PB26);
		else
			pio_clear(PIOB, PIO_PB26);
	}
	
	/* check if one byte sent */
	if (++bit >= MDI_SND_BITS) {
		set_MSDA(1);
		delay_us(25);
		mdi.status = DONE;
	}
	
	return *data;
}

/**
 * send_data
 *
 * return 0: success -1:error
 * send data via external interrupt
 */
int send_data(unsigned char *data_ptr, unsigned long len)
{
	mdi.data_ptr = data_ptr;
	if (mdi.dir == RECV){
		delay_us(500);
	}
	
	
	for (unsigned long i = 0; i < len; i++) {
		active_MSCL_rising_edge_IT(1);		// ToDo: SCL rising edge is expected after MSDA set low-> move after pio_set_output
		mdi.dir = SEND;
		mdi.status = IDLE;
		set_MSDA(0);
		if (wait_ops_done() < 0){
			active_MSCL_rising_edge_IT(0);
			set_MSDA(1);
			return -1;
		}
		if(i >= c_nr_aufruf){
				c_nr_aufruf = i;
		}
		active_MSCL_rising_edge_IT(0);
		
		mdi.transfer = i + 1;
		if (mdi.transfer >= len)
			mdi.transfer = 0;
	}
	
	return 0;
}

/**
 * send_mdi_cmd
 *
 * return 0: success -1:error
 * send cmd (1 byte) to mdi
 */
int send_mdi_cmd(unsigned char byte)
{
	mdi.data[0] = byte;
	mdi.data_ptr = &mdi.data[0];

	if (mdi.dir == RECV){
		delay_us(500);
	}


	active_MSCL_rising_edge_IT(1);		// ToDo: SCL rising edge is expected after MSDA set low-> move after pio_set_output
	mdi.dir = SEND;
	mdi.status = IDLE;
	set_MSDA(0);
	if (wait_ops_done() < 0){
		active_MSCL_rising_edge_IT(0);
		set_MSDA(1);
		return -1;
	}

	active_MSCL_rising_edge_IT(0);

	mdi.transfer = 0;

	return 0;
}

/**
 * send_data
 *
 * return 0: success -1:error
 * recv data via external interrupt
 */
int recv_data(unsigned long len)
{		
	if (mdi.dir == SEND){
		delay_us(500);
	}
	
	for (unsigned long i = 0; i < len; i++)
	{

		mdi.dir = RECV;
		mdi.status = INIT;
		
		set_MSDA(0);			// Pull SDA low to signalize PCF to start transfer
		
		active_MSCL_rising_edge_IT(1);
		
		if (wait_ops_done() < 0){
			if(i>0){
				return TOO_LESS_DATA;
			}
			else{
				return RCV_TOUT;
			}
		}
		mdi.transfer = i + 1;
		if (mdi.transfer >= len)
			mdi.transfer = 0;
	}
	
	return 0;
}

/**
 * data_handler
 *
 * return none
 * called byexternal interrupt handler
 */
void data_handler(void)
{	
	if(debugflag){
		LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_3);
	}
	
	if (mdi.dir == RECV){ 
		if (mdi.status == INIT){	// when data_handler called first for receiving, then it was due to a MSCL rising edge to set pullup

			active_MSCL_falling_edge_IT(1);	// start falling edge IT to sample bit data on every falling edge
			
			set_MSDA_input_pullup();

		}
		LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_3);

		mdi.data[mdi.transfer] = recv_byte();
	}	
	else{
		send_byte(mdi.data_ptr + mdi.transfer);
	}
	
	return;
}



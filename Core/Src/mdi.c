#include <user_cmd.h>
#include "Sysdefines.h"
#include "mdi.h"
#include "Utility.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_it.h"
#include "dwt_stm32_delay.h"

/***************************
 * Data Declarations & Variable Initializations
 ****************************/
volatile unsigned int bit = 0;
static unsigned char data = 0;

/* magic number used as security key for erase and EROM_CKS_NORM cmd */
const unsigned char magic[16] = {0x55, 0x45, 0xE8, 0x92, 0xD6, 0xB1, 0x62, 0x59, 0xFC, 0x8A, 0xC8, 0xF2, 0xD6, 0xE1, 0x4A, 0x35};

/* Initialize mdi states */
struct mdi_data_s mdi = {.dir = RECV, .status = IDLE};

/* debug variables */
volatile uint8_t test = 255;
volatile unsigned long nr_aufruf = 0;
volatile unsigned long c_nr_aufruf = UINT32_MAX;
uint8_t debugflag = 0;



/*********************************
  Function definitions
 ********************************/

/*
 * wait until operation on mdi interface has been finished which is indicated by
 * MSCL pin set to high level
 * return:  timeout=-1 done=0
 */
__inline static int wait_mdi_ops_done(void)
{
	unsigned long timeout = 0;
	while(mdi.status != DONE) {
		if (++timeout >= TIMEOUT_OPS_DONE)
		return -1;
		delay_us(1);
	};
	
	return 0;
}

/*
 * wait until MSCL pin is set to high level
 * input: wait time µs
 * return:  timeout=-1 MSCL high=0
 */
int wait_mscl_high(unsigned long wait_time)
{
	unsigned long timeout = 0;
	
	while(MSCL() == 0) {
		delay_us(1);
		if (++timeout >= wait_time)
			return -1;
	};
	
	return 0;
}


/*
 * wait until MSCL pin is set to low level
 * input: wait time µs
 * return:  timeout=-1 MSCL low=0
 */
__inline static int wait_mscl_low(unsigned long wait_time)
{
	unsigned long timeout = 0;
	
	while(MSCL() == 1) {
		delay_us(1);
		if (++timeout >= wait_time)
			return -1;
		};
	
	return 0;
}

/*
 * Initialize MDI mode of PCF79xx. There are different init sequences needed for different chips.
 * The sequence type is specified by user cmd parameter 2 (former known as "Length")
 * Parameter 2	For Chip		Sequence		
 * 0			26A0700			BATT_ON and MSDA_OUTPUT_0, wait until SCL high, wait until SCL low, MSDA_OUTPUT_1 (MDI internal mode), wait ~25us
 * 1			PCF7945C05		BATT_ON and MSDA_OUTPUT_0, wait 500us (225...725us), MSDA_OUTPUT_1, wait until SCL low, TBC
 * return: -1=error 0=success
 */
int enter_monitor_mode(void)
{
	unsigned long timeout = 0;
	
	/* Disable PIO controller IRQs. */
	set_IRQ_and_EXTI_Line_Cmd(0,0);	//DISABLE MSCL interrupt, (falling edge)
	
	/* power down */
	set_BAT(0); set_MSDA(0);
	delay_ms(100);
		
	/* Set electrical config for MSCL pin */
	if(mdi_type == PCF7945)
	{
		set_MSCL_input_floating();
	}
	else
	{
		set_MSCL_input_pulldown();
	}	
		
	/* power up */
	set_BAT(1);

	

	if(mdi_type == PCF7945)
	{
		delay_us(T_DLY_SDA_HIGH_AFT_PON);
		set_MSDA(1);

		delay_us(T_DLY_SDA_LOW_AFT_SDA_HIGH);


		/* If MSCL is not high, PCF seems to be not connected */
		if(MSCL() != 1){
			return CONNECT_ERR;
		}

		/* Wait PCF setting MSCL low, right afterwards master must select internal or external mdi mode */
		if (wait_mscl_low(TIMEOUT_MSCL_LOW) < 0)
			return CONNECT_ERR_PROT_MODE;
	}
	else
	{
		if (wait_mscl_high(TIMEOUT_MSCL_HIGH) < 0)
		{
			return CONNECT_ERR;
		}
		if (wait_mscl_low(TIMEOUT_MSCL_LOW) < 0)
			return CONNECT_ERR;
	}	


	
	if(mdi_type == PCF7945){
		/* Sequence for PCF7945C05 */
		set_MSDA(0);
		delay_us(8);
		set_MSDA(1);
	}
	else{
		/* Sequence for 26A0700 */
		delay_us(2);
		set_MSDA(1);
	}	
	
	delay_us(50);

	recv_data(1);
	
	/* rising edge signalizes confirmation of MDI mode request */
	active_MSCL_rising_edge_IT(1);

	LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_3); // Test point
	
	timeout = 0;
	while(mdi.status != DONE) {
		if (++timeout >= TIMEOUT_DEV_ACK)
			return RECV_TOUT;
		delay_us(1);
	};
	
	if (mdi.data[0] != DEV_ACK)
		return UNEXPECTED_RESPONSE;
	
	return 0;
}


/*
 * Switch-on/-off supply voltage (BAT_PIN) for PCF
 * All other MDI pins are set to low
 */
int pcf_power_on(void){
	/* Switch power off and wait for discharge */
	set_BAT(0);
	delay_ms(500);

	/* Power up */
	set_MSDA(1);
	set_MSCL(0);
	set_BAT(1);

	return 0;
}


/*
 * Receive single byte from PCF via MDI.
 * This function is called by external interrupt handler
 */
static unsigned char recv_byte(void)
{
	/* First bit (= falling edge of MSCL)? -> Change mdi status and reset data */
	if (bit == 0){
		mdi.status = BUSY;
		data = 0;
	} 
	
	data |= MSDA() << (bit);

	/* check if one byte received */
	if (++bit >= MDI_RCV_BITS) {
		if(mdi.status == WAIT_LAST_PULSE){		
			if (wait_mscl_low(TIMEOUT_MSCL_LOW) < 0)
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

/*
 * Set the bit levels of a data byte which is sent to PCF via MDI.
 * This function is called by external interrupt handler.
 * return 0: success -1:error
 */
static inline int __attribute__((optimize("O1"))) send_byte(unsigned char *data)
{
	if (bit < MDI_SND_BITS) {
		if (((*data >> bit) & 0x01) != 0){
			//Set MSDA;
			MSDA_PORT->BSRR = (1<<MSDA_PIN_number);
		}
		else{
			//Clear MSDA
			MSDA_PORT->BRR =  (1<<MSDA_PIN_number);
		}
	}

	if (mdi.status == IDLE)	{
		mdi.status = BUSY;
		active_MSCL_rising_edge_IT(0);
		active_MSCL_falling_edge_IT(1);
	}

	
	/* check if one byte sent */
	if (bit >= MDI_SND_BITS ) {
		//delay_us(25);
		LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_2);
		//wait_mscl_low(25);
		bit = 0;
		set_MSDA(1);
		mdi.status = DONE;
	}
	bit++;
	return *data;
}

/*
 * Send multiple bytes of data to PCF
 * return 0: success -1:error
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
		bit = 0;
		set_MSDA(0);
		if (wait_mdi_ops_done() < 0){
			active_MSCL_rising_edge_IT(0);
			active_MSCL_falling_edge_IT(0);
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

/*
 * Sends one command byte to the PCF
 * return 0: success -1:error
 */
int send_mdi_cmd(unsigned char byte)
{
	mdi.data[0] = byte;
	mdi.data_ptr = &mdi.data[0];

	if (mdi.dir == RECV)
	{
		delay_us(500);
	}


	active_MSCL_rising_edge_IT(1);
	mdi.dir = SEND;
	mdi.status = IDLE;
	bit = 0;
	set_MSDA(0);
	if (wait_mdi_ops_done() < 0)
	{
		active_MSCL_rising_edge_IT(0);
		active_MSCL_falling_edge_IT(0);
		set_MSDA(1);
		return -1;
	}

	active_MSCL_rising_edge_IT(0);

	mdi.transfer = 0;

	return 0;
}

/*
 * Receive multiple data bytes from PCF
 * return 0: success -1:error
 */
int recv_data(unsigned long len)
{		
	mdi.transfer = 0;

	if (mdi.dir == SEND){
		delay_us(500);
	}
	
	for (unsigned long i = 0; i < len; i++)
	{

		mdi.dir = RECV;
		mdi.status = INIT;
		
		set_MSDA(0);			// Pull SDA low to signalize PCF to start transfer
		
		active_MSCL_rising_edge_IT(1);
		
		if (wait_mdi_ops_done() < 0){
			if(i>0)
			{
				return TOO_LESS_DATA;
			}
			else
			{
				return RCV_TOUT;
			}
		}
		mdi.transfer = i + 1;
		if (mdi.transfer >= len)
			mdi.transfer = 0;
	}
	
	return 0;
}

/*
 * Handler function for external (SCL Pin logic level) interrupt
 */
void exti_handler(void)
{	
	if (mdi.dir == RECV){ 
		/* On first call of exti_handler for receiving data,
		  then it was due to a MSCL rising edge used for switching on the pullup */
		if (mdi.status == INIT){

			set_MSDA_input_pullup();
			mdi.status = IDLE;
			bit = 0;

			/* Activate falling edge IT to sample bit data on every falling edge */
			active_MSCL_falling_edge_IT(1);
		}
		else{
			mdi.data[mdi.transfer] = recv_byte();
		}


		LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_3);


	}	
	else{
		send_byte(mdi.data_ptr + mdi.transfer);
	}
	
	return;
}



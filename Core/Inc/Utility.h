#ifndef Utility_H
#define Utility_H

#include "stdint.h"

void active_MSCL_rising_edge_IT(uint8_t active);
void active_MSCL_falling_edge_IT(uint8_t active);
void set_MSDA(uint8_t lv_active);
void set_MSDA_output_pushpull(void);
void set_MSCL(uint8_t lv_active);
void set_BAT(uint8_t lv_active);
uint8_t MSCL(void);
uint8_t MSDA(void);
void set_MSDA_input_pullup(void);
void set_IRQ_and_EXTI_Line_Cmd(uint8_t enable, uint8_t edge);
void set_MSDA_input_floating(void);
void set_MSCL_input_floating(void);
void set_MSCL_input_pulldown(void);
void set_MSCL_input_pullup(void);
status_code_t RcvBytesUSB(uint8_t Data[], uint16_t bytes, uint32_t tout);
void SendBytesUsb(uint8_t Data[], uint16_t bytes, uint32_t tout);
void revert_bytes(uint8_t *Data, uint8_t length);

/************BEGIN Section for Test Instrumentation ********************/
// Prototype for testing function
void GetAdcValue_convert_and_print_serial(void);

extern uint32_t raw_adc;
extern uint8_t adc_offset_hex;
extern uint8_t converted_adc_str[4]; // Voltage format example: "2" , "," , "7" , [newline]

/**************** END Section Test Instrumentation ********************/

#endif

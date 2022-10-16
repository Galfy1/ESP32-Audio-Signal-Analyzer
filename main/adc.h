#ifndef ADC_H_INCLUDED
#define ADC_H_INCLUDED

#include <stdio.h>





//void adc_init(uint16_t * pu16_adc_buf_left, uint16_t * pu16_adc_buf_right, int32_t i32_res_buffer_len);
void adc_task();

void adc_init(uint16_t * pu16_adc1_shared, uint16_t * pu16_adc2_shared, TaskHandle_t dsp_left_ch_task_handle, TaskHandle_t dsp_right_ch_task_handle);


#endif /*ADC_H_INCLUDED */
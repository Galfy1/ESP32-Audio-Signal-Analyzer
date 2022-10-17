#ifndef DSP_H_INCLUDED
#define DSP_H_INCLUDED

#include <stdio.h>



void dsp_left_ch_task();
void dsp_right_ch_task();

void dsp_init(uint16_t * pu16_adc1_shared, uint16_t * pu16_adc2_shared, SemaphoreHandle_t adc1_shared_mem_sem, SemaphoreHandle_t adc2_shared_mem_sem);

#endif /*DSP_H_INCLUDED */
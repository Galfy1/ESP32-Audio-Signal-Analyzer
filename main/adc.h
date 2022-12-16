#ifndef ADC_H_INCLUDED
#define ADC_H_INCLUDED

#include <stdio.h>

void adc_task();

void adc_init(float *pu16_adc1_shared, float *pu16_adc2_shared, SemaphoreHandle_t adc1_shared_mem_sem, SemaphoreHandle_t adc2_shared_mem_sem);

void adc_set_dsp_task_handles(TaskHandle_t dsp_left_task_handle, TaskHandle_t dsp_right_task_handle);

#endif /*ADC_H_INCLUDED */
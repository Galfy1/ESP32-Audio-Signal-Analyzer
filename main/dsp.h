#ifndef DSP_H_INCLUDED
#define DSP_H_INCLUDED

#include <stdio.h>

void dsp_ecep_handler_task();
void dsp_left_ch_task();
void dsp_right_ch_task();

void dsp_init(float *pf32_adc1_shared,
              float *pf32_adc2_shared,
              SemaphoreHandle_t adc1_shared_mem_sem,
              SemaphoreHandle_t adc2_shared_mem_sem,
              float *pf32_L_fft_res_shr,
              float *pf32_R_fft_res_shr,
              SemaphoreHandle_t fft_L_res_shr_mem_sem,
              SemaphoreHandle_t fft_R_res_shr_mem_sem,
              QueueHandle_t xQueue_dsp_ecep_cmds);

#endif /*DSP_H_INCLUDED */
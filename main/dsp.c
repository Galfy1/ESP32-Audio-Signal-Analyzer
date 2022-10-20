#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_dsp.h" // FOR TEST

#include "dsp.h"
#include "common_defines.h"
#include "fft_wrapper.h"


static const char *ADC_TAG = "DSP.C";

#define RADIX_USED         radix2           // eFFT_radix_type_t   (Ops: the chosen radix will have to support the DATA_BLOCK_SIZE)
#define FFT_OUTPUT_FORMAT  ampltitude_db    // eFFT_output_form_t
#define WINDOW_TYPE        hann             // eFFT_window_type_t

static float * gs_pf32_adc1_shared;
static float * gs_pf32_adc2_shared;
static SemaphoreHandle_t gs_adc1_shared_mem_sem;   
static SemaphoreHandle_t gs_adc2_shared_mem_sem;

static float * gs_pf32_L_fft_res_shr;
static float * gs_pf32_R_fft_res_shr;
static SemaphoreHandle_t gs_fft_L_res_shr_mem_sem;
static SemaphoreHandle_t gs_fft_R_res_shr_mem_sem;

// Arrayes are here 16bit alligned by the compiler in memory to eliminate any unsesessay memory accessesing overhead
// See: http://www.songho.ca/misc/alignment/dataalign.html for more info
__attribute__((aligned(16))) 
static float gs_af32_adc1_temp_buf[DATA_BLOCK_SIZE] = {0};  
__attribute__((aligned(16))) 
static float gs_af32_adc2_temp_buf[DATA_BLOCK_SIZE] = {0};  


// FOR FFT TEST:
// __attribute__((aligned(16)))
// float x1[DATA_BLOCK_SIZE];


void dsp_left_ch_task()
{

    // ---FFT TEST BEGIN---
    // dsps_tone_gen_f32(x1, DATA_BLOCK_SIZE, 3000.0, 0.7, 0);  // De hentyder i doc til at 1 er fny... MEN det argere mere somom at 1 er fs ogs 0.5 er fny...
    // // for(int i = 0 ; i < 300 ; i++)
    // // {   
    // //     ESP_LOGI(ADC_TAG, "X1 Index %d: %f", i, x1[i]);
    // // }
    // // KØR INIT OG FFT HER
    // fft_wrapper_init_fast(hann);
    // fft_wrapper_start_fft_1real_fast(x1, radix2, ampltitude_db);
    // for(int i = 0 ; i < DATA_BLOCK_SIZE/2 ; i++)
    // {   
    //     ESP_LOGI(ADC_TAG, "X1 Index %d: %f", i, x1[i]);
    // }
    // // First half of x1 is aplitude values. Second half is junk.
    // dsps_view_spectrum(x1, DATA_BLOCK_SIZE/2, -60, 40);  // Amplitude output of fft is only of size DATA_BLOCK_SIZE/2 (it takes takes one complex number (two indexes) to create one aplitude value (one index))
    // ---FFT TEST END---

    while(1)
    {  
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if(xSemaphoreTake(gs_adc1_shared_mem_sem, portMAX_DELAY) == pdTRUE)
        {
            memcpy(gs_af32_adc1_temp_buf , gs_pf32_adc1_shared , sizeof(gs_af32_adc1_temp_buf));  // Copy from shared memory to local copy
            memset(gs_pf32_adc1_shared, 0, sizeof(gs_af32_adc1_temp_buf));  // Reset the shared memory, just to be sure (gs_af32_adc1_temp_buf is same size as shared space array)
            xSemaphoreGive(gs_adc1_shared_mem_sem);
            
            //ESP_LOGI(ADC_TAG, "WUHUUU: %f" , gs_af32_adc1_temp_buf[0]);
            // for(int32_t i = 0 ; i <= DATA_BLOCK_SIZE ; i += 1)
            // {
            //     ESP_LOGI(ADC_TAG, "WUHUUU %d: %d" , (int)i, gs_af32_adc1_temp_buf[i]);
            // }

            fft_wrapper_start_fft_1real_fast(gs_af32_adc1_temp_buf, RADIX_USED, FFT_OUTPUT_FORMAT); // Run FFT

            if(xSemaphoreTake(gs_fft_L_res_shr_mem_sem, portMAX_DELAY) == pdTRUE)
            {
                memcpy(gs_pf32_L_fft_res_shr, gs_af32_adc1_temp_buf, sizeof(gs_af32_adc1_temp_buf)/2); // Copy FFT result from local copy to shared memory
                                                                                                       // (Only half of gs_af32_adc1_temp_buf is the result. rest is junk)
                xSemaphoreGive(gs_fft_L_res_shr_mem_sem);
            }
        }

         // Print test:

        

        //  for(int32_t i = 0 ; i <= sizeof(gs_af32_adc1_temp_buf) ; i += 100)
        //  {
        //     ESP_LOGI(ADC_TAG, "WUHUUU: %d" , gs_af32_adc1_temp_buf[i]);
        //  }


         //vTaskDelay(1); // DEN HER SKAL NOK VÆK
    }
    
}

void dsp_right_ch_task()
{
   
    while(1)
    {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if(xSemaphoreTake(gs_adc2_shared_mem_sem, portMAX_DELAY) == pdTRUE)
        {
            memcpy(gs_af32_adc2_temp_buf , gs_pf32_adc2_shared , sizeof(gs_af32_adc2_temp_buf)); // Copy from shared memory to local copy
            memset(gs_pf32_adc2_shared, 0, sizeof(gs_af32_adc2_temp_buf));  // Reset the shared memory
            xSemaphoreGive(gs_adc2_shared_mem_sem);

            fft_wrapper_start_fft_1real_fast(gs_af32_adc2_temp_buf, RADIX_USED, FFT_OUTPUT_FORMAT); // Run FFT
            if(xSemaphoreTake(gs_fft_R_res_shr_mem_sem, portMAX_DELAY) == pdTRUE)
            {
                memcpy(gs_pf32_R_fft_res_shr, gs_af32_adc2_temp_buf, sizeof(gs_af32_adc2_temp_buf)/2); // Copy FFT result from local copy to shared memory
                xSemaphoreGive(gs_fft_R_res_shr_mem_sem);
            }
        }


        //vTaskDelay(1);
    }
}


void dsp_init(float * pf32_adc1_shared, 
              float * pf32_adc2_shared, 
              SemaphoreHandle_t adc1_shared_mem_sem, 
              SemaphoreHandle_t adc2_shared_mem_sem, 
              float * pf32_L_fft_res_shr, 
              float * pf32_R_fft_res_shr,
              SemaphoreHandle_t fft_L_res_shr_mem_sem,
              SemaphoreHandle_t fft_R_res_shr_mem_sem)
{
    gs_pf32_adc1_shared = pf32_adc1_shared;
    gs_pf32_adc2_shared = pf32_adc2_shared;
    gs_adc1_shared_mem_sem = adc1_shared_mem_sem;
    gs_adc2_shared_mem_sem = adc2_shared_mem_sem;

    gs_pf32_left_fft_res = pf32_L_fft_res_shr;
    gs_pf32_right_fft_res = pf32_R_fft_res_shr;
    gs_fft_L_res_shr_mem_sem = fft_L_res_shr_mem_sem;
    gs_fft_R_res_shr_mem_sem = fft_R_res_shr_mem_sem;

    fft_wrapper_init_fast(WINDOW_TYPE);
}
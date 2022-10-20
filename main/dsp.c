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



static float * gs_pf32_adc1_shared;
static float * gs_pf32_adc2_shared;

// Arrayes are here 16bit alligned by the compiler in memory to make eliminate any unsesessay memory accessesing overhead
// See: http://www.songho.ca/misc/alignment/dataalign.html for more info
__attribute__((aligned(16))) 
static float gs_af32_adc1_temp_buf[DATA_BLOCK_SIZE] = {0};  
__attribute__((aligned(16))) 
static float gs_af32_adc2_temp_buf[DATA_BLOCK_SIZE] = {0};  

static SemaphoreHandle_t gs_adc1_shared_mem_sem;   
static SemaphoreHandle_t gs_adc2_shared_mem_sem;

// FOR FFT TEST:
__attribute__((aligned(16)))
float x1[DATA_BLOCK_SIZE];


void dsp_left_ch_task()
{

    // FOR FFT TEST:
    dsps_tone_gen_f32(x1, DATA_BLOCK_SIZE, 3000.0, 0.7, 0);  // De hentyder i doc til at 1 er fny... MEN det argere mere somom at 1 er fs ogs 0.5 er fny...
    // for(int i = 0 ; i < 300 ; i++)
    // {   
    //     ESP_LOGI(ADC_TAG, "X1 Index %d: %f", i, x1[i]);
    // }
    // KØR INIT OG FFT HER
    //fft_wrapper_init_normal(hann);
    fft_wrapper_init_fast(hann);
    fft_wrapper_start_fft_1real_fast(x1, radix4);

    for(int i = 0 ; i < DATA_BLOCK_SIZE/2 ; i++)
    {   
        ESP_LOGI(ADC_TAG, "X1 Index %d: %f", i, x1[i]);
    }

    // First half of x1 is aplitude values. Second half is junk.
    dsps_view_spectrum(x1, DATA_BLOCK_SIZE/2, -60, 40);  // Amplitude output of fft is only of size DATA_BLOCK_SIZE/2 (it takes takes one complex number (two indexes) to create one aplitude value (one index))



    while(1)
    {  
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if(xSemaphoreTake(gs_adc1_shared_mem_sem, portMAX_DELAY) == pdTRUE)
        {
            memcpy(gs_af32_adc1_temp_buf , gs_pf32_adc1_shared , sizeof(gs_af32_adc1_temp_buf));
            memset(gs_pf32_adc1_shared, 0, sizeof(gs_af32_adc1_temp_buf));  // Reset the shared memory, just to be sure (gs_af32_adc1_temp_buf is same size as shared space array)
            xSemaphoreGive(gs_adc1_shared_mem_sem);

            //ESP_LOGI(ADC_TAG, "WUHUUU: %f" , gs_af32_adc1_temp_buf[0]);

            // for(int32_t i = 0 ; i <= DATA_BLOCK_SIZE ; i += 1)
            // {
            //     ESP_LOGI(ADC_TAG, "WUHUUU %d: %d" , (int)i, gs_af32_adc1_temp_buf[i]);
            // }


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
            memcpy(gs_af32_adc2_temp_buf , gs_pf32_adc2_shared , sizeof(gs_af32_adc2_temp_buf));
            memset(gs_pf32_adc2_shared, 0, sizeof(gs_af32_adc2_temp_buf));  // Reset the shared memory, just to be sure (gs_af32_adc1_temp_buf is same size as shared space array)
            xSemaphoreGive(gs_adc2_shared_mem_sem);
        }


        //vTaskDelay(1);
    }
}


void dsp_init(float * pu16_adc1_shared, float * pu16_adc2_shared, SemaphoreHandle_t adc1_shared_mem_sem, SemaphoreHandle_t adc2_shared_mem_sem)
{
    gs_pf32_adc1_shared = pu16_adc1_shared;
    gs_pf32_adc2_shared = pu16_adc2_shared;

    gs_adc1_shared_mem_sem = adc1_shared_mem_sem;
    gs_adc2_shared_mem_sem = adc2_shared_mem_sem;
}
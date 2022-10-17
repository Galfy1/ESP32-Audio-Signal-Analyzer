#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "dsp.h"
#include "common_defines.h"


static const char *ADC_TAG = "DSP.C";



static uint16_t * gs_pu16_adc1_shared;
static uint16_t * gs_pu16_adc2_shared;

static uint16_t gs_au16_adc1_temp_buf[RES_BUFFER_SIZE] = {0};  
static uint16_t gs_au16_adc2_temp_buf[RES_BUFFER_SIZE] = {0};  

static SemaphoreHandle_t gs_adc1_shared_mem_sem;   
static SemaphoreHandle_t gs_adc2_shared_mem_sem;


void dsp_left_ch_task()
{

    while(1)
    {  
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if(xSemaphoreTake(gs_adc1_shared_mem_sem, portMAX_DELAY) == pdTRUE)
        {
            memcpy(gs_au16_adc1_temp_buf , gs_pu16_adc1_shared , sizeof(gs_au16_adc1_temp_buf));
            memset(gs_pu16_adc1_shared, 0, sizeof(gs_au16_adc1_temp_buf));  // Reset the shared memory, just to be sure (gs_au16_adc1_temp_buf is same size as shared space array)
            xSemaphoreGive(gs_adc1_shared_mem_sem);

            ESP_LOGI(ADC_TAG, "WUHUUU: %d" , gs_au16_adc1_temp_buf[0]);
            // for(int32_t i = 0 ; i <= RES_BUFFER_SIZE ; i += 1)
            // {
            //     ESP_LOGI(ADC_TAG, "WUHUUU %d: %d" , (int)i, gs_au16_adc1_temp_buf[i]);
            // }


        }

         // Print test:

        

        //  for(int32_t i = 0 ; i <= sizeof(gs_au16_adc1_temp_buf) ; i += 100)
        //  {
        //     ESP_LOGI(ADC_TAG, "WUHUUU: %d" , gs_au16_adc1_temp_buf[i]);
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
            memcpy(gs_au16_adc2_temp_buf , gs_pu16_adc2_shared , sizeof(gs_au16_adc2_temp_buf));
            memset(gs_pu16_adc2_shared, 0, sizeof(gs_au16_adc2_temp_buf));  // Reset the shared memory, just to be sure (gs_au16_adc1_temp_buf is same size as shared space array)
            xSemaphoreGive(gs_adc2_shared_mem_sem);
        }


        //vTaskDelay(1);
    }
}


void dsp_init(uint16_t * pu16_adc1_shared, uint16_t * pu16_adc2_shared, SemaphoreHandle_t adc1_shared_mem_sem, SemaphoreHandle_t adc2_shared_mem_sem)
{
    gs_pu16_adc1_shared = pu16_adc1_shared;
    gs_pu16_adc2_shared = pu16_adc2_shared;

    gs_adc1_shared_mem_sem = adc1_shared_mem_sem;
    gs_adc2_shared_mem_sem = adc2_shared_mem_sem;
}
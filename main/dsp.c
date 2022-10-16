#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "dsp.h"
#include "common_defines.h"


static uint16_t * gs_pu16_adc1_shared;
static uint16_t * gs_pu16_adc2_shared;

static TaskHandle_t gs_adc_task_handle;

static uint16_t gs_au16_adc1_temp_buf[RES_BUFFER_SIZE] = {0};  
static uint16_t gs_au16_adc2_temp_buf[RES_BUFFER_SIZE] = {0};  


void dsp_left_ch_task()
{
    ulTaskNotifyTakeIndexed(ADC_LEFT_SHARED_MEM_TASK_NOTI_INDEX, pdTRUE, portMAX_DELAY);  // use task noticiation as a mutex / binary semaphore
    memcpy(gs_au16_adc1_temp_buf , gs_pu16_adc1_shared , sizeof(gs_au16_adc1_temp_buf));
    memset(gs_pu16_adc1_shared, 0, sizeof(gs_au16_adc1_temp_buf));  // Reset the shared memory, just to be sure (gs_au16_adc1_temp_buf is same size as shared space array)
    xTaskNotifyGiveIndexed(gs_adc_task_handle, ADC_LEFT_SHARED_MEM_TASK_NOTI_INDEX);         // use task noticiation as a mutex / binary semaphore 

    // Print test:
    
}

void dsp_right_ch_task()
{
    ulTaskNotifyTakeIndexed(ADC_RIGHT_SHARED_MEM_TASK_NOTI_INDEX, pdTRUE, portMAX_DELAY);  // use task noticiation as a mutex / binary semaphore
    memcpy(gs_au16_adc2_temp_buf , gs_pu16_adc2_shared , sizeof(gs_au16_adc2_temp_buf));
    memset(gs_pu16_adc2_shared, 0, sizeof(gs_au16_adc2_temp_buf));  // Reset the shared memory, just to be sure (gs_au16_adc1_temp_buf is same size as shared space array)
    xTaskNotifyGiveIndexed(gs_adc_task_handle, ADC_RIGHT_SHARED_MEM_TASK_NOTI_INDEX);         // use task noticiation as a mutex / binary semaphore 
    
}


void dsp_init(uint16_t * pu16_adc1_shared, uint16_t * pu16_adc2_shared, TaskHandle_t adc_task_handle)
{
    gs_pu16_adc1_shared = pu16_adc1_shared;
    gs_pu16_adc2_shared = pu16_adc2_shared;

    gs_adc_task_handle = adc_task_handle;
}
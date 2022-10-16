#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "common_defines.h"
#include "adc.h"
#include "dsp.h"

static const char *TAG = "TeestLog";

/*
Unlike Vanilla FreeRTOS, users must not call vTaskStartScheduler(). 
Instead, ESP-IDF FreeRTOS is started automatically. 
The entry point is a user defined void app_main(void) function.
See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html for more info
*/

static uint16_t gs_au16_adc1_shared[RES_BUFFER_SIZE] = {0};  
static uint16_t gs_au16_adc2_shared[RES_BUFFER_SIZE] = {0};  

void app_main(void)
{    
    TaskHandle_t adc_task_handle;
    TaskHandle_t dsp_left_ch_task_handle;
    TaskHandle_t dsp_right_ch_task_handle;

    ESP_LOGI(TAG, "DET VIRKER :D");


    // Create tasks:
    // FIND LIGE UD AF MED DE PRIORITETER HER...
    // HAR SAT dsp_left_ch_task og dsp_right_ch_task TIL BEGGE AT KØRE OP CORE 1 TIL AT STARTE MED. MEN DEN ENE SKAL OVER PÅ CORE 2 PÅ ET TIDSPUNKT
    xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 2, &adc_task_handle, 1); // Use core 1 
    xTaskCreatePinnedToCore(dsp_left_ch_task, "dsp_left_task", 4096, NULL, 4, &dsp_left_ch_task_handle, 1); // Use core 1 
    xTaskCreatePinnedToCore(dsp_right_ch_task, "dsp_right_task", 4096, NULL, 4, &dsp_right_ch_task_handle, 1); // Use core 2 


    adc_init(gs_au16_adc1_shared, gs_au16_adc2_shared, dsp_left_ch_task_handle, dsp_right_ch_task_handle); 
    dsp_init(gs_au16_adc1_shared, gs_au16_adc2_shared, adc_task_handle);

}

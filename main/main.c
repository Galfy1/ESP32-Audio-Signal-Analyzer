#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

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

// Uart GPIO19 and GPIO20 are reserved for JTAG debugging (tror jeg. slet hvis jeg ikke kan få det til at virke)


static float gs_af32_adc1_shared[DATA_BLOCK_SIZE] = {0};  // Shared memory ADC1 (Left mic ch.)
static float gs_af32_adc2_shared[DATA_BLOCK_SIZE] = {0};  // Shared memory ADC2 (Right mic ch.)



// SKAL FINDE UD AF HVORDAN JEG PASSER SEMAPHORE TIL FUNCITONER. OM DET ER OKAY DEN TAGER EN KOPI AF HANDLEN. DET ER DET VELL egentlig


void app_main(void)
{    
    SemaphoreHandle_t adc1_shared_mem_sem;   
    SemaphoreHandle_t adc2_shared_mem_sem;
    TaskHandle_t dsp_left_task_handle;
    TaskHandle_t dsp_right_task_handle;


    ESP_LOGI(TAG, "DET VIRKER :D");

    //Create sempahores:
    adc1_shared_mem_sem = xSemaphoreCreateMutex();
    adc2_shared_mem_sem = xSemaphoreCreateMutex();
    if( (adc1_shared_mem_sem == NULL) || (adc2_shared_mem_sem == NULL))
    {
        ESP_LOGI(TAG, "ERROR: while creating one or more semaphores");
    }

    // Has to run before creating tasks:
    adc_init(gs_af32_adc1_shared, gs_af32_adc2_shared, adc1_shared_mem_sem, adc2_shared_mem_sem); 
    dsp_init(gs_af32_adc1_shared, gs_af32_adc2_shared, adc1_shared_mem_sem, adc2_shared_mem_sem);

    // Create tasks:
    // FIND LIGE UD AF MED DE PRIORITETER HER...
    // HAR SAT dsp_left_ch_task og dsp_right_ch_task TIL BEGGE AT KØRE OP CORE 1 TIL AT STARTE MED. MEN DEN ENE SKAL OVER PÅ CORE 2 PÅ ET TIDSPUNKT
    xTaskCreatePinnedToCore(dsp_left_ch_task, "dsp_left_task", 4096, NULL, 4, &dsp_left_task_handle, 1); // Use core 1 
    xTaskCreatePinnedToCore(dsp_right_ch_task, "dsp_right_task", 4096, NULL, 4, &dsp_right_task_handle, 1); // Use core 2 
    adc_set_dsp_task_handles(dsp_left_task_handle, dsp_right_task_handle);   /* Give the handles of the dsp tasks to the adc task.
                                                                                This is done so the adc task can use direct task notification to syncronize use of the shared memory
                                                                             */ 
    xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 2, NULL, 1); // Use core 1 


}

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_heap_caps.h"

#include "common_defines.h"
#include "adc.h"
#include "dsp.h"
#include "ecep_uart.h"
#include "patient_response.h"
#include "ecep_defines.h"

static const char *TAG = "main.c";

/*
Unlike Vanilla FreeRTOS, users must not call vTaskStartScheduler(). 
Instead, ESP-IDF FreeRTOS is started automatically. 
The entry point is a user defined void app_main(void) function.
See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html for more info
*/

/* Structure guidelines for this project:
    All Tasks should be created in main.c
    All Queues, Semaphores and Shared memory used by multiple files should be created in main.c
    All Queues, Semaphores and Shared memory used by only one file should be created just within that file
*/

void app_main(void)
{    
    TaskHandle_t dsp_left_task_handle;
    TaskHandle_t dsp_right_task_handle;

    QueueHandle_t xQueue_pr_cmd;
    QueueHandle_t xQueue_event;
    
    ESP_LOGI(TAG, "DET VIRKER :D");

    // Create Shared Memory Located in PSRAM (created as arrays using a special malloc() for PSRAM)
    float * pf32_adc1_shared = heap_caps_malloc(DATA_BLOCK_SIZE * sizeof(float), MALLOC_CAP_SPIRAM); // Shared memory ADC1 (Left mic ch.)
    float * pf32_adc2_shared = heap_caps_malloc(DATA_BLOCK_SIZE * sizeof(float), MALLOC_CAP_SPIRAM); // Shared memory ADC2 (Right mic ch.)
    if((pf32_adc1_shared == NULL) || (pf32_adc2_shared == NULL))
    {
        ESP_LOGE(TAG, "ERROR: while allocating shared ADC memory in PSRAM");
    }
    memset(pf32_adc1_shared, 0 ,DATA_BLOCK_SIZE * sizeof(float)); 
    memset(pf32_adc2_shared, 0 ,DATA_BLOCK_SIZE * sizeof(float));

    float * pf32_L_fft_res_shr = heap_caps_malloc(DATA_BLOCK_SIZE/2 * sizeof(float), MALLOC_CAP_SPIRAM); // Shared memory of post proceesed FFT output from DSP task (Left mic ch.)  // SKAL MÅSKE HEDE DSP RES I STEDET??
    float * pf32_R_fft_res_shr = heap_caps_malloc(DATA_BLOCK_SIZE/2 * sizeof(float), MALLOC_CAP_SPIRAM); // Shared memory of post proceesed FFT output from DSP task (Right mic ch.)
    if((pf32_L_fft_res_shr == NULL) || (pf32_R_fft_res_shr == NULL))
    {
        ESP_LOGE(TAG, "ERROR: while allocating shared FFT res memory in PSRAM");
    }
    memset(pf32_L_fft_res_shr, 0 ,DATA_BLOCK_SIZE/2 * sizeof(float)); 
    memset(pf32_R_fft_res_shr, 0 ,DATA_BLOCK_SIZE/2 * sizeof(float));


    // Create sempahores:
    SemaphoreHandle_t adc1_shared_mem_sem = xSemaphoreCreateMutex();    // Mutex for gs_af32_adc1_shared
    SemaphoreHandle_t adc2_shared_mem_sem = xSemaphoreCreateMutex();    // Mutex for gs_af32_adc2_shared
    SemaphoreHandle_t fft_L_res_shr_mem_sem = xSemaphoreCreateMutex();  // Mutex for gs_af32_L_fft_res_shr  
    SemaphoreHandle_t fft_R_res_shr_mem_sem = xSemaphoreCreateMutex();  // Mutex for gs_af32_R_fft_res_shr
    if( (adc1_shared_mem_sem == NULL) || (adc2_shared_mem_sem == NULL) || (fft_L_res_shr_mem_sem == NULL) || (fft_R_res_shr_mem_sem == NULL))
    {
        ESP_LOGE(TAG, "ERROR: while creating one or more semaphores");
    }
    
    // Create queues:
    xQueue_pr_cmd = xQueueCreate(10, sizeof(sIntern_command_setup_t));
    xQueue_event = xQueueCreate(10, sizeof(sIntern_event_setup_t));
    if ((xQueue_pr_cmd == NULL) || (xQueue_event == NULL))
    {
        ESP_LOGE(TAG, "ERROR: while creating one or more queues");
    }

    // Init functions (has to run before creating tasks):
    adc_init(
        pf32_adc1_shared, 
        pf32_adc2_shared, 
        adc1_shared_mem_sem, 
        adc2_shared_mem_sem); 

    dsp_init(
        pf32_adc1_shared, 
        pf32_adc2_shared, 
        adc1_shared_mem_sem, 
        adc2_shared_mem_sem,
        pf32_L_fft_res_shr,
        pf32_R_fft_res_shr,
        fft_L_res_shr_mem_sem,
        fft_R_res_shr_mem_sem);

    patient_resp_init(
        xQueue_pr_cmd,
        xQueue_event);

    //ecep_uart_init();
/*
*/
    // Create tasks:
    // FIND LIGE UD AF MED DE PRIORITETER HER... ADCEN tasken skal nok have højeste priroritet! 
    // HAR SAT dsp_left_ch_task og dsp_right_ch_task TIL BEGGE AT KØRE OP CORE 1 TIL AT STARTE MED. MEN DEN ENE SKAL OVER PÅ CORE 2 PÅ ET TIDSPUNKT

    // Create tasks (order is important)
    // xTaskCreatePinnedToCore(dsp_left_ch_task, "dsp_left_task", 4096, NULL, 4, &dsp_left_task_handle, 1); // Use core 1 
    // xTaskCreatePinnedToCore(dsp_right_ch_task, "dsp_right_task", 4096, NULL, 4, &dsp_right_task_handle, 1); // Use core 2 
    // adc_set_dsp_task_handles(dsp_left_task_handle, dsp_right_task_handle);  /* Give the handles of the dsp tasks to the adc task.
    //                                                                            This is done so the adc task can use direct task notification to syncronize use of the shared memory */
    // xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 8, NULL, 1); // Use core 1 . ADC task needs the higher priority compared to other tasks
    xTaskCreatePinnedToCore(dummy_task, "dummy_task", 4096, NULL, 4, NULL, 1); // Use core 1. 
    xTaskCreatePinnedToCore(patient_resp_task, "patient_resp_task", 4096, NULL, 4, NULL, 1); // Use core 1.     
    //xTaskCreatePinnedToCore(ecep_uart_event_task, "ecep_uart_event", 4096, NULL, 4, NULL, 1);
    //xTaskCreatePinnedToCore(ecep_packet_handler_task, "ecep_packet_handler", 4096, NULL, 4, NULL, 1);


    // Amount of PSRAM left:
    ESP_LOGI(TAG, "Amount of PSRAM left: %i" , (int) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

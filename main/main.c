#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "common_defines.h"
#include "adc.h"
#include "dsp.h"
#include "ecep_uart.h"

static const char *TAG = "main.c";

/*
Unlike Vanilla FreeRTOS, users must not call vTaskStartScheduler(). 
Instead, ESP-IDF FreeRTOS is started automatically. 
The entry point is a user defined void app_main(void) function.
See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html for more info
*/

// Uart GPIO19 and GPIO20 are reserved for JTAG debugging (tror jeg. slet hvis jeg ikke kan få det til at virke)

// Shared Memory:
static float gs_af32_adc1_shared[DATA_BLOCK_SIZE] = {0};  // Shared memory ADC1 (Left mic ch.)
static float gs_af32_adc2_shared[DATA_BLOCK_SIZE] = {0};  // Shared memory ADC2 (Right mic ch.)

static float gs_af32_L_fft_res_shr[DATA_BLOCK_SIZE/2] = {0}; // Shared memory of post proceesed FFT output from DSP task (Left mic ch.)
static float gs_af32_R_fft_res_shr[DATA_BLOCK_SIZE/2] = {0}; // Shared memory of post proceesed FFT output from DSP task (Right mic ch.)



/* Structure guidelines for this project:
    All Tasks should be created in main.c
    All Queues, Semaphores and Shared memory used by multiple files should be created in main.c
    All Queues, Semaphores and Shared memory used by only one file should be created just within that file
*/


void app_main(void)
{    
    SemaphoreHandle_t adc1_shared_mem_sem;   
    SemaphoreHandle_t adc2_shared_mem_sem;
    SemaphoreHandle_t fft_L_res_shr_mem_sem;
    SemaphoreHandle_t fft_R_res_shr_mem_sem;

    TaskHandle_t dsp_left_task_handle;
    TaskHandle_t dsp_right_task_handle;

    ESP_LOGI(TAG, "DET VIRKER :D");

    //Create sempahores:
    adc1_shared_mem_sem = xSemaphoreCreateMutex();    // Mutex for gs_af32_adc1_shared
    adc2_shared_mem_sem = xSemaphoreCreateMutex();    // Mutex for gs_af32_adc2_shared
    fft_L_res_shr_mem_sem = xSemaphoreCreateMutex();  // Mutex for gs_af32_L_fft_res_shr  
    fft_R_res_shr_mem_sem = xSemaphoreCreateMutex();  // Mutex for gs_af32_R_fft_res_shr
    if( (adc1_shared_mem_sem == NULL) || (adc2_shared_mem_sem == NULL) || (fft_L_res_shr_mem_sem == NULL) || (fft_R_res_shr_mem_sem == NULL))
    {
        ESP_LOGE(TAG, "ERROR: while creating one or more semaphores");
    }

    // Init functions (has to run before creating tasks):
    adc_init(
        gs_af32_adc1_shared, 
        gs_af32_adc2_shared, 
        adc1_shared_mem_sem, 
        adc2_shared_mem_sem); 
    dsp_init(
        gs_af32_adc1_shared, 
        gs_af32_adc2_shared, 
        adc1_shared_mem_sem, 
        adc2_shared_mem_sem,
        gs_af32_L_fft_res_shr,
        gs_af32_R_fft_res_shr,
        fft_L_res_shr_mem_sem,
        fft_R_res_shr_mem_sem);
    //ecep_uart_init();

    // Create tasks:
    // FIND LIGE UD AF MED DE PRIORITETER HER... ADCEN tasken skal nok have højeste priroritet! 
    // HAR SAT dsp_left_ch_task og dsp_right_ch_task TIL BEGGE AT KØRE OP CORE 1 TIL AT STARTE MED. MEN DEN ENE SKAL OVER PÅ CORE 2 PÅ ET TIDSPUNKT
    xTaskCreatePinnedToCore(dsp_left_ch_task, "dsp_left_task", 4096, NULL, 4, &dsp_left_task_handle, 1); // Use core 1 
    xTaskCreatePinnedToCore(dsp_right_ch_task, "dsp_right_task", 4096, NULL, 4, &dsp_right_task_handle, 1); // Use core 2 
    adc_set_dsp_task_handles(dsp_left_task_handle, dsp_right_task_handle);   /* Give the handles of the dsp tasks to the adc task.
                                                                                This is done so the adc task can use direct task notification to syncronize use of the shared memory
                                                                             */ 
    xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 8, NULL, 1); // Use core 1 
    //xTaskCreatePinnedToCore(ecep_uart_event_task, "ecep_uart_event", 4096, NULL, 4, NULL, 1);
    //xTaskCreatePinnedToCore(ecep_packet_handler_task, "ecep_packet_handler", 4096, NULL, 4, NULL, 1);


}

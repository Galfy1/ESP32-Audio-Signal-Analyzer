#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_adc/adc_continuous.h"


static const char *TAG = "ADC.C";

#define CONV_FRAME_SIZE     256  // In bytes
#define ADC_CONV_MODE       ADC_CONV_BOTH_UNIT
#define ADC_OUTPUT_TYPE     ADC_DIGI_OUTPUT_FORMAT_TYPE2

static int32_t i32AmountOfConvResInFrame = CONV_FRAME_SIZE / sizeof(adc_digi_output_data_t);
static QueueHandle_t xQueue_ADCDataFromISR;


void adc_task()
{
    TickType_t xDelay500 = 1 / portTICK_PERIOD_MS;

    for(;;)
    {

        adc_digi_output_data_t p_temp_res;    

        if(xQueueReceive(xQueue_ADCDataFromISR, (void *)&p_temp_res, (TickType_t)portMAX_DELAY ) == pdPASS)
        {
            ESP_LOGI(TAG, "Unit: %d,_Channel: %d, Value: %x", p_temp_res.type2.unit+1, p_temp_res.type2.channel, p_temp_res.type2.data);
        }



        // check_valid_data()

        //ESP_LOGI(TAG, "WWWWEEEE :D");
        vTaskDelay(xDelay500);
    }
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE; // From espressif example

    BaseType_t xHigherPriorityTaskWoken; // From xQueueSendFromISR(). (I dont use it for anything atm)
    xHigherPriorityTaskWoken = pdFALSE;

    //adc_digi_output_data_t testy[edata->size / sizeof(adc_digi_output_data_t)];
   //memcpy(testy, edata->conv_frame_buffer, edata->size);

    uint8_t u8AmountOfRes = edata->size / sizeof(adc_digi_output_data_t);

    for(int32_t i = 0 ; i < u8AmountOfRes ; i++)
    {
        xQueueSendFromISR(xQueue_ADCDataFromISR, (adc_digi_output_data_t *)(edata->conv_frame_buffer), &xHigherPriorityTaskWoken);
    }





   // ESP_LOGI(TAG, "DATA", );

    //Notify that ADC continuous driver has done enough number of conversions
    //vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

void adc_init()
{
    adc_continuous_handle_t handle = NULL;

    // Create ADC handle:
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = CONV_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 80 * 1000,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_OUTPUT_TYPE,
    };

    // Config ADC handle:
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = 2;

    adc_pattern[0].atten = ADC_ATTEN_DB_2_5;
    adc_pattern[0].channel = 0; //GPIO 1
    adc_pattern[0].unit = 0;    //ADC 1
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    adc_pattern[1].atten = ADC_ATTEN_DB_2_5;
    adc_pattern[1].channel = 9; // GPIO 20
    adc_pattern[1].unit = 1;    // ADC 2
    adc_pattern[1].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));


    //Set callback function:
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));


    //ADC task:
    // Create a task to handler HCI UART events:
    xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 8, NULL, 1); // Use core 1 (application core)

    //ADC queue (from ads isr to adc task)
    xQueue_ADCDataFromISR = xQueueCreate(i32AmountOfConvResInFrame,  sizeof(adc_digi_output_data_t));
  
}



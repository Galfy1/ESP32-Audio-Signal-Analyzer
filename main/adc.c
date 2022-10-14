#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_adc/adc_continuous.h"

static const char *ADC_TAG = "ADC.C";


#define CONV_FRAME_SIZE     256  // In bytes
#define ADC_CONV_MODE       ADC_CONV_BOTH_UNIT

#define RES_BUFFER_SIZE 4096        // VÆRDIEN SKAL NOK PASSER MED DEN FFT STØRELSE VI VÆLGER. DEN SKAL NOK VÆRE DEFINED ET STED. 
                                        // LIGE NU BRUGER JEG BARE 4096 TIL TEST
uint16_t au16_adc1_buf[RES_BUFFER_SIZE] = {0};  
uint16_t au16_adc2_buf[RES_BUFFER_SIZE] = {0};  



static TaskHandle_t adc_task_handle;

// static int32_t i32AmountOfConvResInFrame = CONV_FRAME_SIZE / sizeof(adc_digi_output_data_t);
// static QueueHandle_t xQueue_ADCDataFromISR;


// void adc_task()
// {
//     TickType_t xDelay500 = 1 / portTICK_PERIOD_MS;

//     for(;;)
//     {

//         adc_digi_output_data_t p_temp_res;    

//         if(xQueueReceive(xQueue_ADCDataFromISR, (void *)&p_temp_res, (TickType_t)portMAX_DELAY ) == pdPASS)
//         {
//             ESP_LOGI(ADC_TAG, "Unit: %d,_Channel: %d, Value: %x", p_temp_res.type2.unit, p_temp_res.type2.channel, p_temp_res.type2.data);
//         }



//         // check_valid_data()

//         //ESP_LOGI(ADC_TAG, "WWWWEEEE :D");
//         vTaskDelay(xDelay500);
//     }
// }

// static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
// {
//     BaseType_t mustYield = pdFALSE; // From espressif example

//     BaseType_t xHigherPriorityTaskWoken; // From xQueueSendFromISR(). (I dont use it for anything atm)
//     xHigherPriorityTaskWoken = pdFALSE;

//     //adc_digi_output_data_t testy[edata->size / sizeof(adc_digi_output_data_t)];
//    //memcpy(testy, edata->conv_frame_buffer, edata->size);

//     uint8_t u8AmountOfRes = edata->size / sizeof(adc_digi_output_data_t);

//     for(int32_t i = 0 ; i < u8AmountOfRes ; i++)
//     {
//         xQueueSendFromISR(xQueue_ADCDataFromISR, (adc_digi_output_data_t *)(edata->conv_frame_buffer), &xHigherPriorityTaskWoken);
//     }


//    // ESP_LOGI(ADC_TAG, "DATA", );

//     //Notify that ADC continuous driver has done enough number of conversions
//     //vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
//     return (mustYield == pdTRUE);
// }

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;

    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}


static bool check_valid_data(const adc_digi_output_data_t *data)
{
    const unsigned int unit = data->type2.unit;
    if (unit > 2) return false;
    if (data->type2.channel >= SOC_ADC_CHANNEL_NUM(unit)) return false;

    return true;
}

void adc_task()
{
    adc_continuous_handle_t adc_handle = NULL;
    esp_err_t ret;
    uint32_t ret_num = 0;       
    uint8_t result[CONV_FRAME_SIZE] = {0};
    memset(result, 0xcc, CONV_FRAME_SIZE);  // ikke sikker på hvorfor vi sætter dem til 0xcc (0b11001100)

    adc_task_handle = xTaskGetCurrentTaskHandle();

    // Create ADC handle:
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = CONV_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 80 * 1000,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    // Config ADC handle:
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = 2;

    adc_pattern[0].atten = ADC_ATTEN_DB_2_5;
    adc_pattern[0].channel = ADC_CHANNEL_0; //GPIO 1
    adc_pattern[0].unit = ADC_UNIT_1;    //ADC 1
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    adc_pattern[1].atten = ADC_ATTEN_DB_2_5;
    adc_pattern[1].channel = ADC_CHANNEL_9; // GPIO 20
    adc_pattern[1].unit = ADC_UNIT_2;    // ADC 2
    adc_pattern[1].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));


    //Set callback function:
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));


    // for test
    int32_t testCounterADC1 = 0;
    int32_t testCounterADC2 = 0;
  

    while(1)
    {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while(1)
        {
            ret = adc_continuous_read(adc_handle, result, CONV_FRAME_SIZE, &ret_num, 0);
            if (ret == ESP_OK) 
            {
                for(int32_t i = 0 ; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
                {
                    adc_digi_output_data_t *p_conv_res = (void*)&result[i]; // make a pointer for each conversion result of type adc_digi_output_data_t so its associated variables can be accessed
                    if (check_valid_data(p_conv_res)) {
                        ESP_LOGI(ADC_TAG, "Unit: %d,_Channel: %d, Value: %x", p_conv_res->type2.unit+1, p_conv_res->type2.channel, p_conv_res->type2.data);
                        // if(p_conv_res->type2.unit == ADC_UNIT_1)
                        // {
                        //     testCounterADC1++;
                        //     if(testCounterADC1 >= 80)
                        //     {
                        //         ESP_LOGI(ADC_TAG, "ADC 1 80k");
                        //         testCounterADC1 = 0;
                        //     }

                        // }
                        // else if(p_conv_res->type2.unit == ADC_UNIT_2)
                        // {
                        //     testCounterADC2++;
                        //     if(testCounterADC2 >= 80)
                        //     {
                        //         ESP_LOGI(ADC_TAG, "ADC 2 80k");
                        //         testCounterADC2 = 0;
                        //     }
                        // }
                    } else {
                        ESP_LOGI(ADC_TAG, "Invalid data [%d_%d_%x]", p_conv_res->type2.unit+1, p_conv_res->type2.channel, p_conv_res->type2.data);
                    }
                }

                //FJERN DET DELAY HER EFTER TEST:
                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
                 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                vTaskDelay(1);
            }
            else if (ret == ESP_ERR_TIMEOUT)
             {
                //We try to read `CONV_FRAME_SIZE` until API returns timeout, which means there's no available data
                break;
             }

        }

    }

}



// // param [out]: pu16_adc_buf_left  pointer to result buffer created by adc.c
// // param [out]: i32_res_buffer_len  length of result buffer. (in amount of elements of type uint16_t)
// void adc_init(uint16_t * pu16_adc_buf_left, uint16_t * pu16_adc_buf_right, int32_t i32_res_buffer_len, QueueHandle_t xQueue_adc_full_left, QueueHandle_t xQueue_adc_full_right)     
// {

//     // JEG KAN NOK IKKE GØRE SÅDAN HER MED POINTERS: DET SKAL NOK VÆRE POINTER POINTER!
//     pu16_adc_buf_left = au16_adc1_buf;
//     pu16_adc_buf_right = au16_adc2_buf;
//     i32_res_buffer_len = RES_BUFFER_SIZE;

//     //Tasks:
//     //(tror den skal være høj prio, siden den skal kunne wakes med førstepritet fra ADC ISR'ren)
//     xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 2, NULL, 1); // Use core 1 (application core)


//     // ER IKKE PÅ MAN KAN GØRE DET DER MED QUEUE HANDLES SOM OUTPUT
// }


void adc_init()
{
    xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 2, NULL, 1); // Use core 1 (application core)
}



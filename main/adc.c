#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"

#include "adc.h"
#include "common_defines.h"

static const char *ADC_TAG = "ADC.C";

// ADC 1 --> Left mic channel
// ADC 2 --> Right mic channel

#define ADC_1_CHANNEL ADC_CHANNEL_6 // GPIO 7
#define ADC_2_CHANNEL ADC_CHANNEL_6 // GPIO 17

#define ADC_CONV_MODE   ADC_CONV_BOTH_UNIT
#define CONV_FRAME_SIZE 256 // In bytes
// ADC_SAMPLING_FREQ and ADC_ATTEN_DB is located in common_defines.h

static float *gs_pf32_adc1_shared;
static float *gs_pf32_adc2_shared;

static TaskHandle_t gs_adc_task_handle;

// static float gs_af32_adc1_temp_buf[DATA_BLOCK_SIZE] = {0};
// static float gs_af32_adc2_temp_buf[DATA_BLOCK_SIZE] = {0};

static float *gs_pf32_adc1_temp_buf; // (allocated in adc_init() and located in PSRAM)
static float *gs_pf32_adc2_temp_buf; // (allocated in adc_init() and located in PSRAM)

static SemaphoreHandle_t gs_adc1_shared_mem_sem;
static SemaphoreHandle_t gs_adc2_shared_mem_sem;

static TaskHandle_t gs_adc_task_handle;
static TaskHandle_t gs_dsp_left_task_handle;
static TaskHandle_t gs_dsp_right_task_handle;

// ISR is placed in IRAM for low latency
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;

    // Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(gs_adc_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static bool check_valid_data(const adc_digi_output_data_t *data)
{
    const unsigned int unit = data->type2.unit;
    if (unit > 2)
        return false;
    if (data->type2.channel >= SOC_ADC_CHANNEL_NUM(unit))
        return false;

    return true;
}

void adc_task()
{
    adc_continuous_handle_t adc_handle = NULL;

    adc_cali_handle_t adc1_cali_handle = NULL;
    adc_cali_handle_t adc2_cali_handle = NULL;
    int               adc1_temp_voltage; // int to match the adc_cali_raw_to_voltage() input type
    int               adc2_temp_voltage;

    esp_err_t ret;
    uint32_t  ret_num                     = 0;
    uint8_t   conv_frame[CONV_FRAME_SIZE] = {0};

    memset(conv_frame, 0xcc, sizeof(conv_frame)); // ikke sikker på hvorfor vi sætter dem til 0xcc (0b11001100)

    int32_t i32_adc1_idx_counter = 0;
    int32_t i32_adc2_idx_counter = 0;

    gs_adc_task_handle = xTaskGetCurrentTaskHandle();

    // Create ADC handle:
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size    = CONV_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = ADC_SAMPLING_FREQ,
        .conv_mode      = ADC_CONV_MODE,
        .format         = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    // Config ADC handle:
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num                                         = 2;

    adc_pattern[0].atten     = ADC_ATTEN_DB;
    adc_pattern[0].channel   = ADC_1_CHANNEL;
    adc_pattern[0].unit      = ADC_UNIT_1; // ADC 1
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    adc_pattern[1].atten     = ADC_ATTEN_DB;
    adc_pattern[1].channel   = ADC_2_CHANNEL;
    adc_pattern[1].unit      = ADC_UNIT_2; // ADC 2
    adc_pattern[1].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    // Set callback function:
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    // ADC calibration:
    adc_cali_curve_fitting_config_t cali_config_1 = {
        .unit_id  = ADC_UNIT_1,
        .atten    = ADC_ATTEN_DB,
        .bitwidth = SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    adc_cali_curve_fitting_config_t cali_config_2 = {
        .unit_id  = ADC_UNIT_2,
        .atten    = ADC_ATTEN_DB,
        .bitwidth = SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config_1, &adc1_cali_handle));
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config_2, &adc2_cali_handle));
    // SKAL BRUGE adc_cali_raw_to_voltage()

    // // for samping rate validation:
    // int32_t testCounterADC1 = 0;
    // int32_t testCounterADC2 = 0;

    while (1)
    {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // pdTRUE means clear count on exit

        while (1)
        {
            ret = adc_continuous_read(adc_handle, conv_frame, CONV_FRAME_SIZE, &ret_num, 0);
            if (ret == ESP_OK)
            {
                for (int32_t i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
                {
                    adc_digi_output_data_t *p_conv_res = (void *)&conv_frame[i]; // make a pointer for each conversion conv_frame of type adc_digi_output_data_t so its associated variables can be accessed
                    if (check_valid_data(p_conv_res))
                    {
                        // ESP_LOGI(ADC_TAG, "Unit: %d,_Channel: %d, Value: %x", p_conv_res->type2.unit+1, p_conv_res->type2.channel, p_conv_res->type2.data);

                        if (p_conv_res->type2.unit == ADC_UNIT_1)
                        {
                            adc_cali_raw_to_voltage(adc1_cali_handle, (int)(p_conv_res->type2.data), &adc1_temp_voltage);

                            gs_pf32_adc1_temp_buf[i32_adc1_idx_counter] = (float)adc1_temp_voltage;
                            i32_adc1_idx_counter++;

                            if (i32_adc1_idx_counter > (DATA_BLOCK_SIZE - 1)) // SKAL LIGE TÆNKE OM DET VIRKER. NÅR DET ENE ER INDEX OG DET ANDET ER STØRELSE
                            {
                                i32_adc1_idx_counter = 0;
                                if (xSemaphoreTake(gs_adc1_shared_mem_sem, portMAX_DELAY) == pdTRUE) // Binary sempahore used to mutual exclusion
                                {
                                    memcpy(gs_pf32_adc1_shared, gs_pf32_adc1_temp_buf, DATA_BLOCK_SIZE * sizeof(float));
                                    xSemaphoreGive(gs_adc1_shared_mem_sem);
                                    xTaskNotifyGive(gs_dsp_left_task_handle); // Use direct task notification to syncronize use of the shared memory
                                }
                            }
                        }

                        else if (p_conv_res->type2.unit == ADC_UNIT_2)
                        {
                            adc_cali_raw_to_voltage(adc2_cali_handle, (int)(p_conv_res->type2.data), &adc2_temp_voltage);

                            gs_pf32_adc2_temp_buf[i32_adc1_idx_counter] = (float)adc2_temp_voltage;
                            i32_adc2_idx_counter++;

                            if (i32_adc2_idx_counter > (DATA_BLOCK_SIZE - 1))
                            {
                                i32_adc2_idx_counter = 0;
                                if (xSemaphoreTake(gs_adc2_shared_mem_sem, portMAX_DELAY) == pdTRUE)
                                {
                                    memcpy(gs_pf32_adc2_shared, gs_pf32_adc2_temp_buf, DATA_BLOCK_SIZE * sizeof(float));
                                    xSemaphoreGive(gs_adc2_shared_mem_sem);
                                    xTaskNotifyGive(gs_dsp_right_task_handle);
                                }
                            }
                        }

                        // //Sampling rate validation:
                        // if(p_conv_res->type2.unit == ADC_UNIT_1)
                        // {
                        //     testCounterADC1++;
                        //     if(testCounterADC1 >= 40000)
                        //     {
                        //         //ESP_LOGI(ADC_TAG, "ADC 1 80k");
                        //         ESP_LOGI(ADC_TAG, "Unit: %d,_Channel: %d, Value: %x", p_conv_res->type2.unit+1, p_conv_res->type2.channel, p_conv_res->type2.data);
                        //         testCounterADC1 = 0;
                        //     }

                        // }
                        // else if(p_conv_res->type2.unit == ADC_UNIT_2)
                        // {
                        //     testCounterADC2++;
                        //     if(testCounterADC2 >= 40000)
                        //     {
                        //         //ESP_LOGI(ADC_TAG, "ADC 2 80k");
                        //         ESP_LOGI(ADC_TAG, "Unit: %d,_Channel: %d, Value: %x", p_conv_res->type2.unit+1, p_conv_res->type2.channel, p_conv_res->type2.data);
                        //         testCounterADC2 = 0;
                        //     }
                        // }
                    }
                    else
                    {
                        ESP_LOGW(ADC_TAG, "Invalid data [%d_%d_%x]", p_conv_res->type2.unit + 1, p_conv_res->type2.channel, p_conv_res->type2.data);
                    }
                }

                // FJERN DET DELAY HER EFTER TEST:
                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
                 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                // vTaskDelay(1);
            }
            else if (ret == ESP_ERR_TIMEOUT)
            {
                // We try to read `CONV_FRAME_SIZE` until API returns timeout, which means there's no available data
                break;
            }
        }
    }
}

// // param [out]: pu16_adc_buf_left  pointer to conv_frame buffer created by adc.c
// // param [out]: i32_res_buffer_len  length of conv_frame buffer. (in amount of elements of type uint16_t)
// void adc_init(uint16_t * pu16_adc_buf_left, uint16_t * pu16_adc_buf_right, int32_t i32_res_buffer_len, QueueHandle_t xQueue_adc_full_left, QueueHandle_t xQueue_adc_full_right)
// {

//     // JEG KAN NOK IKKE GØRE SÅDAN HER MED POINTERS: DET SKAL NOK VÆRE POINTER POINTER!
//     pu16_adc_buf_left = au16_adc1_shared;
//     pu16_adc_buf_right = au16_adc2_shared;
//     i32_res_buffer_len = DATA_BLOCK_SIZE;

//     //Tasks:
//     //(tror den skal være høj prio, siden den skal kunne wakes med førstepritet fra ADC ISR'ren)
//     xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 2, NULL, 1); // Use core 1 (application core)

//     // ER IKKE PÅ MAN KAN GØRE DET DER MED QUEUE HANDLES SOM OUTPUT
// }

void adc_set_dsp_task_handles(TaskHandle_t dsp_left_task_handle, TaskHandle_t dsp_right_task_handle)
{
    gs_dsp_left_task_handle  = dsp_left_task_handle;
    gs_dsp_right_task_handle = dsp_right_task_handle;
}

void adc_init(float *pu16_adc1_shared, float *pu16_adc2_shared, SemaphoreHandle_t adc1_shared_mem_sem, SemaphoreHandle_t adc2_shared_mem_sem)
{
    gs_pf32_adc1_shared = pu16_adc1_shared;
    gs_pf32_adc2_shared = pu16_adc2_shared;

    gs_adc1_shared_mem_sem = adc1_shared_mem_sem;
    gs_adc2_shared_mem_sem = adc2_shared_mem_sem;

    // Allocate arrays in PSRAM:
    gs_pf32_adc1_temp_buf = heap_caps_malloc(DATA_BLOCK_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);
    gs_pf32_adc2_temp_buf = heap_caps_malloc(DATA_BLOCK_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);
    if ((gs_pf32_adc1_temp_buf == NULL) || (gs_pf32_adc2_temp_buf == NULL))
    {
        ESP_LOGE(ADC_TAG, "ERROR: while allocating ADC temp buf array in PSRAM");
    }
    memset(gs_pf32_adc1_temp_buf, 0, DATA_BLOCK_SIZE * sizeof(float));
    memset(gs_pf32_adc2_temp_buf, 0, DATA_BLOCK_SIZE * sizeof(float));
}

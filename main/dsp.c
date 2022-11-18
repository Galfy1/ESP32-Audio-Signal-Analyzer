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
#include "ecep_defines.h"
#include "ecep_uart.h" // For convenience functions used to access the ecep event queue

static const char *DSP_TAG = "DSP.C";

#define RADIX_USED radix4 // eFFT_radix_type_t      (Ops: the chosen radix will have to support the DATA_BLOCK_SIZE)

// Default user settings:
#define DFLT_FFT_OUTPUT_FORMAT ampltitude_dB_SPL // eFFT_output_form_t     Default output format
#define DFLT_WIND_USED hann                      // eFFT_window_type_t     Default window type
#define DFLT_FFT_AVG_MAX_COUNT 5                 // Default amount of FFT result blocks to average before passing it on.
#define DFLT_HW_GAIN_STAGE up_to_90dB_SPL        // eFFT_hw_gain_stage_t   Default Hardware Gain stage
#define DFLT_WIND_CORR_FACTOR amplitude          // eFFT_window_correction_factor_t Default window correction factor type

// Declare settings variables with default values:
eFFT_output_form_t gs_fft_out_format = DFLT_FFT_OUTPUT_FORMAT;
eFFT_window_type_t gs_window_used = DFLT_WIND_USED;
int32_t gs_i32_fft_avg_max_cnt = DFLT_FFT_AVG_MAX_COUNT; // (remember to re-calc gs_f32_total_fft_dur_ms if this variable is changed)
eFFT_hw_gain_stage_t gs_hw_gain_stage = DFLT_HW_GAIN_STAGE;
eFFT_wind_corr_factor_t gs_wind_corr_factor = DFLT_WIND_CORR_FACTOR;

float gs_f32_total_fft_dur_ms; // The total duration of the averaged FFT result in ms

static float *gs_pf32_adc1_shared;
static float *gs_pf32_adc2_shared;
static SemaphoreHandle_t gs_adc1_shared_mem_sem;
static SemaphoreHandle_t gs_adc2_shared_mem_sem;

static float *gs_pf32_L_fft_res_shr;
static float *gs_pf32_R_fft_res_shr;
static SemaphoreHandle_t gs_fft_L_res_shr_mem_sem;
static SemaphoreHandle_t gs_fft_R_res_shr_mem_sem;

static QueueHandle_t gs_xQueue_dsp_ecep_cmds;

// VED IKKE OM DE HER ARRAYS KAN NØJES MED AT VÆRE I DSP TASKSNE I STEDET
// Arrayes are here 16bit alligned by the compiler in memory to eliminate any unsesessay memory accessesing overhead
// See: http://www.songho.ca/misc/alignment/dataalign.html for more info
__attribute__((aligned(16))) static float gs_af32_adc1_temp_buf[DATA_BLOCK_SIZE] = {0};
__attribute__((aligned(16))) static float gs_af32_adc2_temp_buf[DATA_BLOCK_SIZE] = {0};

// Stores the latest center frequency indexes and values:
static int32_t gs_i32_L_cent_freq_idx;
static float gs_f32_L_cent_freq_val;
static int32_t gs_i32_R_cent_freq_idx;
static float gs_f32_R_cent_freq_val;

// For Spectral Estimation: Welch's method is used with 0% overlap

// IKKE TESTET:
static void calc_fft_duration_ms()
{
    gs_f32_total_fft_dur_ms = (1.0 / ADC_SAMPLING_FREQ) * 1000.0 * DATA_BLOCK_SIZE * gs_i32_fft_avg_max_cnt;
}

/**
 * @brief
 *
 * @param[in] array
 * @param[in] array_size
 * @param[out] p_cent_freq_idx
 * @param[out] p_cent_freq_val
 */
static void find_cent_freq(float *array, int32_t array_size, int32_t *p_cent_freq_idx, float *p_cent_freq_val)
{
    float temp_val;
    float biggest_val = 0;
    int32_t biggest_val_index = 0xFFFFFF;

    for (int32_t i = 0; i < array_size; i++)
    {
        temp_val = array[i];
        if (temp_val > biggest_val)
        {
            biggest_val = temp_val;
            biggest_val_index = i;
        }
    }
    *p_cent_freq_idx = biggest_val_index;
    *p_cent_freq_val = biggest_val;
    // ESP_LOGI(DSP_TAG, "WEEE CENTER IDX: %d" , (int)biggest_val_index);
}

// FOR FFT DEBUGGING TEST:
// __attribute__((aligned(16)))
// float x1[DATA_BLOCK_SIZE];
// static void debugging_fft_test()
// {
//     dsps_tone_gen_f32(x1, DATA_BLOCK_SIZE, 875.0, 0.7, 0);  // De hentyder i doc til at 1 er fny... MEN det argere mere somom at 1 er fs ogs 0.5 er fny...
//     // for(int i = 0 ; i < 300 ; i++)
//     // {
//     //     ESP_LOGI(DSP_TAG, "X1 Index %d: %f", i, x1[i]);
//     // }
//     // KØR INIT OG FFT HER
//     //fft_wrapper_init_fast(hann);
//     fft_wrapper_run_fft_tech3(x1, radix2, ampltitude_mV, gs_hw_gain_stage, gs_wind_corr_factor);
//     for(int i = 0 ; i < DATA_BLOCK_SIZE/2 ; i++)
//     {
//         ESP_LOGI(DSP_TAG, "X1 Index %d: %f", i, x1[i]);
//     }
//     // First half of x1 is aplitude values. Second half is junk.
//     dsps_view_spectrum(x1, DATA_BLOCK_SIZE/2, -60, 40);  // Amplitude output of fft is only of size DATA_BLOCK_SIZE/2 (it takes takes one complex number (two indexes) to create one aplitude value (one index))
// }

void dsp_ecep_handler_task()
{
    eFFT_output_form_t output_form_temp;
    eFFT_window_type_t wind_type_temp;
    eFFT_wind_corr_factor_t wind_corr_fact_temp;
    eFFT_hw_gain_stage_t hw_gain_stage_temp;

    sIntern_command_setup_t dsp_ecep_command;
    uint8_t u8_ecep_cmd_id;

    while (1)
    {

        if (xQueueReceive(gs_xQueue_dsp_ecep_cmds, (void *)&dsp_ecep_command, portMAX_DELAY))
        {
            u8_ecep_cmd_id = dsp_ecep_command.u8_id;

            switch (u8_ecep_cmd_id)
            {
            case ECEP_CMD_FETCH_FFT:
                // ASD
                break;

            case ECEP_CMD_SET_FFT_OUTP_FORM:
                output_form_temp = dsp_ecep_command.param.fft_output_format;

                if ((output_form_temp != ampltitude_mV) && (output_form_temp != ampltitude_dB_SPL) && (output_form_temp != power_mV))
                {
                    send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_PARAM, ECEP_CMD_SET_FFT_OUTP_FORM);
                    ESP_LOGE(DSP_TAG, "ERROR: in dsp_ecep_handler_task(). Invalid param for ECEP_CMD_SET_FFT_OUTP_FORM");
                    break; // Ignore the command
                }

                // Execute command:
                gs_fft_out_format = output_form_temp;
                send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_SET_FFT_OUTP_FORM);
                break;

            case ECEP_CMD_SET_FFT_WIND_TYPE:
                wind_type_temp = dsp_ecep_command.param.fft_window;

                if ((wind_type_temp != hann) && (wind_type_temp != blackman) && (wind_type_temp != blackman_harris) && (wind_type_temp != blackman_nuttall) && (wind_type_temp != nuttall) && (wind_type_temp != flat_top))
                {
                    send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_PARAM, ECEP_CMD_SET_FFT_WIND_TYPE);
                    ESP_LOGE(DSP_TAG, "ERROR: in dsp_ecep_handler_task(). Invalid param for ECEP_CMD_SET_FFT_WIND_TYPE");
                    break; // Ignore the command
                }

                // Execute command:
                gs_window_used = dsp_ecep_command.param.fft_window;
                send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_SET_FFT_WIND_TYPE);
                break;

            case ECEP_CMD_SET_FFT_RES_AVG_CT:
                gs_i32_fft_avg_max_cnt = dsp_ecep_command.param.u32_fft_res_avg_count;
                calc_fft_duration_ms(); // recalc total fft time
                send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_SET_FFT_RES_AVG_CT);
                break;

            case ECEP_CMD_SET_WIND_CORR_FACTOR:
                wind_corr_fact_temp = dsp_ecep_command.param.fft_win_corr_factor;

                if ((wind_corr_fact_temp != none) && (wind_corr_fact_temp != amplitude) && (wind_corr_fact_temp != energy))
                {
                    send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_PARAM, ECEP_CMD_SET_WIND_CORR_FACTOR);
                    ESP_LOGE(DSP_TAG, "ERROR: in dsp_ecep_handler_task(). Invalid param for ECEP_CMD_SET_WIND_CORR_FACTOR");
                    break; // Ignore the command
                }

                // Execute command:
                gs_wind_corr_factor = wind_corr_fact_temp;
                send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_SET_WIND_CORR_FACTOR);
                break;

            case ECEP_CMD_FETCH_FFT_STGS:
                // ASD
                break;

            case ECEP_CMD_FETCH_FFT_TOTAL_DUR:
                // ASD
                break;

            case ECEP_CMD_FETCH_ADC_RAW_DATA:
                // ASD
                break;

            case ECEP_CMD_FETCH_CEN_FREQ:
                // ASD
                break;

            case ECEP_CMD_FETCH_CEN_FREQ_AMP:
                // ASD
                break;

            case ECEP_CMD_SET_MIC_RANGE:
                hw_gain_stage_temp = dsp_ecep_command.param.range;

                if ((hw_gain_stage_temp != up_to_90dB_SPL) && (hw_gain_stage_temp != up_to_120dB_SPL))
                {
                    send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_PARAM, ECEP_CMD_SET_MIC_RANGE);
                    ESP_LOGE(DSP_TAG, "ERROR: in dsp_ecep_handler_task(). Invalid param for ECEP_CMD_SET_MIC_RANGE");
                    break; // Ignore the command
                }

                // Execute command:
                gs_hw_gain_stage = hw_gain_stage_temp;
                send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_SET_MIC_RANGE);
                break;

            case ECEP_CMD_FETCH_MIC_RANGE:
                // ASD
                break;

            default:
                ESP_LOGE(DSP_TAG, "ERROR: in dsp_ecep_handler_task(). A command not relevant to dsp was located in gs_xQueue_dsp_ecep_cmds");
                break;
            }
        }
    }
}

void dsp_left_ch_task()
{
    // A Place to compute the averaged FFT result (located is PSRAM):
    float *pf32_L_fft_res_avg_array = heap_caps_malloc(DATA_BLOCK_SIZE / 2 * sizeof(float), MALLOC_CAP_SPIRAM);
    if (pf32_L_fft_res_avg_array == NULL)
    {
        ESP_LOGE(DSP_TAG, "ERROR: while allocating FFT res averaging array in PSRAM");
    }
    memset(pf32_L_fft_res_avg_array, 0, DATA_BLOCK_SIZE / 2 * sizeof(float));

    static int32_t i32_avg_counter = 0;

    // ---FFT TEST BEGIN---
    // debugging_fft_test();
    // ---FFT TEST END---

    uint32_t ret;

    uint32_t testy_counter = 0;
    uint32_t testy_sum = 0;

    while (1)
    {
        ret = ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Block this task untill triggeret by adc task.

        if (ret > 1)
        {
            ESP_LOGE(DSP_TAG, "Left dsp task is too slow! Number of missed ADC blocks: %d", (int)(ret - 1));
        }

        if (xSemaphoreTake(gs_adc1_shared_mem_sem, portMAX_DELAY) == pdTRUE)
        {
            memcpy(gs_af32_adc1_temp_buf, gs_pf32_adc1_shared, sizeof(gs_af32_adc1_temp_buf)); // Copy from shared memory to local copy
            // memset(gs_pf32_adc1_shared, 0, sizeof(gs_af32_adc1_temp_buf));  // Reset the shared memory, just to be sure (gs_af32_adc1_temp_buf is same size as shared space array)
            xSemaphoreGive(gs_adc1_shared_mem_sem);

            // ESP_LOGI(DSP_TAG, "ADC1 Read Voltage: %f", gs_af32_adc1_temp_buf[0]);

            // for(int32_t i = 0 ; i <= DATA_BLOCK_SIZE ; i += 1)
            // {
            //     ESP_LOGI(DSP_TAG, "WUHUUU %d: %f" , (int)i, gs_af32_adc1_temp_buf[i]);
            // }

            // Run FFT:
            if (gs_fft_out_format == ampltitude_dB_SPL)
            {
                fft_wrapper_run_fft_tech3(gs_af32_adc1_temp_buf, RADIX_USED, ampltitude_mV, 0, gs_wind_corr_factor); /*  We dont want to average the dB SPL results since they are logarithmic,
                                                                                                    so we wait to convert to dB SPL untill after averageing) */
            }
            else
            {
                fft_wrapper_run_fft_tech3(gs_af32_adc1_temp_buf, RADIX_USED, gs_fft_out_format, gs_hw_gain_stage, gs_wind_corr_factor);
            }
            // Remove DC-offset (DC-offset was added by our hardware. aka not a part of the original signal)
            gs_af32_adc1_temp_buf[0] = 0;
            gs_af32_adc1_temp_buf[1] = 0;

            // Test stuff:

            // testy_counter++;
            // if(testy_counter == 40)
            // {
            //     testy_counter = 0;
            //     //dsps_view_spectrum(gs_af32_adc1_temp_buf, DATA_BLOCK_SIZE/2, 0, 55); // for debugging
            //     ESP_LOGI(DSP_TAG, "Center freq idx: %i" , (int) gs_i32_L_cent_freq_idx);
            //     ESP_LOGI(DSP_TAG, "Center freq val: %f" , gs_f32_L_cent_freq_val);

            //     // for(int i = DATA_BLOCK_SIZE/12 ; i < DATA_BLOCK_SIZE/6 ; i++)
            //     // {
            //     //     testy_sum += gs_af32_adc1_temp_buf[i];
            //     //     //ESP_LOGI(DSP_TAG, "FFT Index %d: %f", i, gs_af32_adc1_temp_buf[i]);
            //     // }
            //     //  ESP_LOGI(DSP_TAG, "TESTY SUM: %d", (int)testy_sum);
            //     //  testy_sum = 0;

            // }

            // Test stuff end

            // Add result into averaging array (remember that only half of gs_af32_adc1_temp_buf is the result. rest is junk):
            for (int32_t i = 0; i < DATA_BLOCK_SIZE / 2; i++)
            {
                pf32_L_fft_res_avg_array[i] += gs_af32_adc1_temp_buf[i];
            }
            i32_avg_counter++;

            // If fft averaging is complete:
            if (i32_avg_counter >= gs_i32_fft_avg_max_cnt)
            {
                // Divide array by gs_i32_fft_avg_max_cnt to average:
                for (int32_t i = 0; i < DATA_BLOCK_SIZE / 2; i++)
                {
                    pf32_L_fft_res_avg_array[i] /= gs_i32_fft_avg_max_cnt;
                }

                // Convert averaged result to dB SPL if necessaery (remember that we originally requested the fft res in mV. This was so we could average them easily):
                if (gs_fft_out_format == ampltitude_dB_SPL)
                {
                    conv_ampl_mv_to_ampl_db_spl(pf32_L_fft_res_avg_array, DATA_BLOCK_SIZE / 2, gs_hw_gain_stage);
                }

                // Find center frequency:
                find_cent_freq(pf32_L_fft_res_avg_array, DATA_BLOCK_SIZE / 2, &gs_i32_L_cent_freq_idx, &gs_f32_L_cent_freq_val); // SKAL HAVE ANDRE INPUTS

                // Copy FFT averaged result from local copy into shared memory:
                if (xSemaphoreTake(gs_fft_L_res_shr_mem_sem, portMAX_DELAY) == pdTRUE)
                {
                    memcpy(gs_pf32_L_fft_res_shr, pf32_L_fft_res_avg_array, DATA_BLOCK_SIZE / 2 * sizeof(float));
                    xSemaphoreGive(gs_fft_L_res_shr_mem_sem);
                }

                // Reset counter and averaging array:
                i32_avg_counter = 0;
                memset(pf32_L_fft_res_avg_array, 0, DATA_BLOCK_SIZE / 2 * sizeof(float));
            }
        }

        // Print test:
        //  for(int32_t i = 0 ; i <= sizeof(gs_af32_adc1_temp_buf) ; i += 100)
        //  {
        //     ESP_LOGI(DSP_TAG, "WUHUUU: %d" , gs_af32_adc1_temp_buf[i]);
        //  }
    }
}

void dsp_right_ch_task()
{

    while (1)
    {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xSemaphoreTake(gs_adc2_shared_mem_sem, portMAX_DELAY) == pdTRUE)
        {
            memcpy(gs_af32_adc2_temp_buf, gs_pf32_adc2_shared, sizeof(gs_af32_adc2_temp_buf)); // Copy from shared memory to local copy
            memset(gs_pf32_adc2_shared, 0, sizeof(gs_af32_adc2_temp_buf));                     // Reset the shared memory
            xSemaphoreGive(gs_adc2_shared_mem_sem);

            fft_wrapper_run_fft_tech3(gs_af32_adc2_temp_buf, RADIX_USED, gs_fft_out_format, gs_hw_gain_stage, gs_wind_corr_factor); // Run FFT
            if (xSemaphoreTake(gs_fft_R_res_shr_mem_sem, portMAX_DELAY) == pdTRUE)
            {
                memcpy(gs_pf32_R_fft_res_shr, gs_af32_adc2_temp_buf, sizeof(gs_af32_adc2_temp_buf) / 2); // Copy FFT result from local copy to shared memory
                xSemaphoreGive(gs_fft_R_res_shr_mem_sem);
            }
        }
    }
}

void dsp_init(float *pf32_adc1_shared,
              float *pf32_adc2_shared,
              SemaphoreHandle_t adc1_shared_mem_sem,
              SemaphoreHandle_t adc2_shared_mem_sem,
              float *pf32_L_fft_res_shr,
              float *pf32_R_fft_res_shr,
              SemaphoreHandle_t fft_L_res_shr_mem_sem,
              SemaphoreHandle_t fft_R_res_shr_mem_sem,
              QueueHandle_t xQueue_dsp_ecep_cmds)
{
    gs_pf32_adc1_shared = pf32_adc1_shared;
    gs_pf32_adc2_shared = pf32_adc2_shared;
    gs_adc1_shared_mem_sem = adc1_shared_mem_sem;
    gs_adc2_shared_mem_sem = adc2_shared_mem_sem;

    gs_pf32_L_fft_res_shr = pf32_L_fft_res_shr;
    gs_pf32_R_fft_res_shr = pf32_R_fft_res_shr;
    gs_fft_L_res_shr_mem_sem = fft_L_res_shr_mem_sem;
    gs_fft_R_res_shr_mem_sem = fft_R_res_shr_mem_sem;

    gs_xQueue_dsp_ecep_cmds = xQueue_dsp_ecep_cmds;

    calc_fft_duration_ms();

    fft_wrapper_init_tech3(gs_window_used);
    // SÆT HW GAIN STAGE GPIO TIL HVAD END DEFAULT_HW_G_STAGE ER
}
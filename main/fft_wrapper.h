
#ifndef FFTWRAPPER_H_INCLUDED
#define FFTWRAPPER_H_INCLUDED

#include "ecep_defines.h"

typedef enum
{
    radix2,
    radix4,
} eFFT_radix_type_t;

/**
 * @brief Initialize the FFT for acording to "technique 3" (technique 3 refers to using N/2 complex size fft with 1 real input sequency of size N)
 *
 * @param eFFT_window_type Desired window type
 * @return esp_err_t
 */
esp_err_t fft_wrapper_init_tech3(ecep_window_type_t eFFT_window_type);

/**
 * @brief  Run FFT acording to "technuiqe 3" with pre and post processing. (technique 3 refers to using N/2 complex size fft with 1 real input sequency of size N)
 *         Preprosessing: format real data for fft, multiply with window function
 *         Postprocessing: Bit reverse, Run splitting operation, extract desired output format
 *         All computation is done in-place.
 *
 * @param[in,out] pf32_data: Pointer to Real input sequence of size DATA_BLOCK_SIZE. Real output of size DATA_BLOCK_SIZE/2 is places at same location.
 *                          (size here refers to the amount of float elements)
 * @param[in] eFFT_radix_type: Desired FFT radix
 * @param[in] eFFT_output_form: Desired output format
 * @param eFFT_hw_gain_stage The used hardware gain stage (this param is only used if amplitude_dB_SPL is chosen as output form)
 * @param[in] eFFT_window_corr_factor The window correction factor type
 *
 * @return esp_err_t:  returns ESP_OK if sucessfull, else returns error.
 */
esp_err_t fft_wrapper_run_fft_tech3(float *pf32_data, eFFT_radix_type_t eFFT_radix_type, ecep_output_form_t eFFT_output_form,
                                    ecep_hw_gain_stage_t eFFT_hw_gain_stage, ecep_wind_corr_factor_t eFFT_window_corr_factor);

/**
 * @brief Used to change the window type. The window type is used duing FFT preprocessing.
 *
 * @param eFFT_window_type Desired window type
 * @return esp_err_t
 */
esp_err_t fft_wrapper_set_wind(ecep_window_type_t eFFT_window_type);

esp_err_t conv_ampl_mv_to_ampl_db_spl(float *pf32_data, int32_t i32_bin_size, ecep_hw_gain_stage_t eFFT_hw_gain_stage);

#endif /*FFTWRAPPER_H_INCLUDED */
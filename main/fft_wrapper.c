#include <math.h>
#include <stdint.h>
#include <string.h>

#include "esp_dsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "common_defines.h"
#include "common_tools.h"
#include "ecep_defines.h"
#include "fft_wrapper.h"

#define P0 0.000020 // dB SPL Reference in Pascal

static const char *TAG = "fft_wrapper";

static float gs_max_adc_val_mv;

/* Procedure to get amplitude output in dB SPL:
    1: Find the ratio between ADC input mV vs. Pascal from microphone
       1: Use the expression for dB SPL to find the pascal value for a dB SPL value where the equavilent mV output is known
       2: Divide the known mV value with the newly found pascal value to get the ratio
    2: Divide the amplitude values with this ratio to convert from mV to Pascal
    3: Use the expression for dB SPL to find the dB SPL value for each Pascal value (using the standard p0 value)

    A more effecient way:
    Instead of continuously converting each amplitude value to pascal and later diving by p0, Let us instead convert p0 to mV once and use that to convert amplitude to dB SPL.
    (the ratio p/p0 is the same in mV or Pascal as long as both are the same unit, and assuming a proportional relation between mV and Pascal)
    This is done by multiplying p0 with the mV vs. Pascal ratio mentioned before.
    This will result in a a new p0 value for each hardware gain stages
*/
static float gs_f32_p0_mv_90dB_stage;  // P0 value converted to mV for the "up to 90dB SPL" gain stage
static float gs_f32_p0_mv_120dB_stage; // P0 value converted to mV for the "up to 120dB SPL" gain stage

// Window Correction Factors (used to correct for amplitude or energy distortion introduced by the window function):
typedef struct
{
    float f32_no_correction;
    float f32_hann_ampl;
    float f32_hann_energy;
    float f32_blackman_ampl;
    float f32_blackman_energy;
    float f32_blackman_harris_ampl;
    float f32_blackman_harris_energy;
    float f32_blackman_nuttall_ampl;
    float f32_blackman_nuttall_energy;
    float f32_nuttall_ampl;
    float f32_nuttall_energy;
    float f32_flat_top_ampl;
    float f32_flat_top_energy;
} s_correction_factors_t;

static s_correction_factors_t gs_correction_factors;

static bool gs_init_called = false;

// Window coefficients:
__attribute__((aligned(16))) static float gs_af32_wind[DATA_BLOCK_SIZE];

static ecep_window_type_t gs_curr_selected_wind;

// NESP SKRIV KORT OM I RAPPORTEN HVORFOR VI BRUGER WINDOWS
// DE nævner det her:
// https://www.sjsu.edu/people/burford.furman/docs/me120/FFT_tutorial_NI.pdf
// MÅSKE FIND AFSNITTET I DSP BOGEN MED DET, OG SE HVAD DE HAR AT SIGE
// TROR DET STÅR PÅ SIDE 112 I DEN NYE DSP BOG
// BARE SKRIV SUPER KORT OM DET

static float mean_of_float_array(float *p_f32_array, int32_t i32_array_size_in_elements)
{
    float f32_sum = 0;

    for (int32_t i = 0; i < i32_array_size_in_elements; i++)
    {
        f32_sum += p_f32_array[i];
    }
    return (f32_sum / (float)i32_array_size_in_elements);
}

static float rms_of_float_array(float *p_f32_array, int32_t i32_array_size_in_elements)
{
    float f32_square = 0.0, f32_mean = 0.0, f32_root = 0.0;

    // Sqaure each value and sum:
    for (int32_t i = 0; i < i32_array_size_in_elements; i++)
    {
        f32_square += pow(p_f32_array[i], 2);
    }

    // Mean the sum:
    f32_mean = f32_square / (float)i32_array_size_in_elements;

    // Root the mean:
    f32_root = sqrtf(f32_mean);

    return f32_root;
}

static esp_err_t calc_wind_correction_factors()
{
    float *pf32_temp_wind_array = malloc_to_psram(DATA_BLOCK_SIZE * sizeof(float)); // (Located in PSRAM)
                                                                                    // Array does not need to be 16bit aligned since speed is not of essence here

    // No correction:
    gs_correction_factors.f32_no_correction = 1.0;

    // Hanning:
    dsps_wind_hann_f32(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_hann_ampl   = 1.0 / mean_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_hann_energy = 1.0 / rms_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);

    // Blackman:
    dsps_wind_blackman_f32(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_blackman_ampl   = 1.0 / mean_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_blackman_energy = 1.0 / rms_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);

    // Blackman_harris:
    dsps_wind_blackman_harris_f32(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_blackman_harris_ampl   = 1.0 / mean_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_blackman_harris_energy = 1.0 / rms_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);

    // Blackman_nuttall:
    dsps_wind_blackman_nuttall_f32(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_blackman_nuttall_ampl   = 1.0 / mean_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_blackman_nuttall_energy = 1.0 / rms_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);

    // Nuttall:
    dsps_wind_nuttall_f32(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_nuttall_ampl   = 1.0 / mean_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_nuttall_energy = 1.0 / rms_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);

    // Flat_top:
    dsps_wind_flat_top_f32(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_flat_top_ampl   = 1.0 / mean_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);
    gs_correction_factors.f32_flat_top_energy = 1.0 / rms_of_float_array(pf32_temp_wind_array, DATA_BLOCK_SIZE);

    free(pf32_temp_wind_array); // release the memory for the wind array that is no longer needed.

    return ESP_OK;
}

static float get_selected_corr_factor(ecep_wind_corr_factor_t eFFT_window_corr_factor)
{
    switch (eFFT_window_corr_factor)
    {
    case none:
        return gs_correction_factors.f32_no_correction;
        break;
    case amplitude:
        switch (gs_curr_selected_wind)
        {
        case hann:
            return gs_correction_factors.f32_hann_ampl;
            break;
        case blackman:
            return gs_correction_factors.f32_blackman_ampl;
            break;
        case blackman_harris:
            return gs_correction_factors.f32_blackman_harris_ampl;
            break;
        case blackman_nuttall:
            return gs_correction_factors.f32_blackman_nuttall_ampl;
            break;
        case nuttall:
            return gs_correction_factors.f32_nuttall_ampl;
            break;
        case flat_top:
            return gs_correction_factors.f32_flat_top_ampl;
            break;
        default:
            ESP_LOGE(TAG, "ERROR: Currently selected window is invalid");
            return ESP_FAIL;
            break;
        }
        break;
    case energy:
        switch (gs_curr_selected_wind)
        {
        case hann:
            return gs_correction_factors.f32_hann_energy;
            break;
        case blackman:
            return gs_correction_factors.f32_blackman_energy;
            break;
        case blackman_harris:
            return gs_correction_factors.f32_blackman_harris_energy;
            break;
        case blackman_nuttall:
            return gs_correction_factors.f32_blackman_nuttall_energy;
            break;
        case nuttall:
            return gs_correction_factors.f32_nuttall_energy;
            break;
        case flat_top:
            return gs_correction_factors.f32_flat_top_energy;
            break;
        default:
            ESP_LOGE(TAG, "ERROR: Currently selected window is invalid");
            return ESP_FAIL;
            break;
        }
        break;
    default:
        ESP_LOGE(TAG, "ERROR: Invalid argument (eFFT_window_corr_factor) in get_selected_corr_factor()");
        return ESP_ERR_INVALID_ARG;
        break;
    }
    return ESP_FAIL;
}

esp_err_t conv_ampl_mv_to_ampl_db_spl(float *pf32_data, int32_t i32_bin_size, ecep_hw_gain_stage_t eFFT_hw_gain_stage)
{
    float f32_p0_mv;

    if (gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: conv_ampl_mv_to_ampl_db_spl() called before fft_wrapper_init()");
        return ESP_FAIL;
    }

    switch (eFFT_hw_gain_stage)
    {
    case up_to_90dB_SPL:
        f32_p0_mv = gs_f32_p0_mv_90dB_stage;
        break;
    case up_to_120dB_SPL:
        f32_p0_mv = gs_f32_p0_mv_120dB_stage;
        break;
    default:
        ESP_LOGE(TAG, "ERROR: in conv_ampl_mv_to_ampl_db_spl() - Invalid param: eFFT_hw_gain_stage");
        return ESP_ERR_INVALID_ARG;
        break;
    }

    for (int i = 0; i < i32_bin_size; i++)
    {
        pf32_data[i] = 20 * log10f(pf32_data[i] / f32_p0_mv);
    }

    return ESP_OK;
}

/**
 * @brief Convert complex fft result to real amplitude values.
 *        (See page 103 in "Digital Signal Processing: Fundamentals and Applications 3ed." for the math)
 *
 * @param[in, out] pf32_data Pointer to complex fft result. Calculations are done "in place" (output is placed at same location)
 * @param[in] i32_bin_size amount of complex values in fft result array
 * @param[in] b_result_in_db_spl true = return amplitude result in dB SPL, false = return amplitude result in mV
 * @param[in] eFFT_hw_gain_stage the currently used hardware gain stage (this param is only used if b_result_in_db_spl is set true)
 * @param[in] eFFT_window_corr_factor The window correction factor type
 * @return esp_err_t
 */
static esp_err_t conv_fft_res_to_ampl(float *pf32_data, int32_t i32_bin_size, bool b_result_in_db_spl, ecep_hw_gain_stage_t eFFT_hw_gain_stage, ecep_wind_corr_factor_t eFFT_window_corr_factor)
{
    // Obtain the selected window-correction-factor for the window in use:
    float f32_selected_wind_corr_factor = get_selected_corr_factor(eFFT_window_corr_factor);

    // Calculate amplitude:
    for (int32_t i = 0; i < i32_bin_size; i++)
    {
        pf32_data[i] = f32_selected_wind_corr_factor * (sqrtf(pf32_data[i * 2 + 0] * pf32_data[i * 2 + 0] + pf32_data[i * 2 + 1] * pf32_data[i * 2 + 1] + 0.0000001) / i32_bin_size);
        // Ampli = sqrt(Real[X(k)]^2 + Imag[X(k)]^2) / N
        // log10(0) is not a valid statement! (therefore add 0.0000001 to make sure the argument is never 0))
    }

    // Convert to dB SPL if specified:
    if (b_result_in_db_spl == true)
    {
        conv_ampl_mv_to_ampl_db_spl(pf32_data, i32_bin_size, eFFT_hw_gain_stage);
    }

    return ESP_OK;
}

/**
 * @brief Convert complex fft result to real power values
 *        (See page 103 in "Digital Signal Processing: Fundamentals and Applications 3ed." for the math)
 *
 * @param[in, out] pf32_data Pointer to complex fft result. Calculations are done "in place" (output is placed at same location)
 * @param[in] i32_bin_size amount of complex values in fft result array
 * @param[in] eFFT_window_corr_factor The window correction factor type
 * @return esp_err_t
 */
static esp_err_t conv_fft_res_to_pow(float *pf32_data, int32_t i32_bin_size, ecep_wind_corr_factor_t eFFT_window_corr_factor)
{
    // Obtain the selected window-correction-factor for the window in use:
    float f32_selected_wind_corr_factor = get_selected_corr_factor(eFFT_window_corr_factor);

    for (int32_t i = 0; i < i32_bin_size; i++)
    {
        pf32_data[i] = pow(f32_selected_wind_corr_factor, 2.0) * ((pf32_data[i * 2 + 0] * pf32_data[i * 2 + 0] + pf32_data[i * 2 + 1] * pf32_data[i * 2 + 1] + 0.0000001) / (i32_bin_size * i32_bin_size));
        // Pow = 1/(N^2) * (Real[X(k)]^2 + Imag[X(k)]^2)
        // (f32_selected_wind_corr_factor is to the power of 2 because we are dealing with power. Remeber Pow = ampl^2)
    }

    return ESP_OK;
}

static esp_err_t generate_window(ecep_window_type_t eFFT_window_type)
{
    switch (eFFT_window_type)
    {
    case hann:
        // Generate hann window:
        dsps_wind_hann_f32(gs_af32_wind, DATA_BLOCK_SIZE);
        break;
    case blackman:
        // Generate Blackman window
        dsps_wind_blackman_f32(gs_af32_wind, DATA_BLOCK_SIZE);
        break;
    case blackman_harris:
        // Generate Blackman-Harris window
        dsps_wind_blackman_harris_f32(gs_af32_wind, DATA_BLOCK_SIZE);
        break;
    case blackman_nuttall:
        // Generate Blackman-Nuttall window
        dsps_wind_blackman_nuttall_f32(gs_af32_wind, DATA_BLOCK_SIZE);
        break;
    case nuttall:
        // Generate Nuttall window
        dsps_wind_nuttall_f32(gs_af32_wind, DATA_BLOCK_SIZE);
        break;
    case flat_top:
        // Generate Flat-Top window
        dsps_wind_flat_top_f32(gs_af32_wind, DATA_BLOCK_SIZE);
        break;
    default:
        ESP_LOGE(TAG, "ERROR: in generate_window() - Invalid param: eFFT_window_type");
        return ESP_ERR_INVALID_ARG;
        break;
    }
    return ESP_OK;
}

esp_err_t fft_wrapper_set_wind(ecep_window_type_t eFFT_window_type)
{
    if (gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_set_wind() called before fft_wrapper_init()");
        return ESP_FAIL;
    }

    generate_window(eFFT_window_type);
    gs_curr_selected_wind = eFFT_window_type;

    return ESP_OK;
}

esp_err_t fft_wrapper_run_fft_tech3(float *pf32_data, eFFT_radix_type_t eFFT_radix_type, ecep_output_form_t eFFT_output_form, ecep_hw_gain_stage_t eFFT_hw_gain_stage, ecep_wind_corr_factor_t eFFT_window_corr_factor)
{
    esp_err_t ret;

    if (gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init()");
        return ESP_FAIL;
    }

    // Format for fft api and multiply with window function:
    for (int32_t i = 0; i < DATA_BLOCK_SIZE; i++)
    {
        pf32_data[i] = pf32_data[i] * gs_af32_wind[i];
    }

    switch (eFFT_radix_type)
    {
    case radix4:

        // FFT Radix-4:
        ret = dsps_fft4r_fc32(pf32_data, DATA_BLOCK_SIZE / 2); // Half size FFT
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ERROR: Radix4 FFT failed Error = %i", ret);
            return ret;
        }

        // Bit reverse:
        ret = dsps_bit_rev4r_fc32(pf32_data, DATA_BLOCK_SIZE / 2);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ERROR: Radix4 Bit Reversal failed Error = %i", ret);
            return ret;
        }

        break;

    case radix2:

        // FFT Radix-2:
        ret = dsps_fft2r_fc32(pf32_data, DATA_BLOCK_SIZE / 2); // Half size FFT
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ERROR: Radix4 FFT failed Error = %i", ret);
            return ret;
        }

        // Bit reverse:
        ret = dsps_bit_rev2r_fc32(pf32_data, DATA_BLOCK_SIZE / 2);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ERROR: Radix4 FFT failed Error = %i", ret);
            return ret;
        }

        break;

    default:
        ESP_LOGE(TAG, "ERROR: Invalid argument (eFFT_radix_type) in fft_wrapper_run_fft_tech3()");
        return ESP_ERR_INVALID_ARG;
        break;
    }

    // Splitting operation:
    ret = dsps_cplx2real_fc32(pf32_data, DATA_BLOCK_SIZE / 2); /* The output of the FFT is normally "dual sided" (from 0hz to fs(-1 bin)).
                                                            HOWEVER the output of dsps_cplx2real_fc32 is single sided (from 0hz til fny(-1 bin))
                                                            (Aka this api does not take the complex conjugate at append it (as descbried in our paper) to get the dual sided fft doing the splitting operation)
                                                            Source: github quote from a dev of the espressif fft api "If you use dsps_cplx2reC or dsps_cplx2real_fc32 you will have only single sided"
                                                        */
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ERROR: Splitting Operation failed = %i", ret);
        return ret;
    }

    // Extract amplitude or power:
    switch (eFFT_output_form)
    {
    case ampltitude_mV:
        conv_fft_res_to_ampl(pf32_data, DATA_BLOCK_SIZE / 2, false, 0, eFFT_window_corr_factor);
        break;
    case ampltitude_dB_SPL:
        conv_fft_res_to_ampl(pf32_data, DATA_BLOCK_SIZE / 2, true, eFFT_hw_gain_stage, eFFT_window_corr_factor);
        break;
    case power_mV:
        conv_fft_res_to_pow(pf32_data, DATA_BLOCK_SIZE / 2, eFFT_window_corr_factor);
        break;
    default:
        ESP_LOGE(TAG, "ERROR: Invalid argument (eFFT_output_form) in fft_wrapper_run_fft_tech3()");
        return ESP_ERR_INVALID_ARG;
        break;
    }

    return ESP_OK;
}

esp_err_t fft_wrapper_init_tech3(ecep_window_type_t eFFT_window_type)
{
    esp_err_t ret;

    ret = dsps_fft2r_init_fc32(NULL, DATA_BLOCK_SIZE / 2); // Half size FFT (amount of complex values)
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ERROR: Not possible to initialize FFT2R. Error = %i", ret);
        return ret;
    }

    ret = dsps_fft4r_init_fc32(NULL, DATA_BLOCK_SIZE / 2); // Half size FFT (amount of complex values)
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ERROR: Not possible to initialize FFT4R. Error = %i", ret);
        return ret;
    }

    generate_window(eFFT_window_type);
    gs_curr_selected_wind = eFFT_window_type;

    switch (ADC_ATTEN_DB)
    {
    case ADC_ATTEN_DB_0:
        gs_max_adc_val_mv = 950.0;
        break;
    case ADC_ATTEN_DB_2_5:
        gs_max_adc_val_mv = 1250.0;
        break;
    case ADC_ATTEN_DB_6:
        gs_max_adc_val_mv = 1750.0;
        break;
    case ADC_ATTEN_DB_11:
        gs_max_adc_val_mv = 3100.0;
        break;
    default:
        ESP_LOGE(TAG, "ERROR: Invalid argument (ADC_ATTEN_DB) in fft_wrapper_init_tech3()");
        return ESP_ERR_INVALID_ARG;
        break;
    }

    gs_f32_p0_mv_90dB_stage = ((gs_max_adc_val_mv / 2) / (powf(10.0, 90.0 / 20.0) * P0)) * P0; /* (see top of this file for explanation of the math)
                                             Lp = 20*log10(p/p0)  -->  p = 10^(Lp/20)*p0 = 10^(90/20)*(20*10^(-6)) = 0.6324555320 Pa
                                             mV_Pascal_ratio = (max_adc_mV / 2) / 0.6324555320 = 1383.496476   (Remember the input signal is DC biased. That is why max_adc_mV is divded by 2)
                                             gs_f32_p0_mv_90dB_stage = mV_Pascal_ratio * p0 = 0.02766992952 */
    gs_f32_p0_mv_120dB_stage = ((gs_max_adc_val_mv / 2) / (powf(10.0, 120.0 / 20.0) * P0)) * P0;

    calc_wind_correction_factors();

    gs_init_called = true;
    return ESP_OK;
}
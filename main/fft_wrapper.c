
#include <stdint.h> 
#include <math.h>

#include "esp_dsp.h"

#include "fft_wrapper.h"
#include "common_defines.h"

static const char *TAG = "fft_wrapper"; 

#define P0   0.000020  // dB SPL Reference in Pascal

static float gs_max_adc_val_mv;
//static eFFT_hw_gain_stage_t gs_current_gain_stage;  // Currently selected gain stage

/* Procedure to get amplitude output in dB SPL:
    1: Find the ratio between ADC input mV vs. Pascal from microphone
       1: Use the expression for dB SPL to find the pascal value for a dB SPL value where the equavilent mV output is known  
       2: Divide the known mV value with the newly found pascal value to get the ratio
    2: Divide the amplitude values with this ratio to convert from mV to Pascal
    3: Use the expression for dB SPL to find the dB SPL associated with amplitude value in Pascal (using the standard p0 value)

    A more effecient way:
    Instead of continously converting each amplitude value to pascal and later diving by p0, Let us instead convert p0 to mV once and use that to convert amplitude to dB SPL.
    (the ratio p/p0 is the same in mV or Pascal as long as both are the same unit, and asuming a proportional relation between mV and Pascal)
    This is done by multiplying p0 with the mV vs. Pascal ratio mentioned before.
    This will result in a a new p0 value for each hardware gain stages
*/
static float gs_f32_p0_mv_90dB_stage;   // P0 value converted to mV for the "up to 90dB SPL" gain stage
static float gs_f32_p0_mv_120dB_stage;  // P0 value converted to mV for the "up to 120dB SPL" gain stage

static int32_t i32_N = DATA_BLOCK_SIZE;    // gs_ FORAN??? måske kand andet end N, da det ikke er bin størrelsen

static bool gs_init_called = false;

// Window coefficients:
__attribute__((aligned(16))) 
static float gs_af32_wind[DATA_BLOCK_SIZE]; 

// SKRIV KORT OM I RAPPORTEN HVORFOR VI BRUGER WINDOWS
// DE nævner det her:
// https://www.sjsu.edu/people/burford.furman/docs/me120/FFT_tutorial_NI.pdf 
// MÅSKE FIND AFSNITTET I DSP BOGEN MED DET, OG SE HVAD DE HAR AT SIGE
// TROR DET STÅR PÅ SIDE 112 I DEN NYE DSP BOG
// BARE SKRIV SUPER KORT OM DET


// Husk måske også lige at convert "x-aksen" fra bin nr til frekvens (skal måske ikke gøres her men et andet sted idk)
// (se (4.26) på side 103 i ny dsp bog for denne formel)


/**
 * @brief Convert complex fft result to real amplitude values. 
 *        (See page 103 in "Digital Signal Processing: Fundamentals and Applications 3ed." for the math)
 *        SKRIV AT OUTPUT ER mV fra 0 - 1750 HVIS OUPUTTET IKKE ER DB. OG DB SPL HVIS DET ER 
 * 
 * @param[in, out] pf32_data Pointer to complex fft result. Calculations are done "in place" (output is placed at same location)
 * @param[in] i32_bin_size amount of complex values in fft result array
 * @param[in] b_result_in_db_spl true = return amplitude result in dB SPL, false = return amplitude result in mV 
 * @param[in] eFFT_hw_gain_stage the currently used hardware gain stage (this param is only used if b_result_in_db_spl is set true)
 * @return esp_err_t 
 */
static esp_err_t conv_fft_res_to_ampl(float * pf32_data, int32_t i32_bin_size, bool b_result_in_db_spl, eFFT_hw_gain_stage_t eFFT_hw_gain_stage)
{
    float f32_p0_mv;

    if(b_result_in_db_spl == true)
    {
        switch(eFFT_hw_gain_stage)
        {
            case up_to_90dB_SPL:
                f32_p0_mv = gs_f32_p0_mv_90dB_stage;
                break;
            case up_to_120dB_SPL:
                f32_p0_mv = gs_f32_p0_mv_120dB_stage;
                break;
            default:
                ESP_LOGE(TAG, "ERROR: in conv_fft_res_to_ampl() - Invalid param: eFFT_hw_gain_stage"); 
                return ESP_ERR_INVALID_ARG;
                break;
        }

        for(int i = 0; i < i32_bin_size; i++) // return amplitude result in dB SPL
        {
            pf32_data[i] = 20 * log10f((sqrtf(pf32_data[i* 2 + 0] * pf32_data[i* 2 + 0] +  pf32_data[i* 2 + 1] * pf32_data[i* 2 + 1] + 0.0000001) / i32_bin_size) / f32_p0_mv);    // Ampli = sqrt(Real[X(k)]^2 + Imag[X(k)]^2) / N                                                                                                                                           
        }                                                                                                                                                             // log10(0) is not a valid statement! (therefore add 0.0000001 to make sure the argument is never 0))
    }
    else
    {
        for(int i = 0; i < i32_bin_size; i++) // return amplitude result in mV 
        {
            pf32_data[i] = sqrtf(pf32_data[i* 2 + 0] * pf32_data[i* 2 + 0] +  pf32_data[i* 2 + 1] * pf32_data[i* 2 + 1] + 0.0000001) / i32_bin_size;    // Ak = 1/N * sqrt(Real[X(k)]^2 + Imag[X(k)]^2)
        }
    }

    return ESP_OK;
}

/**
 * @brief Convert complex fft result to real power values
 *        (See page 103 in "Digital Signal Processing: Fundamentals and Applications 3ed." for the math)
 * 
 * @param[in, out] pf32_data Pointer to complex fft result. Calculations are done "in place" (output is placed at same location)
 * @param[in] i32_bin_size amount of complex values in fft result array
 * @return esp_err_t 
 */
static esp_err_t conv_fft_res_to_pow(float * pf32_data, int32_t i32_bin_size)
{
    // VED IKKE HVORFOR HAN DELER MED 2N I HANS OG IKKE N^2 SOM DER STÅR I BOGEN 
    for(int i = 0; i < i32_bin_size; i++)
    {
        pf32_data[i] = (pf32_data[i* 2 + 0] * pf32_data[i* 2 + 0] +  pf32_data[i* 2 + 1] * pf32_data[i* 2 + 1] + 0.0000001) / (i32_bin_size*i32_bin_size);    // Ak = 1/(N^2) * (Real[X(k)]^2 + Imag[X(k)]^2)
    }

    return ESP_OK;
}

esp_err_t conv_ampl_mv_to_ampl_db_spl(float * pf32_data, int32_t i32_bin_size, eFFT_hw_gain_stage_t eFFT_hw_gain_stage)
{
    float f32_p0_mv;

    if(gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: conv_ampl_mv_to_ampl_db_spl() called before fft_wrapper_init()" );
        return ESP_FAIL;
    }

    switch(eFFT_hw_gain_stage)
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

    for(int i = 0; i < i32_bin_size; i++)
    {
        pf32_data[i] = 20 * log10f(pf32_data[i] /  f32_p0_mv);
    }

    return ESP_OK;
}

static esp_err_t generate_window(eFFT_window_type_t eFFT_window_type)
{
    switch (eFFT_window_type)
    {
        case hann:
            // Generate hann window:
            dsps_wind_hann_f32(gs_af32_wind, i32_N); 
            break;
        
        case blackman:
            // Generate Blackman window
            dsps_wind_blackman_f32(gs_af32_wind, i32_N);
            break;

        case blackman_harris:
            // Generate Blackman-Harris window
            dsps_wind_blackman_harris_f32(gs_af32_wind, i32_N);
            break;

        case blackman_nuttall:
            // Generate Blackman-Nuttall window
            dsps_wind_blackman_nuttall_f32(gs_af32_wind, i32_N);
            break;

        case nuttall:
            // Generate Nuttall window
            dsps_wind_nuttall_f32(gs_af32_wind, i32_N);
            break;
        
        case flat_top:
            // Generate Flat-Top window
            dsps_wind_flat_top_f32(gs_af32_wind, i32_N);
            break;

        default:
            ESP_LOGE(TAG, "ERROR: in generate_window() - Invalid param: eFFT_window_type"); 
            return ESP_ERR_INVALID_ARG;
            break;
    }

    return ESP_OK;
}


esp_err_t fft_wrapper_init_tech3(eFFT_window_type_t eFFT_window_type)
{
    esp_err_t ret;

    ret = dsps_fft2r_init_fc32(NULL, i32_N >> 1); // Half size FFT (amount of complex values)
    if (ret  != ESP_OK)
    {
        ESP_LOGE(TAG, "ERROR: Not possible to initialize FFT2R. Error = %i", ret);
        return ret;
    }

    ret = dsps_fft4r_init_fc32(NULL, i32_N >> 1); // Half size FFT (amount of complex values)
    if (ret  != ESP_OK)
    {
        ESP_LOGE(TAG, "ERROR: Not possible to initialize FFT4R. Error = %i", ret);
        return ret;
    }

    generate_window(eFFT_window_type);

    switch(ADC_ATTEN_DB)
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
            ESP_LOGE(TAG, "ERROR: invalid ADC_ATTEN_DB");
            return ESP_FAIL;
            break;
    }

    gs_f32_p0_mv_90dB_stage = ((gs_max_adc_val_mv / 2) / (powf(10.0, 90.0/20.0)*P0)) * P0;    /* (see top of this file for explanation of the math)
                                            Lp = 20*log10(p/p0)  -->  p = 10^(Lp/20)*p0 = 10^(90/20)*(20*10^(-6)) = 0.6324555320 Pa
                                            mV_Pascal_ratio = (max_adc_mV / 2) / 0.6324555320 = 1383.496476   (Remember the input signal is DC biased. That is why max_adc_mV is divded by 2)
                                            gs_f32_p0_mv_90dB_stage = mV_Pascal_ratio * p0 = 0.02766992952
                                            */ 
    gs_f32_p0_mv_120dB_stage = ((gs_max_adc_val_mv / 2) / (powf(10.0, 120.0/20.0)*P0)) * P0; 

    gs_init_called = true;
    return ESP_OK;
}

esp_err_t fft_wrapper_set_wind(eFFT_window_type_t eFFT_window_type)
{
    if(gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_set_wind() called before fft_wrapper_init()" );
        return ESP_FAIL;
    }

    generate_window(eFFT_window_type);
    return ESP_OK;
}


esp_err_t fft_wrapper_run_fft_tech3(float *pf32_data, eFFT_radix_type_t eFFT_radix_type, eFFT_output_form_t eFFT_output_form, eFFT_hw_gain_stage_t eFFT_hw_gain_stage)
{
    esp_err_t ret;

    if(gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init()" );
        return ESP_FAIL;
    }

    // Format for fft api and multiply with window function:
    for(int32_t i = 0; i < i32_N; i++)
    {
        pf32_data[i] = pf32_data[i] * gs_af32_wind[i];
    }

    switch(eFFT_radix_type)
    {
        case radix4:

            // FFT Radix-4:
            ret = dsps_fft4r_fc32(pf32_data, i32_N>>1); // Half size FFT
            if(ret != ESP_OK)
            {
                ESP_LOGE(TAG, "ERROR: Radix4 FFT failed Error = %i", ret);;
                return ret;
            }

            // Bit reverse:
            ret = dsps_bit_rev4r_fc32(pf32_data, i32_N>>1);
            if(ret != ESP_OK)
            {
                ESP_LOGE(TAG, "ERROR: Radix4 Bit Reversal failed Error = %i", ret);;
                return ret;
            }

            break;

        case radix2:

            // FFT Radix-2:
            ret = dsps_fft2r_fc32(pf32_data, i32_N >> 1); // Half size FFT
            if(ret != ESP_OK)
            {
                ESP_LOGE(TAG, "ERROR: Radix4 FFT failed Error = %i", ret);;
                return ret;
            }

            // Bit reverse:
            ret = dsps_bit_rev2r_fc32(pf32_data, i32_N >> 1);
            if(ret != ESP_OK)
            {
                ESP_LOGE(TAG, "ERROR: Radix4 FFT failed Error = %i", ret);;
                return ret;
            }

            break;

        default:
            ESP_LOGE(TAG, "ERROR: Invalid paramater (radix type)");
            break;
    }

    // Splitting operation:
    ret = dsps_cplx2real_fc32(pf32_data, i32_N>>1);                 /* The output of the FFT is normally "dual sided" (from 0hz to fs(-1 bin)). 
                                                                        HOWEVER the output of dsps_cplx2real_fc32 is single sided (from 0hz til fny(-1 bin))
                                                                        (Aka this api does not take the complex conjugate at append it (as descbried in our paper) to get the dual sided fft doing the splitting operation)
                                                                        Source: github quote from a dev of the espressif fft api "If you use dsps_cplx2reC or dsps_cplx2real_fc32 you will have only single sided"
                                                                    */
    if(ret != ESP_OK)                                               
    {                                                               
        ESP_LOGE(TAG, "ERROR: Splitting Operation failed = %i", ret);;
        return ret;
    }

    // Extract amplitude or power:
    switch(eFFT_output_form)
    {

        case ampltitude_mV:
            conv_fft_res_to_ampl(pf32_data, i32_N>>1, false, 0);
            break;

        case ampltitude_dB_SPL:
            conv_fft_res_to_ampl(pf32_data, i32_N>>1, true, eFFT_hw_gain_stage);
            break;

        case power_mV:
            conv_fft_res_to_pow(pf32_data, i32_N>>1);
            break;

        default:
            ESP_LOGE(TAG, "ERROR: Invalid paramater (output format type)");
            break;
    }
    
    return ESP_OK;
}

#include <stdint.h> 
#include <math.h>

#include "esp_dsp.h"

#include "fft_wrapper.h"
#include "common_defines.h"

static const char *TAG = "fft_wrapper"; 


static eFFT_hw_gain_stage_t gs_hw_gain_stage;

static float gs_max_adc_val_mv;
static float gs_up_to_90dB_SPL_sf;     // Scaling factor for amplitude output when using a microphone preamp so that the max ADC value equals 120db SPL
static float gs_up_to_120dB_SPL_sf;    // Scaling factor for amplitude output when using a microphone preamp so that the max ADC value equals 90db SPL
static float gs_dB_SPL_scaling_factor;

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
 * @param[in] b_result_in_db true = convert amplitude result to dB, false = dont convert amplitude result to dB
 * @return esp_err_t 
 */
static esp_err_t conv_fft_res_to_ampl(float * pf32_data, int32_t i32_bin_size, bool b_result_in_db)
{
    if(b_result_in_db == true)
    {
        //unsigned int start_amp_db = dsp_get_cpu_cycle_count(); // for benchmark
        for(int i = 0; i < i32_bin_size; i++)
        {
            pf32_data[i] = 20 * log10f( sqrtf(pf32_data[i* 2 + 0] * pf32_data[i* 2 + 0] +  pf32_data[i* 2 + 1] * pf32_data[i* 2 + 1] + 0.0000001) / i32_bin_size);    // Ak = 1/N * sqrt(Real[X(k)]^2 + Imag[X(k)]^2)
                                                                                                                                                                      // 20*log10() is used to convert Power to dB
        }
        //unsigned int stop_amp_db = dsp_get_cpu_cycle_count(); // for benchmark
        // ESP_LOGI(TAG, "SPEED TEST, ampl_db:  %i", stop_amp_db-start_amp_db); // for benchmark
    }
    else
    {
        for(int i = 0; i < i32_bin_size; i++)
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
 * @param[in] b_result_in_db true = convert power result to dB, false = dont convert power result to dB
 * @return esp_err_t 
 */
static esp_err_t conv_fft_res_to_pow(float * pf32_data, int32_t i32_bin_size, bool b_result_in_db)
{
    // VED IKKE HVORFOR HAN DELER MED 2N I HANS OG IKKE N^2 SOM DER STÅR I BOGEN 

    if(b_result_in_db == true)
    {
        //unsigned int start_pow_db = dsp_get_cpu_cycle_count(); // for benchmark
        for(int i = 0; i < i32_bin_size; i++)
        {
            pf32_data[i] = 10 * log10f( (pf32_data[i* 2 + 0] * pf32_data[i* 2 + 0] +  pf32_data[i* 2 + 1] * pf32_data[i* 2 + 1] + 0.0000001) / (i32_bin_size*i32_bin_size));    // Ak = 1/(N^2) * (Real[X(k)]^2 + Imag[X(k)]^2)
                                                                                                                                                                                // 10*log10() is used to convert Power to dB
        }
        //unsigned int stop_pow_db = dsp_get_cpu_cycle_count(); // for benchmark
        //ESP_LOGI(TAG, "SPEED TEST, pow_db:  %i", stop_pow_db-start_pow_db); // for benchmark
    }
    else
    {
        for(int i = 0; i < i32_bin_size; i++)
        {
            pf32_data[i] = (pf32_data[i* 2 + 0] * pf32_data[i* 2 + 0] +  pf32_data[i* 2 + 1] * pf32_data[i* 2 + 1] + 0.0000001) / (i32_bin_size*i32_bin_size);    // Ak = 1/(N^2) * (Real[X(k)]^2 + Imag[X(k)]^2)
        }
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
            break;
    }

    return ESP_OK;
}


esp_err_t fft_wrapper_init_tech3(eFFT_window_type_t eFFT_window_type, eFFT_hw_gain_stage_t eFFT_hw_gain_stage)
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
    gs_hw_gain_stage = eFFT_hw_gain_stage;

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
            break;
    }

    gs_up_to_90dB_SPL_sf  = 1000000.0  / gs_max_adc_val_mv;  // (120dB --> 1,000,000)  
    gs_up_to_120dB_SPL_sf = 31622.7766 / gs_max_adc_val_mv;  // (120dB --> 31622.7766)  

    ESP_LOGE(TAG, "gs_up_to_90dB_SPL_sf: %f", gs_up_to_90dB_SPL_sf); // DEBUGGING
    ESP_LOGE(TAG, "gs_up_to_120dB_SPL_sf: %f", gs_up_to_120dB_SPL_sf); // DEBUGGING

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

esp_err_t fft_wrapper_set_hw_gain_stage(eFFT_hw_gain_stage_t eFFT_hw_gain_stage)
{
    if(gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_set_hw_gain_stage() called before fft_wrapper_init()" );
        return ESP_FAIL;
    }

    gs_hw_gain_stage = eFFT_hw_gain_stage;

    return ESP_OK;
}


esp_err_t fft_wrapper_run_fft_tech3(float *pf32_data, eFFT_radix_type_t s_radix, eFFT_output_form_t s_output_form)
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

    switch(s_radix)
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

    // Extract amplitude/power and convert to dB if specified:
    switch(s_output_form)
    {

        case ampltitude:
            conv_fft_res_to_ampl(pf32_data, i32_N>>1, false);
            break;

        case ampltitude_db:
            conv_fft_res_to_ampl(pf32_data, i32_N>>1, true);
            break;

        case power:
            conv_fft_res_to_pow(pf32_data, i32_N>>1, false) ;
            break;

        case power_db:
            conv_fft_res_to_ampl(pf32_data, i32_N>>1, true);  // conv_fft_res_to_ampl in db gives same result as conv_fft_res_to_pow in db. However conv_fft_res_to_ampl is faster! (around 1095848 vs 576608 cycles)
            break;

        default:
            ESP_LOGE(TAG, "ERROR: Invalid paramater (output format type)");
            break;
    }
    
    return ESP_OK;
}
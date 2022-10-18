
#include <stdint.h> // for int32_t, uint32_t ... naming standard
#include "esp_dsp.h"

static const char *TAG = "fft_wrapper"; // for log

int32_t i32N = N_SAMPLES;


static bool gs_init_called          = false;
static bool gs_generete_wind_called = false


// Window coefficients
__attribute__((aligned(16))) 
static float f32Wind[i32N];




esp_err_t fft_wrapper_init()
{
    esp_err_t ret;

    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return ret;
    }

    gs_init_called = true;
    return ESP_OK;
}


esp_err_t fft_wrapper_generete_wind(eFFT_window_type_t eFFT_window_type)
{
    if(gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_generete_wind() called before fft_wrapper_init()" );
        return ESP_FAIL;
    }

    switch (eFFT_window_type)
    {
        case hann:
            // Generate hann window:
            dsps_wind_hann_f32(f32Wind, i32N); 
            break;
        
        case blackman:
            // Generate Blackman window
            dsps_wind_blackman_f32(f32Wind, i32NN);
            break;

        case blackman_harris:
            // Generate Blackman-Harris window
            dsps_wind_blackman_harris_f32(f32Wind, i32NN);
            break;

        case blackman_nuttall:
            // Generate Blackman-Nuttall window
            dsps_wind_blackman_nuttall_f32(f32Wind, i32NN)
            break;

        case nuttall:
            // Generate Nuttall window
            dsps_wind_nuttall_f32(f32Wind, i32NN);
            break;
        
        case flat_top:
            // Generate Flat-Top window
            dsps_wind_flat_top_f32(f32Wind, i32NN);
            break;

        default:
            break;
    }

    gs_generete_wind_called = true;
    return ESP_OK;
}


// esp_err_t fft_wrapper_start_fft(eFFT_type_t eFFT_type, eFFT_radix_type_t eFFT_radix_type)
// {

    

//     if((gs_init_called == false) || (gs_generete_wind_called == false))
//     {
//         ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init() and fft_wrapper_generete_wind()" );
//         return ESP_FAIL;
//     }


//     switch (eFFT_type)
//     {
//     case one_real_input_normal:

//         for (int i = 0; i < i32N; i++)
//         {
            
//         }

//         switch (eFFT_radix_type)
//         {
//             case radix2:
//                 break;

//             case radix4:
//                 break;

//             default:
//                 break;
//         }

//         break;

//     case one_real_input_fast:

//         switch (eFFT_radix_type)
//         {
//             case radix2:
//                 break;

//             case radix4:
//                 break;

//             default:
//                 break;
//         }

//         break;

//     case two_real_input_fast:

//         switch (eFFT_radix_type)
//         {
//             case radix2:
//                 break;

//             case radix4:
//                 break;

//             default:
//                 break;
//         }

//         break;

//     default:
//         break;
//     }
// }




// (Det noget med man ikke kan lave 1real_fast tricket med 4 radix. Det nævner de i "Real-valued fast Fourier transform algorithms" og han bruger ikke radix 4 i eksemplet hvor han laver tricket)
// Skriv at det array man smider ind skal være N_SAMPLES størrelse af floats med __attribute__((aligned(16))) foran
// Skriv at N skal være power of 2 ved radix 2 (og power of 4 ved radix 4 tror jeg)
// Og at N skal være: (has to be under or equal the max fft length set in menuconfig)
esp_err_t fft_wrapper_start_fft_1real_normal(float *gs_pf32BinLocation, ASD ET POINTER TIL OUTPUT MÅSKE)
{


    if((gs_init_called == false) || (gs_generete_wind_called == false))
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init() and fft_wrapper_generete_wind()" );
        return ESP_FAIL;
    }


    return ESP_OK;
}

esp_err_t fft_wrapper_start_fft_1real_fast(float *gs_pf32BinLocation)
{

    if((gs_init_called == false) || (gs_generete_wind_called == false))
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init() and fft_wrapper_generete_wind()" );
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t fft_wrapper_start_fft_2real_fast(float *gs_pf32BinLocation1, float *gs_pf32BinLocation2)
{

    if((gs_init_called == false) || (gs_generete_wind_called == false))
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init() and fft_wrapper_generete_wind()" );
        return ESP_FAIL;
    }

    return ESP_OK;
}

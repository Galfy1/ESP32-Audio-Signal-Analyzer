
#ifndef FFTWRAPPER_H_INCLUDED
#define FFTWRAPPER_H_INCLUDED


// typedef enum{
//     one_real_input_normal,
//     one_real_input_fast,
//     two_real_input_fast
// } eFFT_type_t;

typedef enum{
    ampltitude,
    ampltitude_db,
    power,
    power_db,
} eFFT_output_form_t;

typedef enum {
    radix2,
    radix4,
} eFFT_radix_type_t;


typedef enum {
    hann,
    blackman,
    blackman_harris,
    blackman_nuttall,
    nuttall,
    flat_top,
} eFFT_window_type_t;

/**
 * @brief Initialize the FFT for acording to "technique 3" (technique 3 refers to using N/2 complex size fft with 1 real input sequency of size N)
 * 
 * @return esp_err_t 
 */
esp_err_t fft_wrapper_init_tech3();


/**
 * @brief Used to specify which window function to use duing FFT preprocessing. (DEFAULT_WIND is used as default if this function is not used)
 * 
 * @param eFFT_window_type Desired window type
 * @return esp_err_t 
 */
esp_err_t fft_wrapper_set_wind(eFFT_window_type_t eFFT_window_type);

/**
 * @brief  Run FFT acording to "technuiqe 3" with pre and post processing. (technique 3 refers to using N/2 complex size fft with 1 real input sequency of size N)
 *         Preprosessing: format real data for fft, multiply with window function 
 *         Postprocessing: Bit reverse, Run splitting operation, extract desired output format
 *         All computation is done in-place.
 * 
 * @param[in,out] pf32_data: Pointer to Real input sequence of size DATA_BLOCK_SIZE. Real output of size DATA_BLOCK_SIZE/2 is places at same location.
 *                          (size here refers to the amount of float elements)
 * @param[in] s_radix: Desired FFT radix
 * @param[in] s_output_form: Desired output format
 * 
 * @return esp_err_t:  returns ESP_OK if sucessfull, else returns error. 
 */
esp_err_t fft_wrapper_run_fft_tech3(float *pf32_data, eFFT_radix_type_t s_radix, eFFT_output_form_t s_output_form);



#endif /*FFTWRAPPER_H_INCLUDED */
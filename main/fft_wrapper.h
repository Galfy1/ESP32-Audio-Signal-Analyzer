
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

esp_err_t fft_wrapper_init_normal(eFFT_window_type_t eFFT_window_type);
esp_err_t fft_wrapper_init_fast(eFFT_window_type_t eFFT_window_type);

esp_err_t fft_wrapper_start_fft_1real_fast(float *pf32_data, eFFT_radix_type_t s_radix, eFFT_output_form_t s_output_form);



#endif /*FFTWRAPPER_H_INCLUDED */
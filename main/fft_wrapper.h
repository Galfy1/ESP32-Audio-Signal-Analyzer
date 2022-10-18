
#ifndef FFTWRAPPER_H_INCLUDED
#define FFTWRAPPER_H_INCLUDED

// Skriv at N skal være power of 2 ved radix 2 (og power of 4 ved radix 4 tror jeg)
#define N_SAMPLES 4096 // FFT bin size (has to be under or equal the max fft length set in menuconfig)

typedef enum{
    one_real_input_normal,
    one_real_input_fast,
    two_real_input_fast
} eFFT_type_t

typedef enum {
    radix2,
    radix4
} eFFT_radix_type_t


typedef enum {
    hann,
    blackman,
    blackman_harris,
    blackman_nuttall,
    nuttall,
    flat_top,
} eFFT_window_type_t





#endif /*FFTWRAPPER_H_INCLUDED */
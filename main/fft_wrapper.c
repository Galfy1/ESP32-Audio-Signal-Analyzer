
#include <stdint.h> // for int32_t, uint32_t ... naming standard
#include <math.h>

#include "esp_dsp.h"

#include "fft_wrapper.h"
#include "common_defines.h"

static const char *TAG = "fft_wrapper"; // for log

int32_t i32_N = DATA_BLOCK_SIZE;   // FIKS DEN HER STØRELSE TING. DEN SKAL VÆRE LIG DEN DATA_BLOCK_SIZE I COMMON DEFINES. Måske laver jeg bare en kald "DATA_BLOCK_SIZE" eller sådan noget.. og så skriv at size er antal elementer..


static bool gs_init_called          = false;

// Ved ikke om de kan være static (det var de ikke i hans eksempel):

// Window coefficients
__attribute__((aligned(16))) 
static float gs_af32_wind[DATA_BLOCK_SIZE];  // husk den der N_SAMPLES

// Data array:
// PROBLLEM: HVORDAN VIRKER DET ALIGNED HER, NÅR ARRAYED ER LAVET ET ANDET STED?? SKAL DET SÅ VÆRE ALLIGNED DER HVOR DET ORIGINALE ARRAY ER LAVET? 
// MEN SÅ KAN JEG IKKE BARE MEMCPY DATA IND I DET I DSP.c DET SHARED MEM ARRAY SOM IKKE ER ALLIGNED??? KAN JEG??? SÅ SKAL DET OGSÅ VÆRE ALLIGNED?
      


// SKRIV KORT OM I RAPPORTEN HVORFOR VI BRUGER WINDOWS
// DE nævner det her:
// https://www.sjsu.edu/people/burford.furman/docs/me120/FFT_tutorial_NI.pdf 
// MÅSKE FIND AFSNITTET I DSP BOGEN MED DET, OG SE HVAD DE HAR AT SIGE
// TROR DET STÅR PÅ SIDE 112 I DEN NYE DSP BOG
// BARE SKRIV SUPER KORT OM DET



//Output processing function:
// Se side 103 i ny dsp bog for at se hvordan man extracter amplitude eller power fra fft resultat
// 10*log10() bruges når man konverter power til db
// 20*log10() bruges når man konverter amplitude til db
static esp_err_t conv_fft_res_to_ampl_db(float * pf32_data, int32_t bin_size)
{
    // Husk: DEN SKAL KALDES MED N>>1 i der hvor vi bruger half fft'er
    for(int i = 0; i < bin_size; i++)
    {
        pf32_data[i] = 20 * log10f( sqrtf(pf32_data[i* 2 + 0] * pf32_data[i* 2 + 0] +  pf32_data[i* 2 + 1] * pf32_data[i* 2 + 1]) / bin_size);    // Ak = 1/N sqrt(Real[X(k)]^2 + Imag[X(k)]^2)
    }
    return ESP_OK;
}

// OPS: skal jeg gange med 2 (borset fra DC), for at få enkelside plot? Det skal jeg nok ja (ved ikke hvorfor han ikke gør det).
// NEJ VENT: ud fra hans kommentar til min esp-dsp github: If you use dsps_cplx2reC or dsps_cplx2real_fc32 you will have only single side.
// aka tror det bliver gjort inde i de splitting operation kommandoer (dog ikke 100% sikker)
// Men så igen... gør det det? eller er det "dobbelt sided" hvor man kun ser på den positive frekvens side, men hvor man ser den complex conjugate fra næste spectral kopi?
// Kommer lidt an på hvordan de dsps_cplx2reC or dsps_cplx2real_fc32 der virker... må jeg teste mig frem til ...
// (nævn i rapporten hvordan det er! nævn at dsps_cplx2reC or dsps_cplx2real_fc32 returner enten single eller double sided)

// Husk måske også lige at convert "x-aksen" fra bin nr til frekvens (skal måske ikke gøres her men et andet sted idk)
// (se (4.26) på side 103 i ny dsp bog for denne formel)

// static esp_err_t conv_fft_res_to_pow_db()
// {
//  // VED IKKE HVORFOR HAN DELER MED 2N I HANS OG IKKE N^2 SOM DER STÅR I BOGEN 
// }


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



// Does not make use of the effecieny techniques for real input signals.
// Allows for the use of the "normal" labeled FFT wrapper functions
// esp_err_t fft_wrapper_init_normal(eFFT_window_type_t eFFT_window_type)
// {
//     esp_err_t ret;

//     ret = dsps_fft2r_init_fc32(NULL, i32_N); // Full size FFT (amount of complex values)
//     if (ret  != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Not possible to initialize FFT2R. Error = %i", ret);
//         return ret;
//     }

//     ret = dsps_fft4r_init_fc32(NULL, i32_N); // Full size FFT (amount of complex values)
//     if (ret  != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Not possible to initialize FFT4R. Error = %i", ret);
//         return ret;
//     }

//     generate_window(eFFT_window_type);

//     gs_init_called = true;
//     return ESP_OK;
// }


// Makes use of the effecieny techniques for real input signals.
// Allows for the use of the "fast" labeled FFT wrapper functions
esp_err_t fft_wrapper_init_fast(eFFT_window_type_t eFFT_window_type)
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

    gs_init_called = true;
    return ESP_OK;
}






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



// (Det noget med man ikke kan lave 1real_fast tricket med 4 radix. Det nævner de i "Real-valued fast Fourier transform algorithms" og han bruger ikke radix 4 i eksemplet hvor han laver tricket)
// Skriv at det array man smider ind skal være N_SAMPLES størrelse af floats med __attribute__((aligned(16))) foran
// Skriv at N skal være power of 2 ved radix 2 (og power of 4 ved radix 4 tror jeg)
// Og at N skal være: (has to be under or equal the max fft length set in menuconfig)
// esp_err_t fft_wrapper_start_fft_1real_normal(float *gs_pf32BinLocation, ASD ET POINTER TIL OUTPUT MÅSKE)
// {

//     if(gs_init_called == false)
//     {
//         ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init() and fft_wrapper_generete_wind()" );
//         return ESP_FAIL;
//     }


//     return ESP_OK;
// }


// PROBLEM: DEN FUNKTOIN HER KRÆVER INPUT ARRAYED ER I FLOATS.. MEN DET ER I uint16_t... Det skal jeg lige finde ua af...
// Skriv at outputte er done in place og derfor har samme lokation som inputarrayed
// SKriv at pf32_data er et REAL input
esp_err_t fft_wrapper_start_fft_1real_fast(float *pf32_data, eFFT_radix_type_t s_radix)
{
    if(gs_init_called == false)
    {
        ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init() and fft_wrapper_generete_wind()" );
        return ESP_FAIL;
    }

    // Get location of data:
    // EEEEH DET KAN JEG DA IKKE DET HER...gs_af32_data er ikke bare en pointer, men et array i sig selv...
    // Tror faktiske bare det er mit DSP array der skal have den der attribyte til sig...

    switch(s_radix)
    {
        esp_err_t ret;

        case radix4:
            
            // SKAL JEG LAVE TJEK FOR AT N STØRELSEN GÅR OP HER? Nej det er nok spild af processerkræft at tjekke hver gang. 
            // Ved ikke om api funktoinen tjekker for det. 
            // ELlers kan jeg bare tilføje det som en kommentar til funktionen, at N skal gå på i 4, hvis man bruger raix 4
            // Har lige tjekket radix 4 funktionen... er ret sikker på den tjekker. Så jeg tilføjer bare en kommentar i min function

            // Format for fft api and multiply with window function:
            for(int32_t i = 0; i < i32_N; i++)
            {
                pf32_data[i] = pf32_data[i] * gs_af32_wind[i];
            }

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

            // Splitting operation:
            ret = dsps_cplx2real_fc32(pf32_data, i32_N>>1);
            if(ret != ESP_OK)
            {
                ESP_LOGE(TAG, "ERROR: Radix4 Splitting Operation failed = %i", ret);;
                return ret;
            }

            // Extract amplitude and convert to dB:
            conv_fft_res_to_ampl_db(pf32_data, i32_N>>1);
            break;

        case radix2:

            // Format for fft api and multiply with window function:
            for(int32_t i = 0; i < i32_N; i++)
            {
                pf32_data[i] = pf32_data[i] * gs_af32_wind[i];
            }

            // FFT Radix-2:
            dsps_fft2r_fc32(pf32_data, i32_N >> 1); // Half size FFT
            // Bit reverse:
            dsps_bit_rev2r_fc32(pf32_data, i32_N >> 1);
            // Splitting operation:
            dsps_cplx2real_fc32(pf32_data, i32_N>>1);
            // Extract amplitude and convert to dB:
            conv_fft_res_to_ampl_db(pf32_data, i32_N>>1);

            break;

        default:
            break;
    }
    
    return ESP_OK;
}


// !!!Tænk lige over hvad størrelsen skal være under init, når man bruger 2input fast!!!
// esp_err_t fft_wrapper_start_fft_2real_fast(float *gs_pf32BinLocation1, float *gs_pf32BinLocation2)
// {

//     if(gs_init_called == false)
//     {
//         ESP_LOGE(TAG, "Error: fft_wrapper_start_fft() called before fft_wrapper_init() and fft_wrapper_generete_wind()" );
//         return ESP_FAIL;
//     }

//     return ESP_OK;
// }

#ifndef COMMON_DEFINES_H_INCLUDED
#define COMMON_DEFINES_H_INCLUDED

#include "esp_adc/adc_continuous.h" // For ADC atten types

//Burde man måske køre radix 2? Så kan man have DATA_BLOCK_SIZE 4096 (måske)
//og så bare smid hver anden data blok væk, så esp'en har tid til at lave fft
// Opøsning:
// Ved radix4 (DATA_BLOCK_SIZE = 2048):
        // Hver bin index er  BW/(DATA_BLOCK_SIZE/2)  -->  (16000-125)/(2048/2) = 15.503... Hz
// Ved radix2 (DATA_BLOCK_SIZE = 4096):
        // Hver bin index er  (16000-125)/(2048/2) = 7.751... Hz


#define DATA_BLOCK_SIZE 8192        // Size of 1 block of samples in amount of elements
                                    /* Requirements for this value:

                                            Must not exeed CONFIG_DSP_MAX_FFT_SIZE (set as "Maximum FFT length" in menuconfig)

                                            For Radix2:  DATA_BLOCK_SIZE/2 = 2^r  where r is an integer 
                                            For Radix4:  DATA_BLOCK_SIZE/2 = 4^r  where r is an integer 
                                            (FFT of bin size half the data block size is used. This is why DATA_BLOCK_SIZE is divided by 2)

                                            Note: If this values is too big, the ESP will run out of DRAM 
                                            (see printed size summary or "ESP-ISF: Size analysis of the binaries" command after building)
                                    */ 
                                    
                                    // SKRIV I RAPPORTEN HVORFOR VI VALGTE 2048 (hvis den næste 4^r værdi bruges, 8192, ville esp'en løbe tør for DRAM 
                                    // (Fordi vi bruger så mange 32 bit float arrays af størrelsen DATA_BLOCK_SIZE)

                                    // SKRIV OGSÅ I RAPOORTEN AF DEN HER VÆRDI SKAL VÆRE OVER 600 (se min maple fil hvor jeg berenger det) FOR AT FÅ EN HEL PERIODE AF DEN LAVESTE FREKVENS MED
                                       

#define ADC_SAMPLING_FREQ   80 * 1000      // see soc caps file for max value (limited by the internal ADC's of the ESP32-S3) 
#define ADC_ATTEN_DB        ADC_ATTEN_DB_6


#endif /*COMMON_DEFINES_H_INCLUDED */

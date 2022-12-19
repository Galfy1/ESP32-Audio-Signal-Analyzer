#ifndef COMMON_DEFINES_H_INCLUDED
#define COMMON_DEFINES_H_INCLUDED

#include "esp_adc/adc_continuous.h" // For ADC atten types

#define DATA_BLOCK_SIZE 8192 // Size of 1 block of samples in amount of elements
                             /* Requirements for this value:
                             
                                     Must not exeed CONFIG_DSP_MAX_FFT_SIZE (set as "Maximum FFT length" in menuconfig)
                             
                                     For Radix2:  DATA_BLOCK_SIZE/2 = 2^r  where r is an integer
                                     For Radix4:  DATA_BLOCK_SIZE/2 = 4^r  where r is an integer
                                     (FFT of bin size half the data block size is used. This is why DATA_BLOCK_SIZE is divided by 2)
                             
                                     Note: If this values is too big, the ESP will run out of DRAM. This problem is lessened by the fact that most arrays utilizing DATA_BLOCK_SIZE is placed in PSRAM (not DRAM!)
                                     ( see printed size summary or "ESP-ISF: Size analysis of the binaries" command after building. Note that this size summary does not include
                                      the dynamically allocated memory that is allocated at runtime. The remaining internal memory (aka excluding PSRAM) after dynamic allocation
                                      are printed at the end of app_main() )
                             */

#define ADC_SAMPLING_FREQ 80 * 1000 // see soc caps file for max value (limited by the internal ADC's of the ESP32-S3)
#define ADC_ATTEN_DB      ADC_ATTEN_DB_6

#define ASA_SPECIFICATION_MIN_FREQ 125 // Specifcation for the smallest frequency in Hz that the Audio Signal Analyzer supports

#define GPIO_HIGH 1
#define GPIO_LOW  0

#endif /*COMMON_DEFINES_H_INCLUDED */

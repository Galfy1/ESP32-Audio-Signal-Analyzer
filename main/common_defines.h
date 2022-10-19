#ifndef COMMON_DEFINES_H_INCLUDED
#define COMMON_DEFINES_H_INCLUDED



#define DATA_BLOCK_SIZE 2048        // Size of 1 block of samples in amount of elements
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
                                       







#endif /*COMMON_DEFINES_H_INCLUDED */

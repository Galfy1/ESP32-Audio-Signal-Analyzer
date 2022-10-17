#ifndef COMMON_DEFINES_H_INCLUDED
#define COMMON_DEFINES_H_INCLUDED




#define RES_BUFFER_SIZE 4096        // VÆRDIEN SKAL NOK PASSER MED DEN FFT STØRELSE VI VÆLGER. DEN SKAL NOK VÆRE DEFINED ET ANDET STED
                                    // Størrelsen er antal 16 bit elementer
                                        // LIGE NU BRUGER JEG BARE 4096 TIL TEST
#define RES_BUFFER_SIZE_IN_BYTES (RES_BUFFER_SIZE*2)  // (Each element is 16 bit) // SLET HVIS VI IKKE FÅR BRUG FOR DEN 






#endif /*COMMON_DEFINES_H_INCLUDED */

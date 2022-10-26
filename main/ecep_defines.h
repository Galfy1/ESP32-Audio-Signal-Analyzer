

#ifndef ECEP_DEFINES_H_INCLUDED
#define ECEP_DEFINES_H_INCLUDED


#define ECEP_HEADER_SIZE        4  // In bytes
#define ECEP_MAX_PACKAGE_SIZE   ECEP_HEADER_SIZE + (4*UART_RD_BUF_SIZE)   // Max package size in bytes.
                                                                          // (chosen by biggest command or event. In this case: Left/Right FFT Result event)

                                                                          // HOV er det smart at have en define fra common_defines her inde??   
                                                                          // Og hvis R/L FFT result har en param struct, skal dens størrelse også lige tilføjes
#define ECEP_PACKET_TYPE 42




// typedef struct
// {
//     int blaVar1;
//     float blaVar2;
// } s_event1_t;

// typedef struct
// {
//     int blaVarr1;
//     float blaVarr2;
//     float blaVarr3;
// } s_event2_t;



// typedef union
// {
//     s_event1_t wee;
//     s_event2_t wee2;
// } ecep_events;


// // Structure for queues:
// typedef struct
// {
//     int Event  // Hvilket event det er (eventkoden) Så tasken ved hvordan den skal læse uEvents 

//     ecep_events Params;     

// } s_BlaBlaStruct_t;








#endif /*ECEP_DEFINES_H_INCLUDED */

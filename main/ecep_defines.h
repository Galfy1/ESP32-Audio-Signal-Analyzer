

#ifndef ECEP_DEFINES_H_INCLUDED
#define ECEP_DEFINES_H_INCLUDED


#define ECEP_HEADER_SIZE        4  // In bytes
#define ECEP_MAX_PACKAGE_SIZE   ECEP_HEADER_SIZE + (4*UART_RD_BUF_SIZE)   // Max package size in bytes.
                                                                          // (chosen by biggest command or event. In this case: Left/Right FFT Result event)

                                                                          // HOV er det smart at have en define fra common_defines her inde??   
                                                                          // Og hvis R/L FFT result har en param struct, skal dens størrelse også lige tilføjes
#define ECEP_PACKET_TYPE 42

// Command ID  
#define FETCH_FFT                   1
#define SET_FFT_OUTPUT_FORMAT       2
#define SET_FFT_WINDOW_TYPE         3
#define SET_FFT_RES_AVG_COUNT       4
#define FETCH_FFT_SETTING           5
#define FETCH_FFT_TOTAL_DURATION    6
#define FETCH_ADC_RAW_DATA          7
#define FETCH_CENFREQ_HZ            8
#define FETCH_CENFREQ_AMP           9
#define SET_MIC_RANGE               10
#define TRIG_PR_CLICK               11
#define TRIG_PR_HOLD                12
#define TRIG_PR_RELEASE             13
#define TRIG_PR_PRESS               14
#define SET_PR_DEFAULT_STATE        15
#define PING                        16




// parameter enums
typedef enum{
    right = 1,
    left = 2,
    both = 3,
} eAudio_channel_t;

typedef enum{
    amplitude_mV = 1, 
    amplitude_dB_SPL = 2, 
    power_mV = 3,
} eFFT_output_format_t;

typedef enum{
    hann = 1, 
    blackman = 2, 
    blackman_harris = 3, 
    blackman_nuttall = 4, 
    nuttall = 5, 
    flat_top = 6,
} eFFT_window_type_t;

typedef enum{
    dBSPL_90 = 1,
    dBSPL_120 = 2,
} eMIC_range_dBSPL_t;

typedef enum{
    norm_open = 1,
    norm_closed = 2,
} eRelay_state_t;


// Parameter structs
typedef struct{
    eAudio_channel_t channel;
    eMIC_range_dBSPL_t range;
} sMIC_range_t;



// Paramter union (write which commands the parameters are used for to the left of the parameter)
typedef union uni_parameters{
    eAudio_channel_t channel;                   
    eFFT_output_format_t fft_output_format;     
    eFFT_output_format_t fft_window;      
    uint32_t fft_res_avg_count;                 
    sMIC_range_t mic_range;         
    uint32_t time;                              
    eRelay_state_t relay_default;               
} uCMD_parameters_t;


// Command struct for internal task communication.
typedef struct{

    uint32_t cmd_id;
    uCMD_parameters_t param;

} sIntern_command_t;




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

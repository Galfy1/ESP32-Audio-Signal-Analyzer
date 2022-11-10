#ifndef ECEP_DEFINES_H_INCLUDED
#define ECEP_DEFINES_H_INCLUDED

#include "common_defines.h"

#define ECEP_HEADER_LEN       4                                       // Header length In bytes

#define ECEP_MAX_CMD_PACKET_LEN 1000                                        // Max packet length for an event packet in bytes. 
                                                                            /* (value is arbitrarily chosen. Is simply has to be at least the size of the largest command package.
                                                                               No command package is currently over 1000 bytes in size.) */
#define ECEP_MAX_EVNT_PACKET_LEN  ECEP_HEADER_LEN + (4*DATA_BLOCK_SIZE)   // Max packet length for an event packet in bytes.
                                                                           /* (chosen by biggest Event. In this case: Left/Right ADC Raw data event)
                                                                             (*4 because bulk raw adc data is stored as floats. The L/R ADC Raw data event has no param struct size to account for)  */
#define ECEP_PACKET_TYPE 42

// Command IDs  
#define ECEP_CMD_FETCH_FFT                  1
#define ECEP_CMD_SET_FFT_OUTP_FORM          2
#define ECEP_CMD_SET_FFT_WIND_TYPE          3
#define ECEP_CMD_SET_FFT_RES_AVG_CT         4
#define ECEP_CMD_SET_WIND_CORR_FACTOR       5
#define ECEP_CMD_FETCH_FFT_STGS             6
#define ECEP_CMD_FETCH_FFT_TOTAL_DUR        7
#define ECEP_CMD_FETCH_ADC_RAW_DATA         8
#define ECEP_CMD_FETCH_CEN_FREQ             9
#define ECEP_CMD_FETCH_CEN_FREQ_AMP         10
#define ECEP_CMD_SET_MIC_RANGE              11
#define ECEP_CMD_FETCH_MIC_RANGE            12
#define ECEP_CMD_TRIG_PR_CLICK              13
#define ECEP_CMD_TRIG_PR_HOLD               14
#define ECEP_CMD_TRIG_PR_RELEASE            15
#define ECEP_CMD_TRIG_PR_PRESS              16
#define ECEP_CMD_SET_PR_DEF_STATE           17
#define ECEP_CMD_FETCH_PR_DEF_STATE         18
#define PING                                19

// Event IDs
#define ECEP_EVNT_COMMAND_STATUS            1
#define ECEP_EVNT_LEFT_FFT_RES              2
#define ECEP_EVNT_RIGHT_FFT_RES             3
#define ECEP_EVNT_FFT_STGS                  4
#define ECEP_EVNT_FFT_TOT_DUR               5
#define ECEP_EVNT_LEFT_ADC_RAW              6
#define ECEP_EVNT_RIGHT_ADC_RAW             7
#define ECEP_EVNT_CEN_FREQ                  8
#define ECEP_EVNT_CEN_FREQ_AMP              9
#define ECEP_EVNT_PR_DEF_STATE              10
#define PONG                                11



// parameter enums (for both commands and events)
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
    none = 1,
    amplitude = 2,
    energy = 3,
} eFFT_corr_factor_t;

typedef enum{
    dBSPL_90 = 1,
    dBSPL_120 = 2,
} eMIC_range_dBSPL_t;

typedef enum{
    norm_open = 1,
    norm_closed = 2,
} eRelay_state_t;

typedef enum{
    ASA_SUCCESS = 1,
    ASA_failed = 2,
    ASA_errorcode1 = 3,
    ASA_errorcode2 = 4,     // Husk at ændre disse fejlkoder og tilføje flere efter behov!!!
} eCMD_status_t;


// Parameter structs (for both commands and events)

typedef struct{

    eFFT_output_format_t fft_output_format;
    eFFT_window_type_t fft_window;
    uint32_t fft_res_avg_count;
    eFFT_corr_factor_t fft_corr_factor;

} sFFT_stgs_t;


// Paramter union
typedef union uCommand_parameters{

    eAudio_channel_t channel;                   
    eFFT_output_format_t fft_output_format;     
    eFFT_output_format_t fft_window;      
    uint32_t fft_res_avg_count;
    eFFT_corr_factor_t fft_corr_factor;                 
    eMIC_range_dBSPL_t range;       
    uint32_t time;                              
    eRelay_state_t relay_default;  
                 
} uCMD_params_t;


typedef union uEvent_parameters{

    sFFT_stgs_t FFT_stgs;
    uint32_t time;
    uint32_t cen_freq_hz;
    uint32_t cen_freq_amp;
    eMIC_range_dBSPL_t range;   
    eRelay_state_t relay_default;  

} uEVNT_params_t;


// Command and event structs for internal task communication.
typedef struct{

    uint8_t id;
    uCMD_params_t param;

} sIntern_command_setup_t;

typedef struct{

    uint8_t id;
    eCMD_status_t status;
    uEVNT_params_t param;

} sIntern_event_setup_t;


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

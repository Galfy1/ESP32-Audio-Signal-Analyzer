

#ifndef ECEP_DEFINES_H_INCLUDED
#define ECEP_DEFINES_H_INCLUDED


#define ECEP_HEADER_SIZE        4  // In bytes
#define ECEP_MAX_PACKAGE_SIZE   ECEP_HEADER_SIZE + (4*UART_RD_BUF_SIZE)   // Max package size in bytes.
                                                                          // (chosen by biggest command or event. In this case: Left/Right FFT Result event)

                                                                          // HOV er det smart at have en define fra common_defines her inde??   
                                                                          // Og hvis R/L FFT result har en param struct, skal dens størrelse også lige tilføjes
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
#define ECEP_CMD_PING                       19

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
#define ECEP_EVNT_PONG                      11



// parameter enums (for both commands and events)
typedef enum{
    right = 1,
    left = 2,
    both = 3,
} eAudio_channel_t;

typedef enum{
    ampltitude_mV = 1,      // return result as amplitude in mV 
    ampltitude_dB_SPL = 2,  // return result as amplitude in dB SPL
    power_mV = 3,           // return result as power in mV 
} eFFT_output_form_t;

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
} eFFT_wind_corr_factor_t;

typedef enum {
    up_to_90dB_SPL,
    up_to_120dB_SPL,
} eFFT_hw_gain_stage_t;

typedef enum{
    norm_open = 1,
    norm_closed = 2,
} eRelay_state_t;

typedef enum{
    ECEP_SUCCESS = 1,
    ECEP_FAIL = 2,
    ECEP_ERR_INVALID_ARG = 3,
    ECEP_errorcode2 = 4,
    ECEP_errorcode3 = 5,     // Husk at ændre disse fejlkoder og tilføje flere efter behov!!!
} eCMD_status_t;


// Parameter structs (for both commands and events)

typedef struct{

    eFFT_output_form_t fft_output_format;
    eFFT_window_type_t fft_window;
    uint32_t fft_res_avg_count;
    eFFT_wind_corr_factor_t fft_corr_factor;

} sFFT_stgs_t;


// Paramter union
typedef union uCommand_parameters{

    eAudio_channel_t channel;                   
    eFFT_output_form_t fft_output_format;     
    eFFT_window_type_t fft_window;      
    uint32_t fft_res_avg_count;
    eFFT_wind_corr_factor_t fft_win_corr_factor;                 
    eFFT_hw_gain_stage_t range;       
    uint32_t time;                              
    eRelay_state_t relay_default;  
                 
} uCMD_params_t;


typedef union uEvent_parameters{

    eCMD_status_t status;
    sFFT_stgs_t FFT_stgs;
    uint32_t time;
    uint32_t cen_freq_hz;
    uint32_t cen_freq_amp;
    eFFT_hw_gain_stage_t range;   
    eRelay_state_t relay_default;  

} uEVNT_params_t;


// Command and event structs for internal task communication.
typedef struct{

    uint8_t id;
    uCMD_params_t param;

} sIntern_command_setup_t;

typedef struct{

    uint8_t id;
    uEVNT_params_t param;

} sIntern_event_setup_t;


#endif /*ECEP_DEFINES_H_INCLUDED */

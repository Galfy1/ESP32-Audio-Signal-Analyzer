#ifndef ECEP_DEFINES_H_INCLUDED
#define ECEP_DEFINES_H_INCLUDED

#include "common_defines.h"

// ECEP: External Command Event Protocol

// ######## ECEP HEADER ########
// | Packet Type | Command/event code | Packet Length (low byte) | Packet Length (high byte) |
//    (1 byte)         (1 byte)                 (1 byte)                    (1 byte)

// ######## ECEP PACKAGE ########
// |   Header   |   Cmd/Evt param struct   |   Bulk data   |
//    (4 bytes)           (differ)              (differ)

#define ECEP_HEADER_LEN 4 // Header length In bytes

#define ECEP_MAX_CMD_PACKET_LEN 1000                                                 // Max packet length for an event packet in bytes.
                                                                                     /* (value is arbitrarily chosen. Is simply has to be at least the size of the largest command package.
                                                                                        No command package is currently over 1000 bytes in size.) */
#define ECEP_MAX_EVNT_PACKET_LEN ECEP_HEADER_LEN + (sizeof(float) * DATA_BLOCK_SIZE) // Max packet length for an event packet in bytes.
                                                                                     // (chosen by biggest Event. In this case: Left/Right ADC Raw data event)

#define ECEP_PACKET_TYPE 0x42

#define TX_PACKET_TYPE_BYTE          0
#define TX_PACKET_CMD_EVNT_BYTE      1
#define TX_PACKET_PACK_LEN_LOW_BYTE  2
#define TX_PACKET_PACK_LEN_HIGH_BYTE 3

// ---------------------------- Command/Event IDs  ----------------------------

// Command IDs
#define ECEP_CMD_FETCH_FFT            0x01
#define ECEP_CMD_SET_FFT_OUTP_FORM    0x02
#define ECEP_CMD_SET_FFT_WIND_TYPE    0x03
#define ECEP_CMD_SET_FFT_RES_AVG_CT   0x04
#define ECEP_CMD_SET_WIND_CORR_FACTOR 0x05
#define ECEP_CMD_FETCH_FFT_STGS       0x06
#define ECEP_CMD_FETCH_FFT_TOTAL_DUR  0x07
#define ECEP_CMD_FETCH_ADC_RAW_DATA   0x08
#define ECEP_CMD_FETCH_CEN_FREQ       0x09
#define ECEP_CMD_FETCH_CEN_FREQ_AMP   0x0A
#define ECEP_CMD_SET_MIC_RANGE        0x0B
#define ECEP_CMD_FETCH_MIC_RANGE      0x0C
#define ECEP_CMD_TRIG_PR_CLICK        0x0D
#define ECEP_CMD_TRIG_PR_HOLD         0x0E
#define ECEP_CMD_TRIG_PR_RELEASE      0x0F
#define ECEP_CMD_TRIG_PR_PRESS        0x10
#define ECEP_CMD_SET_PR_DEF_STATE     0x11
#define ECEP_CMD_FETCH_PR_DEF_STATE   0x12
#define ECEP_CMD_PING                 0x13

// Event IDs
#define ECEP_EVNT_COMMAND_STATUS     0x01
#define ECEP_EVNT_LEFT_FFT_RES       0x02
#define ECEP_EVNT_RIGHT_FFT_RES      0x03
#define ECEP_EVNT_FFT_STGS           0x04
#define ECEP_EVNT_FFT_TOT_DUR        0x05
#define ECEP_EVNT_LEFT_ADC_RAW       0x06
#define ECEP_EVNT_RIGHT_ADC_RAW      0x07
#define ECEP_EVNT_LEFT_CEN_FREQ      0x08
#define ECEP_EVNT_RIGHT_CEN_FREQ     0x09
#define ECEP_EVNT_LEFT_CEN_FREQ_AMP  0x0A
#define ECEP_EVNT_RIGHT_CEN_FREQ_AMP 0x0B
#define ECEP_EVNT_MIC_RANGE          0x0C
#define ECEP_EVNT_PR_DEF_STATE       0x0D
#define ECEP_EVNT_PONG               0x0E

// ------------------------- Params (enums and structs)  -------------------------

// DE SKAL MÅSKE HAVE ECEP PREFIX FORAN SIG:

// Parameter enums (for both commands and events)
typedef enum
{
    left  = 1,
    right = 2,
    both  = 3,
} eAudio_channel_t;

typedef enum
{
    ampltitude_mV     = 1, // return result as amplitude in mV
    ampltitude_dB_SPL = 2, // return result as amplitude in dB SPL
    power_mV          = 3, // return result as power in mV
} eFFT_output_form_t;

typedef enum
{
    hann             = 1,
    blackman         = 2,
    blackman_harris  = 3,
    blackman_nuttall = 4,
    nuttall          = 5,
    flat_top         = 6,
} eFFT_window_type_t;

typedef enum
{
    none      = 1,
    amplitude = 2,
    energy    = 3,
} eFFT_wind_corr_factor_t;

typedef enum
{
    up_to_90dB_SPL  = 1,
    up_to_120dB_SPL = 2,
} eFFT_hw_gain_stage_t; // Skal måske ikke hede FFT_ foran, hvis den ikke er en del af FFT settings.

typedef enum
{
    norm_open   = 1,
    norm_closed = 2,
} eRelay_state_t;

typedef enum
{
    ECEP_SUCCESS           = 1,
    ECEP_FAIL              = 2,
    ECEP_ERR_INVALID_PARAM = 3,
    ECEP_ERR_INVALID_CMD   = 4,
    // ECEP_ERR_INVALID_PACKET_LEN = 4,  // Kan ikke bruges (da vi resetter for at prøve at fixe problemet) aka bare slet
    ECEP_ERR_UNKNOWN = 5,
} eCMD_status_t;

// Parameter structs (for both commands and events):
typedef struct
{

    eFFT_output_form_t      fft_output_format;
    eFFT_window_type_t      fft_window;
    uint32_t                u32_fft_res_avg_count;
    eFFT_wind_corr_factor_t fft_corr_factor;

} sFFT_stgs_t;

typedef struct
{

    uint8_t       u8_parent_cmd_id;
    eCMD_status_t status;

} sStatus_event_t;

// ---------------- Unions/Structs used for Internal task communication ----------------

typedef union uCommand_parameters
{
    eAudio_channel_t        channel;
    eFFT_output_form_t      fft_output_format;
    eFFT_window_type_t      fft_window;
    uint32_t                u32_fft_res_avg_count;
    eFFT_wind_corr_factor_t fft_win_corr_factor;
    eFFT_hw_gain_stage_t    range;    // Måske hedde mic_range i stedet?
    uint32_t                u32_time; // Skal måske hedde fft duration eller noget lignene, så man kan se den hører til ECEP_EVNT_FFT_TOT_DUR. Eller bruges den til andre ting også? svar: ja det gør den vidst
    eRelay_state_t          relay_state;
} uCMD_params_t;

typedef union uEvent_parameters
{
    sStatus_event_t      status_event; // Burde den ikke hedde command status? Det er vel underforstået at det er et event siden den ligger i en EVNT_param union. (også så den matcher event ID navnet)
    sFFT_stgs_t          FFT_stgs;
    uint32_t             u32_time; // Time in ms // Skal måske hedde fft duration eller noget lignene, så man kan se den hører til ECEP_EVNT_FFT_TOT_DUR. Eller bruges den til andre ting også?
    uint32_t             u32_cen_freq_hz;
    float                f32_cen_freq_amp;
    eFFT_hw_gain_stage_t range; // Måske hedde mic_range i stedet?
    eRelay_state_t       pr_default;
} uEVNT_params_t;

// Command/Event structs used as elements for the command/event queues:
typedef struct
{

    uint8_t       u8_id;
    uCMD_params_t param;

} sIntern_command_setup_t;

typedef struct
{

    uint8_t        u8_id;
    uEVNT_params_t param;

} sIntern_event_setup_t;

#endif /*ECEP_DEFINES_H_INCLUDED */

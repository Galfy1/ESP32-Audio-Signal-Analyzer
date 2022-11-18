#ifndef ECEP_UART_H_INCLUDED
#define ECEP_UART_H_INCLUDED

#include <stdio.h>

#include "ecep_defines.h"

//ECEP --> External Command Event Protocol




void ecep_packet_handler_task();
void ecep_uart_event_task();

void ecep_uart_init(QueueHandle_t xQueue_pr_ecep_cmds, QueueHandle_t xQueue_dsp_ecep_cmds);




// ----------------------- Convenience functions for accessing the ecep event queue -----------------------

void send_ecep_evnt_cmd_status(eCMD_status_t status_type, uint8_t u8_cmd_id);
void send_ecep_evnt_pr_def_state(eRelay_state_t default_relay_state);
void send_ecep_evnt_fft_stgs(eFFT_output_form_t fft_output_format, eFFT_window_type_t fft_window, 
                                   uint32_t u32_fft_res_avg_count, eFFT_wind_corr_factor_t fft_corr_factor);
void send_ecep_evnt_fft_tot_dur(uint32_t u32_fft_total_dur_ms);
void send_ecep_evnt_L_cen_freq(uint32_t u32_center_freq_hz);
void send_ecep_evnt_R_cen_freq(uint32_t u32_center_freq_hz);
void send_ecep_evnt_L_cen_freq_amp(float f32_center_freq_amp);
void send_ecep_evnt_R_cen_freq_amp(float f32_center_freq_amp);
void send_ecep_evnt_mic_range(eFFT_hw_gain_stage_t hw_gain_stage);
void send_ecep_evnt_L_fft_res();
void send_ecep_evnt_R_fft_res();
void send_ecep_evnt_L_adc_raw();
void send_ecep_evnt_R_adc_raw();
void send_ecep_evnt_pong();

#endif /*ECEP_UART_H_INCLUDED */

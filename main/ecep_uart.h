#ifndef ECEP_UART_H_INCLUDED
#define ECEP_UART_H_INCLUDED

#include <stdio.h>

#include "ecep_defines.h"

//ECEP --> External Command Event Protocol




void ecep_packet_handler_task();
void ecep_uart_event_task();
void ecep_uart_tx_task();

void ecep_uart_init(float * pf32_adc1_shared, 
                    float * pf32_adc2_shared, 
                    SemaphoreHandle_t adc1_shared_mem_sem, 
                    SemaphoreHandle_t adc2_shared_mem_sem, 
                    float * pf32_L_fft_res_shr, 
                    float * pf32_R_fft_res_shr,
                    SemaphoreHandle_t fft_L_res_shr_mem_sem,
                    SemaphoreHandle_t fft_R_res_shr_mem_sem);




// ----------------------- Convenience functions for accessing the ecep event queue -----------------------

void send_ecep_evnt_cmd_status(eCMD_status_t status_type, uint8_t u8_cmd_id);
void send_ecep_evnt_pr_def_state(eRelay_state_t default_relay_state);


#endif /*ECEP_UART_H_INCLUDED */

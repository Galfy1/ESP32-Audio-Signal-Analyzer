#ifndef ECEP_UART_H_INCLUDED
#define ECEP_UART_H_INCLUDED

#include <stdio.h>

#include "ecep_defines.h"

//ECEP --> External Command Event Protocol




void ecep_packet_handler_task();
void ecep_uart_event_task();

void ecep_uart_init();




// ----------------------- Convenience functions for accessing the ecep event queue -----------------------

void send_ecep_evnt_cmd_status(eCMD_status_t status_type, uint8_t u8_cmd_id);
void send_ecep_evnt_pr_def_state(eRelay_state_t default_relay_state);


#endif /*ECEP_UART_H_INCLUDED */

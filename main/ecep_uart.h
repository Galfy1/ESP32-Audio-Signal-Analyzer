#ifndef ECEP_UART_H_INCLUDED
#define ECEP_UART_H_INCLUDED

#include <stdio.h>

//ECEP --> External Command Event Protocol




void ecep_packet_handler_task();
void ecep_uart_event_task();

void ecep_uart_init();




#endif /*ECEP_UART_H_INCLUDED */

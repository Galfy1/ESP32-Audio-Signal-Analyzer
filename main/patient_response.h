#ifndef PATIENT_RESPONSE_H_INCLUDED
#define PATIENT_RESPONSE_H_INCLUDED

#include <stdint.h>

void dummy_task();

void patient_resp_task();
void patient_resp_init(QueueHandle_t xQueue_pr_cmd);


#endif /*PATIENT_RESPONSE_H_INCLUDED */
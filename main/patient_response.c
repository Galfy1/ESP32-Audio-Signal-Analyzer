#include <stdint.h>
#include <string.h> // For memcpy() and memset()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "patient_response.h"
#include "common_defines.h"


static const char *PATIENT_RESP_TAG = "patient_response.c";


void patient_resp_task()
{
    ESP_LOGI(PATIENT_RESP_TAG, "Det virker...");

    while(1)
    {
        vTaskDelay(500);
    }

}


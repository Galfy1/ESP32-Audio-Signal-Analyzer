#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_adc/adc_continuous.h" // For testing

static const char *TAG = "TeestLog";

void app_main(void)
{
    ESP_LOGI(TAG, "DET VIRKER :D");
    ESP_LOGI(TAG, "SIZE OF OUTPUT DATA STRUCT: %d", sizeof(adc_digi_output_data_t));
}

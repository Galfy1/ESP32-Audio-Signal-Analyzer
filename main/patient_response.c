#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "patient_response.h"
#include "common_defines.h"

static const char *PATIENT_RESP_TAG = "patient_response.c";

#define CLICK_DELAY 20 / portTICK_PERIOD_MS // Delay in ms
#define GPIO_HIGH 1
#define GPIO_LOW 0
#define GPIO_PR_PIN GPIO_NUM_6  // 

static uint8_t gs_u8_default_gpio_level = GPIO_LOW;

static QueueHandle_t gs_xQueue_pr_cmd;


static void patient_resp_hold()
{   
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)  // Can toggle GPIO if GPIO equal default value.
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));  // Toggle gpio 
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Patient response button is already active.");
    }
}

static void patient_resp_release()
{   
    if (gpio_get_level(GPIO_PR_PIN) != gs_u8_default_gpio_level) // Can toggle GPIO if GPIO do not equal default value.
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN)); // Toggle gpio 
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Patient response button is already inactive.");
    }
}

static void patient_resp_press(int press_time)
{
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)
    {
        if (press_time > 0)
        {
            gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
            vTaskDelay(press_time / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));         
        }
        else
        {
            ESP_LOGE(PATIENT_RESP_TAG, "Press time cannot be less than 1ms.");
        }
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to perform press. Button is already active.");
    } 
}

static void patient_resp_click()
{   
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
        vTaskDelay(CLICK_DELAY);
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to perform click. Button is already active.");
    }
}

static void patient_resp_cdl() // Change default gpio level (cdl). If high --> low, if low --> high.
{   
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
        gs_u8_default_gpio_level = !gs_u8_default_gpio_level;
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to change default state as long as button is in use.");
    }
    
}

void patient_resp_task()
{

    // Initialize the GPIO 6 config structure.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1<<GPIO_PR_PIN), // GPIO pin 6.
        .intr_type = GPIO_INTR_DISABLE, // disable interrupts.
        .mode = GPIO_MODE_INPUT_OUTPUT, // Set as output/input mode.
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull up.
        .pull_up_en = GPIO_PULLUP_ENABLE, // Disable pull down.
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(GPIO_PR_PIN, gs_u8_default_gpio_level);
    ESP_LOGI(PATIENT_RESP_TAG, "default gpio level: %i", (int) gs_u8_default_gpio_level);

    while(1)
    {  
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    }

}


void patient_resp_init(QueueHandle_t xQueue_pr_cmd)
{
    gs_xQueue_pr_cmd = xQueue_pr_cmd;
}

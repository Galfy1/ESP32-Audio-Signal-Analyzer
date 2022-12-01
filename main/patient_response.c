#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "common_defines.h"
#include "ecep_defines.h"
#include "ecep_uart.h" // For convenience functions used to access the ecep event queue
#include "patient_response.h"

static const char *PATIENT_RESP_TAG = "patient_response.c";

#define CLICK_DELAY 20 / portTICK_PERIOD_MS // Delay in ms
#define GPIO_PR_PIN GPIO_NUM_6

#define MAX_PRESS_DUR 120000 // No more than 2^32 (large values not recommended)
#define MIN_PRESS_DUR 1      // No less than 1

static uint8_t gs_u8_default_gpio_level = GPIO_LOW;

static QueueHandle_t gs_xQueue_pr_ecep_cmds;

static void patient_resp_hold()
{
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level) // Can toggle GPIO if GPIO equal default value.
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN)); // Toggle gpio

        ESP_LOGI(PATIENT_RESP_TAG, "Butten's being hold!");
        send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_HOLD);
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Patient response button is already active.");
        send_ecep_evnt_cmd_status(ECEP_FAIL, ECEP_CMD_TRIG_PR_HOLD);
    }
}

static void patient_resp_release()
{
    if (gpio_get_level(GPIO_PR_PIN) != gs_u8_default_gpio_level) // Can toggle GPIO if GPIO do not equal default value.
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN)); // Toggle gpio

        ESP_LOGI(PATIENT_RESP_TAG, "Butten released!");
        send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_RELEASE);
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Patient response button is already inactive.");
        send_ecep_evnt_cmd_status(ECEP_FAIL, ECEP_CMD_TRIG_PR_RELEASE);
    }
}

static void patient_resp_press(uint32_t press_time)
{
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)
    {
        if ((press_time >= MIN_PRESS_DUR) && (press_time <= MAX_PRESS_DUR))
        {
            ESP_LOGI(PATIENT_RESP_TAG, "Butten's being pressed for %i ms", (int)press_time);

            gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
            vTaskDelay((int)press_time / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));

            ESP_LOGI(PATIENT_RESP_TAG, "Butten released");
            send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_PRESS);
        }
        else
        {
            ESP_LOGE(PATIENT_RESP_TAG, "Press time cannot be less than %ims or more than %ims", (int)MIN_PRESS_DUR, (int)MAX_PRESS_DUR);
            send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_PARAM, ECEP_CMD_TRIG_PR_PRESS);
        }
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to perform press. Button is already active.");
        send_ecep_evnt_cmd_status(ECEP_FAIL, ECEP_CMD_TRIG_PR_PRESS);
    }
}

static void patient_resp_click()
{
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
        vTaskDelay(CLICK_DELAY);
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));

        ESP_LOGI(PATIENT_RESP_TAG, "Butten clicked");
        send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_CLICK);
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to perform click. Button is already active.");
        send_ecep_evnt_cmd_status(ECEP_FAIL, ECEP_CMD_TRIG_PR_CLICK);
    }
}

static void patient_resp_cdl(eRelay_state_t relay_state) // Set default gpio level (cdl)
{
    if ((gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)) // The button is not being pressed atm
    {
        switch (relay_state)
        {
        case norm_open:
            gs_u8_default_gpio_level = GPIO_LOW;
            gpio_set_level(GPIO_PR_PIN, gs_u8_default_gpio_level);
            send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_SET_PR_DEF_STATE);
            break;

        case norm_closed:
            gs_u8_default_gpio_level = GPIO_HIGH;
            gpio_set_level(GPIO_PR_PIN, gs_u8_default_gpio_level);
            send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_SET_PR_DEF_STATE);
            break;

        default:
            send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_PARAM, ECEP_CMD_SET_PR_DEF_STATE);
            break;
        }
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to change default state as long as button is in use.");
        send_ecep_evnt_cmd_status(ECEP_FAIL, ECEP_CMD_SET_PR_DEF_STATE);
    }
}

static void patient_resp_fetch_def_state()
{
    ESP_LOGI(PATIENT_RESP_TAG, "default gpio level: %i", (int)gs_u8_default_gpio_level);

    sIntern_event_setup_t pr_def_state_event;

    if (gs_u8_default_gpio_level == GPIO_HIGH)
    {
        send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_FETCH_PR_DEF_STATE);
        send_ecep_evnt_pr_def_state(norm_closed);
    }
    else if (gs_u8_default_gpio_level == GPIO_LOW)
    {
        send_ecep_evnt_cmd_status(ECEP_SUCCESS, ECEP_CMD_FETCH_PR_DEF_STATE);
        send_ecep_evnt_pr_def_state(norm_open);
    }
    else
    {
        send_ecep_evnt_cmd_status(ECEP_ERR_UNKNOWN, ECEP_CMD_FETCH_PR_DEF_STATE);
    }
}

void patient_resp_task()
{

    sIntern_command_setup_t user_command;

    // Initialize the GPIO 6 config structure.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1 << GPIO_PR_PIN),     // GPIO pin 6.
        .intr_type    = GPIO_INTR_DISABLE,      // disable interrupts.
        .mode         = GPIO_MODE_INPUT_OUTPUT, // Set as output/input mode.
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Disable pull up.
        .pull_up_en   = GPIO_PULLUP_ENABLE,     // Disable pull down.
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(GPIO_PR_PIN, gs_u8_default_gpio_level);

    while (1)
    {
        if (xQueueReceive(gs_xQueue_pr_ecep_cmds, &user_command, 0) == pdTRUE)
        {
            switch (user_command.u8_id)
            {
            case ECEP_CMD_TRIG_PR_CLICK:
                patient_resp_click();
                break;

            case ECEP_CMD_TRIG_PR_HOLD:
                patient_resp_hold();
                break;

            case ECEP_CMD_TRIG_PR_RELEASE:
                patient_resp_release();
                break;

            case ECEP_CMD_TRIG_PR_PRESS:
                patient_resp_press(user_command.param.u32_time);
                break;

            case ECEP_CMD_SET_PR_DEF_STATE:
                patient_resp_cdl(user_command.param.relay_state);
                break;

            case ECEP_CMD_FETCH_PR_DEF_STATE:
                patient_resp_fetch_def_state();
                break;

            default:
                send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_CMD, user_command.u8_id);
                ESP_LOGE(PATIENT_RESP_TAG, "Invalid command id %i passed to patient response task", (int)user_command.u8_id);

                break;
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void patient_resp_init(QueueHandle_t xQueue_pr_ecep_cmds)
{
    gs_xQueue_pr_ecep_cmds = xQueue_pr_ecep_cmds;
}
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "patient_response.h"
#include "common_defines.h"
#include "ecep_defines.h"

static const char *PATIENT_RESP_TAG = "patient_response.c";

#define CLICK_DELAY 20 / portTICK_PERIOD_MS // Delay in ms
#define GPIO_PR_PIN GPIO_NUM_6  // 

#define MAX_PRESS_DUR 120000    // No more than 2^32 (large values not recommended)
#define MIN_PRESS_DUR 1         // No less than 1

static uint8_t gs_u8_default_gpio_level = GPIO_LOW;

static QueueHandle_t gs_xQueue_pr_cmd;
static QueueHandle_t gs_xQueue_event;

static void send_status_event(eCMD_status_t status_type, uint8_t u8_cmd_id)
{
    sIntern_event_setup_t event;

    event.u8_id = ECEP_EVNT_COMMAND_STATUS;
    event.param.status_event.u8_parent_cmd_id = u8_cmd_id;
    event.param.status_event.status = status_type;

    xQueueSend(gs_xQueue_event, &event, 0);
}

static void patient_resp_hold()
{   

    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)  // Can toggle GPIO if GPIO equal default value.
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));  // Toggle gpio 
        
        ESP_LOGI(PATIENT_RESP_TAG, "Butten's being hold!");
        send_status_event(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_HOLD);
    }
    else
    {   
        ESP_LOGE(PATIENT_RESP_TAG, "Patient response button is already active.");
        send_status_event(ECEP_FAIL, ECEP_CMD_TRIG_PR_HOLD);
    }
}

static void patient_resp_release()
{   

    if (gpio_get_level(GPIO_PR_PIN) != gs_u8_default_gpio_level) // Can toggle GPIO if GPIO do not equal default value.
    {
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN)); // Toggle gpio 

        ESP_LOGI(PATIENT_RESP_TAG, "Butten released!");
        send_status_event(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_RELEASE);
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Patient response button is already inactive.");
        send_status_event(ECEP_FAIL, ECEP_CMD_TRIG_PR_RELEASE);
    }
}

static void patient_resp_press(uint32_t press_time)
{

    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)
    {
        if ((press_time >= MIN_PRESS_DUR) && (press_time <= MAX_PRESS_DUR)) 
        {
            ESP_LOGI(PATIENT_RESP_TAG, "Butten's being pressed for %i ms", (int) press_time);

            gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
            vTaskDelay( (int) press_time / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));    

            ESP_LOGI(PATIENT_RESP_TAG, "Butten released");
            send_status_event(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_PRESS);


        }
        else
        {
            ESP_LOGE(PATIENT_RESP_TAG, "Press time cannot be less than %ims or more than %ims", (int) MIN_PRESS_DUR, (int) MAX_PRESS_DUR);
            send_status_event(ECEP_ERR_INVALID_ARG, ECEP_CMD_TRIG_PR_PRESS);

        }
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to perform press. Button is already active.");
        send_status_event(ECEP_FAIL, ECEP_CMD_TRIG_PR_PRESS);
        
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
        send_status_event(ECEP_SUCCESS, ECEP_CMD_TRIG_PR_CLICK);
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to perform click. Button is already active.");
        send_status_event(ECEP_FAIL, ECEP_CMD_TRIG_PR_CLICK);
    }
}

static void patient_resp_cdl() // Change default gpio level (cdl). If high --> low, if low --> high.
{   
    if (gpio_get_level(GPIO_PR_PIN) == gs_u8_default_gpio_level)
    {   
        gpio_set_level(GPIO_PR_PIN, !gpio_get_level(GPIO_PR_PIN));
        gs_u8_default_gpio_level = !gs_u8_default_gpio_level;

        ESP_LOGI(PATIENT_RESP_TAG, "Default level of button gpio changed to %i", gpio_get_level(GPIO_PR_PIN));
        send_status_event(ECEP_SUCCESS, ECEP_CMD_TOG_PR_DEF_STATE);
    }
    else
    {
        ESP_LOGE(PATIENT_RESP_TAG, "Unable to change default state as long as button is in use.");
        send_status_event(ECEP_FAIL, ECEP_CMD_TOG_PR_DEF_STATE);
    }
    
}

static void patient_resp_fetch_def_state()
{   
    ESP_LOGI(PATIENT_RESP_TAG, "default gpio level: %i", (int) gs_u8_default_gpio_level);

    sIntern_event_setup_t pr_def_state_event;

    if(gs_u8_default_gpio_level == GPIO_HIGH)
    {
        pr_def_state_event.u8_id = ECEP_EVNT_PR_DEF_STATE;
        pr_def_state_event.param.pr_default = norm_closed;
        send_status_event(ECEP_SUCCESS, ECEP_CMD_FETCH_PR_DEF_STATE);
        xQueueSend(gs_xQueue_event, &pr_def_state_event, 0);
    }
    else if (gs_u8_default_gpio_level == GPIO_LOW)
    {
        pr_def_state_event.u8_id = ECEP_EVNT_PR_DEF_STATE;
        pr_def_state_event.param.pr_default = norm_open;
        send_status_event(ECEP_SUCCESS, ECEP_CMD_FETCH_PR_DEF_STATE);
        xQueueSend(gs_xQueue_event, &pr_def_state_event, 0);
    }
    else
    {   
        send_status_event(ECEP_ERR_UNKNOWN, ECEP_CMD_FETCH_PR_DEF_STATE);
    }
}

void patient_resp_task()
{
    
    sIntern_command_setup_t user_command;

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

    while(1)
    {
        if( xQueueReceive(gs_xQueue_pr_cmd, &user_command, 0) == pdTRUE)
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

            case ECEP_CMD_TOG_PR_DEF_STATE:
                patient_resp_cdl();
                break;
            
            case ECEP_CMD_FETCH_PR_DEF_STATE:
                patient_resp_fetch_def_state();
                break;
                
            default:
                send_status_event(ECEP_ERR_IVALID_CMD, user_command.u8_id);
                ESP_LOGE(PATIENT_RESP_TAG, "Invalid command id %i passed to patient response task", (int) user_command.u8_id);
                
                break;
            }

            
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}


void patient_resp_init(QueueHandle_t xQueue_pr_cmd, QueueHandle_t xQueue_event)
{
    gs_xQueue_pr_cmd = xQueue_pr_cmd;
    gs_xQueue_event = xQueue_event;
}

// static const char *DUMMY_TASK = "Dummy_task";

// void dummy_task() // Slettes når færdig!!!
// {   
//     sIntern_command_setup_t cmd_send;
//     sIntern_event_setup_t esp_event;

//     // TRIG_PR_CLICK
//     // TRIG_PR_HOLD
//     // TRIG_PR_RELEASE
//     // TRIG_PR_PRESS
//     // SET_PR_DEFAULT_STATE

//     // cmd_send.param.time = 5000;
    
//     vTaskDelay(4000 / portTICK_PERIOD_MS);

//     cmd_send.u8_id = ECEP_CMD_TOG_PR_DEF_STATE;
//     xQueueSend(gs_xQueue_pr_cmd, &cmd_send, 0);
//     cmd_send.u8_id = ECEP_CMD_FETCH_PR_DEF_STATE;
//     xQueueSend(gs_xQueue_pr_cmd, &cmd_send, 0);
//     cmd_send.u8_id = ECEP_CMD_TRIG_PR_HOLD;
//     xQueueSend(gs_xQueue_pr_cmd, &cmd_send, 0);
//     cmd_send.u8_id = ECEP_CMD_TOG_PR_DEF_STATE;
//     xQueueSend(gs_xQueue_pr_cmd, &cmd_send, 0);
//     cmd_send.u8_id = ECEP_CMD_FETCH_PR_DEF_STATE;
//     xQueueSend(gs_xQueue_pr_cmd, &cmd_send, 0);
//     cmd_send.u8_id = ECEP_CMD_TRIG_PR_RELEASE;
//     xQueueSend(gs_xQueue_pr_cmd, &cmd_send, 0);

//     while(1)
//     {
//         if( xQueueReceive(gs_xQueue_event, &esp_event, 0) == pdTRUE)
//         {
//             if (esp_event.u8_id == ECEP_EVNT_PR_DEF_STATE){
//                 if(esp_event.param.pr_default == norm_closed){
//                     ESP_LOGI(DUMMY_TASK, "Command id: %i - norm_closed", esp_event.u8_id);
//                 }
//                 else if(esp_event.param.pr_default == norm_open){
//                     ESP_LOGI(DUMMY_TASK, "Command id: %i - norm_open", esp_event.u8_id);
//                 }
//             }
//             else if(esp_event.u8_id == ECEP_EVNT_COMMAND_STATUS){
//                 if(esp_event.param.status_event.status == ECEP_SUCCESS){
//                     ESP_LOGI(DUMMY_TASK, "Command id: %i - ECEP_SUCCESS", esp_event.u8_id);
//                 }
//                 else if(esp_event.param.status_event.status == ECEP_FAIL){
//                     ESP_LOGI(DUMMY_TASK, "Command id: %i - ECEP_FAIL", esp_event.u8_id);
//                 }
//                 else if(esp_event.param.status_event.status == ECEP_ERR_INVALID_ARG){
//                     ESP_LOGI(DUMMY_TASK, "Command id: %i - ECEP_ERR_INVALID_ARG", esp_event.u8_id);
//                 }
//                 else if(esp_event.param.status_event.status == ECEP_ERR_UNKNOWN){
//                     ESP_LOGI(DUMMY_TASK, "Command id: %i - ECEP_ERR_UNKNOWN", esp_event.u8_id);
//                 }
//             }
//         }
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
// }
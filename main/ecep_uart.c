#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_log.h"

#include "common_defines.h"
#include "ecep_defines.h"
#include "ecep_uart.h"

static const char *TAG = "ecep_uart.c";

// (Debug console is moved from UART0 to UART1 in menuconfig)

#define ECEP_UART_NUM      UART_NUM_0               // Uart 0: The uart routed to the uart<->usb chip on the ESP32-S3 dev-kit-c (aka the "uart" usb port)
#define ECEP_UART_CFG_BAUD 921600                   // Lave test til at se hvor hurtig vi kan slippe afsted med uden stor fejl
#define ECEP_UART_CFG_FC   UART_HW_FLOWCTRL_DISABLE // Flow control (usb<->uart chip on esp32-s3 devkit-c is not hooked up to CTS)
#define ECEP_TXD_PIN       43                       // (GPIO --> UART0 TX. aka uart-usb-port on devkit-c)
#define ECEP_RXD_PIN       44                       // (GPIO --> UART0 TX. aka uart-usb-port on devkit-c)
#define ECEP_RTS_PIN       UART_PIN_NO_CHANGE       // Not used if HW flowcontrol is disabled
#define ECEP_CTS_PIN       UART_PIN_NO_CHANGE       // Not used if HW flowcontrol is disabled

#define BUF_SIZE         1024 // Ved ikke om det bliver et problem hvis den her er under størrelsen på en command pakke? Måske ikke Finder vi måske aldrig ud af, da vi ikke har en command pakke over 1024 i størrelse anyway.
#define UART_RD_BUF_SIZE BUF_SIZE

static QueueHandle_t ecep_uart_queue;   // Queue for uart api
static QueueHandle_t ecep_data_len_que; // Queue used to syncronize ecep_rx_packet_handler_task() from ecep_uart_event_task()

static QueueHandle_t gs_xQueue_ecep_events;   // Queue for ECEP events
static QueueHandle_t gs_xQueue_pr_ecep_cmds;  // Queue for Patient Response ECEP commands
static QueueHandle_t gs_xQueue_dsp_ecep_cmds; // Queue for DSP ECEP commands

static uint8_t           gs_au8_raw_uart_shr[UART_RD_BUF_SIZE]; // Raw uart read data (shared memory)
static SemaphoreHandle_t gs_raw_uart_shr_mem_sem;

static float            *gs_pf32_adc1_shared;
static float            *gs_pf32_adc2_shared;
static SemaphoreHandle_t gs_adc1_shared_mem_sem;
static SemaphoreHandle_t gs_adc2_shared_mem_sem;

static float            *gs_pf32_L_fft_res_shr;
static float            *gs_pf32_R_fft_res_shr;
static SemaphoreHandle_t gs_fft_L_res_shr_mem_sem;
static SemaphoreHandle_t gs_fft_R_res_shr_mem_sem;

static void ecep_command_handler(uint8_t u8_command_code, uint8_t *pv_params_and_bulk, uint16_t u16_params_and_bulk_len)
{

    // for(int32_t i = 0; i < u16_params_and_bulk_len ; i++)
    // {
    //     ESP_LOGI(TAG, "0x%04x", pv_params_and_bulk[i]);
    // }

    /* Note : No command currently uses bulk data
                (if bulk data for a command is needed in the future, add a Queue or Mutex protetected Shared Memory to pass to the relevenat task.
                for now: no need to have allocated shamed memory for bulk data if no command uses it.
                ECEP_MAX_CMD_PACKET_LEN would also have to be increase if the new packet with bulk exceeds this size. consider creating the u8_packet_data array
                in PSRAM if the size gets too big)
    */

    sIntern_command_setup_t sIntern_command_setup;

    switch (u8_command_code)
    {
    case ECEP_CMD_FETCH_FFT:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id         = u8_command_code;
        sIntern_command_setup.param.channel = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_SET_FFT_OUTP_FORM:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id                   = u8_command_code;
        sIntern_command_setup.param.fft_output_format = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_SET_FFT_WIND_TYPE:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id            = u8_command_code;
        sIntern_command_setup.param.fft_window = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_SET_FFT_RES_AVG_CT:

        if (u16_params_and_bulk_len != sizeof(uint32_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id                       = u8_command_code;
        sIntern_command_setup.param.u32_fft_res_avg_count = *((uint32_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_SET_WIND_CORR_FACTOR:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id                     = u8_command_code;
        sIntern_command_setup.param.fft_win_corr_factor = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_FETCH_FFT_STGS:

        if (u16_params_and_bulk_len != 0) // there is no param for this command
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id = u8_command_code;

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }

        break;

    case ECEP_CMD_FETCH_FFT_TOTAL_DUR:

        if (u16_params_and_bulk_len != 0) // there is no param for this command
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id = u8_command_code;

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_FETCH_ADC_RAW_DATA:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id         = u8_command_code;
        sIntern_command_setup.param.channel = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_FETCH_CEN_FREQ:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id         = u8_command_code;
        sIntern_command_setup.param.channel = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_FETCH_CEN_FREQ_AMP:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id         = u8_command_code;
        sIntern_command_setup.param.channel = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_SET_MIC_RANGE:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id       = u8_command_code;
        sIntern_command_setup.param.range = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }

        break;

    case ECEP_CMD_FETCH_MIC_RANGE:

        if (u16_params_and_bulk_len != 0) // there is no param for this command
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id = u8_command_code;

        if (xQueueSend(gs_xQueue_dsp_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). DSP command queue is full");
        }
        break;

    case ECEP_CMD_TRIG_PR_CLICK:

        if (u16_params_and_bulk_len != 0) // there is no param for this command
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id = u8_command_code;

        if (xQueueSend(gs_xQueue_pr_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). PR command queue is full");
        }
        break;

    case ECEP_CMD_TRIG_PR_HOLD:

        if (u16_params_and_bulk_len != 0) // there is no param for this command
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id = u8_command_code;

        if (xQueueSend(gs_xQueue_pr_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). PR command queue is full");
        }
        break;

    case ECEP_CMD_TRIG_PR_RELEASE:

        if (u16_params_and_bulk_len != 0) // there is no param for this command
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id = u8_command_code;

        if (xQueueSend(gs_xQueue_pr_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). PR command queue is full");
        }
        break;

    case ECEP_CMD_TRIG_PR_PRESS:

        if (u16_params_and_bulk_len != sizeof(uint32_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id          = u8_command_code;
        sIntern_command_setup.param.u32_time = *((uint32_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_pr_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). PR command queue is full");
        }
        break;

    case ECEP_CMD_SET_PR_DEF_STATE:

        if (u16_params_and_bulk_len != sizeof(uint8_t))
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id             = u8_command_code;
        sIntern_command_setup.param.relay_state = *((uint8_t *)pv_params_and_bulk);

        if (xQueueSend(gs_xQueue_pr_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). PR command queue is full");
        }
        break;

    case ECEP_CMD_FETCH_PR_DEF_STATE:

        if (u16_params_and_bulk_len != 0) // there is no param for this command
        {
            // Something went wrong.
            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
            esp_restart();
        }

        sIntern_command_setup.u8_id = u8_command_code;

        if (xQueueSend(gs_xQueue_pr_ecep_cmds, &sIntern_command_setup, 0) == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "ERROR: in ecep_command_handler(). PR command queue is full");
        }
        break;

    case ECEP_CMD_PING:
        send_ecep_evnt_pong();
        break;

    default:
        send_ecep_evnt_cmd_status(ECEP_ERR_INVALID_CMD, u8_command_code);
        break;
    }

    // Reset struct - just to be sure:
    memset(&sIntern_command_setup, 0, sizeof(sIntern_command_setup));
}

void ecep_rx_packet_handler_task()
{
    uint32_t u32_data_len = 0;
    uint8_t  au8_raw_uart_copy[UART_RD_BUF_SIZE];

    uint8_t  u8_packet_data[ECEP_MAX_CMD_PACKET_LEN]; // This array is small enough that we dont need to place it in PSRAM
    uint32_t u32_package_byte_counter = 0;

    uint32_t byte_counter;

    uint8_t  u8_packet_type;
    uint8_t  u8_command_code = 0xFF;
    uint16_t u16_packet_len  = 0xFFFF;

    uint32_t remaining_package_len;
    uint32_t remaining_data_len;
    uint32_t chunk_size_to_cpy;

    while (1)
    {
        if (xQueueReceive(ecep_data_len_que, (void *)&u32_data_len, portMAX_DELAY))
        {
            // Take a copy of the shared memory:
            if (xSemaphoreTake(gs_raw_uart_shr_mem_sem, portMAX_DELAY) == pdTRUE)
            {
                memcpy(au8_raw_uart_copy, gs_au8_raw_uart_shr, sizeof(au8_raw_uart_copy));
                xSemaphoreGive(gs_raw_uart_shr_mem_sem);
            }

            // Assemble ECEP package from uart data:

            byte_counter = 0;

            while (byte_counter < u32_data_len)
            {

                if (u32_package_byte_counter < ECEP_HEADER_LEN) // Header as not been fully read yet
                {
                    // Read header 1 byte at a time:
                    u8_packet_data[u32_package_byte_counter] = au8_raw_uart_copy[byte_counter];

                    u32_package_byte_counter++;
                    byte_counter++;

                    if (u32_package_byte_counter == ECEP_HEADER_LEN) // Header has been fully read
                    {
                        u8_packet_type  = u8_packet_data[0];
                        u8_command_code = u8_packet_data[1];
                        u16_packet_len  = u8_packet_data[2] | (u8_packet_data[3] << 8);

                        ESP_LOGI(TAG, "Received Packet - u8_packet_type:  0x%04x", (unsigned int)u8_packet_type);
                        ESP_LOGI(TAG, "Received Packet - u8_command_code: 0x%04x", (unsigned int)u8_command_code);
                        ESP_LOGI(TAG, "Received Packet - u16_packet_len:  0x%04x", (unsigned int)u16_packet_len);

                        // Header error checks:
                        if (u8_packet_type != ECEP_PACKET_TYPE)
                        {
                            // Something went wrong.
                            ESP_LOGE(TAG, "ERROR: invalid packet type. Packets are now potentially unsyncronized. Trying to fix by restarting...");
                            esp_restart();
                        }
                        if (u16_packet_len > ECEP_MAX_CMD_PACKET_LEN)
                        {
                            // Something went wrong.
                            ESP_LOGE(TAG, "ERROR: invalid packet length. Packets are now potentially unsyncronized. Trying to fix by restarting...");
                            esp_restart();
                        }
                    }
                }

                if ((u32_package_byte_counter >= ECEP_HEADER_LEN) && (u32_package_byte_counter != u16_packet_len)) // Copy the rest of the package in big chunks (faster then extracting 1 byte at a time)
                {
                    remaining_package_len = u16_packet_len - u32_package_byte_counter;
                    remaining_data_len    = u32_data_len - byte_counter;
                    chunk_size_to_cpy     = (remaining_package_len < remaining_data_len ? remaining_package_len : remaining_data_len); // chunk size to copy is limited by either u16_packet_len or u32_data_len

                    // ESP_LOGE(TAG, "remaining_package_len: %i", (int) remaining_package_len);
                    // ESP_LOGE(TAG, "remaining_data_len: %i", (int) remaining_data_len);
                    // ESP_LOGE(TAG, "chunk_size_to_cpy: %i", (int) chunk_size_to_cpy);
                    // ESP_LOGE(TAG, "u8_packet_data[u32_package_byte_counter]: %i", (int) u8_packet_data[u32_package_byte_counter]);
                    // ESP_LOGE(TAG, "u32_package_byte_counter: %i", (int) u32_package_byte_counter);

                    memcpy(&u8_packet_data[u32_package_byte_counter], &au8_raw_uart_copy[byte_counter], chunk_size_to_cpy);

                    u32_package_byte_counter += chunk_size_to_cpy;
                    byte_counter += chunk_size_to_cpy;

                    // ESP_LOGE(TAG, "u32_package_byte_counter 2: %i", (int) u32_package_byte_counter);
                }

                if ((u32_package_byte_counter >= ECEP_HEADER_LEN) && (u32_package_byte_counter == u16_packet_len)) // Packet assembly complete
                {
                    // ESP_LOGI(TAG, "Jamen Okay da ASDASD");
                    ecep_command_handler(u8_command_code, &u8_packet_data[4], u16_packet_len - ECEP_HEADER_LEN); // Pass packet without header

                    // Reset Package byte counter and data:
                    u32_package_byte_counter = 0;
                    memset(u8_packet_data, 0, sizeof(u8_packet_data));
                }
            }
        }
    }
}

// Note: "uart events" refeers to the esp uart api. Dont confuse this "event" with ECEP events.
void ecep_uart_event_task() // GIV ANDET NAVN!
{
    uart_event_t event;

    while (1)
    {
        //  Waiting for UART event.
        if (xQueueReceive(ecep_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            switch (event.type)
            {

            // Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[DATA EVT]:");
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);

                // Error check:
                if (uxQueueMessagesWaiting(ecep_data_len_que))
                {
                    // ecep_rx_packet_handler_task() have missed the data stored in gs_au8_raw_uart_shr (it was too slow to handle the previous uart data)
                    // Packets are now unsyncronized. Trying to fix by restarting.
                    ESP_LOGE(TAG, "ERROR: ecep_rx_packet_handler_task() missed the uart data stored in gs_au8_raw_uart_shr... Restarting");
                    esp_restart();
                }

                // Read uart data:
                if (xSemaphoreTake(gs_raw_uart_shr_mem_sem, portMAX_DELAY) == pdTRUE)
                {
                    memset(gs_au8_raw_uart_shr, 0, sizeof(gs_au8_raw_uart_shr)); // Reset shared memory (just to be sure)
                    uart_read_bytes(ECEP_UART_NUM, gs_au8_raw_uart_shr, event.size, portMAX_DELAY);
                    xSemaphoreGive(gs_raw_uart_shr_mem_sem);
                }

                // Trigger ecep_rx_packet_handler_task():
                if (xQueueSend(ecep_data_len_que, (void *)&event.size, (TickType_t)10) != pdPASS)
                {
                    ESP_LOGE(TAG, "ERROR: while sending to ecep_data_len_que queue");
                }
                break;

            // Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(ECEP_UART_NUM);
                xQueueReset(ecep_uart_queue);
                break;

            // Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(ECEP_UART_NUM);
                xQueueReset(ecep_uart_queue);
                break;

            // Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;

            // Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "uart parity error");
                break;

            // Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "uart frame error");
                break;

            // Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
}

void ecep_tx_packet_header_prep(uint8_t *pu8_tx_packet_buf, uint8_t packet_type, uint8_t event_id, uint16_t packet_len)
{
    // | Packet Type | Command/event code | Packet Length (low byte) | Packet Length (high byte) |
    //    (1 byte)         (1 byte)                 (1 byte)                    (1 byte)

    pu8_tx_packet_buf[TX_PACKET_TYPE_BYTE]          = (uint8_t)packet_type;
    pu8_tx_packet_buf[TX_PACKET_CMD_EVNT_BYTE]      = (uint8_t)event_id;
    pu8_tx_packet_buf[TX_PACKET_PACK_LEN_LOW_BYTE]  = (uint8_t)packet_len & 0x00FF;
    pu8_tx_packet_buf[TX_PACKET_PACK_LEN_HIGH_BYTE] = (uint8_t)(packet_len >> 8) & 0x00FF;
}

void ecep_uart_tx_task()
{

    // Packet:
    // |   Header   |   Cmd/Evt param struct   |   Bulk data   |
    //    (4 bytes)           (differ)              (differ)

    sIntern_event_setup_t ecep_event;
    uint16_t              packet_len = 0;

    uint8_t *pu8_tx_packet_buf = heap_caps_malloc(ECEP_MAX_EVNT_PACKET_LEN, MALLOC_CAP_SPIRAM); // TX packet buffer to be transmitted. It size equal the maximum packet size in bytes - 2^16 = 65536
    if ((pu8_tx_packet_buf == NULL))
    {
        ESP_LOGE(TAG, "ERROR: while allocating memory for TX packet buffer in PSRAM");
    }
    memset(pu8_tx_packet_buf, 0, ECEP_MAX_EVNT_PACKET_LEN);

    // Temporary for testing purposes.
    // sIntern_event_setup_t ecep_init_event;
    // ecep_init_event.u8_id = ECEP_EVNT_LEFT_ADC_RAW;
    // ecep_init_event.param.FFT_stgs.fft_output_format = ampltitude_dB_SPL;
    // ecep_init_event.param.FFT_stgs.fft_window = blackman_nuttall;
    // ecep_init_event.param.FFT_stgs.fft_corr_factor = amplitude;
    // ecep_init_event.param.FFT_stgs.u32_fft_res_avg_count = 100001;

    // xQueueSend(gs_xQueue_ecep_events, &ecep_init_event, 0);
    // xQueueSend(gs_xQueue_ecep_events, &ecep_init_event, 0);

    // send_ecep_evnt_cmd_status(ECEP_FAIL, ECEP_CMD_FETCH_PR_DEF_STATE);
    // send_ecep_evnt_pr_def_state(norm_open);
    // vTaskDelay(5000 / portTICK_PERIOD_MS);

    while (1)
    {

        if (xQueueReceive(gs_xQueue_ecep_events, &ecep_event, (TickType_t)portMAX_DELAY) == pdTRUE)
        {
            packet_len = ECEP_HEADER_LEN;

            switch (ecep_event.u8_id)
            {
            case ECEP_EVNT_COMMAND_STATUS:

                ESP_LOGI(TAG, "Entered ECEP_EVNT_COMMAND_STATUS");

                packet_len += sizeof(uint8_t) * 2; // Total of 6 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_COMMAND_STATUS, packet_len);
                pu8_tx_packet_buf[4] = (uint8_t)ecep_event.param.status_event.u8_parent_cmd_id;
                pu8_tx_packet_buf[5] = (uint8_t)ecep_event.param.status_event.status;

                break;

            case ECEP_EVNT_LEFT_FFT_RES:

                packet_len += (uint16_t)(DATA_BLOCK_SIZE / 2 * sizeof(float)); // Total of 16388 (0x4004) bytes

                ESP_LOGI(TAG, "Packet length: %i", (int)packet_len);

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_LEFT_FFT_RES, packet_len);

                // Copy the left channel FFT result in shared memory into the TX packet in PSRAM
                if (xSemaphoreTake(gs_fft_L_res_shr_mem_sem, portMAX_DELAY) == pdTRUE)
                {
                    memcpy(&pu8_tx_packet_buf[4], gs_pf32_L_fft_res_shr, DATA_BLOCK_SIZE / 2 * sizeof(float));
                    xSemaphoreGive(gs_fft_L_res_shr_mem_sem);
                }

                break;

            case ECEP_EVNT_RIGHT_FFT_RES:

                packet_len += DATA_BLOCK_SIZE / 2 * sizeof(float); // Total of 16388 (0x4004) bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_RIGHT_FFT_RES, packet_len);

                // Copy the right channel FFT result in shared memory into the TX packet in PSRAM
                if (xSemaphoreTake(gs_fft_R_res_shr_mem_sem, portMAX_DELAY) == pdTRUE)
                {
                    memcpy(&pu8_tx_packet_buf[4], gs_pf32_R_fft_res_shr, DATA_BLOCK_SIZE / 2 * sizeof(float));
                    xSemaphoreGive(gs_fft_R_res_shr_mem_sem);
                }
                break;

            case ECEP_EVNT_FFT_STGS:

                packet_len += sizeof(uint8_t) * 3 + sizeof(uint32_t); // Total of 11 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_FFT_STGS, packet_len);
                pu8_tx_packet_buf[4] = (uint8_t)ecep_event.param.FFT_stgs.fft_output_format;
                pu8_tx_packet_buf[5] = (uint8_t)ecep_event.param.FFT_stgs.fft_window;
                pu8_tx_packet_buf[6] = (uint8_t)ecep_event.param.FFT_stgs.fft_corr_factor;
                memcpy(&pu8_tx_packet_buf[7], &ecep_event.param.FFT_stgs.u32_fft_res_avg_count, sizeof(uint32_t));

                break;

            case ECEP_EVNT_FFT_TOT_DUR:

                packet_len += sizeof(uint32_t); // Total of 8 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_FFT_TOT_DUR, packet_len);
                memcpy(&pu8_tx_packet_buf[4], &ecep_event.param.u32_time, sizeof(uint32_t));

                break;

            case ECEP_EVNT_LEFT_ADC_RAW:

                packet_len += DATA_BLOCK_SIZE * sizeof(float); // Total of 32772 (0x8004) bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_LEFT_ADC_RAW, packet_len);

                // Copy the left channel raw ADC data in shared memory into the TX packet in PSRAM
                if (xSemaphoreTake(gs_adc1_shared_mem_sem, portMAX_DELAY) == pdTRUE)
                {
                    memcpy(&pu8_tx_packet_buf[4], gs_pf32_adc1_shared, DATA_BLOCK_SIZE * sizeof(float));
                    xSemaphoreGive(gs_adc1_shared_mem_sem);
                }

                break;

            case ECEP_EVNT_RIGHT_ADC_RAW:

                packet_len += DATA_BLOCK_SIZE * sizeof(float); // Total of 32772 (0x8004) bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_RIGHT_ADC_RAW, packet_len);

                // Copy the right channel raw ADC data in shared memory into the TX packet in PSRAM
                if (xSemaphoreTake(gs_adc2_shared_mem_sem, portMAX_DELAY) == pdTRUE)
                {
                    memcpy(&pu8_tx_packet_buf[4], gs_pf32_adc2_shared, DATA_BLOCK_SIZE * sizeof(float));
                    xSemaphoreGive(gs_adc2_shared_mem_sem);
                }

                break;

            case ECEP_EVNT_LEFT_CEN_FREQ:

                packet_len += sizeof(uint32_t); // Total of 8 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_LEFT_CEN_FREQ, packet_len);
                memcpy(&pu8_tx_packet_buf[4], &ecep_event.param.u32_cen_freq_hz, sizeof(uint32_t));

                break;

            case ECEP_EVNT_RIGHT_CEN_FREQ:

                packet_len += sizeof(uint32_t); // Total of 8 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_RIGHT_CEN_FREQ, packet_len);
                memcpy(&pu8_tx_packet_buf[4], &ecep_event.param.u32_cen_freq_hz, sizeof(uint32_t));

                break;

            case ECEP_EVNT_LEFT_CEN_FREQ_AMP:

                packet_len += sizeof(float); // Total of 8 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_LEFT_CEN_FREQ_AMP, packet_len);
                memcpy(&pu8_tx_packet_buf[4], &ecep_event.param.f32_cen_freq_amp, sizeof(float));

                break;

            case ECEP_EVNT_RIGHT_CEN_FREQ_AMP:

                packet_len += sizeof(float); // Total of 8 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_RIGHT_CEN_FREQ_AMP, packet_len);
                memcpy(&pu8_tx_packet_buf[4], &ecep_event.param.f32_cen_freq_amp, sizeof(float));

                break;

            case ECEP_EVNT_MIC_RANGE:

                packet_len += sizeof(uint8_t); // Total of 5 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_MIC_RANGE, packet_len);
                pu8_tx_packet_buf[4] = (uint8_t)ecep_event.param.range;

                break;

            case ECEP_EVNT_PR_DEF_STATE:

                packet_len += sizeof(uint8_t); // Total of 5 bytes

                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_PR_DEF_STATE, packet_len);
                pu8_tx_packet_buf[4] = (uint8_t)ecep_event.param.pr_default;

                break;

            case ECEP_EVNT_PONG:

                // Ping reply, Event packet only containts header (4 bytes).
                ecep_tx_packet_header_prep(pu8_tx_packet_buf, ECEP_PACKET_TYPE, ECEP_EVNT_PONG, packet_len);

                break;

            default:
                ESP_LOGE(TAG, "Error when generating event packet. Invalid event ID");
                break;
            }

            uart_write_bytes(UART_NUM_0, pu8_tx_packet_buf, packet_len);
        }
    }
}

static void baud_error_test(int32_t i32_uart_num, int32_t i32_baud)
{
    uint32_t u32_actual_baud;

    uart_config_t baud_test_config = {
        .baud_rate           = i32_baud,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = ECEP_UART_CFG_FC,
        .rx_flow_ctrl_thresh = 122,
        .source_clk          = UART_SCLK_DEFAULT, // Default source clk is from APB (is fastest -> least baud to reg rounding error)
    };
    uart_param_config(i32_uart_num, &baud_test_config);
    uart_get_baudrate(i32_uart_num, &u32_actual_baud);
    ESP_LOGI(TAG, "BAUD TEST. Set baud: %d  Actual baud: %d  Error ratio: %f", (int)i32_baud, (int)u32_actual_baud, (float)u32_actual_baud / (float)i32_baud);
}

void ecep_uart_init(QueueHandle_t     xQueue_pr_ecep_cmds,
                    QueueHandle_t     xQueue_dsp_ecep_cmds,
                    float            *pf32_adc1_shared,
                    float            *pf32_adc2_shared,
                    SemaphoreHandle_t adc1_shared_mem_sem,
                    SemaphoreHandle_t adc2_shared_mem_sem,
                    float            *pf32_L_fft_res_shr,
                    float            *pf32_R_fft_res_shr,
                    SemaphoreHandle_t fft_L_res_shr_mem_sem,
                    SemaphoreHandle_t fft_R_res_shr_mem_sem)
{
    gs_xQueue_pr_ecep_cmds  = xQueue_pr_ecep_cmds;
    gs_xQueue_dsp_ecep_cmds = xQueue_dsp_ecep_cmds;

    // Create ECEP event queue:
    /* The programmer is expected to use the "Convenience functions" at the bottom of this file to access the event queue from throughout the application.
        If the programmer instead want to access the event queue directly: create this event queue in main.c instead and pass it to the relevant files. */
    gs_xQueue_ecep_events = xQueueCreate(10, sizeof(sIntern_event_setup_t));
    if (gs_xQueue_ecep_events == NULL)
    {
        ESP_LOGE(TAG, "ERROR: while creating gs_xQueue_ecep_events");
    }

    // ECEP UART config:
    uart_config_t ecep_uart_config = {
        .baud_rate           = ECEP_UART_CFG_BAUD,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = ECEP_UART_CFG_FC,
        .rx_flow_ctrl_thresh = 122,
        .source_clk          = UART_SCLK_DEFAULT, // Default source clk is from APB (is fastest -> least baud to reg rounding error) // TEST LIGE
    };
    // Set ECEP UART config:
    ESP_ERROR_CHECK(uart_param_config(ECEP_UART_NUM, &ecep_uart_config));
    // Set ECEP UART pins:
    ESP_ERROR_CHECK(uart_set_pin(ECEP_UART_NUM, ECEP_TXD_PIN, ECEP_RXD_PIN, ECEP_RTS_PIN, ECEP_CTS_PIN));
    // Install UART driver:
    ESP_ERROR_CHECK(uart_driver_install(ECEP_UART_NUM, BUF_SIZE * 2, ECEP_MAX_EVNT_PACKET_LEN + 100, 10, &ecep_uart_queue, 0)); // NOTE: tx_buffer_size can be made smaller if
                                                                                                                                // internal memory is limited. A total tx buffer
                                                                                                                                // of 100 may cause problems.
                                                                                                                                // (TX buffer does not have to be above ECEP_MAX_EVNT_PACKET_LEN)
    // baud_error_test(UART_NUM_1, 115200);
    // baud_error_test(UART_NUM_1, 230400);
    // baud_error_test(UART_NUM_1, 460800);
    // baud_error_test(UART_NUM_1, 576000);
    // baud_error_test(UART_NUM_1, 921600);

    // Mutex for gs_au8_raw_uart_shr:
    gs_raw_uart_shr_mem_sem = xSemaphoreCreateMutex();

    // Packet handler:
    ecep_data_len_que = xQueueCreate(10, sizeof(uint32_t)); // (event.size from uart event is uint32_t)
    if (ecep_data_len_que == NULL)
    {
        ESP_LOGE(TAG, "ERROR: while creating ecep_data_len_que queue");
    }

    // TX Packet handler
    gs_pf32_adc1_shared    = pf32_adc1_shared;
    gs_pf32_adc2_shared    = pf32_adc2_shared;
    gs_adc1_shared_mem_sem = adc1_shared_mem_sem;
    gs_adc2_shared_mem_sem = adc2_shared_mem_sem;

    gs_pf32_L_fft_res_shr    = pf32_L_fft_res_shr;
    gs_pf32_R_fft_res_shr    = pf32_R_fft_res_shr;
    gs_fft_L_res_shr_mem_sem = fft_L_res_shr_mem_sem;
    gs_fft_R_res_shr_mem_sem = fft_R_res_shr_mem_sem;
}

// ----------------------- Convenience functions for accessing the ecep event queue -----------------------

// Send ECEP_EVNT_COMMAND_STATUS event:
void send_ecep_evnt_cmd_status(eCMD_status_t status_type, uint8_t u8_cmd_id)
{
    sIntern_event_setup_t event;

    event.u8_id                               = ECEP_EVNT_COMMAND_STATUS;
    event.param.status_event.u8_parent_cmd_id = u8_cmd_id;
    event.param.status_event.status           = status_type;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_status_event(). Event queue is full");
    }
}

// Send ECEP_EVNT_PR_DEF_STATE event:
void send_ecep_evnt_pr_def_state(eRelay_state_t default_relay_state)
{
    sIntern_event_setup_t event;

    event.u8_id            = ECEP_EVNT_PR_DEF_STATE;
    event.param.pr_default = default_relay_state;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_pr_def_state(). Event queue is full");
    }
}

// Send ECEP_EVNT_FFT_STGS event:
void send_ecep_evnt_fft_stgs(eFFT_output_form_t fft_output_format, eFFT_window_type_t fft_window,
                             uint32_t u32_fft_res_avg_count, eFFT_wind_corr_factor_t fft_corr_factor)
{
    sIntern_event_setup_t event;

    event.u8_id                                = ECEP_EVNT_FFT_STGS;
    event.param.FFT_stgs.fft_output_format     = fft_output_format;
    event.param.FFT_stgs.fft_window            = fft_window;
    event.param.FFT_stgs.u32_fft_res_avg_count = u32_fft_res_avg_count;
    event.param.FFT_stgs.fft_corr_factor       = fft_corr_factor;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_fft_stgs(). Event queue is full");
    }
}

// Send ECEP_EVNT_FFT_TOT_DUR event:
void send_ecep_evnt_fft_tot_dur(uint32_t u32_fft_total_dur_ms)
{
    sIntern_event_setup_t event;

    event.u8_id          = ECEP_EVNT_FFT_TOT_DUR;
    event.param.u32_time = u32_fft_total_dur_ms;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_fft_tot_dur(). Event queue is full");
    }
}

// Send ECEP_EVNT_LEFT_CEN_FREQ:
void send_ecep_evnt_L_cen_freq(uint32_t u32_center_freq_hz)
{
    sIntern_event_setup_t event;

    event.u8_id                 = ECEP_EVNT_LEFT_CEN_FREQ;
    event.param.u32_cen_freq_hz = u32_center_freq_hz;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_L_cen_freq(). Event queue is full");
    }
}

// Send ECEP_EVNT_RIGHT_CEN_FREQ:
void send_ecep_evnt_R_cen_freq(uint32_t u32_center_freq_hz)
{
    sIntern_event_setup_t event;

    event.u8_id                 = ECEP_EVNT_RIGHT_CEN_FREQ;
    event.param.u32_cen_freq_hz = u32_center_freq_hz;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_R_cen_freq(). Event queue is full");
    }
}

// Send ECEP_EVNT_LEFT_CEN_FREQ_AMP:
void send_ecep_evnt_L_cen_freq_amp(float f32_center_freq_amp)
{
    sIntern_event_setup_t event;

    event.u8_id                  = ECEP_EVNT_LEFT_CEN_FREQ_AMP;
    event.param.f32_cen_freq_amp = f32_center_freq_amp;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_L_cen_freq_amp(). Event queue is full");
    }
}

// Send ECEP_EVNT_RIGHT_CEN_FREQ_AMP:
void send_ecep_evnt_R_cen_freq_amp(float f32_center_freq_amp)
{
    sIntern_event_setup_t event;

    event.u8_id                  = ECEP_EVNT_RIGHT_CEN_FREQ_AMP;
    event.param.f32_cen_freq_amp = f32_center_freq_amp;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_R_cen_freq_amp(). Event queue is full");
    }
}

// Send ECEP_EVNT_MIC_RANGE:
void send_ecep_evnt_mic_range(eFFT_hw_gain_stage_t hw_gain_stage)
{
    sIntern_event_setup_t event;

    event.u8_id       = ECEP_EVNT_MIC_RANGE;
    event.param.range = hw_gain_stage;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_mic_range(). Event queue is full");
    }
}

// Send ECEP_EVNT_LEFT_FFT_RES:
void send_ecep_evnt_L_fft_res()
{
    sIntern_event_setup_t event;
    event.u8_id = ECEP_EVNT_LEFT_FFT_RES;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_L_fft_res(). Event queue is full");
    }
}

// Send ECEP_EVNT_RIGHT_FFT_RES:
void send_ecep_evnt_R_fft_res()
{
    sIntern_event_setup_t event;
    event.u8_id = ECEP_EVNT_RIGHT_FFT_RES;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_R_fft_res(). Event queue is full");
    }
}

// Send ECEP_EVNT_LEFT_ADC_RAW:
void send_ecep_evnt_L_adc_raw()
{
    sIntern_event_setup_t event;
    event.u8_id = ECEP_EVNT_LEFT_ADC_RAW;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_L_adc_raw(). Event queue is full");
    }
}

// Send ECEP_EVNT_RIGHT_ADC_RAW
void send_ecep_evnt_R_adc_raw()
{
    sIntern_event_setup_t event;
    event.u8_id = ECEP_EVNT_RIGHT_ADC_RAW;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_R_adc_raw(). Event queue is full");
    }
}

// Send ECEP_EVNT_PONG:
void send_ecep_evnt_pong()
{
    sIntern_event_setup_t event;
    event.u8_id = ECEP_EVNT_PONG;

    if (xQueueSend(gs_xQueue_ecep_events, &event, 0) == errQUEUE_FULL)
    {
        ESP_LOGE(TAG, "ERROR: in send_ecep_evnt_pong(). Event queue is full");
    }
}

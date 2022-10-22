#include <stdio.h>
#include <string.h> // For memcpy()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "driver/uart.h"

#include "common_defines.h"
#include "ecep_uart.h"
#include "ecep_defines.h"

static const char *TAG = "ecep_uart.c";

#define ECEP_UART_NUM UART_NUM_0   // Uart 0: The uart routed to the uart<->usb chip on the ESP32-S3 dev-kit-c (aka the "uart" usb port)

#define UART_RD_BUF_SIZE  1024

static QueueHandle_t ecep_uart_queue;     // Queue for uart api
static QueueHandle_t ecep_data_len_que;   // Queue used to syncronize ecep_packet_handler_task() from ecep_uart_event_task()

static uint8_t gs_au8_raw_uart_shr[UART_RD_BUF_SIZE];  // Raw uart read data (shared memory)
static SemaphoreHandle_t gs_raw_uart_shr_mem_sem;


void ecep_packet_handler_task()
{
    uint32_t u32_data_len = 0; 
    uint8_t au8_raw_uart_copy[UART_RD_BUF_SIZE];

    uint8_t u8_packet_data[ECEP_MAX_PACKAGE_SIZE];
    uint32_t u32_package_byte_counter = 0; 

    uint32_t u32_chunk_size_to_cpy;

    uint32_t byte_counter;
    uint8_t  u8_packet_type;
    uint8_t  u8_cmd_evt_code;
    uint16_t u16_packet_len;

    while(1)
    {
        if(xQueueReceive(ecep_data_len_que, (void *)&u32_data_len, portMAX_DELAY))
        {

            // Take a copy of the shared memory:
            if(xSemaphoreTake(gs_raw_uart_shr_mem_sem, portMAX_DELAY) == pdTRUE)
            {
               memcpy(au8_raw_uart_copy, gs_au8_raw_uart_shr, sizeof(au8_raw_uart_copy)); 
               xSemaphoreGive(gs_raw_uart_shr_mem_sem);     
            }



            // Assemble ECEP package from uart data:

            byte_counter = 0; 
            u8_packet_type = 0;
            u8_cmd_evt_code = 0;
            u16_packet_len = 0;

            if(u32_package_byte_counter < ECEP_HEADER_SIZE)  // Header as not been fully read yet
            {
                for(byte_counter; byte_counter < u32_data_len, byte_counter++) // Extract 1 byte at a time
                {
                    u32_package_byte_counter++;

                    // Read header:
                    u8_packet_data[u32_package_byte_counter-1] = au8_raw_uart_copy[byte_counter];

                    if(u32_package_byte_counter == ECEP_HEADER_SIZE)
                    {
                        //Extract elements in header:
                        u8_packet_type  = u8_packet_data[0];
                        u8_cmd_evt_code = u8_packet_data[1];
                        u16_packet_len  = u8_packet_data[2] | (u8_packet_data[3] << 8);

                        ESP_LOGI(TAG, "Received Packet - u8_packet_type:  %08x", (uint32_t)u8_packet_type);
                        ESP_LOGI(TAG, "Received Packet - u8_cmd_evt_code: %08x", (uint32_t)u8_cmd_evt_code);
                        ESP_LOGI(TAG, "Received Packet - u16_packet_len:  %08x", (uint32_t)u16_packet_len);

                        // Header error checks:
                        if (u8_packet_type != ECEP_PACKET_TYPE)
                        {
                            // Something went wrong. Packets are now unsyncronized. Trying to fix by restarting.
                            ESP_LOGI(TAG, "ERROR: invalid packet type");
                            esp_restart(); 
                        }
                        if (u16_packet_len > ECEP_MAX_PACKAGE_SIZE)
                        {
                            // Something went wrong. Packets are now unsyncronized. Trying to fix by restarting.
                            ESP_LOGI(TAG, "ERROR: invalid packet length");
                            esp_restart(); 
                        }

                        break;  
                        // PROBLEM... Nu ved vi ikke ude fra det loop her, hvor mange bytes vi har læst fra au8_raw_uart_copy...æ
                        // Måske lave det til et while loop i stedet ? 
                    }
                }
            }

            if(u32_package_byte_counter >= ECEP_HEADER_SIZE)  // Copy the rest of the package in big chunks (faster then extracting 1 byte at a time)
            {
                u32_chunk_size_to_cpy = (u16_packet_len < u32_data_len ? u16_packet_len : u32_data_len);  // chunk size to copy is limited by either u16_packet_len or u32_data_len
                memcpy(u8_packet_data[u32_package_byte_counter-1], au8_raw_uart_copy[byte_counter], u32_chunk_size_to_cpy);

                u32_package_byte_counter += u32_chunk_size_to_cpy;
            }

            if(u32_package_byte_counter == u16_packet_len)  // Sikre lige man ikke kommer herned hvis headeren ikke er helt læst
            {
                // CMD HANDLER

                // RESET u32_package_byte_counter og packet data arreyed

            }



            // Bruges ikke længere:

            // for(uint32_t byte_counter = 0 ; byte_counter < u32_data_len; byte_counter++)
            // {
            //     u32_package_byte_counter++;

            //     if(u32_package_byte_counter <= ECEP_HEADER_SIZE)
            //     {
            //         u8_packet_data[u32_package_byte_counter-1] = au8_raw_uart_copy[byte_counter];
            //     }

            //     if(u32_package_byte_counter == ECEP_HEADER_SIZE)
            //     {
            //         //Extract header:
            //         u8_packet_type  = u8_packet_data[0];
            //         u8_cmd_evt_code = u8_packet_data[1];
            //         u16_packet_len  = u8_packet_data[2] | (u8_packet_data[3] << 8);

            //         ESP_LOGI(TAG, "Received Packet - u8_packet_type:  %08x", (uint32_t)u8_packet_type);
            //         ESP_LOGI(TAG, "Received Packet - u8_cmd_evt_code: %08x", (uint32_t)u8_cmd_evt_code);
            //         ESP_LOGI(TAG, "Received Packet - u16_packet_len:  %08x", (uint32_t)u16_packet_len);

            //         // Header error checks:
            //         if (u8_packet_type != ECEP_PACKET_TYPE)
            //         {
            //             ESP_LOGI(TAG, "ERROR: invalid packet type");
            //             return;
            //         }
            //         if (u16_packet_len > ECEP_MAX_PACKAGE_SIZE)
            //         {
            //             ESP_LOGI(TAG, "ERROR: invalid packet length");
            //             return;
            //         }
            //     }

            //     // DEN HER KOMMENTAR SKAL MÅSKE STÅ I CMD HANDLERE I STEDET. IDK:
            //     /* Bulk data is discarded since no command uses bulk data
            //         (if bulk data for a command is needed in the future, add a Queue or Mutex protetected Shared Memory to pass to the relevenat task)
            //         (for now: no need to have allocated shamed memory for bulk data if no commands uses it)
            //     */
                


            // if(u32_package_byte_counter == u16_packet_len)
            // {
            //     // KALD COMMAND HANDLER

            //     //RESET PAKKE:
            //     //RESET PACKAGEVBYTE COUNTER
            //     // RESET PACKET DATA
            // }






        }
    }
}


//Note: "uart events" refeers to the esp uart api. Dont confuse this "event" with ECEP events.
void ecep_uart_event_task()
{
    uart_event_t event;

    while(1)
    {
        //  Waiting for UART event.
        if (xQueueReceive(ecep_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            switch(event.type) {

                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    ESP_LOGI(TAG, "[DATA EVT]:");
                    ESP_LOGI(TAG, "[UART DATA]: %d", event.size);

                    // Error check: 
                    if(uxQueueMessagesWaiting(ecep_data_len_que))
                    {
                        // ecep_packet_handler_task() missed the data stored in gs_au8_raw_uart_shr (it was too slow to handle the previous uart data)
                        // Packets are now unsyncronized. Trying to fix by restarting.
                        ESP_LOGE(TAG, "ERROR: ecep_packet_handler_task() missed the uart data stored in gs_au8_raw_uart_shr... Restarting");
                        esp_restart(); 
                    }

                    // Read uart data:
                    if(xSemaphoreTake(gs_raw_uart_shr_mem_sem, portMAX_DELAY) == pdTRUE)
                    {
                        memset(gs_au8_raw_uart_shr, 0, sizeof(gs_au8_raw_uart_shr)) // Reset shared memory (just to be sure)
                        uart_read_bytes(ECEP_UART_NUM, gs_au8_raw_uart_shr, event.size, portMAX_DELAY);
                        xSemaphoreGive(gs_raw_uart_shr_mem_sem);
                    }

                    // Trigger ecep_packet_handler_task():
                    if (xQueueSend(ecep_data_len_que, (void *)&event.size, (TickType_t)10) != pdPASS)
                    {
                        ESP_LOGE(TAG, "ERROR: while sending to ecep_data_len_que queue");
                    }
                    break;

                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(ECEP_UART_NUM);
                    xQueueReset(ecep_uart_queue);
                    break;
                    
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "ring buffer full");
                    // If buffer full happened, you should consider increasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(ECEP_UART_NUM);
                    xQueueReset(ecep_uart_queue);
                    break;

                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break");
                    break;

                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "uart parity error");
                    break;

                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "uart frame error");
                    break;
                    
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }   
}

void ecep_uart_init()
{

    raw_uart_shr_mem_sem = xSemaphoreCreateMutex();  // Mutex for gs_au8_raw_uart_shr  

    //Packet handler:
    ecep_data_len_que = xQueueCreate(10, sizeof(uint32_t));   // (event.size from uart event is uint32_t)
    if( ecep_data_len_que == NULL )
    {
         ESP_LOGE(TAG, "ERROR: while creating ecep_data_len_que queue");
    }


}






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

static QueueHandle_t ecep_uart_queue;

static uint8_t gs_au8_raw_uart_shr[UART_RD_BUF_SIZE];  // Raw uart read data (shared memory)
static SemaphoreHandle_t gs_raw_uart_shr_mem_sem;


//Note: "uart events" refeers to the esp uart api. Dont confuse this "event" with ECEP events.
ecep_uart_event_task()
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

                    if(xSemaphoreTake(gs_raw_uart_shr_mem_sem, portMAX_DELAY) == pdTRUE)
                    {
                        memset(gs_au8_raw_uart_shr, 0, sizeof(gs_au8_raw_uart_shr)) // Reset shared memory (just to be sure)
                        uart_read_bytes(ECEP_UART_NUM, gs_au8_raw_uart_shr, event.size, portMAX_DELAY);
                        xSemaphoreGive(gs_raw_uart_shr_mem_sem);
                    }

                    // Sync ved at lægge noget i queuen
                    // queue is used to syncronize [task navnet]
                

                    break;

                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(ECEP_UART_NUM);
                    xQueueReset(ecep_uart_queue);
                    break;
                    
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
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
                    ESP_LOGI(TAG, "uart parity error");
                    break;

                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
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

}






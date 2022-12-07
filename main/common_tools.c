#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "common_tools.h"

static const char *TAG = "common_functions.c";

void *malloc_to_psram(uint32_t size)
{
    void *psram_addr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (psram_addr == NULL)
    {
        ESP_LOGE(TAG, "ERROR: while allocating shared memory in PSRAM");
    }

    memset(psram_addr, 0, size);

    return psram_addr;
}

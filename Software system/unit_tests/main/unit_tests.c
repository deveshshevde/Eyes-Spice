#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_private/esp_clk.h" 

const char *reset_reason_to_str(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:     return "Power-on";
        case ESP_RST_EXT:         return "External reset";
        case ESP_RST_SW:          return "Software reset";
        case ESP_RST_PANIC:       return "Exception/panic";
        case ESP_RST_INT_WDT:     return "Interrupt watchdog";
        case ESP_RST_TASK_WDT:    return "Task watchdog";
        case ESP_RST_WDT:         return "Other watchdogs";
        case ESP_RST_DEEPSLEEP:   return "Deep sleep";
        case ESP_RST_BROWNOUT:    return "Brownout";
        case ESP_RST_SDIO:        return "SDIO reset";
        default:                  return "Unknown";
    }
}

void print_memory_map(void)
{
    printf("\n========= MEMORY MAP =========\n");

    extern int _text_start, _text_end;
    extern int _data_start, _data_end;
    extern int _bss_start, _bss_end;
    extern int _heap_start, _heap_end;
    extern int _iram_start, _iram_end;

    printf(".text (code):   %p - %p\n", &_text_start, &_text_end);
    printf(".data (init):   %p - %p\n", &_data_start, &_data_end);
    printf(".bss  (zeroed): %p - %p\n", &_bss_start, &_bss_end);
    printf(".heap:          %p - %p\n", &_heap_start, &_heap_end);
    printf(".iram:          %p - %p\n", &_iram_start, &_iram_end);

    // Internal heap
    multi_heap_info_t internal_info;
    heap_caps_get_info(&internal_info, MALLOC_CAP_INTERNAL);
    printf("Heap (internal): Free=%u Largest Free Block=%u\n",
           internal_info.total_free_bytes,
           internal_info.largest_free_block);

    // PSRAM
    if (esp_psram_is_initialized()) {
        multi_heap_info_t psram_info;
        heap_caps_get_info(&psram_info, MALLOC_CAP_SPIRAM);
        printf("Heap (PSRAM):    Free=%u Largest Free Block=%u\n",
               psram_info.total_free_bytes,
               psram_info.largest_free_block);
    }

    printf("================================\n");
}


void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("\n======= EXTENDED UNIT TEST START =======\n");

    printf("ESP32 Chip with %d CPU cores, WiFi%s%s, silicon rev %d\n",
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
           chip_info.revision);

    printf("ESP-IDF Version: %s\n", esp_get_idf_version());

    uint32_t flash_size = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flash_size) == ESP_OK) {
        printf("Flash size: %lu MB\n", (unsigned long)(flash_size / (1024 * 1024)));
    }

    if (esp_psram_is_initialized()) {
        printf("PSRAM detected: %lu MB\n", (unsigned long)(esp_psram_get_size() / (1024 * 1024)));
    } else {
        printf("No PSRAM found.\n");
    }

    printf("CPU Clock: %d MHz\n", (int)(esp_clk_cpu_freq() / 1000000));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    printf("WiFi MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    printf("Heap: Total=%u, Internal=%u, PSRAM=%u\n",
           (unsigned int)esp_get_free_heap_size(),
           (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    printf("Minimum ever free heap: %u bytes\n",
           (unsigned int)esp_get_minimum_free_heap_size());

    esp_reset_reason_t reason = esp_reset_reason();
    printf("Last reset reason: %s (%d)\n", reset_reason_to_str(reason), reason);

    printf("FreeRTOS Tick Rate: %d Hz\n", configTICK_RATE_HZ);
    printf("Number of FreeRTOS tasks: %u\n", (unsigned int)uxTaskGetNumberOfTasks());
    printf("Uptime (us): %lld\n", esp_timer_get_time());

    print_memory_map();

    printf("======= EXTENDED UNIT TEST DONE =======\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

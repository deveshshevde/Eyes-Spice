#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_rom_sys.h"
#include "driver/temperature_sensor.h"

#define TAG "TOVI::Profiler"

void tovi_profiler_task(void *arg) {
    while (1) {
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);

        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);

        multi_heap_info_t heap_info_internal;
        heap_caps_get_info(&heap_info_internal, MALLOC_CAP_INTERNAL);

        multi_heap_info_t heap_info_psram;
        heap_caps_get_info(&heap_info_psram, MALLOC_CAP_SPIRAM);

        esp_reset_reason_t reset_reason = esp_reset_reason();

        const esp_partition_t* flash_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

        // Temperature Sensor Setup
        ESP_LOGI(TAG, "Install temperature sensor, expected temp ranger range: 10~50 ℃");
        temperature_sensor_handle_t temp_sensor = NULL;
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
        ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
        ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
        float tsens_value;
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));
        ESP_ERROR_CHECK(temperature_sensor_disable(temp_sensor));
        ESP_ERROR_CHECK(temperature_sensor_uninstall(temp_sensor));

        // Logging
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "               TOVI SYSTEM PROFILER               ");
        ESP_LOGI(TAG, "==================================================");

        ESP_LOGI(TAG, "> Chip Model         : %s", CONFIG_IDF_TARGET);
        ESP_LOGI(TAG, "> Chip Revision      : %d", chip_info.revision);
        ESP_LOGI(TAG, "> CPU Cores          : %d", chip_info.cores);
        ESP_LOGI(TAG, "> CPU Frequency      : %d MHz", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
        ESP_LOGI(TAG, "> IDF Version        : %s", esp_get_idf_version());

        ESP_LOGI(TAG, "> Wi-Fi Support      : %s", (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "Yes" : "No");
        ESP_LOGI(TAG, "> Bluetooth Classic  : %s", (chip_info.features & CHIP_FEATURE_BT) ? "Yes" : "No");
        ESP_LOGI(TAG, "> BLE Support        : %s", (chip_info.features & CHIP_FEATURE_BLE) ? "Yes" : "No");

        ESP_LOGI(TAG, "> MAC Address        : %02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        ESP_LOGI(TAG, "> Flash Size         : %d KB", flash_part ? flash_part->size / 1024 : 0);

    #if CONFIG_ESP32S3_SPIRAM_SUPPORT
        if (esp_psram_is_initialized()) {
            ESP_LOGI(TAG, "> PSRAM Detected     : %lu MB", (unsigned long)(esp_psram_get_size() / (1024 * 1024)));
            ESP_LOGI(TAG, "> PSRAM Free         : %d bytes", heap_info_psram.total_free_bytes);
        } else {
            ESP_LOGI(TAG, "> PSRAM Detected     : No");
        }
    #else
        ESP_LOGI(TAG, "> PSRAM Support      : Not enabled in config");
    #endif

        ESP_LOGI(TAG, "> Internal RAM Free  : %d bytes", heap_info_internal.total_free_bytes);
        ESP_LOGI(TAG, "> Internal RAM Used  : %d bytes", heap_info_internal.total_allocated_bytes);
        ESP_LOGI(TAG, "> Reset Cause        : %d", reset_reason);
        ESP_LOGI(TAG, "> Temperature        : %.2f °C (ROM Sensor)", tsens_value);
        ESP_LOGI(TAG, "> Time Since Boot    : %lld ms", esp_timer_get_time() / 1000);

        ESP_LOGI(TAG, "--------------------------------------------------");
        ESP_LOGI(TAG, " Heap Summary (by capability):");
        ESP_LOGI(TAG, "--------------------------------------------------");

        ESP_LOGI(TAG, " Default heap        : %d free", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
        ESP_LOGI(TAG, " Internal RAM        : %d free", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        ESP_LOGI(TAG, " SPIRAM (PSRAM)      : %d free", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI(TAG, " DMA-Capable         : %d free", heap_caps_get_free_size(MALLOC_CAP_DMA));

        ESP_LOGI(TAG, "--------------------------------------------------");
        ESP_LOGI(TAG, " TOVI profiler complete.");
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "");

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


void app_main(void) {
    xTaskCreate(tovi_profiler_task, "ToviProfiler", 4096, NULL, 5, NULL);
}

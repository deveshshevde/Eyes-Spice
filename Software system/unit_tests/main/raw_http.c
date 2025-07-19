#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#define WIFI_SSID       "Airtel_gaur_2614"
#define WIFI_PASS       "air97600"
#define SERVER_URL      "http://192.168.1.32:8080/upload"

#define TEST_SIZE       (16000 * 5 * 2) // 16-bit 16kHz stereo 5 sec = 160KB
#define ITERATIONS      -1  // -1 = infinite

static const char *TAG = "HTTP_BENCHMARK";

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Wi-Fi started and connecting...");
}

esp_err_t send_buffer_http_post(uint8_t *data, size_t len) {
    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .timeout_ms = 10000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field(client, (const char *)data, len);

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    return err;
}

void app_main(void) {
    wifi_init_sta();

    static uint8_t buffer[TEST_SIZE];
    memset(buffer, 0xAA, sizeof(buffer));  // Dummy audio data

    uint64_t total_bytes = 0;
    uint64_t start_time = esp_timer_get_time(); // in µs
    int i = 0;

    while (ITERATIONS < 0 || i < ITERATIONS) {
        ESP_LOGI(TAG, "Sending iteration %d...", i + 1);
        esp_err_t err = send_buffer_http_post(buffer, sizeof(buffer));

        if (err == ESP_OK) {
            total_bytes += TEST_SIZE;
        } else {
            ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        }

        i++;
        vTaskDelay(pdMS_TO_TICKS(500));  // Short delay between sends
    }

    uint64_t end_time = esp_timer_get_time();
    double elapsed_sec = (end_time - start_time) / 1000000.0;
    double mb_sent = total_bytes / (1024.0 * 1024.0);
    double mbps = mb_sent / elapsed_sec;

    ESP_LOGI(TAG, "======== Benchmark Result ========");
    ESP_LOGI(TAG, "Total Transferred: %.2f MB", mb_sent);
    ESP_LOGI(TAG, "Elapsed Time     : %.2f s", elapsed_sec);
    ESP_LOGI(TAG, "Bandwidth        : %.2f MB/s", mbps);
}
/*

[Benchmark Result]
  Total transferred: 6.26 MB
  Elapsed time     : 33.04 s
  Bandwidth        : 0.19 MB/s
[42] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.41 MB
  Elapsed time     : 33.85 s
  Bandwidth        : 0.19 MB/s
[43] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.56 MB
  Elapsed time     : 34.63 s
  Bandwidth        : 0.19 MB/s
[44] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.71 MB
  Elapsed time     : 35.54 s
  Bandwidth        : 0.19 MB/s
[45] Received 160000 bytes

[Benchmark Result]
  Total transferred: 6.87 MB
  Elapsed time     : 36.30 s
  Bandwidth        : 0.19 MB/s

*/
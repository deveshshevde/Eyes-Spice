#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_timer.h"

#define WIFI_SSID       "Airtel_gaur_2614"
#define WIFI_PASS       "air97600"

#define SERVER_IP       "192.168.1.32"
#define SERVER_PORT     8000

#define TEST_SIZE (16000*5*2)
#define ITERATIONS 10

static const char *TAG = "BENCHMARK";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// Wi-Fi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi started, waiting for connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to Wi-Fi.");
}

void tcp_benchmark_task(void *param) {
    uint8_t *tx_buffer = malloc(TEST_SIZE);
    uint8_t *rx_buffer = malloc(TEST_SIZE);

    if (!tx_buffer || !rx_buffer) {
        ESP_LOGE(TAG, "Memory allocation failed!");
        vTaskDelete(NULL);
    }

    memset(tx_buffer, 'A', TEST_SIZE);

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(SERVER_PORT);
        dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Socket creation failed");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "Connecting to %s:%d...", SERVER_IP, SERVER_PORT);
        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
            ESP_LOGE(TAG, "Connect failed");
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "Connected! Starting benchmark...");

        uint64_t start_time = esp_timer_get_time();
        size_t total_bytes = 0;

        for (int i = 0; i < ITERATIONS; i++) {
            int sent = send(sock, tx_buffer, TEST_SIZE, 0);
            if (sent < 0) {
                ESP_LOGE(TAG, "Send failed at iteration %d", i);
                break;
            }

            total_bytes += sent;

            int received = 0;
            while (received < TEST_SIZE) {
                int len = recv(sock, rx_buffer + received, TEST_SIZE - received, 0);
                if (len <= 0) {
                    ESP_LOGE(TAG, "Receive failed at iteration %d", i);
                    break;
                }
                received += len;
            }

            total_bytes += received;
            // vTaskDelay(pdMS_TO_TICKS(10));
        }

        uint64_t elapsed_us = esp_timer_get_time() - start_time;
        float seconds = elapsed_us / 1000000.0;
        float mb = (float)total_bytes / (1024.0 * 1024.0);
        float mbps = mb / seconds;

        ESP_LOGI(TAG, "Transferred: %.2f MB in %.2f sec → %.2f MB/s", mb, seconds, mbps);

        close(sock);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    free(tx_buffer);
    free(rx_buffer);
    vTaskDelete(NULL);
}


void app_main(void) {
    wifi_init_sta();
    xTaskCreate(tcp_benchmark_task, "tcp_benchmark", 8192, NULL, 5, NULL);
}


/* Result */
/* 
[Benchmark Result]
  Total transferred: 3.05 MB
  Elapsed time     : 5.50 s
  Bandwidth        : 0.55 MB/s
[Server] Connection from 192.168.1.33:59573
[1] Received and echoed 160000 bytes
[2] Received and echoed 160000 bytes
[3] Received and echoed 160000 bytes
[4] Received and echoed 160000 bytes
[5] Received and echoed 160000 bytes
[6] Received and echoed 160000 bytes
[7] Received and echoed 160000 bytes
[8] Received and echoed 160000 bytes
[9] Received and echoed 160000 bytes
[10] Received and echoed 160000 bytes
*/
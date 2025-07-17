#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"

#include "driver/i2s_pdm.h"
#include "esp_heap_caps.h"

#define SAMPLE_RATE     16000
#define RECORD_SECONDS  5 /* PROLLY MAX FOR DRAM*/
#define SAMPLE_SIZE     2 
#define BUFFER_SIZE     (SAMPLE_RATE * SAMPLE_SIZE * RECORD_SECONDS)

#define I2S_BUFFER_LEN  1024
#define VOLUME_GAIN     4 

#define WIFI_SSID       "Airtel_7774031837"
#define WIFI_PASS       "air52604"

#define I2S_MIC_CLK     GPIO_NUM_42
#define I2S_MIC_DATA    GPIO_NUM_41

static const char *TAG = "MIC_STREAM";
static i2s_chan_handle_t rx_chan = NULL;

void wifi_init_sta(void) {
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
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Wi-Fi initialized and connected");
}

void i2s_init(void) {
    i2s_chan_config_t chan_cfg = {
        .id = 0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 4,
        .dma_frame_num = I2S_BUFFER_LEN / 2,
        .auto_clear = true,
    };

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_PDM_SLOT_LEFT,
        },
        .gpio_cfg = {
            .clk = I2S_MIC_CLK,
            .din = I2S_MIC_DATA,
            .invert_flags = {
                .clk_inv = false,
            },
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG, "I2S PDM microphone initialized");
}

void send_audio_chunk(const char *data, size_t len) {
    esp_http_client_config_t config = {
        .url = "http://192.168.1.13:5000/upload",  
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_open(client, len) == ESP_OK) {
        esp_http_client_write(client, data, len);
        esp_http_client_close(client);
        ESP_LOGI(TAG, "Audio uploaded successfully");
    } else {
        ESP_LOGW(TAG, "Failed to connect to server");
    }

    esp_http_client_cleanup(client);
}

void app_main(void) {
    ESP_LOGI(TAG, "Free DRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    wifi_init_sta();
    i2s_init();

    char *full_buffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!full_buffer) {
        ESP_LOGE(TAG, "Failed to allocate recording buffer");
        return;
    }

    ESP_LOGI(TAG, "Recording audio for %d seconds...", RECORD_SECONDS);

    size_t total_read = 0;
    size_t bytes_read = 0;

    while (total_read < BUFFER_SIZE) {
        size_t to_read = I2S_BUFFER_LEN;
        if ((BUFFER_SIZE - total_read) < I2S_BUFFER_LEN)
            to_read = BUFFER_SIZE - total_read;

        esp_err_t err = i2s_channel_read(rx_chan, full_buffer + total_read, to_read, &bytes_read, portMAX_DELAY);
        if (err != ESP_OK || bytes_read == 0) {
            ESP_LOGW(TAG, "I2S read failed (%d)", err);
            break;
        }

        total_read += bytes_read;
    }

    
    for (size_t i = 0; i < total_read; i += 2) {
        int16_t *s = (int16_t *)(full_buffer + i);
        int32_t amplified = (*s) << VOLUME_GAIN;
        if (amplified > INT16_MAX) amplified = INT16_MAX;
        if (amplified < INT16_MIN) amplified = INT16_MIN;

        *s = (int16_t)amplified;
    }

    ESP_LOGI(TAG, "Recording complete, %d bytes. Sending...", total_read);
    send_audio_chunk(full_buffer, total_read);
    free(full_buffer);
    ESP_LOGI(TAG, "Done");
}

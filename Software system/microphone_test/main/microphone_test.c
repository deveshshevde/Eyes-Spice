#define FREERTOS

#if defined(BARE_METAL)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "driver/i2s_pdm.h"
#include "esp_heap_caps.h"

#define SAMPLE_RATE     16000                  // 16 kHz voice sampling
#define SAMPLE_BITS     16                     // 16-bit samples
#define SECONDS         5                      // Duration to record
#define SAMPLE_SIZE     (SAMPLE_BITS / 8)      // 2 bytes
#define TOTAL_SAMPLES   (SAMPLE_RATE * SECONDS)
#define CHUNK_SIZE      (TOTAL_SAMPLES * SAMPLE_SIZE)  // 64KB buffer
#define VOLUME_GAIN     4                      // Shift left by 2 = x4 volume

#define WIFI_SSID       "Airtel_gaur_2614"
#define WIFI_PASS       "air97600"

#define I2S_MIC_CLK     GPIO_NUM_42
#define I2S_MIC_DATA    GPIO_NUM_41

#define STREAM_URL      "http://192.168.1.32:5000/stream"

static const char *TAG = "MIC_STREAM";
static i2s_chan_handle_t rx_chan = NULL;


void high_pass_filter(int16_t *data, size_t length) {
    static int16_t prev_input = 0;
    static int16_t prev_output = 0;

    for (size_t i = 0; i < length / 2; i++) {
        int16_t x = data[i];
        int16_t y = x - prev_input + 0.9 * prev_output; // alpha ≈ 0.98
        prev_input = x;
        prev_output = y;
        data[i] = y;
    }
}
void moving_average_filter(int16_t *data, size_t length) {
    for (size_t i = 1; i < (length / 2) - 1; i++) {
        int32_t avg = (int32_t)data[i-1] + data[i] + data[i+1];
        data[i] = avg / 3;
    }
}
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

    ESP_LOGI(TAG, "Wi-Fi connected to %s", WIFI_SSID);
}

void i2s_init(void) {
    i2s_chan_config_t chan_cfg = {
        .id = 0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 4,
        .dma_frame_num = CHUNK_SIZE / 2,
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
    ESP_LOGI(TAG, "I2S microphone initialized");
}

void amplify_audio(int16_t *buffer, size_t length) {
    size_t sample_count = length / sizeof(int16_t);
    for (size_t i = 0; i < sample_count; i++) {
        int32_t amplified = buffer[i] << VOLUME_GAIN;
        if (amplified > INT16_MAX) amplified = INT16_MAX;
        if (amplified < INT16_MIN) amplified = INT16_MIN;
        buffer[i] = (int16_t)amplified;
    }
}

void stream_audio(void) {
    char *buffer = malloc(CHUNK_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        return;
    }

    while (1) {
        size_t total_read = 0;

        // Read 2 seconds of audio
        while (total_read < CHUNK_SIZE) {
            size_t bytes_read = 0;
            esp_err_t err = i2s_channel_read(rx_chan, buffer + total_read, CHUNK_SIZE - total_read, &bytes_read, portMAX_DELAY);
            if (err != ESP_OK || bytes_read == 0) {
                ESP_LOGW(TAG, "I2S read error: %d", err);
                continue;
            }
            total_read += bytes_read;
        }
        amplify_audio((int16_t *)buffer, CHUNK_SIZE);
        high_pass_filter((int16_t *)buffer, CHUNK_SIZE);
        moving_average_filter((int16_t *)buffer, CHUNK_SIZE);

        esp_http_client_config_t config = {
            .url = STREAM_URL,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 5000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

        if (esp_http_client_open(client, CHUNK_SIZE) == ESP_OK) {
            esp_http_client_write(client, buffer, CHUNK_SIZE);
            esp_http_client_close(client);
            ESP_LOGI(TAG, "🔊 Sent 2s audio: %d bytes", CHUNK_SIZE);
        } else {
            ESP_LOGW(TAG, "Failed to send audio chunk");
        }

        esp_http_client_cleanup(client);
    }

    free(buffer);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();
    i2s_init();
    stream_audio();
}
#endif // BARE_METAL

#if defined(FREERTOS)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_http_server.h"

#include "driver/i2s_pdm.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"

// ---------- Audio Parameters ----------
#define SAMPLE_RATE     16000
#define SAMPLE_BITS     16
#define SECONDS         4
#define SAMPLE_SIZE     (SAMPLE_BITS / 8)
#define TOTAL_SAMPLES   (SAMPLE_RATE * SECONDS)
#define CHUNK_SIZE      (TOTAL_SAMPLES * SAMPLE_SIZE)
#define VOLUME_GAIN     4

// ---------- Wi-Fi Config ----------
#define WIFI_SSID       "Airtel_gaur_2614"
#define WIFI_PASS       "air97600"

// ---------- I2S Pins ----------
#define I2S_MIC_CLK     GPIO_NUM_42
#define I2S_MIC_DATA    GPIO_NUM_41

// ---------- LED GPIO ----------
#define LED_GPIO        GPIO_NUM_21   

// ---------- Server URL ----------
#define AUDIO_UPLOAD_URL "http://192.168.1.32:5000/upload"

static const char *TAG = "MIC_TRIGGERED";
static i2s_chan_handle_t rx_chan = NULL;
static bool trigger_flag = false;

/* ------------ Audio Filters ------------ */
void amplify_audio(int16_t *buffer, size_t length) {
    size_t count = length / sizeof(int16_t);
    for (size_t i = 0; i < count; i++) {
        int32_t amplified = buffer[i] << VOLUME_GAIN;
        if (amplified > INT16_MAX) amplified = INT16_MAX;
        if (amplified < INT16_MIN) amplified = INT16_MIN;
        buffer[i] = (int16_t)amplified;
    }
}

void high_pass_filter(int16_t *data, size_t length) {
    static int16_t prev_input = 0, prev_output = 0;
    for (size_t i = 0; i < length / 2; i++) {
        int16_t x = data[i];
        int16_t y = x - prev_input + 0.9 * prev_output;
        prev_input = x;
        prev_output = y;
        data[i] = y;
    }
}

void moving_average_filter(int16_t *data, size_t length) {
    for (size_t i = 1; i < (length / 2) - 1; i++) {
        int32_t avg = (int32_t)data[i-1] + data[i] + data[i+1];
        data[i] = avg / 3;
    }
}

/* ------------ Wi-Fi ------------ */
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

    ESP_LOGI(TAG, "Wi-Fi connecting to %s...", WIFI_SSID);
}

/* ------------ I2S ------------ */
void i2s_init(void) {
    i2s_chan_config_t chan_cfg = {
        .id = 0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 4,
        .dma_frame_num = CHUNK_SIZE / 2,
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
    ESP_LOGI(TAG, "I2S microphone initialized");
}

/* ------------ HTTP Trigger Server ------------ */
esp_err_t trigger_handler(httpd_req_t *req) {
    trigger_flag = true;
    httpd_resp_send(req, "Trigger received", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "📥 Triggered from server");
    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t trigger_uri = {
            .uri = "/trigger",
            .method = HTTP_GET,
            .handler = trigger_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &trigger_uri);
    }
    return server;
}

/* ------------ Recording Task ------------ */
void task_triggered_audio_send(void *arg) {
    char *buffer = malloc(CHUNK_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Buffer allocation failed");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        if (!trigger_flag) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        trigger_flag = false;

        gpio_set_level(LED_GPIO, 1); // LED ON
        ESP_LOGI(TAG, "🎙️ Recording started");

        size_t total_read = 0;
        while (total_read < CHUNK_SIZE) {
            size_t bytes_read = 0;
            i2s_channel_read(rx_chan, buffer + total_read, CHUNK_SIZE - total_read, &bytes_read, portMAX_DELAY);
            total_read += bytes_read;
        }

        amplify_audio((int16_t *)buffer, CHUNK_SIZE);
        high_pass_filter((int16_t *)buffer, CHUNK_SIZE);
        moving_average_filter((int16_t *)buffer, CHUNK_SIZE);

        esp_http_client_config_t config = {
            .url = AUDIO_UPLOAD_URL,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 10000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

        if (esp_http_client_open(client, CHUNK_SIZE) == ESP_OK) {
            esp_http_client_write(client, buffer, CHUNK_SIZE);
            esp_http_client_close(client);
            ESP_LOGI(TAG, "Audio sent to server");
        } else {
            ESP_LOGW(TAG, "Failed to send audio");
        }

        esp_http_client_cleanup(client);
        gpio_set_level(LED_GPIO, 0); // LED OFF
    }

    free(buffer);
    vTaskDelete(NULL);
}

/* ------------ Main ------------ */
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0); 

    wifi_init_sta();
    i2s_init();
    start_webserver();

    xTaskCreatePinnedToCore(task_triggered_audio_send, "audio_trigger_task", 8192, NULL, 5, NULL, 1);
}

#endif // FREERTOS
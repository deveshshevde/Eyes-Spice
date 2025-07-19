idf_component_register(
    SRCS "microphone_test.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_http_client esp_http_server esp_wifi esp_wifi esp_event esp_netif nvs_flash 
)

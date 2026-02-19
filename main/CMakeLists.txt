idf_component_register(
        SRCS
            "main.c"
            "wifi.c"
            "uart.c"
            "server.c"
        EMBED_FILES "${GZ_INDEX_HTML_PATH}"
        INCLUDE_DIRS "." "include"
        REQUIRES esp_driver_gpio esp_driver_uart nvs_flash esp_wifi mdns esp_http_server app_update wifi_provisioning
)
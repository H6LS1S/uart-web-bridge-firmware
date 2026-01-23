idf_component_register(
        SRCS
            "main.c"
            "wifi.c"
            "uart.c"
            "server.c"
        EMBED_FILES "../client/dist/index.html"
        INCLUDE_DIRS "." "include"
        REQUIRES esp_driver_gpio esp_driver_uart nvs_flash esp_wifi mdns esp_http_server app_update wifi_provisioning
)
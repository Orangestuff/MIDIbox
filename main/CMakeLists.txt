idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES driver bt tinyusb esp_timer esp_http_server esp_wifi nvs_flash json esp_adc
                    )

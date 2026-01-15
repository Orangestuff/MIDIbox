idf_component_register(
    SRCS "nimble_midi.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash bt esp_hw_support
)
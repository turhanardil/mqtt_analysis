idf_component_register(SRCS "receiver_mqtt.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash mqtt esp_wifi driver)

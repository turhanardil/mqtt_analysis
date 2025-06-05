#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "esp_netif.h"
#include "cJSON.h"

static const char *TAG = "MQTT_PUBLISHER";

/* Replace these placeholders with your own broker URI and topic */
static const char *MQTT_BROKER_URI = "<YOUR_MQTT_BROKER_URI>";
static const char *MQTT_TOPIC      = "<YOUR_MQTT_TOPIC>";

/* Replace with your Wi-Fi credentials */
static const char *WIFI_SSID = "<YOUR_WIFI_SSID>";
static const char *WIFI_PASS = "<YOUR_WIFI_PASSWORD>";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_got_ip);

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid,     WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(event->client, MQTT_TOPIC, 0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s",  event->data_len,  event->data);

        /* Extract the JSON string from the payload */
        char json_string[event->data_len + 1];
        strncpy(json_string, event->data, event->data_len);
        json_string[event->data_len] = '\0';

        /* Find the first '{' and last '}' to strip extra quotes */
        char *start = strchr(json_string, '{');
        char *end   = strrchr(json_string, '}');
        if (start && end && start < end) {
            end[1] = '\0';  // Terminate right after the last brace
            ESP_LOGI(TAG, "Fixed JSON: %s", start);

            /* Parse the JSON */
            cJSON *json = cJSON_Parse(start);
            if (json == NULL) {
                ESP_LOGE(TAG, "Error parsing fixed JSON");
                return;
            }

            /* Print formatted JSON for debugging */
            char *formatted_json = cJSON_Print(json);
            if (formatted_json) {
                ESP_LOGI(TAG, "Formatted JSON: %s", formatted_json);
                cJSON_free(formatted_json);
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGE(TAG, "Invalid JSON format");
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    /* Initialize NVS (required for Wi-Fi) */
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();

    /* Wait for connection */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdTRUE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to Wi-Fi SSID:%s", WIFI_SSID);
        mqtt_app_start();
    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi SSID:%s", WIFI_SSID);
    }
}

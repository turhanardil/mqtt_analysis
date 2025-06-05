#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_LED_CONTROL";

/* Replace these placeholders with your own Wi-Fi credentials */
static const char *WIFI_SSID = "<YOUR_WIFI_SSID>";
static const char *WIFI_PASS = "<YOUR_WIFI_PASSWORD>";

/* Replace with your own MQTT broker URI */
#define MQTT_BROKER_URI       "<YOUR_MQTT_BROKER_URI>"

/* Replace with your desired MQTT topics */
#define MQTT_TOPIC_DATA       "<YOUR_MQTT_TOPIC_DATA>"
#define MQTT_TOPIC_CONTROL    "<YOUR_MQTT_TOPIC_CONTROL>"

#define LED1_GPIO_PIN 2
#define LED2_GPIO_PIN 4
#define LED3_GPIO_PIN 18
#define LED4_GPIO_PIN 19

static EventGroupHandle_t s_wifi_event_group;
static SemaphoreHandle_t   led_control_mutex; // Mutex for LED control state
static esp_mqtt_client_handle_t client;
#define WIFI_CONNECTED_BIT BIT0

typedef struct {
    bool manual_mode;
    int  led1_state;
    int  led2_state;
    int  led3_state;
    int  led4_state;
} led_control_t;

static led_control_t led_control = {
    .manual_mode = false,
    .led1_state  = 0,
    .led2_state  = 0,
    .led3_state  = 0,
    .led4_state  = 0
};

/**
 * @brief Switch the state of a given LED pin.
 */
static void switch_led(gpio_num_t led_pin, int level)
{
    ESP_LOGI(TAG, "Attempting to turn %s LED on pin %d", level ? "ON" : "OFF", led_pin);
    gpio_set_level(led_pin, level);
    ESP_LOGI(TAG, "LED %d turned %s", led_pin, level ? "ON" : "OFF");
}

/**
 * @brief MQTT event handler for incoming messages and connection events.
 */
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(event->client, MQTT_TOPIC_DATA,    0);
        esp_mqtt_client_subscribe(event->client, MQTT_TOPIC_CONTROL, 0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s",  event->data_len,  event->data);

        /* Log the actual length of the received message */
        ESP_LOGI(TAG, "Received message length: %d", event->data_len);

        if (strncmp(event->topic, MQTT_TOPIC_DATA, event->topic_len) == 0) {
            xSemaphoreTake(led_control_mutex, portMAX_DELAY);

            if (!led_control.manual_mode) {
                int received_value = atoi(event->data);
                ESP_LOGI(TAG, "Received value: %d", received_value);

                if (received_value == 0) {
                    ESP_LOGI(TAG, "Setting all LEDs ON");
                    led_control.led1_state = 1;
                    led_control.led2_state = 1;
                    led_control.led3_state = 1;
                    led_control.led4_state = 1;
                    switch_led(LED1_GPIO_PIN, 1);
                    switch_led(LED2_GPIO_PIN, 1);
                    switch_led(LED3_GPIO_PIN, 1);
                    switch_led(LED4_GPIO_PIN, 1);
                } else {
                    ESP_LOGI(TAG, "Setting all LEDs OFF");
                    led_control.led1_state = 0;
                    led_control.led2_state = 0;
                    led_control.led3_state = 0;
                    led_control.led4_state = 0;
                    switch_led(LED1_GPIO_PIN, 0);
                    switch_led(LED2_GPIO_PIN, 0);
                    switch_led(LED3_GPIO_PIN, 0);
                    switch_led(LED4_GPIO_PIN, 0);
                }
            }

            xSemaphoreGive(led_control_mutex);

        } else if (strncmp(event->topic, MQTT_TOPIC_CONTROL, event->topic_len) == 0) {
            char message[50];
            snprintf(message, sizeof(message), "%.*s", event->data_len, event->data);
            ESP_LOGI(TAG, "Parsed Message: %s", message);

            char command[10], led[10], state[10];
            if (sscanf(message, "%s %s %s", command, led, state) == 3) {
                int led_num   = atoi(led);
                int led_state = atoi(state);
                ESP_LOGI(TAG, "Control Command - Command: %s, LED: %d, State: %d",
                         command, led_num, led_state);

                xSemaphoreTake(led_control_mutex, portMAX_DELAY);

                if (strcmp(command, "mode") == 0) {
                    if (led_state == 1 || led_state == 0) {
                        led_control.manual_mode = (led_state == 1);
                        ESP_LOGI(TAG, "Manual mode %s",
                                 led_control.manual_mode ? "enabled" : "disabled");
                    } else {
                        ESP_LOGI(TAG, "Invalid state for mode command");
                    }

                } else if (led_control.manual_mode) {
                    if (led_num >= 0 && led_num <= 3) {
                        switch (led_num) {
                        case 0:
                            led_control.led1_state = led_state;
                            switch_led(LED1_GPIO_PIN, led_state);
                            break;
                        case 1:
                            led_control.led2_state = led_state;
                            switch_led(LED2_GPIO_PIN, led_state);
                            break;
                        case 2:
                            led_control.led3_state = led_state;
                            switch_led(LED3_GPIO_PIN, led_state);
                            break;
                        case 3:
                            led_control.led4_state = led_state;
                            switch_led(LED4_GPIO_PIN, led_state);
                            break;
                        default:
                            ESP_LOGI(TAG, "Invalid LED number");
                            break;
                        }
                    } else {
                        ESP_LOGI(TAG, "Invalid LED number");
                    }
                }

                xSemaphoreGive(led_control_mutex);
            } else {
                ESP_LOGI(TAG, "Invalid control message format");
            }
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

/**
 * @brief Initialize and start the MQTT client.
 */
void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

/**
 * @brief Wi-Fi event handler to manage connection state.
 */
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

/**
 * @brief Initialize Wi-Fi in station mode (using the SSID and PASS above).
 */
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

/**
 * @brief Application entry point.
 */
void app_main(void)
{
    /* Set global log level */
    esp_log_level_set("*", ESP_LOG_INFO);

    /* Initialize NVS (required by Wi-Fi) */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Start Wi-Fi in station mode */
    wifi_init_sta();

    /* Configure LED GPIO pins and ensure they're OFF initially */
    gpio_reset_pin(LED1_GPIO_PIN);
    gpio_set_direction(LED1_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED1_GPIO_PIN, 0);

    gpio_reset_pin(LED2_GPIO_PIN);
    gpio_set_direction(LED2_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED2_GPIO_PIN, 0);

    gpio_reset_pin(LED3_GPIO_PIN);
    gpio_set_direction(LED3_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED3_GPIO_PIN, 0);

    gpio_reset_pin(LED4_GPIO_PIN);
    gpio_set_direction(LED4_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED4_GPIO_PIN, 0);

    /* Create mutex for LED control */
    led_control_mutex = xSemaphoreCreateMutex();

    /* Start the MQTT client */
    mqtt_app_start();

    /* Wait until we're connected to Wi-Fi */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdTRUE,
                                           portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    }

    /* Main loop does nothing; all work is event‐driven */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

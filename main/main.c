/*
 * ESP32 environmental telemetry: BME280 (I2C) -> Wi-Fi -> MQTT.
 * Adapted from the 2026 BHT IoT laboratory implementation.
 * The original laboratory experiment was tested on an ESP32-WROVER-DEV;
 * this reviewed portfolio version has not been rebuilt or hardware-tested.
 * Configuration is supplied via ESP-IDF menuconfig; no credentials in source.
 *
 * Educational demo: original broker uses unencrypted MQTT; use authenticated
 * MQTT over TLS when moving beyond a controlled laboratory environment.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "i2c_bus.h"
#include "bme280.h"

/* Configure locally using `idf.py menuconfig` > IoT Demo Configuration.
 * These values are stored in the local sdkconfig (never commit sdkconfig).
 */
#define WIFI_SSID        CONFIG_IOT_WIFI_SSID
#define WIFI_PASS        CONFIG_IOT_WIFI_PASSWORD
#define MQTT_BROKER_URI  CONFIG_IOT_MQTT_BROKER_URI
#define MQTT_TOPIC       CONFIG_IOT_MQTT_TOPIC

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

static const char *TAG = "Labor2";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bme280_handle_t bme280 = NULL;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            xEventGroupSetBits(wifi_event_group, MQTT_CONNECTED_BIT);
            ESP_LOGI(TAG, "MQTT connected");
            break;

        case MQTT_EVENT_DISCONNECTED:
            xEventGroupClearBits(wifi_event_group, MQTT_CONNECTED_BIT);
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(mqtt_client == NULL ? ESP_FAIL : ESP_OK);

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   NULL));

    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));

    ESP_LOGI(TAG, "MQTT client started");
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting...");
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconnected =
            (wifi_event_sta_disconnected_t *) event_data;

        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi disconnected, reason: %d", disconnected->reason);
        ESP_LOGI(TAG, "Retrying...");
        esp_wifi_connect();

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

        ESP_LOGI(TAG, "Connected to WiFi");
        ESP_LOGI(TAG, "IP address: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(wifi_event_group == NULL ? ESP_FAIL : ESP_OK);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", WIFI_SSID);
}

static void bme280_init_sensor(void)
{
    i2c_bus_handle_t i2c_bus = NULL;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    ESP_ERROR_CHECK(i2c_bus == NULL ? ESP_FAIL : ESP_OK);

    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    ESP_ERROR_CHECK(bme280 == NULL ? ESP_FAIL : ESP_OK);
    ESP_ERROR_CHECK(bme280_default_init(bme280));

    ESP_LOGI(TAG, "BME280 initialized successfully");
}

void app_main(void)
{
    wifi_init_sta();

    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    mqtt_app_start();
    bme280_init_sensor();
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    while (1) {
        float temperature = 0.0;
        float humidity = 0.0;
        float pressure = 0.0;

        esp_err_t temp_rc = bme280_read_temperature(bme280, &temperature);
        esp_err_t hum_rc = bme280_read_humidity(bme280, &humidity);
        esp_err_t press_rc = bme280_read_pressure(bme280, &pressure);
        if (temp_rc != ESP_OK || hum_rc != ESP_OK || press_rc != ESP_OK) {
            ESP_LOGW(TAG, "BME280 read failed; skipping this measurement");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* MQTT may be temporarily unavailable during a reconnect. */
        if ((xEventGroupGetBits(wifi_event_group) &
             (WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT)) !=
            (WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT)) {
            ESP_LOGW(TAG, "MQTT not connected; skipping this measurement");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        char message[128];

        snprintf(message,
                 sizeof(message),
                 "{\"temperature\": %.2f, \"humidity\": %.2f, \"pressure\": %.2f}",
                 temperature,
                 humidity,
                 pressure);

        int msg_id = esp_mqtt_client_publish(mqtt_client,
                                             MQTT_TOPIC,
                                             message,
                                             0,
                                             1,
                                             0);

        if (msg_id < 0) {
            ESP_LOGW(TAG, "MQTT publish failed");
        } else {
            ESP_LOGI(TAG, "MQTT message queued: %s, msg_id=%d", message, msg_id);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

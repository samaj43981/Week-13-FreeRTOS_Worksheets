#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "QUEUE_SETS";

#define LED_SENSOR GPIO_NUM_2
#define LED_USER GPIO_NUM_4
#define LED_NETWORK GPIO_NUM_5
#define LED_TIMER GPIO_NUM_18
#define LED_PROCESSOR GPIO_NUM_19

QueueHandle_t xSensorQueue;
QueueHandle_t xUserQueue;
QueueHandle_t xNetworkQueue;
SemaphoreHandle_t xTimerSemaphore;
QueueSetHandle_t xQueueSet;

typedef struct { int sensor_id; float temperature; float humidity; uint32_t timestamp; } sensor_data_t;
typedef struct { int button_id; bool pressed; uint32_t duration_ms; } user_input_t;
typedef struct { char source[20]; char message[100]; int priority; } network_message_t;
typedef enum { MSG_SENSOR, MSG_USER, MSG_NETWORK, MSG_TIMER } message_type_t;
typedef struct { uint32_t sensor_count; uint32_t user_count; uint32_t network_count; uint32_t timer_count; } message_stats_t;
message_stats_t stats = {0, 0, 0, 0};

void sensor_task(void *pvParameters) {
    sensor_data_t sensor_data;
    int sensor_id = 1;
    ESP_LOGI(TAG, "Sensor task started");
    while (1) {
        sensor_data.sensor_id = sensor_id;
        sensor_data.temperature = 20.0 + (esp_random() % 200) / 10.0;
        sensor_data.humidity = 30.0 + (esp_random() % 400) / 10.0;
        sensor_data.timestamp = xTaskGetTickCount();
        if (xQueueSend(xSensorQueue, &sensor_data, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGI(TAG, "📊 Sensor: T=%.1f°C, H=%.1f%%", sensor_data.temperature, sensor_data.humidity);
            gpio_set_level(LED_SENSOR, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(LED_SENSOR, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(2000 + (esp_random() % 3000)));
    }
}

void user_input_task(void *pvParameters) {
    user_input_t user_input;
    ESP_LOGI(TAG, "User input task started");
    while (1) {
        user_input.button_id = 1 + (esp_random() % 3);
        user_input.pressed = true;
        user_input.duration_ms = 100 + (esp_random() % 1000);
        if (xQueueSend(xUserQueue, &user_input, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGI(TAG, "🔘 User: Button %d pressed for %dms", user_input.button_id, user_input.duration_ms);
            gpio_set_level(LED_USER, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_USER, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(3000 + (esp_random() % 5000)));
    }
}

void network_task(void *pvParameters) {
    network_message_t network_msg;
    const char* sources[] = {"WiFi", "Bluetooth", "LoRa", "Ethernet"};
    const char* messages[] = {"Status update", "Config changed", "Alert", "Sync", "Heartbeat"};
    ESP_LOGI(TAG, "Network task started");
    while (1) {
        strcpy(network_msg.source, sources[esp_random() % 4]);
        strcpy(network_msg.message, messages[esp_random() % 5]);
        network_msg.priority = 1 + (esp_random() % 5);
        if (xQueueSend(xNetworkQueue, &network_msg, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGI(TAG, "🌐 Network [%s]: %s (P:%d)", network_msg.source, network_msg.message, network_msg.priority);
            gpio_set_level(LED_NETWORK, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(LED_NETWORK, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1000 + (esp_random() % 3000))); // Default: 1-4 seconds
        // For Experiment 3, change to: vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void timer_task(void *pvParameters) {
    ESP_LOGI(TAG, "Timer task started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        if (xSemaphoreGive(xTimerSemaphore) == pdPASS) {
            ESP_LOGI(TAG, "⏰ Timer: Periodic timer fired");
            gpio_set_level(LED_TIMER, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_TIMER, 0);
        }
    }
}

void processor_task(void *pvParameters) {
    QueueSetMemberHandle_t xActivatedMember;
    sensor_data_t sensor_data;
    user_input_t user_input;
    network_message_t network_msg;
    ESP_LOGI(TAG, "Processor task started - waiting for events...");
    while (1) {
        xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
        if (xActivatedMember != NULL) {
            gpio_set_level(LED_PROCESSOR, 1);
            if (xActivatedMember == xSensorQueue) {
                if (xQueueReceive(xSensorQueue, &sensor_data, 0) == pdPASS) {
                    stats.sensor_count++;
                    ESP_LOGI(TAG, "→ Processing SENSOR data: T=%.1f°C", sensor_data.temperature);
                }
            } else if (xActivatedMember == xUserQueue) {
                if (xQueueReceive(xUserQueue, &user_input, 0) == pdPASS) {
                    stats.user_count++;
                    ESP_LOGI(TAG, "→ Processing USER input: Button %d", user_input.button_id);
                }
            } else if (xActivatedMember == xNetworkQueue) {
                if (xQueueReceive(xNetworkQueue, &network_msg, 0) == pdPASS) {
                    stats.network_count++;
                    ESP_LOGI(TAG, "→ Processing NETWORK msg: [%s]", network_msg.source);
                }
            } else if (xActivatedMember == xTimerSemaphore) {
                if (xSemaphoreTake(xTimerSemaphore, 0) == pdPASS) {
                    stats.timer_count++;
                    ESP_LOGI(TAG, "→ Processing TIMER event");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_PROCESSOR, 0);
        }
    }
}

void monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
        ESP_LOGI(TAG, "\n═══ SYSTEM MONITOR ═══");
        ESP_LOGI(TAG, "  SensorQ: %d/%d | UserQ: %d/%d | NetworkQ: %d/%d", uxQueueMessagesWaiting(xSensorQueue), 5, uxQueueMessagesWaiting(xUserQueue), 3, uxQueueMessagesWaiting(xNetworkQueue), 8);
        ESP_LOGI(TAG, "  Stats: Sensor:%lu, User:%lu, Network:%lu, Timer:%lu", stats.sensor_count, stats.user_count, stats.network_count, stats.timer_count);
        ESP_LOGI(TAG, "═══════════════════════\n");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Queue Sets Lab Starting...");
    gpio_set_direction(LED_SENSOR, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_USER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_NETWORK, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TIMER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PROCESSOR, GPIO_MODE_OUTPUT);

    xSensorQueue = xQueueCreate(5, sizeof(sensor_data_t));
    xUserQueue = xQueueCreate(3, sizeof(user_input_t));
    xNetworkQueue = xQueueCreate(8, sizeof(network_message_t));
    xTimerSemaphore = xSemaphoreCreateBinary();
    xQueueSet = xQueueCreateSet(5 + 3 + 8 + 1);

    if (xQueueSet) {
        xQueueAddToSet(xSensorQueue, xQueueSet);
        xQueueAddToSet(xUserQueue, xQueueSet);
        xQueueAddToSet(xNetworkQueue, xQueueSet);
        xQueueAddToSet(xTimerSemaphore, xQueueSet);
        ESP_LOGI(TAG, "Queue set created successfully");

        // --- Experiment 2: Disable a source ---
        // Comment out the line below to disable the sensor task
        xTaskCreate(sensor_task, "Sensor", 2048, NULL, 3, NULL);

        xTaskCreate(user_input_task, "UserInput", 2048, NULL, 3, NULL);
        xTaskCreate(network_task, "Network", 2048, NULL, 3, NULL);
        xTaskCreate(timer_task, "Timer", 2048, NULL, 2, NULL);
        xTaskCreate(processor_task, "Processor", 3072, NULL, 4, NULL);
        xTaskCreate(monitor_task, "Monitor", 2048, NULL, 1, NULL);
        ESP_LOGI(TAG, "All tasks created.");
    } else {
        ESP_LOGE(TAG, "Failed to create queue set!");
    }
}

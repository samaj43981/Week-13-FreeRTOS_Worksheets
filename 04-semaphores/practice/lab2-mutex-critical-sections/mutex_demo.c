/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "mutex_demo";

// Comment out the following line to see the race condition
#define USE_MUTEX

// Shared resource
static int shared_resource = 0;

#ifdef USE_MUTEX
// Mutex handle
static SemaphoreHandle_t mutex;
#endif

/**
 * @brief Task that increments the shared resource.
 *
 * @param pvParameters Pointer to task parameters (unused).
 */
static void increment_task(void *pvParameters)
{
    int task_num = *(int *)pvParameters;

    while (1) {
#ifdef USE_MUTEX
        // Take the mutex before accessing the shared resource
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
#endif
            // --- Critical Section Start ---
            ESP_LOGI(TAG, "Task %d: Reading shared resource: %d", task_num, shared_resource);

            // Simulate some processing time
            vTaskDelay(pdMS_TO_TICKS(50));

            int temp = shared_resource;
            temp++;

            // Simulate more processing time, increasing the chance of a context switch
            vTaskDelay(pdMS_TO_TICKS(50));

            shared_resource = temp;
            ESP_LOGI(TAG, "Task %d: Writing shared resource: %d", task_num, shared_resource);
            // --- Critical Section End ---

#ifdef USE_MUTEX
            // Give the mutex back
            xSemaphoreGive(mutex);
        }
#else
            // Without a mutex, a context switch can happen here, leading to a race condition.
#endif

        // Delay to allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Mutex Demo");

#ifdef USE_MUTEX
    // Create the mutex
    mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }
    ESP_LOGI(TAG, "Mutex created successfully");
#else
    ESP_LOGW(TAG, "Mutex is disabled. Expect race conditions!");
#endif

    // Task parameters
    static int task_num_1 = 1;
    static int task_num_2 = 2;

    // Create two tasks that will access the shared resource
    xTaskCreate(increment_task, "IncrementTask1", 2048, &task_num_1, 5, NULL);
    xTaskCreate(increment_task, "IncrementTask2", 2048, &task_num_2, 5, NULL);

    ESP_LOGI(TAG, "Two tasks created. They will now compete for the shared resource.");
}

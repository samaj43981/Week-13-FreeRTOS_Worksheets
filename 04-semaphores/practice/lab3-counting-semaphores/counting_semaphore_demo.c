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

static const char *TAG = "counting_sem_demo";

#define MAX_LICENSES 2

// Counting semaphore handle
static SemaphoreHandle_t license_semaphore;

/**
 * @brief Task that simulates acquiring a license to perform a job.
 *
 * @param pvParameters Pointer to task parameters (task number).
 */
static void worker_task(void *pvParameters)
{
    int task_num = *(int *)pvParameters;

    while (1) {
        ESP_LOGI(TAG, "Task %d: Waiting to acquire license...", task_num);

        // Take a semaphore. This will block if no licenses are available.
        if (xSemaphoreTake(license_semaphore, portMAX_DELAY) == pdTRUE) {
            // --- Resource Acquired --- //
            ESP_LOGI(TAG, "Task %d: Acquired license! Performing work.", task_num);

            // Simulate doing some work for a random amount of time
            int work_time = (esp_random() % 500) + 200;
            vTaskDelay(pdMS_TO_TICKS(work_time));

            ESP_LOGI(TAG, "Task %d: Work finished. Releasing license.", task_num);
            // --- Resource Released --- //

            // Give the semaphore back
            xSemaphoreGive(license_semaphore);

        } else {
            ESP_LOGE(TAG, "Task %d: Failed to acquire license!", task_num);
        }

        // Wait for a bit before trying to acquire a license again
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Counting Semaphore Demo");

    // Create a counting semaphore with a max count of MAX_LICENSES and an initial count of MAX_LICENSES
    license_semaphore = xSemaphoreCreateCounting(MAX_LICENSES, MAX_LICENSES);

    if (license_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create counting semaphore");
        return;
    }

    ESP_LOGI(TAG, "Created %d licenses.", MAX_LICENSES);

    // Create 5 worker tasks
    const int NUM_TASKS = 5;
    static int task_nums[NUM_TASKS];

    for (int i = 0; i < NUM_TASKS; i++) {
        task_nums[i] = i + 1;
        char task_name[20];
        sprintf(task_name, "WorkerTask%d", i + 1);
        xTaskCreate(worker_task, task_name, 2048, &task_nums[i], 5, NULL);
    }

    ESP_LOGI(TAG, "%d worker tasks created. They will now compete for %d licenses.", NUM_TASKS, MAX_LICENSES);
}

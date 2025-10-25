#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define LED_OK GPIO_NUM_2
#define LED_WARNING GPIO_NUM_4

static const char *TAG = "STACK_MONITOR";

#define STACK_WARNING_THRESHOLD 512
#define STACK_CRITICAL_THRESHOLD 256

TaskHandle_t light_task_handle = NULL;
TaskHandle_t medium_task_handle = NULL;
TaskHandle_t heavy_task_handle = NULL;
TaskHandle_t optimized_task_handle = NULL;

void dynamic_stack_monitor(TaskHandle_t task_handle, const char* task_name) {
    static UBaseType_t previous_remaining = 0;
    UBaseType_t current_remaining = uxTaskGetStackHighWaterMark(task_handle);
    if (previous_remaining != 0 && current_remaining < previous_remaining) {
        ESP_LOGW(TAG, "%s stack usage increased by %d bytes", task_name, (previous_remaining - current_remaining) * sizeof(StackType_t));
    }
    previous_remaining = current_remaining;
}

void stack_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Stack Monitor Task started");
    while (1) {
        ESP_LOGI(TAG, "\n=== STACK USAGE REPORT ===");
        TaskHandle_t tasks[] = { light_task_handle, medium_task_handle, heavy_task_handle, optimized_task_handle, xTaskGetCurrentTaskHandle() };
        const char* task_names[] = { "LightTask", "MediumTask", "HeavyTask", "OptimizedTask", "StackMonitor" };
        bool stack_warning = false;
        bool stack_critical = false;
        for (int i = 0; i < 5; i++) {
            if (tasks[i] != NULL) {
                UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(tasks[i]);
                uint32_t stack_bytes = stack_remaining * sizeof(StackType_t);
                ESP_LOGI(TAG, "%s: %d bytes remaining", task_names[i], stack_bytes);
                dynamic_stack_monitor(tasks[i], task_names[i]);
                if (stack_bytes < STACK_CRITICAL_THRESHOLD) {
                    ESP_LOGE(TAG, "CRITICAL: %s stack very low!", task_names[i]);
                    stack_critical = true;
                } else if (stack_bytes < STACK_WARNING_THRESHOLD) {
                    ESP_LOGW(TAG, "WARNING: %s stack low", task_names[i]);
                    stack_warning = true;
                }
            }
        }
        gpio_set_level(LED_OK, !stack_warning && !stack_critical);
        gpio_set_level(LED_WARNING, stack_warning || stack_critical);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void light_stack_task(void *pvParameters) {
    ESP_LOGI(TAG, "Light Stack Task started");
    while (1) {
        ESP_LOGI(TAG, "Light task cycle");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void medium_stack_task(void *pvParameters) {
    ESP_LOGI(TAG, "Medium Stack Task started");
    while (1) {
        char buffer[256];
        memset(buffer, 'A', sizeof(buffer));
        ESP_LOGI(TAG, "Medium task cycle");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void heavy_stack_task(void *pvParameters) {
    ESP_LOGI(TAG, "Heavy Stack Task started");
    while (1) {
        char large_buffer[1024];
        int large_numbers[200];
        memset(large_buffer, 'X', sizeof(large_buffer));
        ESP_LOGW(TAG, "Heavy task cycle");
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

void optimized_heavy_task(void *pvParameters) {
    ESP_LOGI(TAG, "Optimized Heavy Task started");
    char *large_buffer = malloc(1024);
    if (!large_buffer) { vTaskDelete(NULL); return; }
    while (1) {
        memset(large_buffer, 'Y', 1024);
        ESP_LOGI(TAG, "Optimized task cycle");
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Optimized task stack: %d bytes remaining", stack_remaining * sizeof(StackType_t));
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
    free(large_buffer);
}

void recursive_function(int depth) {
    char local_array[100];
    ESP_LOGI(TAG, "Recursion depth: %d, Stack remaining: %u", depth, uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    if (depth < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        recursive_function(depth + 1);
    }
}

void recursion_demo_task(void *pvParameters) {
    ESP_LOGI(TAG, "Recursion Demo Task started");
    while (1) {
        ESP_LOGW(TAG, "STARTING RECURSION DEMO");
        recursive_function(1);
        ESP_LOGW(TAG, "RECURSION DEMO COMPLETED");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    ESP_LOGE("STACK_OVERFLOW", "Task %s has overflowed its stack!", pcTaskName);
    esp_restart();
}

void test_stack_sizes(void) {
    uint32_t test_sizes[] = {512, 1024, 2048, 4096};
    for (int i = 0; i < 4; i++) {
        char task_name[20];
        snprintf(task_name, sizeof(task_name), "Test%d", test_sizes[i]);
        xTaskCreate(heavy_stack_task, task_name, test_sizes[i], NULL, 1, NULL);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== FreeRTOS Stack Monitoring Demo ===");
    gpio_config_t io_conf = { .intr_type = GPIO_INTR_DISABLE, .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = (1ULL << LED_OK) | (1ULL << LED_WARNING) };
    gpio_config(&io_conf);

    xTaskCreate(light_stack_task, "LightTask", 1024, NULL, 2, &light_task_handle);
    xTaskCreate(medium_stack_task, "MediumTask", 2048, NULL, 2, &medium_task_handle);
    // xTaskCreate(heavy_stack_task, "HeavyTask", 2048, NULL, 2, &heavy_task_handle); // Uncomment to see potential overflow
    xTaskCreate(optimized_heavy_task, "OptimizedTask", 2048, NULL, 2, &optimized_task_handle);
    xTaskCreate(recursion_demo_task, "RecursionDemo", 3072, NULL, 1, NULL);
    xTaskCreate(stack_monitor_task, "StackMonitor", 4096, NULL, 3, NULL);

    // test_stack_sizes(); // Uncomment to run stack size test

    ESP_LOGI(TAG, "All tasks created.");
}

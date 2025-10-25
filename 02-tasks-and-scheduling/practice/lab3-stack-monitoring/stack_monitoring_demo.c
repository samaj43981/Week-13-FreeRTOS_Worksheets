#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define LED_OK GPIO_NUM_2       // Stack OK indicator
#define LED_WARNING GPIO_NUM_4  // Stack warning indicator

static const char *TAG = "STACK_MONITOR";

// Stack monitoring configuration
#define STACK_WARNING_THRESHOLD 512  // bytes
#define STACK_CRITICAL_THRESHOLD 256 // bytes

// Task handles for monitoring
TaskHandle_t light_task_handle = NULL;
TaskHandle_t medium_task_handle = NULL;
TaskHandle_t heavy_task_handle = NULL;

// Stack monitoring task
void stack_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Stack Monitor Task started");
    
    while (1) {
        ESP_LOGI(TAG, "\n=== STACK USAGE REPORT ===");
        
        TaskHandle_t tasks[] = { light_task_handle, medium_task_handle, heavy_task_handle, xTaskGetCurrentTaskHandle() };
        const char* task_names[] = { "LightTask", "MediumTask", "HeavyTask", "StackMonitor" };
        bool stack_warning = false;
        bool stack_critical = false;
        
        for (int i = 0; i < 4; i++) {
            if (tasks[i] != NULL) {
                UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(tasks[i]);
                uint32_t stack_bytes = stack_remaining * sizeof(StackType_t);
                ESP_LOGI(TAG, "%s: %d bytes remaining", task_names[i], stack_bytes);
                if (stack_bytes < STACK_CRITICAL_THRESHOLD) {
                    ESP_LOGE(TAG, "CRITICAL: %s stack very low!", task_names[i]);
                    stack_critical = true;
                } else if (stack_bytes < STACK_WARNING_THRESHOLD) {
                    ESP_LOGW(TAG, "WARNING: %s stack low", task_names[i]);
                    stack_warning = true;
                }
            }
        }
        
        if (stack_critical) {
            for (int i = 0; i < 10; i++) {
                gpio_set_level(LED_WARNING, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(LED_WARNING, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            gpio_set_level(LED_OK, 0);
        } else if (stack_warning) {
            gpio_set_level(LED_WARNING, 1);
            gpio_set_level(LED_OK, 0);
        } else {
            gpio_set_level(LED_OK, 1);
            gpio_set_level(LED_WARNING, 0);
        }
        
        ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Min free heap: %d bytes", esp_get_minimum_free_heap_size());
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void light_stack_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Light Stack Task started (minimal usage)");
    int counter = 0;
    while (1) {
        counter++;
        ESP_LOGI(TAG, "Light task cycle: %d", counter);
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGD(TAG, "Light task stack: %d bytes", stack_remaining * sizeof(StackType_t));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void medium_stack_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Medium Stack Task started (moderate usage)");
    while (1) {
        char buffer[256];
        int numbers[50];
        memset(buffer, 'A', sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        for (int i = 0; i < 50; i++) numbers[i] = i * i;
        ESP_LOGI(TAG, "Medium task: buffer[0]=%c, numbers[49]=%d", buffer[0], numbers[49]);
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGD(TAG, "Medium task stack: %d bytes", stack_remaining * sizeof(StackType_t));
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void heavy_stack_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Heavy Stack Task started (high usage - watch for overflow!)");
    int cycle = 0;
    while (1) {
        cycle++;
        char large_buffer[1024];
        int large_numbers[200];
        char another_buffer[512];
        ESP_LOGW(TAG, "Heavy task cycle %d: Using large stack arrays", cycle);
        memset(large_buffer, 'X', sizeof(large_buffer) - 1);
        large_buffer[sizeof(large_buffer) - 1] = '\0';
        for (int i = 0; i < 200; i++) large_numbers[i] = i * cycle;
        snprintf(another_buffer, sizeof(another_buffer), "Cycle %d with large data processing", cycle);
        ESP_LOGI(TAG, "Heavy task: %s", another_buffer);
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        uint32_t stack_bytes = stack_remaining * sizeof(StackType_t);
        if (stack_bytes < STACK_CRITICAL_THRESHOLD) {
            ESP_LOGE(TAG, "DANGER: Heavy task stack critically low: %d bytes!", stack_bytes);
        } else {
            ESP_LOGW(TAG, "Heavy task stack: %d bytes remaining", stack_bytes);
        }
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

void recursive_function(int depth, char *buffer)
{
    char local_array[100];
    snprintf(local_array, sizeof(local_array), "Recursion depth: %d", depth);
    ESP_LOGI(TAG, "%s", local_array);
    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
    uint32_t stack_bytes = stack_remaining * sizeof(StackType_t);
    ESP_LOGI(TAG, "Depth %d: Stack remaining: %d bytes", depth, stack_bytes);
    if (stack_bytes < 200) {
        ESP_LOGE(TAG, "Stopping recursion at depth %d - stack too low!", depth);
        return;
    }
    if (depth < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        recursive_function(depth + 1, buffer);
    }
}

void recursion_demo_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Recursion Demo Task started");
    while (1) {
        ESP_LOGW(TAG, "=== STARTING RECURSION DEMO ===");
        char shared_buffer[200];
        recursive_function(1, shared_buffer);
        ESP_LOGW(TAG, "=== RECURSION DEMO COMPLETED ===");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Stack Monitoring Demo ===");
    gpio_config_t io_conf = { .intr_type = GPIO_INTR_DISABLE, .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = (1ULL << LED_OK) | (1ULL << LED_WARNING) };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "LED Indicators: GPIO2=OK, GPIO4=Warning/Critical");
    ESP_LOGI(TAG, "Creating tasks with different stack sizes...");
    xTaskCreate(light_stack_task, "LightTask", 1024, NULL, 2, &light_task_handle);
    xTaskCreate(medium_stack_task, "MediumTask", 2048, NULL, 2, &medium_task_handle);
    xTaskCreate(heavy_stack_task, "HeavyTask", 2048, NULL, 2, &heavy_task_handle);
    xTaskCreate(recursion_demo_task, "RecursionDemo", 3072, NULL, 1, NULL);
    xTaskCreate(stack_monitor_task, "StackMonitor", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "All tasks created. Monitor will report every 3 seconds.");
    ESP_LOGW(TAG, "Watch for stack warnings from Heavy Task!");
}

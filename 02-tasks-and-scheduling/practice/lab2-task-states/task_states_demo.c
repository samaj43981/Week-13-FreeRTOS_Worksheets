#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_RUNNING GPIO_NUM_2
#define LED_READY GPIO_NUM_4
#define LED_BLOCKED GPIO_NUM_5
#define LED_SUSPENDED GPIO_NUM_18

#define BUTTON1_PIN GPIO_NUM_0
#define BUTTON2_PIN GPIO_NUM_35

static const char *TAG = "TASK_STATES";

TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;
TaskHandle_t external_delete_handle = NULL;
SemaphoreHandle_t demo_semaphore = NULL;

const char* state_names[] = {"Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"};
volatile uint32_t state_changes[5] = {0};

const char* get_state_name(eTaskState state) {
    if (state <= eDeleted) return state_names[state];
    return state_names[5];
}

void update_state_display(eTaskState current_state) {
    gpio_set_level(LED_RUNNING, 0);
    gpio_set_level(LED_READY, 0);
    gpio_set_level(LED_BLOCKED, 0);
    gpio_set_level(LED_SUSPENDED, 0);
    switch (current_state) {
        case eRunning: gpio_set_level(LED_RUNNING, 1); break;
        case eReady: gpio_set_level(LED_READY, 1); break;
        case eBlocked: gpio_set_level(LED_BLOCKED, 1); break;
        case eSuspended: gpio_set_level(LED_SUSPENDED, 1); break;
        default: for (int i = 0; i < 3; i++) {
            gpio_set_level(LED_RUNNING, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(LED_RUNNING, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
    }
}

void count_state_change(eTaskState old_state, eTaskState new_state) {
    if (old_state != new_state && new_state <= eDeleted) {
        state_changes[new_state]++;
        ESP_LOGI(TAG, "State change: %s -> %s (Count: %d)", get_state_name(old_state), get_state_name(new_state), state_changes[new_state]);
    }
}

void state_demo_task(void *pvParameters) {
    ESP_LOGI(TAG, "State Demo Task started");
    eTaskState last_state = eRunning;
    while (1) {
        update_state_display(eRunning);
        ESP_LOGI(TAG, "Task is RUNNING");
        for (int i = 0; i < 1000000; i++) { volatile int dummy = i * 2; }

        update_state_display(eReady);
        ESP_LOGI(TAG, "Task will be READY");
        taskYIELD();
        vTaskDelay(pdMS_TO_TICKS(100));

        update_state_display(eBlocked);
        ESP_LOGI(TAG, "Task will be BLOCKED (waiting for semaphore)");
        if (xSemaphoreTake(demo_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Got semaphore! Task is RUNNING again");
        } else {
            ESP_LOGI(TAG, "Semaphore timeout!");
        }

        update_state_display(eBlocked);
        ESP_LOGI(TAG, "Task is BLOCKED (in vTaskDelay)");
        vTaskDelay(pdMS_TO_TICKS(1000));

        eTaskState current_state = eTaskGetState(state_demo_task_handle);
        count_state_change(last_state, current_state);
        last_state = current_state;
    }
}

void ready_state_demo_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG, "Ready state demo task running");
        for (int i = 0; i < 100000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void self_deleting_task(void *pvParameters) {
    int *lifetime = (int *)pvParameters;
    ESP_LOGI(TAG, "Self-deleting task will live for %d seconds", *lifetime);
    for (int i = *lifetime; i > 0; i--) {
        ESP_LOGI(TAG, "Self-deleting task countdown: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Self-deleting task going to DELETED state");
    vTaskDelete(NULL);
}

void external_delete_task(void *pvParameters) {
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "External delete task running: %d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void monitor_task_states(void) {
    ESP_LOGI(TAG, "=== DETAILED TASK STATE MONITOR ===");
    TaskHandle_t tasks[] = {state_demo_task_handle, control_task_handle, external_delete_handle};
    const char* task_names[] = {"StateDemo", "Control", "ExtDelete"};
    for (int i = 0; i < 3; i++) {
        if (tasks[i] != NULL) {
            eTaskState state = eTaskGetState(tasks[i]);
            ESP_LOGI(TAG, "%s: State=%s", task_names[i], get_state_name(state));
        }
    }
}

void control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Control Task started");
    bool suspended = false;
    int control_cycle = 0;
    bool external_deleted = false;
    while (1) {
        control_cycle++;
        if (gpio_get_level(BUTTON1_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (!suspended) {
                ESP_LOGW(TAG, "SUSPENDING State Demo Task");
                vTaskSuspend(state_demo_task_handle);
                update_state_display(eSuspended);
                suspended = true;
            } else {
                ESP_LOGW(TAG, "RESUMING State Demo Task");
                vTaskResume(state_demo_task_handle);
                suspended = false;
            }
            while (gpio_get_level(BUTTON1_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (gpio_get_level(BUTTON2_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGW(TAG, "GIVING SEMAPHORE");
            xSemaphoreGive(demo_semaphore);
            while (gpio_get_level(BUTTON2_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (control_cycle % 30 == 0) {
            monitor_task_states();
        }
        if (control_cycle == 150 && !external_deleted) {
            ESP_LOGW(TAG, "Deleting external task");
            if(external_delete_handle != NULL) vTaskDelete(external_delete_handle);
            external_deleted = true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void system_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System Monitor started");
    char *buffer = malloc(2048);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        vTaskDelete(NULL);
        return;
    }
    while (1) {
        ESP_LOGI(TAG, "\n=== SYSTEM MONITOR ===");
        vTaskList(buffer);
        ESP_LOGI(TAG, "Task List:\nName\t\tState\tPrio\tStack\tNum\n%s", buffer);
        vTaskGetRunTimeStats(buffer);
        ESP_LOGI(TAG, "\nRuntime Stats:\nTask\t\tAbs Time\t%%Time\n%s", buffer);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    free(buffer);
}

void app_main(void) {
    ESP_LOGI(TAG, "=== FreeRTOS Task States Demo ===");
    gpio_config_t io_conf = { .intr_type = GPIO_INTR_DISABLE, .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = (1ULL << LED_RUNNING) | (1ULL << LED_READY) | (1ULL << LED_BLOCKED) | (1ULL << LED_SUSPENDED) };
    gpio_config(&io_conf);
    gpio_config_t button_conf = { .intr_type = GPIO_INTR_DISABLE, .mode = GPIO_MODE_INPUT, .pin_bit_mask = (1ULL << BUTTON1_PIN) | (1ULL << BUTTON2_PIN), .pull_up_en = 1 };
    gpio_config(&button_conf);

    demo_semaphore = xSemaphoreCreateBinary();

    ESP_LOGI(TAG, "LEDs: GPIO2=Running, GPIO4=Ready, GPIO5=Blocked, GPIO18=Suspended");
    ESP_LOGI(TAG, "Buttons: GPIO0=Suspend/Resume, GPIO35=Give Semaphore");

    xTaskCreate(state_demo_task, "StateDemo", 4096, NULL, 3, &state_demo_task_handle);
    xTaskCreate(ready_state_demo_task, "ReadyDemo", 2048, NULL, 3, NULL);
    xTaskCreate(control_task, "Control", 3072, NULL, 4, &control_task_handle);
    xTaskCreate(system_monitor_task, "Monitor", 4096, NULL, 1, NULL);

    static int self_delete_time = 10;
    xTaskCreate(self_deleting_task, "SelfDelete", 2048, &self_delete_time, 2, NULL);
    xTaskCreate(external_delete_task, "ExtDelete", 2048, NULL, 2, &external_delete_handle);

    ESP_LOGI(TAG, "All tasks created. Monitoring task states...");
}

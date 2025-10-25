/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "timer_app_demo";

// ************************************************************************************
// ** Section 1: Software Watchdog Timer                                             **
// ************************************************************************************

#define SW_WATCHDOG_TIMEOUT_MS 2000

static TimerHandle_t sw_watchdog_timer;

// This task simulates a process that needs to be monitored.
void monitored_task(void *pvParameters)
{
    ESP_LOGI(TAG, "[Monitored Task] Starting up.");
    int counter = 0;

    while (1) {
        ESP_LOGI(TAG, "[Monitored Task] Doing some work... (%d)", counter++);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Simulate a task getting stuck
        if (counter == 5) {
            ESP_LOGW(TAG, "[Monitored Task] Oops, I'm getting stuck in a loop!");
            while(1) {
                vTaskDelay(pdMS_TO_TICKS(100)); // Stuck here
            }
        }

        // "Feed" or "Kick" the software watchdog to prevent it from timing out.
        ESP_LOGI(TAG, "[Monitored Task] Feeding the watchdog.");
        xTimerReset(sw_watchdog_timer, portMAX_DELAY);
    }
}

// Callback function for the software watchdog timer.
// This function gets called ONLY if the timer is NOT reset within its period.
void sw_watchdog_callback(TimerHandle_t xTimer)
{
    ESP_LOGE(TAG, "[SW Watchdog] ALARM! Monitored task is not responding. System might be unstable.");
    // In a real-world scenario, you might log this event, try to restart the task,
    // or even restart the entire device.
}

void setup_software_watchdog(void)
{
    ESP_LOGI(TAG, "Creating software watchdog timer with a %dms timeout.", SW_WATCHDOG_TIMEOUT_MS);
    sw_watchdog_timer = xTimerCreate("SW_Watchdog",
                                     pdMS_TO_TICKS(SW_WATCHDOG_TIMEOUT_MS),
                                     pdFALSE, // pdFALSE for one-shot timer
                                     (void *)0,
                                     sw_watchdog_callback);
    if (sw_watchdog_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create software watchdog timer.");
    } else {
        // Start the timer. The monitored_task is now responsible for resetting it.
        xTimerStart(sw_watchdog_timer, portMAX_DELAY);
        xTaskCreate(monitored_task, "MonitoredTask", 2048, NULL, 5, NULL);
    }
}

// ************************************************************************************
// ** Section 2: Button Debouncing (Optional, requires a button)                     **
// ************************************************************************************

// Uncomment the following line if you have a button connected to GPIO_NUM_4
// #define USE_DEBOUNCE_EXAMPLE

#ifdef USE_DEBOUNCE_EXAMPLE

#define DEBOUNCE_BUTTON_GPIO GPIO_NUM_4
#define DEBOUNCE_TIME_MS 50

static TimerHandle_t debounce_timer;
static QueueHandle_t gpio_evt_queue = NULL;

// ISR handler for the button press.
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    // We do not process the event here. Instead, we just reset the timer.
    // The timer callback will handle the actual logic.
    // This is a common and effective debouncing technique.
    xTimerResetFromISR(debounce_timer, NULL);
}

// Callback function for the debounce timer.
// This gets called only when the button signal has been stable (low) for DEBOUNCE_TIME_MS.
void debounce_timer_callback(TimerHandle_t xTimer)
{
    // Now we can be reasonably sure it was a real press.
    ESP_LOGI(TAG, "[Debounce] Button Pressed!");
    // You could send a message to another task from here to process the button press.
}

void setup_button_debounce(void)
{
    ESP_LOGI(TAG, "Setting up button debouncing on GPIO %d", DEBOUNCE_BUTTON_GPIO);

    debounce_timer = xTimerCreate("DebounceTimer",
                                  pdMS_TO_TICKS(DEBOUNCE_TIME_MS),
                                  pdFALSE, // One-shot timer
                                  (void *)0,
                                  debounce_timer_callback);

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Interrupt on falling edge (press)
    io_conf.pin_bit_mask = (1ULL << DEBOUNCE_BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(DEBOUNCE_BUTTON_GPIO, gpio_isr_handler, (void *)DEBOUNCE_BUTTON_GPIO);

    ESP_LOGI(TAG, "Debounce setup complete. Press the button.");
}

#endif // USE_DEBOUNCE_EXAMPLE

// ************************************************************************************
// ** Main Application                                                             **
// ************************************************************************************

void app_main(void)
{
    ESP_LOGI(TAG, "Timer Applications Demo Starting...");

    // --- Setup Software Watchdog ---
    setup_software_watchdog();

    // --- Setup Button Debounce (if enabled) ---
#ifdef USE_DEBOUNCE_EXAMPLE
    setup_button_debounce();
#else
    ESP_LOGI(TAG, "Button debounce example is disabled. To enable, uncomment #define USE_DEBOUNCE_EXAMPLE.");
#endif
}

void producer_task(void *pvParameters) {
    int event_counter = 0;
    ESP_LOGI(TAG, "Producer task started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000 + (esp_random() % 3000)));
        event_counter++;
        ESP_LOGI(TAG, "🔥 Producer: Generating event #%d", event_counter);

        // --- Default single give ---
        if (xSemaphoreGive(xBinarySemaphore) == pdTRUE) {
            stats.signals_sent++;
            ESP_LOGI(TAG, "✓ Producer: Event signaled successfully");
            gpio_set_level(LED_PRODUCER, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PRODUCER, 0);
        } else {
            ESP_LOGW(TAG, "✗ Producer: Failed to signal (semaphore already given?)");
        }

        /* --- Experiment 2: Multiple Give ---
        // Uncomment the block below to test giving the semaphore multiple times.
        // Notice that only the first 'give' succeeds because it's a binary semaphore.
        for (int i = 0; i < 3; i++) {
            if (xSemaphoreGive(xBinarySemaphore) == pdTRUE) {
                ESP_LOGI(TAG, "Give #%d succeeded", i + 1);
            } else {
                ESP_LOGW(TAG, "Give #%d failed", i + 1);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        */
    }
}

void consumer_task(void *pvParameters) {
    ESP_LOGI(TAG, "Consumer task started - waiting for events...");
    while (1) {
        ESP_LOGI(TAG, "🔍 Consumer: Waiting for event...");

        // --- Default blocking take with long timeout ---
        if (xSemaphoreTake(xBinarySemaphore, pdMS_TO_TICKS(10000)) == pdTRUE) {
            stats.signals_received++;
            ESP_LOGI(TAG, "⚡ Consumer: Event received! Processing...");
            gpio_set_level(LED_CONSUMER, 1);
            vTaskDelay(pdMS_TO_TICKS(1000 + (esp_random() % 2000)));
            gpio_set_level(LED_CONSUMER, 0);
            ESP_LOGI(TAG, "✓ Consumer: Event processed successfully");
        } else {
            ESP_LOGW(TAG, "⏰ Consumer: Timeout waiting for event");
        }

        /* --- Experiment 3: Shorter Timeout ---
        // Replace the block above with this one to test shorter timeouts.
        if (xSemaphoreTake(xBinarySemaphore, pdMS_TO_TICKS(3000)) == pdTRUE) {
            stats.signals_received++;
            ESP_LOGI(TAG, "⚡ Consumer: Event received! Processing...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "✓ Consumer: Event processed successfully");
        } else {
            ESP_LOGW(TAG, "⏰ Consumer: Timeout (3s) waiting for event");
        }
        */
    }
}

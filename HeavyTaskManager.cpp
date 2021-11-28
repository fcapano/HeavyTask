#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "HeavyTaskManager.h"
#include "HeavyTask.h"

#define TAG "HTM"

#if CONFIG_HEAVY_TASK_ENABLED

// Settings
static const int msg_queue_len = 5;

// Globals
static QueueHandle_t msg_queue;
static SemaphoreHandle_t htSemaphore = NULL;

// State
static bool configured = false;
static bool running = false;
static bool paused = false;
static TaskHandle_t * heavyTaskHandle = nullptr;

void heavyTaskMain(void * parameters)
{
    HeavyTask * ht;

    while (running && !paused) {
        if (xQueueReceive(msg_queue, &ht, 0) == pdTRUE) {
            ESP_LOGI(TAG, "run %s", ht->getName());
            ht->longTask();
            ht->longScheduled = false;
            if (xSemaphoreTake(htSemaphore, (TickType_t) 100/portTICK_PERIOD_MS ) == pdTRUE) {
                if (uxQueueMessagesWaiting(msg_queue) == 0) {
                    ESP_LOGI(TAG, "stop");
                    running = false;
                    heavyTaskHandle = nullptr;
                    xSemaphoreGive(htSemaphore);
                    continue;
                }
                xSemaphoreGive(htSemaphore);
            }
            else {
                ESP_LOGE(TAG, "run mutex failed");
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    heavyTaskHandle = nullptr;
    vTaskDelete(NULL);
}

bool addLongTask(HeavyTask * ht)
{
    if (!configured) {
        msg_queue = xQueueCreate(msg_queue_len, sizeof(HeavyTask *));
        htSemaphore = xSemaphoreCreateMutex();
        configASSERT( htSemaphore );
        configured = true;
    }

    bool ok = false;
    if (xSemaphoreTake(htSemaphore, (TickType_t) 100/portTICK_PERIOD_MS ) == pdTRUE) {
        if (ht->longScheduled) {
            ESP_LOGE(TAG, "add failed. Already scheduled");
            xSemaphoreGive(htSemaphore);
            return false;
        }
        ESP_LOGI(TAG, "add");
        ht->longScheduled = true;
        ok = xQueueSend(msg_queue, &ht, 100/portTICK_PERIOD_MS) == pdTRUE;
        if (!running && !paused) {
            running = true;
            ESP_LOGI(TAG, "start");
            xTaskCreatePinnedToCore(heavyTaskMain, "HeavyTask", 1024*8, NULL, 1, heavyTaskHandle, 1);
        }
        xSemaphoreGive(htSemaphore);
    }
    else {
        ESP_LOGE(TAG, "add mutex failed");
    }
    return ok;
}

void abortLongTask(bool pause)
{
    if (!configured) {
        paused = pause;
        return;
    }
    while(xSemaphoreTake(htSemaphore, (TickType_t) 100/portTICK_PERIOD_MS ) != pdTRUE) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if (running && heavyTaskHandle) {
        running = false;
        vTaskDelete(*heavyTaskHandle);
        heavyTaskHandle = nullptr;
    }
    paused = pause;
    ESP_LOGI(TAG, "aborted");
    xSemaphoreGive(htSemaphore);
}

void resumeLongTask()
{
    if (!configured) {
        paused = false;
        return;
    }
    while(xSemaphoreTake(htSemaphore, (TickType_t) 100/portTICK_PERIOD_MS ) != pdTRUE) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    paused = false;
    if (uxQueueMessagesWaiting(msg_queue) == 0) {
        ESP_LOGI(TAG, "restart");
        running = true;
        xTaskCreatePinnedToCore(heavyTaskMain, "HeavyTask", CONFIG_HEAVY_TASK_STACKSIZE, NULL, 1, heavyTaskHandle, 1);
    }
    else {
        ESP_LOGI(TAG, "resumed");
    }
    xSemaphoreGive(htSemaphore);
}

#else // ! CONFIG_HEAVY_TASK_ENABLED

bool addLongTask(HeavyTask * ht)
{
    ESP_LOGW(TAG, "Running on calling task");
    ht->longTask();
    ht->longScheduled = false;
    return true;
}

void abortLongTask(bool pause)
{
    return;
}

void resumeLongTask()
{
    return;
}

#endif // CONFIG_HEAVY_TASK_ENABLED

#include "dataSourceRandom.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_log.h>
#include <esp_system.h>
#include "esp_timer.h"

#include "data_interface.h"

#define PERIOD_TIME_MS  (5000)

static const char *TAG = "dataSourceRandom";
static TaskHandle_t _taskHandle;

static QueueHandle_t outboundQueue = NULL;

static dataSource_t _sourceIndex = DATA_SOURCE_0;

static void task(void* parameters);

void dataSourceRandom_init()
{
    
}

void dataSourceRandom_start()
{
    xTaskCreatePinnedToCore(task, 
                            "dataSourceRandomTask", 
                            2*1024, 
                            NULL, 
                            configMAX_PRIORITIES - 3, 
                            &_taskHandle, 
                            tskNO_AFFINITY
                            );
}

void dataSourceRandom_setOutboundQueue(QueueHandle_t queue)
{
    outboundQueue = queue;
}

void dataSourceRandom_setSourceIndex(dataSource_t sourceIndex)
{
    _sourceIndex = sourceIndex;
}

static void task(void* parameters)
{    
    ESP_LOGI(TAG, "started");

    for(;;)
    {   
        int64_t timestampForNextShot = esp_timer_get_time() + PERIOD_TIME_MS*1000;
        if (outboundQueue != NULL)
        {
            dataQueueContent_t data = {
                .unit = DATA_NO_DIMENSION,
                .value = esp_random() % 10,
            };
            ESP_LOGD(TAG, "Generated random data (%d)", data.value);
            xQueueSendToBack(outboundQueue, &data, 500 / portTICK_RATE_MS);
        }

        int64_t currentTimestamp = esp_timer_get_time();
        int64_t timeLeftMs = (timestampForNextShot - currentTimestamp) / 1000;
        
        vTaskDelay(timeLeftMs / portTICK_RATE_MS);
    }

    vTaskDelete(_taskHandle);
}
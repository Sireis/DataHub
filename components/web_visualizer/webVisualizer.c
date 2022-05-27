#include "webVisualizer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_log.h>
#include <esp_system.h>

#include "esp_timer.h"

#include <esp_https_server.h>

#include "webBase.h"
#include "data_interface.h"

#define RING_BUFFER_SIZE 1440
#define JSON_BUFFER_SIZE 4096
#define MAX_DATA_SOURCES DATA_SOURCE_COUNT

static const char *TAG = "webVisualizer";
static TaskHandle_t _taskHandle;
static QueueHandle_t inboundDataQueue;

typedef struct {
    sample_t data[RING_BUFFER_SIZE];
    uint16_t nextWriteIndex;
    bool isOverflown;
} ringBuffer_t;

ringBuffer_t ringBuffers[MAX_DATA_SOURCES];

static void task(void* parameters);

static void onWebserverCreated(httpd_handle_t* server);
static esp_err_t onGetRequestIndex(httpd_req_t* request);
static esp_err_t onGetRequestData(httpd_req_t* request);
static void addToRingBuffer(ringBuffer_t* ringBuffer, sample_t data);
static sample_t popRingBuffer(ringBuffer_t* ringBuffer);
static sample_t readFromRingBuffer(ringBuffer_t* ringBuffer, int16_t backwardsIndex);
static bool isRingBufferEmpty(ringBuffer_t* ringBuffer);
static uint16_t getMin(uint16_t a, uint16_t b);

void webVisualizer_init()
{
    ESP_LOGI(TAG, "initilizing");
    webBase_registerOnWebserverCreated(onWebserverCreated);
    ESP_LOGI(TAG, "finished initilizing");
}

void webVisualizer_start()
{
    xTaskCreatePinnedToCore(task, 
                            "webVisualizerTask", 
                            2*1024, 
                            NULL, 
                            configMAX_PRIORITIES - 3, 
                            &_taskHandle, 
                            tskNO_AFFINITY
                            );
}

void webVisualizer_setInboundQueue(QueueHandle_t queue)
{
    inboundDataQueue = queue;
}

static void task(void* parameters)
{    
    ESP_LOGI(TAG, "started");

    for(;;)
    {   
        dataQueueContent_t data;

        if (xQueueReceive(inboundDataQueue, &data, portMAX_DELAY) == pdPASS)
        {        
            ESP_LOGD(TAG, "Received data: %d", data.value);
            addToRingBuffer(&ringBuffers[data.source], data.value);
        }
    }

    vTaskDelete(_taskHandle);
}

static void onWebserverCreated(httpd_handle_t* server)
{    
    ESP_LOGD(TAG, "Using web server %p as a data source for clients", *server);
    const httpd_uri_t data = {
        .uri       = "/data",
        .method    = HTTP_GET,
        .handler   = onGetRequestData
    };

    httpd_register_uri_handler(*server, &data);
    webBase_overrideIndex(onGetRequestIndex);
}

static esp_err_t onGetRequestIndex(httpd_req_t* request)
{    
    httpd_resp_set_type(request, "text/html");

    extern const char indexHtmlStart[] asm("_binary_charts_html_start");
    extern const char indexHtmlEnd[]   asm("_binary_charts_html_end");
    uint16_t indexHtmlLength = indexHtmlEnd - indexHtmlStart;
    ESP_LOGD(TAG, "sending text of length: %d", indexHtmlLength);
    ESP_LOGD(TAG, "%s", indexHtmlStart);
    httpd_resp_send(request, indexHtmlStart, indexHtmlLength);

    return ESP_OK;
}

static esp_err_t onGetRequestData(httpd_req_t* request)
{
    httpd_resp_set_type(request, "application/json");
    uint16_t requestedCount = 1;
    ESP_LOGD(TAG, "incoming data get request: %s", request->uri);
    sscanf(request->uri, "/data?count=%hd", &requestedCount);
    ESP_LOGD(TAG, "sscanfed count: %d", requestedCount);
    char json[JSON_BUFFER_SIZE];
    char *p = json;
    p += sprintf(p, "{\"sampleDelta\": 1, \"data\": [");
    for (uint8_t i = 0; i < MAX_DATA_SOURCES; ++i)
    {
        ringBuffer_t* buffer = &ringBuffers[i];
        uint16_t availableCount = buffer->isOverflown ? RING_BUFFER_SIZE : buffer->nextWriteIndex;
        uint16_t count = getMin(requestedCount, availableCount);

        p += sprintf(p, "[");
    
        if (!isRingBufferEmpty(buffer))
        {
            for (uint16_t j = 0; j < count; ++j)
            {
                p += sprintf(p, "%d, ", readFromRingBuffer(buffer, -j));

                if (p - json > JSON_BUFFER_SIZE - 20)
                {
                    httpd_resp_send_chunk(request, json, HTTPD_RESP_USE_STRLEN);
                    p = json;
                }        
            }
            p = p - 2;
        }
        
        p += sprintf(p, "], ");
    }
    sprintf(p - 2, "]}");    
    ESP_LOGI(TAG, "%s", json);
    httpd_resp_send_chunk(request, json, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(request, json, 0);

    return ESP_OK;
}

static void addToRingBuffer(ringBuffer_t* ringBuffer, sample_t data)
{    
    ringBuffer->data[ringBuffer->nextWriteIndex] = data;

    ringBuffer->nextWriteIndex++;
    ringBuffer->nextWriteIndex = ringBuffer->nextWriteIndex % RING_BUFFER_SIZE;
}

static sample_t popRingBuffer(ringBuffer_t* ringBuffer)
{
    uint16_t readingIndex = ringBuffer->nextWriteIndex - 1;
    if (readingIndex > RING_BUFFER_SIZE)
    {
        readingIndex = RING_BUFFER_SIZE - 1;
    }

    sample_t sample = ringBuffer->data[readingIndex];
    ringBuffer->nextWriteIndex = readingIndex;
    return sample;
}

static sample_t readFromRingBuffer(ringBuffer_t* ringBuffer, int16_t backwardsIndex)
{
    uint16_t readingIndex = ringBuffer->nextWriteIndex - 1 + backwardsIndex;
    
    if (readingIndex > RING_BUFFER_SIZE)
    {
        readingIndex = RING_BUFFER_SIZE - 1 - (abs(backwardsIndex) - ringBuffer->nextWriteIndex);
    }

    ESP_LOGI(TAG, "Ring buffer: Requested: %d, Reading from %d", backwardsIndex, readingIndex);
    return ringBuffer->data[readingIndex];
}

static bool isRingBufferEmpty(ringBuffer_t* ringBuffer)
{
    ESP_LOGI(TAG, "isOverflown: %d, nexWriteIndex: %d", ringBuffer->isOverflown, ringBuffer->nextWriteIndex);
    return !ringBuffer->isOverflown && (ringBuffer->nextWriteIndex == 0);
}

static uint16_t getMin(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}
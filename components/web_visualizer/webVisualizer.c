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

#define RING_BUFFER_SIZE 1024

static const char *TAG = "webVisualizer";
static TaskHandle_t _taskHandle;
static QueueHandle_t inboundDataQueue;

typedef struct {
    uint16_t minute;
    uint8_t second;
} timestamp_t;

typedef struct {
    uint16_t value;
    timestamp_t timestamp;
} sample_t;

typedef struct {
    sample_t data[RING_BUFFER_SIZE];
    uint16_t nextWriteIndex;
    uint16_t nextReadIndex;
    bool isOverflown;
} ringBuffer_t;

ringBuffer_t ringBuffer;

static void task(void* parameters);

static void onWebserverCreated(httpd_handle_t* server);
static esp_err_t onGetRequestIndex(httpd_req_t* request);
static esp_err_t onGetRequestData(httpd_req_t* request);
static void addToRingBuffer(dataQueueContent_t data);
static sample_t popRingBuffer();
static sample_t readFromRingBuffer(int16_t backwardsIndex);
static timestamp_t convertToTimestamp(int64_t microSeconds);

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
            addToRingBuffer(data);
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
    uint16_t count = 10;
    ESP_LOGI(TAG, "incoming data get request: %s", request->uri);
    sscanf(request->uri, "/data?count=%hd", &count);
    ESP_LOGI(TAG, "sscanfed count: %d", count);
    char json[4096];
    char *p = json;
    p += sprintf(p, "{\"sampleDelta\": 1, \"data\": [");
    count = ringBuffer.isOverflown ? count : ringBuffer.nextReadIndex - 1;
    for (uint16_t i = 0; i < count; ++i)
    {
        p += sprintf(p, "%d, ", readFromRingBuffer(-i).value);
    }
    sprintf(p - 2, "]}");    
    ESP_LOGI(TAG, "%s", json);
    httpd_resp_send(request, json, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static void addToRingBuffer(dataQueueContent_t data)
{
    int64_t currentMicroSeconds = esp_timer_get_time();
    ringBuffer.data[ringBuffer.nextWriteIndex].timestamp = convertToTimestamp(currentMicroSeconds);
    ringBuffer.data[ringBuffer.nextWriteIndex].value = data.value;

    ringBuffer.nextWriteIndex++;
    ringBuffer.nextWriteIndex = ringBuffer.nextWriteIndex % RING_BUFFER_SIZE;
}

static sample_t popRingBuffer()
{
    uint16_t readingIndex = ringBuffer.nextWriteIndex - 1;
    if (readingIndex > RING_BUFFER_SIZE)
    {
        readingIndex = RING_BUFFER_SIZE - 1;
    }

    sample_t sample = ringBuffer.data[readingIndex];
    ringBuffer.nextWriteIndex = readingIndex;
    return sample;
}

static sample_t readFromRingBuffer(int16_t backwardsIndex)
{
    uint16_t readingIndex = ringBuffer.nextWriteIndex - 1 + backwardsIndex;
    
    if (readingIndex > RING_BUFFER_SIZE)
    {
        readingIndex = RING_BUFFER_SIZE - 1 - (abs(backwardsIndex) - ringBuffer.nextWriteIndex);
    }

    return ringBuffer.data[readingIndex];
}

static timestamp_t convertToTimestamp(int64_t microSeconds)
{
    timestamp_t timestamp = {
        .minute = microSeconds / 1000 / 1000 / 60,
        .second = microSeconds % (1000 * 1000 * 60),
    };

    return timestamp;
}
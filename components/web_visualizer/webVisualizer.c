#include "webVisualizer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_log.h>
#include <esp_system.h>

#include <esp_https_server.h>

#include "webBase.h"

static const char *TAG = "webVisualizer";
static TaskHandle_t _taskHandle;

static void task(void* parameters);

static void onWebserverCreated(httpd_handle_t* server);
static esp_err_t onGetRequestIndex(httpd_req_t* request);
static esp_err_t onGetRequestData(httpd_req_t* request);

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

static void task(void* parameters)
{    
    ESP_LOGI(TAG, "started");

    /*
    for(;;)
    {   
        if (xQueueReceive(inboundDataQueue, data, portMAX_DELAY) == pdPASS)
        {        
            addToRingBuffer(data);
        }
    }
    */

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
    httpd_resp_send(request, "{\"x\": 3,\"y\": 5}", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}
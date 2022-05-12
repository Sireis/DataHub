#include "webFlash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_log.h>
#include <esp_system.h>

#include <esp_https_server.h>

#include "webBase.h"
#include "esp_https_ota.h"

#define START_UPDATE 0x43

static const char *TAG = "webFlash";
static TaskHandle_t _taskHandle;

static void task(void* parameters);

static httpd_handle_t _server = NULL;

static void onWebserverCreated(httpd_handle_t* server);
static esp_err_t onGetRequestFlash(httpd_req_t* request);

void webFlash_init()
{
    ESP_LOGI(TAG, "initilizing");
    webBase_registerOnWebserverCreated(onWebserverCreated);
    ESP_LOGI(TAG, "finished initilizing");
}

void webFlash_start()
{
    xTaskCreatePinnedToCore(task, 
                            "webFlashTask", 
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

    for(;;)
    {   
        BaseType_t result; 
        BaseType_t notifiedValue = 0;
        result = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, portMAX_DELAY);

        if (result == pdPass && notifiedValue == START_DOWNLOAD)
        {        
            esp_http_client_config_t config = {
                .url = CONFIG_FIRMWARE_UPGRADE_URL,
                .cert_pem = (char *)server_cert_pem_start,
            };

            esp_https_ota_config_t otaConfig = {
                .http_config = &config,
            };

            esp_err_t ret = esp_https_ota(&otaConfig);
            if (ret == ESP_OK) 
            {
                esp_restart();
            }
            else 
            {
                return ESP_FAIL;
            }
            return ESP_OK;
        }
    }

    vTaskDelete(_taskHandle);
}

static void onWebserverCreated(httpd_handle_t* server)
{    
    ESP_LOGD(TAG, "Using web server %p for triggering ota procedure", *server);
    const httpd_uri_t flash = {
        .uri       = "/flash",
        .method    = HTTP_GET,
        .handler   = onGetRequestFlash
    };

    httpd_register_uri_handler(*server, &flash);
}

static esp_err_t onGetRequestFlash(httpd_req_t* request)
{
    httpd_resp_set_type(request, "text/html");
    httpd_resp_send(request, "System will download binary and will then restart.", HTTPD_RESP_USE_STRLEN);

    xTaskNotify()

    return ESP_OK;
}
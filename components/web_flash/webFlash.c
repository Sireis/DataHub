#include "webFlash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_log.h>
#include <esp_system.h>

#include <esp_https_server.h>
#include <sys/socket.h>

#include "webBase.h"
#include "esp_https_ota.h"

#define START_UPDATE 0x43


static const char *TAG = "webFlash";
static TaskHandle_t _taskHandle;
static char _downloadUrl[128];

static void task(void* parameters);

static void onWebserverCreated(httpd_handle_t* server);
static esp_err_t onPostRequestFlash(httpd_req_t* request);
static void getIp4AsString(int socketDescriptor, char ip[INET_ADDRSTRLEN]);

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
                            10*1024, 
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
        uint32_t result; 
        uint32_t notifiedValue = 0;
        result = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, portMAX_DELAY);

        if (result == pdPASS && notifiedValue == START_UPDATE)
        {        
            extern const uint8_t certificate[] asm("_binary_otacacert_pem_start");
            extern const uint8_t certificateEnd[] asm("_binary_otacacert_pem_start");
            uint32_t certificateLength = certificateEnd - certificate;

            extern const uint8_t key[] asm("_binary_otacacert_pem_start");
            extern const uint8_t keyEnd[] asm("_binary_otacacert_pem_start");
            uint32_t keyLength = keyEnd - key;

            const esp_http_client_config_t config = {
                .url = _downloadUrl,
                .cert_pem = (char *)certificate,
                .cert_len = certificateLength,
                //.client_key_pem = (char *)key,
                //.client_key_len = keyLength,
                .skip_cert_common_name_check = true,
            };

            ESP_LOGI(TAG, "starting binary download");
            esp_err_t ret = esp_https_ota(&config);
            if (ret == ESP_OK) 
            {
                ESP_LOGI(TAG, "ota download successful, restarting soon.");
                ESP_LOGW(TAG, "restarting in 3");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                ESP_LOGW(TAG, "restarting in 2");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                ESP_LOGW(TAG, "restarting in 1");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                ESP_LOGW(TAG, "restarting in 0");
                esp_restart();
            }
            else 
            {
                ESP_LOGE(TAG, "ota update failed");
            }
        }
    }

    vTaskDelete(_taskHandle);
}

static void onWebserverCreated(httpd_handle_t* server)
{    
    ESP_LOGD(TAG, "Using web server %p for triggering ota procedure", *server);
    const httpd_uri_t flash = {
        .uri       = "/flash",
        .method    = HTTP_POST,
        .handler   = onPostRequestFlash
    };

    httpd_register_uri_handler(*server, &flash);
}

static esp_err_t onPostRequestFlash(httpd_req_t* request)
{
    int socketDescriptor = httpd_req_to_sockfd(request);
    char ip[INET_ADDRSTRLEN];
    getIp4AsString(socketDescriptor, ip);

    char content[64];
    size_t size = ((request->content_len < sizeof(content)) ? request->content_len : sizeof(content) - 1);
    content[size] = '\0';
    int status = httpd_req_recv(request, content, size);
    if (status <= 0) 
    {  /* 0 return value indicates connection closed */
        if (status == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(request);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    httpd_resp_set_type(request, "text/html");
    httpd_resp_send(request, "System will download binary and will then restart.", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Received flash request (ip: %s, project name: %s)", ip, content);
    sprintf(_downloadUrl, "https://%s:80/%s.bin", ip, content);
    ESP_LOGD(TAG, "Download url: %s", _downloadUrl);

    xTaskNotify(_taskHandle, START_UPDATE, eSetBits);

    return ESP_OK;
}

static void getIp4AsString(int socketDescriptor, char ip[INET_ADDRSTRLEN])
{
    struct sockaddr_in6 address;
    socklen_t addressSize = sizeof(address);
    getpeername(socketDescriptor, (struct sockaddr *)&address, &addressSize);
    inet_ntop(AF_INET, &address.sin6_addr.un.u32_addr[3], ip, INET_ADDRSTRLEN);
}
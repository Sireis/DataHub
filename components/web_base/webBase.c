
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_https_server.h>
#include "esp_tls.h"

static const char *TAG = "webBase";
static TaskHandle_t _taskHandle;

static esp_err_t onGetRequestIndex(httpd_req_t *request);
static esp_err_t onGetRequestStyles(httpd_req_t *request);
static esp_err_t onGetRequestFavicon(httpd_req_t *request);
static httpd_handle_t createWebserver(void);
static void deleteWebserver(httpd_handle_t server);
static void onIpConnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void onWifiDisconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void addCertificates(httpd_ssl_config_t* configuration);
static void enableWifi(void);
static void task(void* parameters);

static httpd_handle_t _server = NULL;

static void (*onWebserverCreated[10])(httpd_handle_t* server) = {0};
static uint8_t onWebserverCreatedIndex = 0;
static esp_err_t (*onGetRequestIndexOverride)(httpd_req_t *r) = NULL;
static esp_err_t (*onGetRequestStylesOverride)(httpd_req_t *r) = NULL;
static esp_err_t (*onGetRequestFaviconOverride)(httpd_req_t *r) = NULL;

void webBase_init()
{
    ESP_LOGI(TAG, "initilizing");
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &onIpConnect, &_server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &onWifiDisconnected, &_server));    
    ESP_LOGI(TAG, "finished initilizing");
}

void webBase_start()
{
    xTaskCreatePinnedToCore(task, 
                            "webBaseTask", 
                            10*1024, 
                            NULL, 
                            configMAX_PRIORITIES - 3, 
                            &_taskHandle, 
                            tskNO_AFFINITY
                            );
}

httpd_handle_t webBase_getServer()
{
    return _server;
}

void webBase_registerOnWebserverCreated(void (*handler)(httpd_handle_t* server))
{
    onWebserverCreated[onWebserverCreatedIndex] = handler;
    onWebserverCreatedIndex++;
}

void webBase_overrideIndex(esp_err_t (*handler)(httpd_req_t *r))
{
    onGetRequestIndexOverride = handler;
}

void webBase_overrideStyles(esp_err_t (*handler)(httpd_req_t *r))
{
    onGetRequestStylesOverride = handler;
}

void webBase_overrideFavicon(esp_err_t (*handler)(httpd_req_t *r))
{
    onGetRequestFaviconOverride = handler;
}

static void task(void* parameters)
{    
    ESP_LOGI(TAG, "started");

    enableWifi();

    for(;;)
    {    
        vTaskDelay(1000);    
    }

    vTaskDelete(_taskHandle);
}

static void onIpConnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) 
    {
        *server = createWebserver();
        if (onWebserverCreatedIndex > 0)
        {
            for (uint8_t i = 0; i < onWebserverCreatedIndex; ++i)
            {                
                onWebserverCreated[i](server);
            }            
        }        
    }
}

static void onWifiDisconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    esp_wifi_connect();
}

static esp_err_t onGetRequestIndex(httpd_req_t *request)
{
    if (onGetRequestIndexOverride != NULL)
    {
        return onGetRequestIndexOverride(request);
    }
    
    httpd_resp_set_type(request, "text/html");

    extern const char indexHtmlStart[] asm("_binary_index_html_start");
    extern const char indexHtmlEnd[]   asm("_binary_index_html_end");
    uint16_t indexHtmlLength = indexHtmlEnd - indexHtmlStart;
    ESP_LOGD(TAG, "sending text of length: %d", indexHtmlLength);
    ESP_LOGD(TAG, "%s", indexHtmlStart);
    httpd_resp_send(request, indexHtmlStart, indexHtmlLength);

    return ESP_OK;
}

static esp_err_t onGetRequestStyles(httpd_req_t *request)
{
    if (onGetRequestStylesOverride != NULL)
    {
        return onGetRequestStylesOverride(request);
    }

    httpd_resp_set_type(request, "text/css");

    extern const char stylesCssStart[] asm("_binary_styles_css_start");
    extern const char stylesCssEnd[]   asm("_binary_styles_css_end");
    uint16_t stylesCssLength = stylesCssEnd - stylesCssStart;
    ESP_LOGD(TAG, "sending text of length: %d", stylesCssLength);
    ESP_LOGD(TAG, "%s", stylesCssStart);
    httpd_resp_send(request, stylesCssStart, stylesCssLength);

    return ESP_OK;
}

static esp_err_t onGetRequestFavicon(httpd_req_t *request)
{    
    if (onGetRequestFaviconOverride != NULL)
    {
        return onGetRequestFaviconOverride(request);
    }

    httpd_resp_set_type(request, "image/png");

    extern const char faviconIcoStart[] asm("_binary_favicon_png_start");
    extern const char faviconIcoEnd[]   asm("_binary_favicon_png_end");
    uint16_t faviconIcoLength = faviconIcoEnd - faviconIcoStart;
    ESP_LOGD(TAG, "sending icon of length: %d", faviconIcoLength);
    ESP_LOGD(TAG, "%s", faviconIcoStart);
    httpd_resp_send(request, faviconIcoStart, faviconIcoLength);

    return ESP_OK;
}

static httpd_handle_t createWebserver(void)
{
    httpd_handle_t server = NULL;
    
    ESP_LOGI(TAG, "Starting server");
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.httpd.max_uri_handlers = 20;
    addCertificates(&config);

    esp_err_t error = httpd_ssl_start(&server, &config);
    if (error != ESP_OK) 
    {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    ESP_LOGI(TAG, "Registering URI handlers");
    
    const httpd_uri_t root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = onGetRequestIndex
    };

    const httpd_uri_t stylesheet = {
        .uri       = "/styles.css",
        .method    = HTTP_GET,
        .handler   = onGetRequestStyles
    };
    
    const httpd_uri_t favicon = {
        .uri       = "/favicon.ico",
        .method    = HTTP_GET,
        .handler   = onGetRequestFavicon
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &stylesheet);
    httpd_register_uri_handler(server, &favicon);
    ESP_LOGD(TAG, "Created web server: %p", server);
    return server;
}

static void addCertificates(httpd_ssl_config_t* configuration)
{    
    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    configuration->cacert_pem = cacert_pem_start;
    configuration->cacert_len = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    configuration->prvtkey_pem = prvtkey_pem_start;
    configuration->prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
}

static void deleteWebserver(httpd_handle_t server)
{
    httpd_ssl_stop(server);
}

static void enableWifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Gartenstuhl3",
            .password = "46250655534745888980",
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL, 
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
}
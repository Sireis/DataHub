#include <stdio.h>
#include "nvs_flash.h"
#include "webBase.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "cmake_build_info.h"
#include "webBase.h"
#include "webFlash.h"
#include "webVisualizer.h"

#include "dataSourceRandom.h"
#include "dataSourceTemperature.h"
#include "dataSourceMoisture.h"
#include "dataSourceRaw.h"

#include "data_interface.h"

#include "mbedtls/error.h"

static void initSystemComponents();
static void initUserComponents();
static void configureComponents();
static void startComponents();

void app_main(void)
{    
    printf("Based on commit hash: %s\n", GIT_COMMIT_HASH);
    printf("Build/Flash time:     %s\n", BUILD_TIME);

    initSystemComponents();
    initUserComponents();
    configureComponents();
    startComponents();
}

void initSystemComponents()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void initUserComponents()
{
    webBase_init();
    webFlash_init();
    webVisualizer_init();
    //dataSourceRaw_init();
    dataSourceTemperature_init();
    dataSourceMoisture_init();
}

void configureComponents()
{
    QueueHandle_t queue = dataIf_createQueue(10);
    dataSourceTemperature_setOutboundQueue(queue);
    //dataSourceRaw_setOutboundQueue(queue);
    dataSourceMoisture_setOutboundQueue(queue);
    webVisualizer_setInboundQueue(queue);

    dataSourceTemperature_setSourceIndex(DATA_SOURCE_0);
    dataSourceMoisture_setSourceIndex(DATA_SOURCE_1);
    dataSourceRaw_setSourceIndex(DATA_SOURCE_2);
}

void startComponents()
{
    webBase_start();
    webFlash_start();
    webVisualizer_start();
    //dataSourceRaw_start();
    dataSourceTemperature_start();
    dataSourceMoisture_start();
}

#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void dataSourceMoisture_init();
void dataSourceMoisture_start();
void dataSourceMoisture_setOutboundQueue(QueueHandle_t queue);
void dataSourceMoisture_setSourceIndex(dataSource_t sourceIndex);
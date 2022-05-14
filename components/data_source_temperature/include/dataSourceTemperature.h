#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void dataSourceTemperature_init();
void dataSourceTemperature_start();
void dataSourceTemperature_setOutboundQueue(QueueHandle_t queue);
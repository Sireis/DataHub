#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void dataSourceRandom_init();
void dataSourceRandom_start();
void dataSourceRandom_setOutboundQueue(QueueHandle_t queue);
void dataSourceRandom_setSourceIndex(dataSource_t sourceIndex);
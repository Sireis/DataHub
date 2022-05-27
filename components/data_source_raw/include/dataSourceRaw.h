#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "data_interface.h"

void dataSourceRaw_init();
void dataSourceRaw_start();
void dataSourceRaw_setOutboundQueue(QueueHandle_t queue);
void dataSourceRaw_setSourceIndex(dataSource_t sourceIndex);
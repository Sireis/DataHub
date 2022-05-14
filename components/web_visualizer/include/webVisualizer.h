#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void webVisualizer_init();
void webVisualizer_start();
void webVisualizer_setInboundQueue(QueueHandle_t queue);